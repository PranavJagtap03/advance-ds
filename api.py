import os
import sys
import subprocess
import threading
import sqlite3
import asyncio
import uuid
import time as _time
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
        # M-6: Store (ws, conn_id) tuples
        self.active_connections: List[tuple] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        conn_id = str(uuid.uuid4())
        self.active_connections.append((websocket, conn_id))
        return conn_id

    def disconnect(self, websocket: WebSocket):
        self.active_connections = [(ws, cid) for ws, cid in self.active_connections if ws is not websocket]

    # M-6: Send to a specific connection by conn_id
    async def send_to(self, conn_id: str, message: dict):
        for ws, cid in self.active_connections:
            if cid == conn_id:
                try:
                    await ws.send_json(message)
                except Exception:
                    pass
                return

    # M-6: Broadcast to all connections
    async def broadcast_all(self, message: dict):
        for ws, _ in self.active_connections:
            try:
                await ws.send_json(message)
            except Exception:
                pass

manager = ConnectionManager()
loop = None

class FileCache:
    def __init__(self):
        # H-1: Thread-safe SQLite access
        self._db_lock = threading.Lock()
        home = os.path.expanduser("~")
        idx_dir = os.path.join(home, ".fsm_index")
        os.makedirs(idx_dir, exist_ok=True)
        self.db_path = os.path.join(idx_dir, "files_web.db")
        self.conn = sqlite3.connect(self.db_path, check_same_thread=False)
        self._init_db()

    def _init_db(self):
        with self._db_lock:
            with self.conn:
                self.conn.execute("""
                    CREATE TABLE IF NOT EXISTS files (
                        filepath TEXT PRIMARY KEY,
                        filename TEXT,
                        size INT,
                        mtime INT,
                        parent_dir TEXT,
                        trueType TEXT,
                        entropy REAL
                    )
                """)
                self.conn.execute("CREATE INDEX IF NOT EXISTS idx_parent ON files(parent_dir)")
                
                # Migrations for existing databases
                try:
                    self.conn.execute("ALTER TABLE files ADD COLUMN trueType TEXT DEFAULT 'DATA'")
                except sqlite3.OperationalError:
                    pass
                try:
                    self.conn.execute("ALTER TABLE files ADD COLUMN entropy REAL DEFAULT 0.0")
                except sqlite3.OperationalError:
                    pass

    def save_scan_results(self, root_dir, files_list):
        # files_list items: (fname, path, sz, mt, ttype, ent)
        with self._db_lock:
            with self.conn:
                self.conn.executemany(
                    "INSERT OR REPLACE INTO files (filepath, filename, size, mtime, parent_dir, trueType, entropy) VALUES (?, ?, ?, ?, ?, ?, ?)",
                    [(fp, fname, sz, mt, os.path.dirname(fp), ttype, ent) for (fname, fp, sz, mt, ttype, ent) in files_list]
                )

    def load_all_batch(self, bridge):
        with self._db_lock:
            cursor = self.conn.cursor()
            cursor.execute("SELECT filepath, filename, size, mtime, trueType, entropy FROM files")
            rows = cursor.fetchall()
        
        if not rows:
            return 0
            
        count = 0
        with bridge._lock:
            try:
                bridge.proc.stdin.write("LOAD_BATCH_BEGIN\n")
                for row in rows:
                    fp, fname, sz, mt, ttype, ent = row
                    p_dir = os.path.dirname(fp)
                    if not p_dir: p_dir = "root"
                    ttype = ttype or "DATA"
                    ent = ent or 0.0
                    bridge.proc.stdin.write(f"{fname}|{p_dir}|{sz}|{mt:d}|{ttype}|{ent:.4f}\n")
                    count += 1
                bridge.proc.stdin.write("LOAD_BATCH_END\n")
                bridge.proc.stdin.flush()
            except BrokenPipeError:
                return 0
                
        return count

