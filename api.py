import os
import sys
import subprocess
import threading
import sqlite3
import asyncio
from typing import List
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception:
                pass

manager = ConnectionManager()
loop = None

class FileCache:
    def __init__(self):
        home = os.path.expanduser("~")
        idx_dir = os.path.join(home, ".fsm_index")
        os.makedirs(idx_dir, exist_ok=True)
        self.db_path = os.path.join(idx_dir, "files_web.db")
        self.conn = sqlite3.connect(self.db_path, check_same_thread=False)
        self._init_db()

    def _init_db(self):
        with self.conn:
            self.conn.execute("""
                CREATE TABLE IF NOT EXISTS files (
                    filepath TEXT PRIMARY KEY,
                    filename TEXT,
                    size INT,
                    mtime INT,
                    parent_dir TEXT
                )
            """)
            self.conn.execute("CREATE INDEX IF NOT EXISTS idx_parent ON files(parent_dir)")

    def save_scan_results(self, root_dir, files_list):
        with self.conn:
            self.conn.executemany(
                "INSERT OR REPLACE INTO files (filepath, filename, size, mtime, parent_dir) VALUES (?, ?, ?, ?, ?)",
                [(fp, fname, sz, mt, os.path.dirname(fp)) for (fname, fp, sz, mt) in files_list]
            )

    def load_all_batch(self, bridge):
        cursor = self.conn.cursor()
        cursor.execute("SELECT filepath, filename, size, mtime FROM files")
        rows = cursor.fetchall()
        
        if not rows:
            return 0
            
        count = 0
        with bridge._lock:
            try:
                bridge.proc.stdin.write("LOAD_BATCH_BEGIN\n")
                for fp, fname, sz, mt in rows:
                    p_dir = os.path.dirname(fp)
                    if not p_dir: p_dir = "root"
                    bridge.proc.stdin.write(f"{fname}|{p_dir}|{sz}|{mt:d}\n")
                    count += 1
                bridge.proc.stdin.write("LOAD_BATCH_END\n")
                bridge.proc.stdin.flush()
            except BrokenPipeError:
                return 0
                
        # The dedicated reader loop in SubprocessBridge will catch the "BATCH_DONE|count" response
        return count

class SubprocessBridge:
    TERMINATORS = {"DUP_DONE", "ORPHAN_DONE", "ANALYTICS_DONE",
                   "BENCH_DONE", "VERSION_DONE", "BYE", "BATCH_DONE", "REC_OK", "LOADED",
                   "ROLLBACK_OK", "ROLLBACK_ERROR"}

    def __init__(self):
        self.proc = None
        self._lock = threading.Lock()
        self._start_process()
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()

    def _start_process(self):
        binary = "fsm.exe" if sys.platform == "win32" else "./fsm"
        ds_dir = os.path.dirname(os.path.abspath(__file__))
        full_bin = os.path.join(ds_dir, binary)

        if not os.path.exists(full_bin):
            print(f"ERROR: Cannot find compiled binary at {full_bin}")
            return

        self.proc = subprocess.Popen(
            [full_bin, "--machine"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            cwd=ds_dir,
        )

    def send(self, command: str, tag: str = "default"):
        if not self.proc:
            print("Process not running")
            return
        with self._lock:
            try:
                self.proc.stdin.write(command + "\n")
                self.proc.stdin.flush()
            except BrokenPipeError:
                print("Broken pipe error, restarting process...")
                self._start_process()

    def _broadcast_threadsafe(self, event: str, data: dict):
        if loop and loop.is_running():
            asyncio.run_coroutine_threadsafe(manager.broadcast({"event": event, "data": data}), loop)

    def _reader_loop(self):
        while True:
            if not self.proc:
                threading.Event().wait(1)
                continue
            
            line = self.proc.stdout.readline()
            if not line:
                print("Engine stdout closed")
                break
            
            line = line.rstrip("\n\r")
            if not line:
                continue

            parts = line.split("|")
            prefix = parts[0]
            
            # Map engine output to specific tags or just broadcast as global
            # For the current frontend, we'll broadcast as 'engine_stream'
            # We can try to guess the tag based on prefix, but broadcasting to 'global'
            # and the specific functional tags is safer.
            
            target_tags = ["global", "api_req"]
            if prefix in ("DUP", "DUP_DONE"): target_tags.append("duplicates")
            if prefix in ("ORPHAN", "ORPHAN_DONE"): target_tags.append("orphans")
            if prefix in ("MONTH", "ANALYTICS_DONE"): target_tags.append("analytics")
            if prefix in ("RESULT"):
                if len(parts) > 1 and parts[1] == "VERSION": target_tags.append("history")
                else: target_tags.append("search")
            if prefix in ("ROLLBACK_OK", "ROLLBACK_ERROR"): target_tags.append("history")
            if prefix == "INDEXING":
                self._broadcast_threadsafe("engine_progress", {"tag": "explorer", "line": line})
                continue
            if prefix in ("LOADED", "BATCH_DONE", "REC_OK"):
                target_tags.append("explorer")
                target_tags.append("global_status")

            for t in target_tags:
                self._broadcast_threadsafe("engine_stream", {"tag": t, "line": line})

            if prefix in self.TERMINATORS:
                for t in target_tags:
                    self._broadcast_threadsafe("engine_done", {"tag": t})

cache = None
bridge = None

@app.on_event("startup")
async def startup_event():
    global cache, bridge, loop
    loop = asyncio.get_running_loop()
    cache = FileCache()
    bridge = SubprocessBridge()
    threading.Thread(target=cache.load_all_batch, args=(bridge,), daemon=True).start()

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_json()
            cmd = data.get("command", "")
            tag = data.get("tag", "api_req")
            if cmd == "STATUS":
                count = 0
                if cache:
                    cursor = cache.conn.cursor()
                    cursor.execute("SELECT COUNT(*) FROM files")
                    row = cursor.fetchone()
                    if row: count = row[0]
                await websocket.send_json({
                    "event": "engine_stream", 
                    "data": {"tag": tag, "line": f"INDEX_STATUS|{count}"}
                })
            elif bridge:
                bridge.send(cmd, tag)
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        manager.disconnect(websocket)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("api:app", host="127.0.0.1", port=8000, reload=True)
