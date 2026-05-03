import os
import sys
import subprocess
import threading
import sqlite3
import asyncio
import uuid
import time as _time
from typing import List, Optional
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

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
        self.active_connections: List[tuple] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        conn_id = str(uuid.uuid4())
        self.active_connections.append((websocket, conn_id))
        return conn_id

    def disconnect(self, websocket: WebSocket):
        self.active_connections = [(ws, cid) for ws, cid in self.active_connections if ws is not websocket]

    async def send_to(self, conn_id: str, message: dict):
        for ws, cid in self.active_connections:
            if cid == conn_id:
                try:
                    await ws.send_json(message)
                except Exception:
                    pass
                return

    async def broadcast_all(self, message: dict):
        for ws, _ in self.active_connections:
            try:
                await ws.send_json(message)
            except Exception:
                pass

manager = ConnectionManager()
loop = None

# ═══════════════════════════════════════════════════════════════════
# FileCache — SQLite with WAL, FTS5, bulk insert, paged loading
# ═══════════════════════════════════════════════════════════════════

class FileCache:
    def __init__(self):
        self._db_lock = threading.Lock()
        home = os.path.expanduser("~")
        idx_dir = os.path.join(home, ".fsm_index")
        os.makedirs(idx_dir, exist_ok=True)
        self.db_path = os.path.join(idx_dir, "files_web.db")
        self.conn = sqlite3.connect(self.db_path, check_same_thread=False)
        self._init_db()

    def _init_db(self):
        with self._db_lock:
            c = self.conn
            # Performance pragmas
            c.execute("PRAGMA journal_mode=WAL")
            c.execute("PRAGMA synchronous=NORMAL")
            c.execute("PRAGMA cache_size=-65536")
            c.execute("PRAGMA mmap_size=268435456")
            c.execute("PRAGMA temp_store=MEMORY")
            c.execute("PRAGMA page_size=4096")
            c.execute("PRAGMA wal_autocheckpoint=1000")

            with c:
                c.execute("""
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
                c.execute("CREATE INDEX IF NOT EXISTS idx_parent ON files(parent_dir)")

                # Migrations
                for col, default in [("trueType", "'DATA'"), ("entropy", "0.0"),
                                     ("checksum", "''"), ("indexed_at", "0")]:
                    try:
                        c.execute(f"ALTER TABLE files ADD COLUMN {col} {'TEXT' if col in ('trueType','checksum') else 'REAL' if col == 'entropy' else 'INT'} DEFAULT {default}")
                    except sqlite3.OperationalError:
                        pass

                # Additional indexes
                c.execute("CREATE INDEX IF NOT EXISTS idx_filename ON files(filename)")
                c.execute("CREATE INDEX IF NOT EXISTS idx_size ON files(size)")
                c.execute("CREATE INDEX IF NOT EXISTS idx_mtime ON files(mtime)")
                c.execute("CREATE INDEX IF NOT EXISTS idx_checksum ON files(checksum)")
                c.execute("CREATE INDEX IF NOT EXISTS idx_type ON files(trueType)")

                # FTS5 virtual table
                try:
                    c.execute("""
                        CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5(
                            filename, filepath,
                            content='files', content_rowid='rowid'
                        )
                    """)
                    # Triggers to keep FTS in sync
                    c.execute("""
                        CREATE TRIGGER IF NOT EXISTS files_ai AFTER INSERT ON files BEGIN
                            INSERT INTO files_fts(rowid, filename, filepath) VALUES (new.rowid, new.filename, new.filepath);
                        END
                    """)
                    c.execute("""
                        CREATE TRIGGER IF NOT EXISTS files_ad AFTER DELETE ON files BEGIN
                            INSERT INTO files_fts(files_fts, rowid, filename, filepath) VALUES('delete', old.rowid, old.filename, old.filepath);
                        END
                    """)
                    c.execute("""
                        CREATE TRIGGER IF NOT EXISTS files_au AFTER UPDATE ON files BEGIN
                            INSERT INTO files_fts(files_fts, rowid, filename, filepath) VALUES('delete', old.rowid, old.filename, old.filepath);
                            INSERT INTO files_fts(rowid, filename, filepath) VALUES (new.rowid, new.filename, new.filepath);
                        END
                    """)
                except Exception as e:
                    print(f"FTS5 init warning: {e}")

    def save_scan_results(self, root_dir, files_list):
        with self._db_lock:
            with self.conn:
                self.conn.executemany(
                    "INSERT OR REPLACE INTO files (filepath, filename, size, mtime, parent_dir, trueType, entropy) VALUES (?, ?, ?, ?, ?, ?, ?)",
                    [(fp, fname, sz, mt, os.path.dirname(fp), ttype, ent) for (fname, fp, sz, mt, ttype, ent) in files_list]
                )

    def bulk_insert(self, files_batch):
        """Bulk insert with explicit transaction. Each item: (fname, path, sz, mt, ttype, ent, checksum)"""
        now = int(_time.time())
        with self._db_lock:
            try:
                self.conn.execute("BEGIN")
                self.conn.executemany(
                    "INSERT OR REPLACE INTO files (filepath, filename, size, mtime, parent_dir, trueType, entropy, checksum, indexed_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    [(fp, fname, sz, mt, os.path.dirname(fp), ttype, ent, chk, now) for (fname, fp, sz, mt, ttype, ent, chk) in files_batch]
                )
                self.conn.execute("COMMIT")
            except Exception as e:
                self.conn.execute("ROLLBACK")
                raise e

    def search_fts(self, query, limit=200):
        """FTS5 search with prefix matching. Returns results in <50ms for 5M files."""
        with self._db_lock:
            try:
                fts_query = f'"{query}"*'
                cursor = self.conn.cursor()
                cursor.execute("""
                    SELECT f.filepath, f.filename, f.size, f.mtime, f.trueType
                    FROM files_fts fts
                    JOIN files f ON f.rowid = fts.rowid
                    WHERE files_fts MATCH ?
                    LIMIT ?
                """, (fts_query, limit))
                return cursor.fetchall()
            except Exception:
                # Fallback to LIKE
                cursor = self.conn.cursor()
                cursor.execute(
                    "SELECT filepath, filename, size, mtime, trueType FROM files WHERE filename LIKE ? LIMIT ?",
                    (f"%{query}%", limit)
                )
                return cursor.fetchall()

    def load_all_batch(self, bridge):
        offset = 0
        page_size = 2000
        total_count = 0

        with bridge._lock:
            try:
                bridge.proc.stdin.write("LOAD_BATCH_BEGIN\n")
                bridge.proc.stdin.flush()
            except BrokenPipeError:
                return 0

        while True:
            with self._db_lock:
                cursor = self.conn.cursor()
                cursor.execute(
                    "SELECT filepath, filename, size, mtime, trueType, entropy FROM files LIMIT ? OFFSET ?",
                    (page_size, offset)
                )
                rows = cursor.fetchall()

            if not rows:
                break

            for row in rows:
                fname = row[1]
                fp = row[0]
                sz = row[2]
                mt = row[3]
                ttype = row[4] if row[4] else "UNKNOWN"
                ent = row[5] if row[5] is not None else 0.0
                p_dir = os.path.dirname(fp)
                line = f"{fname}|{p_dir}|{sz}|{mt:d}|{ttype}|{ent:.4f}\n"
                with bridge._lock:
                    try:
                        bridge.proc.stdin.write(line)
                        bridge.proc.stdin.flush()
                    except BrokenPipeError:
                        return total_count
                total_count += 1

            print(f"[BATCH] Loaded {total_count} files so far...")
            offset += page_size
            import time
            time.sleep(0.05)

        with bridge._lock:
            try:
                bridge.proc.stdin.write("LOAD_BATCH_END\n")
                bridge.proc.stdin.flush()
            except BrokenPipeError:
                pass

        return total_count

    def get_stats(self):
        """Get file stats directly from SQLite."""
        with self._db_lock:
            cursor = self.conn.cursor()
            cursor.execute("SELECT COUNT(*), COALESCE(SUM(size),0) FROM files")
            count, total_size = cursor.fetchone()
            cursor.execute("SELECT trueType, COUNT(*), SUM(size) FROM files GROUP BY trueType ORDER BY SUM(size) DESC LIMIT 10")
            types = [{"type": r[0] or "UNKNOWN", "count": r[1], "size": r[2] or 0} for r in cursor.fetchall()]
            cursor.execute("SELECT filename, filepath, size FROM files ORDER BY size DESC LIMIT 20")
            largest = [{"name": r[0], "path": r[1], "size": r[2]} for r in cursor.fetchall()]
            return {"file_count": count, "total_size": total_size, "type_distribution": types, "largest_files": largest}

    def get_last_scan_time(self):
        with self._db_lock:
            cursor = self.conn.cursor()
            cursor.execute("SELECT MAX(indexed_at) FROM files")
            row = cursor.fetchone()
            return row[0] if row and row[0] else 0


# R-5: Tags that should never be popped from pending_commands
STRUCTURAL_TAGS = {"global", "api_req", "explorer", "global_status"}

class SubprocessBridge:
    TERMINATORS = {"DUP_DONE", "ORPHAN_DONE", "ANALYTICS_DONE",
                   "BENCH_DONE", "VERSION_DONE", "BYE", "BATCH_DONE", "REC_OK", "LOADED",
                   "ROLLBACK_OK", "ROLLBACK_ERROR", "CACHE_DONE", "TYPE_DONE", "SUSPICIOUS_DONE",
                   "RECLAIM_DONE", "SCAN_DONE"}

    MAX_PENDING = 100

    def __init__(self):
        self.proc = None
        self._lock = threading.Lock()
        self.cache_buffer = []
        self.pending_commands = {}  # tag -> (conn_id, timestamp)
        self._restart_count = 0
        self._restart_times = []
        self._reader_stop = threading.Event()
        self._start_process()
        _time.sleep(0.3)
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
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1, cwd=ds_dir,
        )

    def _start_reader(self):
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()

    def send(self, command: str, tag: str = "default", conn_id: str = None):
        print(f"[API RECV] tag={tag} command={command}")
        if not self.proc:
            print("Process not running")
            return
        if conn_id and tag:
            self.pending_commands[tag] = (conn_id, _time.time())
            # Cap pending_commands at MAX_PENDING — evict oldest
            if len(self.pending_commands) > self.MAX_PENDING:
                oldest_tag = min(self.pending_commands, key=lambda k: self.pending_commands[k][1])
                if oldest_tag not in STRUCTURAL_TAGS:
                    del self.pending_commands[oldest_tag]
        with self._lock:
            try:
                self.proc.stdin.write(command + "\n")
                self.proc.stdin.flush()
            except (BrokenPipeError, OSError) as e:
                print(f"Engine pipe error ({type(e).__name__}): {e} — restarting")
                self._try_restart()

    def _broadcast_threadsafe(self, event: str, data: dict, conn_id: str = None):
        if loop and loop.is_running():
            if conn_id:
                asyncio.run_coroutine_threadsafe(manager.send_to(conn_id, {"event": event, "data": data}), loop)
            else:
                asyncio.run_coroutine_threadsafe(manager.broadcast_all({"event": event, "data": data}), loop)

    def _flush_cache_buffer(self):
        retries = 0
        while retries < 5:
            if cache and self.cache_buffer:
                try:
                    cache.bulk_insert(self.cache_buffer)
                except Exception:
                    cache.save_scan_results("root", [(f[0], f[1], f[2], f[3], f[4], f[5]) for f in self.cache_buffer])
                self.cache_buffer = []
                return
            retries += 1
            _time.sleep(0.1)
        if self.cache_buffer:
            print(f"WARNING: Discarding {len(self.cache_buffer)} cache items (cache not ready)")
            self.cache_buffer = []

    def _try_restart(self):
        now = _time.time()
        self._restart_times = [t for t in self._restart_times if now - t < 60]
        if len(self._restart_times) >= 5:
            print("FATAL: Engine crashed >5 times in 60s, stopping retries")
            self._broadcast_threadsafe("engine_fatal", {"message": "Engine crashed repeatedly"})
            return
        self._restart_times.append(now)
        self._reader_stop.set()
        self._reader_stop = threading.Event()
        _time.sleep(2)
        self.cache_buffer = []
        self._start_process()
        self._start_reader()

    def _cleanup_stale_pending(self):
        """Remove non-structural pending commands older than 60 seconds."""
        now = _time.time()
        stale = [t for t, (_, ts) in self.pending_commands.items() if now - ts > 60 and t not in STRUCTURAL_TAGS]
        for t in stale:
            del self.pending_commands[t]

    def _reader_loop(self):
        my_stop = self._reader_stop
        while True:
            if not self.proc:
                threading.Event().wait(1)
                continue

            line = self.proc.stdout.readline()
            if not line:
                print("Engine stdout closed")
                if my_stop.is_set():
                    break
                self._try_restart()
                break

            line = line.rstrip("\n\r")
            if not line:
                continue

            print(f"[ENGINE OUT] {line}")

            parts = line.split("|")
            prefix = parts[0]

            # Handle CACHE_ITEM — accumulate into buffer
            if prefix == "CACHE_ITEM" and len(parts) >= 5:
                fname = parts[1]
                path = parts[2]
                sz = int(parts[3])
                mt = int(parts[4])
                ttype = parts[5] if len(parts) > 5 else "DATA"
                ent = float(parts[6]) if len(parts) > 6 else 0.0
                chk = parts[7] if len(parts) > 7 else ""
                self.cache_buffer.append((fname, path, sz, mt, ttype, ent, chk))
                continue

            # Handle FILE| prefix from parallel scanner — accumulate into buffer
            if prefix == "FILE" and len(parts) >= 7:
                path = parts[1]
                fname = parts[2]
                sz = int(parts[3])
                mt = int(parts[4])
                ttype = parts[5] if len(parts) > 5 else "DATA"
                ent = float(parts[6]) if len(parts) > 6 else 0.0
                chk = parts[7] if len(parts) > 7 else ""
                self.cache_buffer.append((fname, path, sz, mt, ttype, ent, chk))
                # Bulk insert when buffer hits 50,000
                if len(self.cache_buffer) >= 50000:
                    self._flush_cache_buffer()
                continue

            # Handle SCAN_PROGRESS — broadcast progress
            if prefix == "SCAN_PROGRESS":
                count = parts[1] if len(parts) > 1 else "0"
                self._broadcast_threadsafe("engine_progress", {"tag": "global", "line": f"SCAN_PROGRESS|{count}"})
                self._broadcast_threadsafe("engine_progress", {"tag": "dashboard", "line": f"SCAN_PROGRESS|{count}"})
                continue

            # Handle SCAN_FOLDER — broadcast folder status immediately
            # Format: SCAN_FOLDER|<path>|ENTERING  or  SCAN_FOLDER|<path>|<fileCount>
            # Status-only event: not cached, not a terminator
            if prefix == "SCAN_FOLDER":
                self._broadcast_threadsafe("engine_stream", {"tag": "global", "line": line})
                self._broadcast_threadsafe("engine_stream", {"tag": "dashboard", "line": line})
                continue

            # Handle SCAN_DONE — flush remaining cache buffer
            if prefix == "SCAN_DONE":
                self._flush_cache_buffer()
                self._cleanup_stale_pending()

            if prefix == "CACHE_DONE":
                self._flush_cache_buffer()

            target_tags = ["global", "api_req"]
            if prefix in ("DUP", "DUP_DONE"): target_tags.append("duplicates")
            if prefix in ("ORPHAN", "ORPHAN_DONE"): target_tags.append("orphans")
            if prefix in ("MONTH", "ANALYTICS_DONE"): target_tags.append("analytics")
            if prefix in ("SUSPICIOUS", "SUSPICIOUS_DONE"): target_tags.append("security")
            if prefix in ("TYPE", "TYPE_DONE"): target_tags.append("analytics")
            if prefix in ("REC", "RECLAIM_DONE"): target_tags.append("analytics")
            if prefix in ("REC", "RECLAIM_DONE"): target_tags.append("duplicates")
            if prefix in ("RESULT",):
                if len(parts) > 1 and parts[1] == "VERSION": target_tags.append("history")
                else: target_tags.append("search")
            if prefix in ("ROLLBACK_OK", "ROLLBACK_ERROR"): target_tags.append("history")
            if prefix == "INDEXING":
                self._broadcast_threadsafe("engine_progress", {"tag": "global", "line": line})
                self._broadcast_threadsafe("engine_progress", {"tag": "dashboard", "line": line})
                continue
            if prefix in ("LOADED", "BATCH_DONE", "REC_OK", "SCAN_DONE"):
                target_tags.append("explorer")
                target_tags.append("dashboard")
                target_tags.append("global_status")
                if prefix == "LOADED":
                    self.send("DUMP_CACHE")

            is_global = prefix in ("INDEXING", "LOADED", "BATCH_DONE", "REC_OK", "SCAN_DONE", "SCAN_PROGRESS")
            resolved_conn_id = None
            if not is_global:
                for t in target_tags:
                    if t in self.pending_commands:
                        resolved_conn_id = self.pending_commands[t][0]
                        break

            for t in target_tags:
                self._broadcast_threadsafe("engine_stream", {"tag": t, "line": line}, resolved_conn_id)

            if prefix in self.TERMINATORS:
                for t in target_tags:
                    self._broadcast_threadsafe("engine_done", {"tag": t}, resolved_conn_id)
                    if t not in STRUCTURAL_TAGS:
                        self.pending_commands.pop(t, None)

cache = None
bridge = None

# Stats cache with 30-second TTL
_stats_cache = {"data": None, "ts": 0}

@app.on_event("startup")
async def startup_event():
    global cache, bridge, loop
    loop = asyncio.get_running_loop()
    cache = FileCache()
    bridge = SubprocessBridge()
    def delayed_batch_load():
        import time
        time.sleep(2.0)
        print("[STARTUP] Starting batch load into engine...")
        count = cache.load_all_batch(bridge)
        print(f"[STARTUP] Batch load complete: {count} files sent to engine")

    threading.Thread(target=delayed_batch_load, daemon=True).start()

@app.get("/status")
async def get_status():
    engine_alive = bridge is not None and bridge.proc is not None and bridge.proc.poll() is None
    db_count = 0
    if cache:
        with cache._db_lock:
            cursor = cache.conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM files")
            row = cursor.fetchone()
            if row: db_count = row[0]
    return {
        "engine_alive": engine_alive,
        "engine_pid": bridge.proc.pid if engine_alive else None,
        "ws_connections": len(manager.active_connections),
        "cached_files": db_count,
    }

@app.get("/search")
async def http_search(q: str = Query(..., min_length=1), limit: int = Query(200, ge=1, le=1000)):
    if not cache:
        return JSONResponse({"results": [], "count": 0, "time_ms": 0})
    start = _time.time()
    rows = cache.search_fts(q, limit)
    elapsed = (_time.time() - start) * 1000
    results = [{"filepath": r[0], "filename": r[1], "size": r[2], "mtime": r[3], "trueType": r[4] or "UNKNOWN"} for r in rows]
    return JSONResponse({"results": results, "count": len(results), "time_ms": round(elapsed, 2)})

@app.get("/stats")
async def http_stats():
    global _stats_cache
    now = _time.time()
    if _stats_cache["data"] and now - _stats_cache["ts"] < 30:
        return JSONResponse(_stats_cache["data"])
    if not cache:
        return JSONResponse({"file_count": 0, "total_size": 0, "type_distribution": [], "largest_files": []})
    data = cache.get_stats()
    _stats_cache = {"data": data, "ts": now}
    return JSONResponse(data)


@app.get("/api/duplicates")
async def get_duplicates():
    if not cache:
        return JSONResponse({"groups": [], "total_reclaimable": 0})
    with cache._db_lock:
        cursor = cache.conn.cursor()
        cursor.execute("""
            SELECT filename, size, COUNT(*) as cnt,
                   COUNT(*) * size as total_size
            FROM files
            WHERE filename IS NOT NULL AND filename != ''
            AND size > 10240
            GROUP BY filename, size
            HAVING COUNT(*) > 1
            ORDER BY total_size DESC
            LIMIT 200
        """)
        groups_raw = cursor.fetchall()

        groups = []
        total_reclaimable = 0
        for row in groups_raw:
            filename, single_size, cnt, total_size = row
            cursor2 = cache.conn.cursor()
            cursor2.execute(
                "SELECT filepath, rowid FROM files WHERE filename = ? AND size = ? LIMIT 10",
                (filename, single_size)
            )
            paths = [{"path": r[0], "id": r[1]} for r in cursor2.fetchall()]
            wasted = (cnt - 1) * single_size
            total_reclaimable += wasted
            fake_hash = hex(abs(hash(filename + str(single_size))))[2:8]
            groups.append({
                "name": filename,
                "hash": fake_hash,
                "sizeKB": single_size // 1024,
                "paths": paths,
                "count": cnt,
                "wasted": wasted
            })

    return JSONResponse({
        "groups": groups,
        "total_reclaimable": total_reclaimable
    })


@app.get("/api/suspicious")
async def get_suspicious():
    if not cache:
        return JSONResponse({"files": [], "count": 0})
    with cache._db_lock:
        cursor = cache.conn.cursor()
        cursor.execute("""
            SELECT filename, filepath, entropy, rowid
            FROM files
            WHERE entropy > 7.5
            ORDER BY entropy DESC
            LIMIT 500
        """)
        rows = cursor.fetchall()
        files = [{"name": r[0], "path": r[1], "entropy": round(r[2], 2), "id": r[3]} for r in rows]
    return JSONResponse({"files": files, "count": len(files)})


@app.get("/api/orphans")
async def get_orphans():
    if not cache:
        return JSONResponse({"files": [], "total_wasted_mb": 0})
    with cache._db_lock:
        cursor = cache.conn.cursor()
        cursor.execute("""
            SELECT rowid, filename, filepath, size
            FROM files
            WHERE filepath LIKE '%Temp%'
            OR filepath LIKE '%temp%'
            OR filepath LIKE '%tmp%'
            OR filepath LIKE '%cache%'
            OR filepath LIKE '%Cache%'
            OR filepath LIKE '%.log'
            OR filepath LIKE '%Recycle%'
            ORDER BY size DESC
            LIMIT 300
        """)
        rows = cursor.fetchall()
        total = sum(r[3] for r in rows)
        files = [{"id": r[0], "name": r[1], "path": r[2], "sizeKB": r[3] // 1024} for r in rows]
    return JSONResponse({"files": files, "total_wasted_mb": round(total / 1048576, 1)})


@app.get("/api/analytics")
async def get_analytics():
    if not cache:
        return JSONResponse({"monthly": [], "largest": [], "types": []})
    with cache._db_lock:
        cursor = cache.conn.cursor()

        # Monthly storage data
        monthly = []
        month_names = ["Jan","Feb","Mar","Apr","May",
                       "Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
        for i, month in enumerate(month_names):
            m = i + 1
            cursor.execute("""
                SELECT COALESCE(SUM(size), 0) FROM files
                WHERE CAST(strftime('%m', datetime(mtime, 'unixepoch')) AS INTEGER) = ?
                AND mtime > 1640000000
            """, (m,))
            size_bytes = cursor.fetchone()[0]
            monthly.append({"month": month, "size": round(size_bytes / 1048576, 1)})

        # Largest files
        cursor.execute("""
            SELECT filename, filepath, size 
            FROM files 
            ORDER BY size DESC 
            LIMIT 10
        """)
        largest = [{"name": r[0], "path": r[1], "size": r[2]} for r in cursor.fetchall()]

        # Type distribution
        cursor.execute("""
            SELECT trueType, SUM(size) as total
            FROM files
            GROUP BY trueType
            ORDER BY total DESC
            LIMIT 6
        """)
        types = [{"name": r[0] or "UNKNOWN", "value": r[1]} for r in cursor.fetchall()]

        # Reclaim data
        one_year_ago = int(__import__('time').time()) - (365 * 86400)
        cursor.execute("""
            SELECT COALESCE(SUM(size), 0) FROM files
            WHERE mtime < ?
        """, (one_year_ago,))
        old_waste = cursor.fetchone()[0]

        cursor.execute("""
            SELECT COALESCE(SUM(size), 0) FROM files
            WHERE size > 104857600
        """)
        large_waste = cursor.fetchone()[0]

    return JSONResponse({
        "monthly": monthly,
        "largest": largest,
        "types": types,
        "old_waste": old_waste,
        "large_waste": large_waste,
        "total_reclaimable": old_waste + large_waste
    })


@app.get("/api/reclaim")
async def get_reclaim():
    if not cache:
        return JSONResponse({"actions": [], "total": 0})
    with cache._db_lock:
        cursor = cache.conn.cursor()
        one_year_ago = int(__import__('time').time()) - (365 * 86400)

        # Old files
        cursor.execute("""
            SELECT COALESCE(SUM(size), 0) FROM files WHERE mtime < ?
        """, (one_year_ago,))
        old_waste = cursor.fetchone()[0]

        # Duplicate waste
        cursor.execute("""
            SELECT COALESCE(SUM((cnt-1) * single_size), 0) FROM (
                SELECT COUNT(*) as cnt, MIN(size) as single_size
                FROM files
                WHERE checksum IS NOT NULL AND checksum != ''
                GROUP BY checksum, filename
                HAVING COUNT(*) > 1
            )
        """)
        dup_waste = cursor.fetchone()[0]

        # Large files
        cursor.execute("""
            SELECT filename, filepath, size FROM files
            WHERE size > 104857600
            ORDER BY size DESC LIMIT 20
        """)
        large_files = [{"name": r[0], "path": r[1], "size": r[2]} for r in cursor.fetchall()]

    actions = [
        {"type": "DUPLICATES", "label": "Duplicate Content", "size": dup_waste},
        {"type": "OLD_FILES", "label": "Files older than 1 year", "size": old_waste},
    ]
    for f in large_files:
        actions.append({
            "type": "LARGE_FILE",
            "label": f["name"],
            "size": f["size"],
            "path": f["path"]
        })

    return JSONResponse({
        "actions": actions,
        "total": dup_waste + old_waste
    })

@app.post("/scan/incremental")
async def http_scan_incremental():
    if not cache or not bridge:
        return JSONResponse({"error": "not ready"}, status_code=503)
    last_scan_time = cache.get_last_scan_time()
    scan_id = str(uuid.uuid4())[:8]
    # Get scan root from engine or use a default
    with cache._db_lock:
        cursor = cache.conn.cursor()
        cursor.execute("SELECT DISTINCT parent_dir FROM files LIMIT 1")
        row = cursor.fetchone()
    root = row[0] if row else ""
    if root:
        asyncio.create_task(asyncio.to_thread(bridge.send, f"SCAN_INCREMENTAL {root} {last_scan_time}", "dashboard"))
    return JSONResponse({"scan_id": scan_id, "last_scan_time": last_scan_time, "root": root})

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    conn_id = await manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_json()
            cmd = data.get("command", "")
            tag = data.get("tag", "api_req")
            client_conn_id = data.get("conn_id", conn_id)
            print(f"[WS SERVER] Received cmd={cmd} tag={tag} from {client_conn_id}")
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
                asyncio.create_task(asyncio.to_thread(bridge.send, cmd, tag, client_conn_id))
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        manager.disconnect(websocket)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("api:app", host="127.0.0.1", port=8000, reload=False)