class SubprocessBridge:
    TERMINATORS = {"DUP_DONE", "ORPHAN_DONE", "ANALYTICS_DONE",
                   "BENCH_DONE", "VERSION_DONE", "BYE", "BATCH_DONE", "REC_OK", "LOADED",
                   "ROLLBACK_OK", "ROLLBACK_ERROR", "CACHE_DONE", "TYPE_DONE", "SUSPICIOUS_DONE",
                   "RECLAIM_DONE"}  # H-6: Added RECLAIM_DONE

    def __init__(self):
        self.proc = None
        self._lock = threading.Lock()
        self.cache_buffer = []
        # M-6: Pending command conn_id tracking
        self.pending_commands = {}
        # E-4: Restart tracking
        self._restart_count = 0
        self._restart_times = []
        self._start_process()
        self._start_reader()

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

    def _start_reader(self):
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()

    def send(self, command: str, tag: str = "default", conn_id: str = None):
        if not self.proc:
            print("Process not running")
            return
        # M-6: Track conn_id for this tag
        if conn_id and tag:
            self.pending_commands[tag] = conn_id
        with self._lock:
            try:
                self.proc.stdin.write(command + "\n")
                self.proc.stdin.flush()
            except BrokenPipeError:
                print("Broken pipe error, restarting process...")
                self._try_restart()

    def _broadcast_threadsafe(self, event: str, data: dict):
        if loop and loop.is_running():
            asyncio.run_coroutine_threadsafe(manager.broadcast_all({"event": event, "data": data}), loop)

    # H-2: Retry-safe cache flush
    def _flush_cache_buffer(self):
        retries = 0
        while retries < 5:
            if cache and self.cache_buffer:
                cache.save_scan_results("root", self.cache_buffer)
                self.cache_buffer = []
                return
            retries += 1
            _time.sleep(0.1)
        # After 5 retries, discard buffer
        if self.cache_buffer:
            print(f"WARNING: Discarding {len(self.cache_buffer)} cache items (cache not ready)")
            self.cache_buffer = []

    # E-4: Auto-restart with fatal cutoff
    def _try_restart(self):
        now = _time.time()
        self._restart_times = [t for t in self._restart_times if now - t < 60]
        if len(self._restart_times) >= 5:
            print("FATAL: Engine crashed >5 times in 60s, stopping retries")
            self._broadcast_threadsafe("engine_fatal", {"message": "Engine crashed repeatedly"})
            return
        self._restart_times.append(now)
        _time.sleep(2)
        self.cache_buffer = []  # E-4+H-2: Reset stale buffer
        self._start_process()
        self._start_reader()

    def _reader_loop(self):
        while True:
            if not self.proc:
                threading.Event().wait(1)
                continue
            
            line = self.proc.stdout.readline()
            if not line:
                print("Engine stdout closed")
                self._try_restart()
                break
            
            line = line.rstrip("\n\r")
            if not line:
                continue

            parts = line.split("|")
            prefix = parts[0]
            
            if prefix == "CACHE_ITEM" and len(parts) >= 5:
                # CACHE_ITEM|fname|path|size|mtime|trueType|entropy
                fname = parts[1]
                path = parts[2]
                sz = int(parts[3])
                mt = int(parts[4])
                ttype = parts[5] if len(parts) > 5 else "DATA"
                ent = float(parts[6]) if len(parts) > 6 else 0.0
                self.cache_buffer.append((fname, path, sz, mt, ttype, ent))
                continue

            if prefix == "CACHE_DONE":
                self._flush_cache_buffer()
            
            target_tags = ["global", "api_req"]
            if prefix in ("DUP", "DUP_DONE"): target_tags.append("duplicates")
            if prefix in ("ORPHAN", "ORPHAN_DONE"): target_tags.append("orphans")
            if prefix in ("MONTH", "ANALYTICS_DONE"): target_tags.append("analytics")
            if prefix in ("SUSPICIOUS", "SUSPICIOUS_DONE"): target_tags.append("security")
            if prefix in ("TYPE", "TYPE_DONE"): target_tags.append("analytics")
            # H-6: Route RECLAIM responses to analytics
            if prefix in ("REC", "RECLAIM_DONE"): target_tags.append("analytics")
            if prefix in ("REC", "RECLAIM_DONE"): target_tags.append("duplicates")
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
                if prefix == "LOADED":
                    self.send("DUMP_CACHE")

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
    conn_id = await manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_json()
            cmd = data.get("command", "")
            tag = data.get("tag", "api_req")
            client_conn_id = data.get("conn_id", conn_id)
            if cmd == "STATUS":
                count = 0
                if cache:
                    with cache._db_lock:
                        cursor = cache.conn.cursor()
                        cursor.execute("SELECT COUNT(*) FROM files")
                        row = cursor.fetchone()
                        if row: count = row[0]
                await websocket.send_json({
                    "event": "engine_stream", 
                    "data": {"tag": tag, "line": f"INDEX_STATUS|{count}"}
                })
            elif bridge:
                bridge.send(cmd, tag, client_conn_id)
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        manager.disconnect(websocket)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("api:app", host="127.0.0.1", port=8000, reload=True)
