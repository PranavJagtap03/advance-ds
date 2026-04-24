"""
FSM — File System Directory Manager GUI
Dark-themed Python tkinter application frontend for the C++ fsm.exe backend.
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, font as tkfont
import subprocess
import threading
import queue
import os
import sys
import time
import math
import json
import sqlite3

# ═══════════════════════════════════════════════════════════════════
# THEME / DESIGN TOKENS
# ═══════════════════════════════════════════════════════════════════
BG         = "#0d1117"   # main window background
PANEL      = "#161b22"   # panel / frame background
PANEL2     = "#21262d"   # slightly lighter panel
BORDER     = "#30363d"   # separator / border
ACCENT     = "#58a6ff"   # primary accent (blue)
ACCENT2    = "#388bfd"   # slightly darker accent
TEXT       = "#c9d1d9"   # primary text
TEXT_DIM   = "#8b949e"   # secondary / dim text
SUCCESS    = "#3fb950"   # green
WARNING    = "#d29922"   # amber / warning
DANGER     = "#f85149"   # red / danger
PURPLE     = "#bc8cff"   # purple accent
TEAL       = "#39d353"   # teal

DS_COLORS = {
    "BPlusTree":    ACCENT,
    "Trie":         PURPLE,
    "SegmentTree":  WARNING,
    "UnionFind":    SUCCESS,
    "PersistentDS": TEAL,
}

DS_COMPLEXITY = {
    "BPlusTree":    "Search: O(log n) | Insert: O(log n) | Range: O(log n + k)",
    "Trie":         "Prefix: O(m) | Insert: O(m) | m = length of key",
    "SegmentTree":  "Range Query: O(log n) | Update: O(log n)",
    "UnionFind":    "Find: O(α(n)) | Union: O(α(n)) | Nearly O(1) amortised",
    "PersistentDS": "Save: O(1) | Retrieve: O(1) | History: O(v) versions",
}

MONTH_NAMES = ["Jan","Feb","Mar","Apr","May","Jun",
               "Jul","Aug","Sep","Oct","Nov","Dec"]


def format_bytes(size_bytes: int) -> str:
    """Format byte counts for compact UI display."""
    size = max(int(size_bytes or 0), 0)
    units = ["B", "KB", "MB", "GB", "TB"]
    value = float(size)
    unit = units[0]
    for unit in units:
        if value < 1024.0 or unit == units[-1]:
            break
        value /= 1024.0
    if unit in ("B", "KB"):
        return f"{int(value)} {unit}"
    return f"{value:.1f} {unit}"


def format_timestamp(ts: int) -> str:
    """Return a friendly modified date for explorer rows."""
    if not ts:
        return "Unknown"
    try:
        return time.strftime("%Y-%m-%d", time.localtime(int(ts)))
    except Exception:
        return "Unknown"


def bind_hover(widget, base_bg, hover_bg, base_fg=TEXT, hover_fg=None):
    """Add lightweight hover feedback to buttons."""
    hover_fg = hover_fg if hover_fg is not None else base_fg

    def _enter(_event):
        try:
            widget.configure(bg=hover_bg, fg=hover_fg)
        except tk.TclError:
            pass

    def _leave(_event):
        try:
            widget.configure(bg=base_bg, fg=base_fg)
        except tk.TclError:
            pass

    widget.bind("<Enter>", _enter)
    widget.bind("<Leave>", _leave)


# ═══════════════════════════════════════════════════════════════════
# 1. SubprocessBridge
# ═══════════════════════════════════════════════════════════════════
class SubprocessBridge:
    """Manages the C++ fsm --machine subprocess and all I/O."""

    TERMINATORS = {"DUP_DONE", "ORPHAN_DONE", "ANALYTICS_DONE",
                   "BENCH_DONE", "VERSION_DONE", "BYE", "BATCH_DONE"}

    def __init__(self, result_queue: queue.Queue):
        self.q = result_queue
        self.proc = None
        self._lock = threading.Lock()
        self._start_process()

    # ── launch ────────────────────────────────────────────────────
    def _start_process(self):
        # Binary is one folder above gui/
        gui_dir = os.path.dirname(os.path.abspath(__file__))
        ds_dir  = os.path.dirname(gui_dir)
        binary  = "fsm.exe" if sys.platform == "win32" else "./fsm"
        full_bin = os.path.join(ds_dir, binary)

        if not os.path.exists(full_bin):
            raise FileNotFoundError(
                f"Cannot find compiled binary at:\n{full_bin}\n\n"
                "Please run   make   in the ds/ directory first."
            )

        self.proc = subprocess.Popen(
            [full_bin, "--machine"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
            cwd=ds_dir,
        )

    # ── send + async read ─────────────────────────────────────────
    def send(self, command: str, tag: str = ""):
        """Send a command and start a reader thread."""
        with self._lock:
            try:
                self.proc.stdin.write(command + "\n")
                self.proc.stdin.flush()
            except BrokenPipeError:
                self.q.put(("ERROR", tag, ["ERROR|send|broken pipe"]))
                return

        is_multi = any(command.startswith(c) for c in
                       ("DUPLICATES", "ORPHANS", "ANALYTICS",
                        "BENCHMARK", "VERSION_HISTORY", "GENERATE",
                        "LOAD_DIR"))
        t = threading.Thread(target=self._reader,
                             args=(tag, is_multi), daemon=True)
        t.start()

    # ── background reader ─────────────────────────────────────────
    def _reader(self, tag: str, multi: bool):
        lines = []
        try:
            if multi:
                while True:
                    line = self.proc.stdout.readline()
                    if not line:
                        break
                    line = line.rstrip("\n\r")
                    lines.append(line)
                    prefix = line.split("|")[0]
                    if prefix in self.TERMINATORS:
                        break
            else:
                line = self.proc.stdout.readline().rstrip("\n\r")
                lines.append(line)
        except Exception as e:
            lines.append(f"ERROR|reader|{e}")

        self.q.put(("RESULT", tag, lines))

    # ── cleanup ───────────────────────────────────────────────────
    def shutdown(self):
        try:
            self.send("EXIT", "exit")
            self.proc.wait(timeout=2)
        except Exception:
            self.proc.kill()


# ═══════════════════════════════════════════════════════════════════
# 2. AnimationCanvas
# ═══════════════════════════════════════════════════════════════════
class AnimationCanvas:
    """Center panel — draws and animates DS visualizations."""

    def __init__(self, parent):
        self.parent = parent
        self.current_ds = "BPlusTree"
        self._anim_job = None
        self._build()

    # ── layout ────────────────────────────────────────────────────
    def _build(self):
        # Header bar
        hdr = tk.Frame(self.parent, bg=PANEL, height=40)
        hdr.pack(fill="x", padx=0, pady=0)
        hdr.pack_propagate(False)

        self.ds_label = tk.Label(hdr, text="● Active DS: B+ Tree",
                                  bg=PANEL, fg=ACCENT,
                                  font=("Consolas", 12, "bold"))
        self.ds_label.pack(side="left", padx=14, pady=8)

        self.mode_label = tk.Label(
            hdr,
            text="Conceptual DS view",
            bg="#1f2530",
            fg=WARNING,
            font=("Segoe UI", 8, "bold"),
            padx=10,
            pady=3,
        )
        self.mode_label.pack(side="right", padx=12, pady=7)

        # Canvas
        self.canvas = tk.Canvas(self.parent, bg=BG,
                                highlightthickness=1,
                                highlightbackground=BORDER)
        self.canvas.pack(fill="both", expand=True, padx=6, pady=4)

        # Status bar
        self.status_var = tk.StringVar(
            value="Ready — visualizations illustrate DS flow and query routing"
        )
        sb = tk.Label(self.parent, textvariable=self.status_var,
                      bg=PANEL2, fg=TEXT_DIM,
                      font=("Consolas", 9), anchor="w", padx=10)
        sb.pack(fill="x", pady=0)

    # ── public interface ──────────────────────────────────────────
    def set_ds(self, ds_name: str):
        self.current_ds = ds_name
        label_map = {
            "BPlusTree":    "B+ Tree",
            "Trie":         "Trie",
            "SegmentTree":  "Segment Tree",
            "UnionFind":    "Union-Find",
            "PersistentDS": "Persistent DS",
        }
        color = DS_COLORS.get(ds_name, ACCENT)
        self.ds_label.config(text=f"● Active DS: {label_map.get(ds_name, ds_name)}",
                             fg=color)
        self._draw_idle(ds_name)

    def set_status(self, text: str):
        self.status_var.set(text)

    def show_spinner(self, text="Searching…"):
        self.canvas.delete("all")
        w = self.canvas.winfo_width() or 600
        h = self.canvas.winfo_height() or 440
        self.canvas.create_text(w//2, h//2 - 20, text="⟳",
                                fill=ACCENT, font=("Arial", 48))
        self.canvas.create_text(w//2, h//2 + 30, text=text,
                                fill=TEXT_DIM, font=("Consolas", 13))

    # ── idle drawings ─────────────────────────────────────────────
    def _draw_idle(self, ds_name: str):
        if self._anim_job:
            self.parent.after_cancel(self._anim_job)
            self._anim_job = None
        self.canvas.delete("all")
        if ds_name == "BPlusTree":
            self._draw_bplustree_idle()
        elif ds_name == "Trie":
            self._draw_trie_idle()
        elif ds_name == "SegmentTree":
            self._draw_segtree_idle()
        elif ds_name == "UnionFind":
            self._draw_unionfind_idle()

    # ─── B+ Tree ─────────────────────────────────────────────────
    def _bpt_layout(self):
        """Return (nodes, edges) for the demo B+ tree.
        nodes: list of (id, label, x, y, is_leaf)
        edges: list of (parent_id, child_id)
        """
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        cx = w // 2
        nodes = [
            # root
            (0, "root\nA–Z", cx, 50, False),
            # internal level
            (1, "A–L\nnode1", cx - 160, 150, False),
            (2, "M–Z\nnode2", cx + 160, 150, False),
            # leaves
            (3, "A–C\nleaf0", cx - 260, 260, True),
            (4, "D–L\nleaf1", cx - 80,  260, True),
            (5, "M–S\nleaf2", cx + 80,  260, True),
            (6, "T–Z\nleaf3", cx + 260, 260, True),
        ]
        edges = [(0,1),(0,2),(1,3),(1,4),(2,5),(2,6)]
        return nodes, edges

    def _draw_bplustree_idle(self, highlight=None, found=None):
        self.canvas.delete("all")
        nodes, edges = self._bpt_layout()
        nmap = {n[0]: n for n in nodes}

        # draw leaf chain arrows
        leaves = [n for n in nodes if n[4]]
        for i in range(len(leaves)-1):
            x1, y1 = leaves[i][2]+38, leaves[i][3]
            x2, y2 = leaves[i+1][2]-38, leaves[i+1][3]
            self.canvas.create_line(x1, y1, x2, y2, fill=TEXT_DIM,
                                    width=1, dash=(4,3))

        for (pid, cid) in edges:
            px, py = nmap[pid][2], nmap[pid][3]
            cx, cy = nmap[cid][2], nmap[cid][3]
            self.canvas.create_line(px, py+20, cx, cy-20,
                                    fill=BORDER, width=2)

        for (nid, label, x, y, is_leaf) in nodes:
            if found is not None and nid == found:
                fill = SUCCESS if found >= 0 else DANGER
            elif highlight and nid in highlight:
                fill = WARNING
            else:
                fill = PANEL2 if not is_leaf else "#1c2128"
            outline = ACCENT if is_leaf else BORDER
            r = 22 if is_leaf else 20
            self._rounded_rect(x-48, y-r, x+48, y+r, 10,
                               fill=fill, outline=outline, width=2)
            lines = label.split("\n")
            if len(lines) == 2:
                self.canvas.create_text(x, y-7, text=lines[0],
                    fill=TEXT, font=("Consolas", 8, "bold"))
                self.canvas.create_text(x, y+7, text=lines[1],
                    fill=TEXT_DIM, font=("Consolas", 8))
            else:
                self.canvas.create_text(x, y, text=label,
                    fill=TEXT, font=("Consolas", 9, "bold"))

    def animate_bptree(self, path_str: str, found: bool, hop_count: int):
        """Animate B+ tree node highlights from path string."""
        # Map path labels to node ids
        label_to_id = {"root": 0, "node1": 1, "node2": 2,
                       "leaf0": 3, "leaf1": 4, "leaf2": 5, "leaf3": 6,
                       "node3": 1, "node4": 2, "node5": 5, "node6": 6,
                       "leaf": 5}
        parts = [p.strip() for p in path_str.split("->") if "->" in path_str or p]
        if "->" in path_str:
            raw_parts = path_str.split("->")
        else:
            raw_parts = [path_str]
        ids_to_visit = []
        for part in raw_parts:
            token = part.strip().split("[")[0].strip()
            if token in label_to_id:
                ids_to_visit.append(label_to_id[token])

        if not ids_to_visit:
            ids_to_visit = [0, 1, 3]  # fallback demo path

        self._animate_bpt_steps(ids_to_visit, found, delay=0)

    def _animate_bpt_steps(self, steps, found, delay):
        visited = steps[:delay+1]
        last_id = steps[-1] if found else -99
        final = (delay >= len(steps) - 1)

        if final:
            found_id = steps[-1] if found else None
            not_found_id = steps[-1] if not found else None
        else:
            found_id = None
            not_found_id = None

        self.canvas.delete("all")
        nodes, edges = self._bpt_layout()
        nmap = {n[0]: n for n in nodes}

        # edges
        leaves = [n for n in nodes if n[4]]
        for i in range(len(leaves)-1):
            x1,y1 = leaves[i][2]+38, leaves[i][3]
            x2,y2 = leaves[i+1][2]-38, leaves[i+1][3]
            self.canvas.create_line(x1,y1,x2,y2,fill=TEXT_DIM,width=1,dash=(4,3))
        for (pid,cid) in edges:
            px,py=nmap[pid][2],nmap[pid][3]
            cx2,cy=nmap[cid][2],nmap[cid][3]
            self.canvas.create_line(px,py+20,cx2,cy-20,fill=BORDER,width=2)

        for (nid, label, x, y, is_leaf) in nodes:
            if final and nid == steps[-1]:
                fill = SUCCESS if found else DANGER
                outline = SUCCESS if found else DANGER
            elif nid in visited:
                fill = "#2d2a1e"
                outline = WARNING
            else:
                fill = PANEL2 if not is_leaf else "#1c2128"
                outline = ACCENT if is_leaf else BORDER
            r = 22 if is_leaf else 20
            self._rounded_rect(x-48, y-r, x+48, y+r, 10,
                               fill=fill, outline=outline, width=2)
            lines = label.split("\n")
            if len(lines)==2:
                self.canvas.create_text(x,y-7,text=lines[0],fill=TEXT,
                    font=("Consolas",8,"bold"))
                self.canvas.create_text(x,y+7,text=lines[1],fill=TEXT_DIM,
                    font=("Consolas",8))
            else:
                self.canvas.create_text(x,y,text=label,fill=TEXT,
                    font=("Consolas",9,"bold"))

        # path label
        w2 = max(self.canvas.winfo_width(), 580)
        h2 = max(self.canvas.winfo_height(), 380)
        path_text = " → ".join(str(s) for s in steps[:delay+1])
        self.canvas.create_text(w2//2, h2-40, text=path_text,
                                fill=TEXT_DIM, font=("Consolas",9))

        if not final:
            self._anim_job = self.parent.after(
                320, self._animate_bpt_steps, steps, found, delay+1)

    # ─── Trie ─────────────────────────────────────────────────────
    def _trie_layout(self, prefix=""):
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        cx = w // 2
        # Demo trie showing "budget" path
        demo = list("budget")
        chars = demo[:max(len(prefix), len(demo))]
        nodes = []
        r = 22
        spacing = 70
        start_x = cx - (len(chars)-1)*spacing//2
        for i, c in enumerate(chars):
            x = start_x + i*spacing
            y = h//2
            nodes.append((i, c, x, y))
        return nodes

    def _draw_trie_idle(self, highlight_count=0):
        self.canvas.delete("all")
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        nodes = self._trie_layout()
        if not nodes: return

        # root node
        rx, ry = nodes[0][2]-70, h//2
        self.canvas.create_oval(rx-22, ry-22, rx+22, ry+22,
                               fill=PANEL2, outline=BORDER, width=2)
        self.canvas.create_text(rx, ry, text="⬤", fill=TEXT_DIM,
                               font=("Arial", 12))

        prev_x, prev_y = rx, ry
        for i,(nid, char, x, y) in enumerate(nodes):
            # edge
            self.canvas.create_line(prev_x+22, prev_y, x-22, y,
                                   fill=BORDER, width=2)
            fill = ACCENT if i < highlight_count else PANEL2
            outline = ACCENT if i < highlight_count else BORDER
            self.canvas.create_oval(x-22, y-22, x+22, y+22,
                                   fill=fill, outline=outline, width=2)
            text_col = BG if i < highlight_count else TEXT
            self.canvas.create_text(x, y, text=char.lower(),
                                   fill=text_col, font=("Consolas",14,"bold"))
            prev_x, prev_y = x, y

        # label
        self.canvas.create_text(w//2, h//2+60, text="Character trie path",
                               fill=TEXT_DIM, font=("Consolas",9))

    def animate_trie(self, prefix: str, matched_count: int):
        """Animate trie character matching."""
        self._animate_trie_steps(prefix, matched_count, 0)

    def _animate_trie_steps(self, prefix, total, step):
        self.canvas.delete("all")
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        cx = w // 2

        chars = list(prefix) if prefix else list("prefix")
        if not chars:
            return

        spacing = min(70, (w-100)//max(len(chars),1))
        start_x = cx - (len(chars)-1)*spacing//2
        r = 20

        # root
        rx = start_x - spacing
        ry = h//2
        self.canvas.create_oval(rx-r,ry-r,rx+r,ry+r,
                               fill=PANEL2,outline=BORDER,width=2)
        self.canvas.create_text(rx,ry,text="⬤",fill=TEXT_DIM,font=("Arial",10))

        prev_x, prev_y = rx, ry
        for i, char in enumerate(chars):
            x = start_x + i*spacing
            y = h//2
            matched = i < step
            self.canvas.create_line(prev_x+r, prev_y, x-r, y,
                                   fill=ACCENT if matched else BORDER, width=2)
            fill = ACCENT if matched else PANEL2
            outline = ACCENT if matched else BORDER
            self.canvas.create_oval(x-r, y-r, x+r, y+r,
                                   fill=fill, outline=outline, width=2)
            tc = BG if matched else TEXT
            self.canvas.create_text(x, y, text=char.lower(),
                                   fill=tc, font=("Consolas",13,"bold"))
            prev_x, prev_y = x, y

        # path text
        matched_path = "→".join(list(prefix[:step].lower()))
        self.canvas.create_text(cx, h//2+60,
                               text=matched_path if matched_path else "matching…",
                               fill=ACCENT, font=("Consolas",10))

        # result indicator
        if step >= len(chars):
            self.canvas.create_text(cx, h//2+85,
                text=f"✓ Matched {total} file(s)",
                fill=SUCCESS, font=("Consolas",10,"bold"))
            return

        self._anim_job = self.parent.after(
            280, self._animate_trie_steps, prefix, total, step+1)

    # ─── Segment Tree ─────────────────────────────────────────────
    def _segtree_layout(self):
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        cx = w // 2
        # 8-node balanced binary tree (3 levels)
        nodes = []
        # level 0: root
        nodes.append((0, "All\nYear", cx, 55, 0, 11))
        # level 1
        nodes.append((1, "Jan\n–Jun", cx-160, 145, 0, 5))
        nodes.append((2, "Jul\n–Dec", cx+160, 145, 6, 11))
        # level 2 (leaves = months)
        nodes.append((3, "Jan\n–Mar", cx-240, 240, 0, 2))
        nodes.append((4, "Apr\n–Jun", cx-80,  240, 3, 5))
        nodes.append((5, "Jul\n–Sep", cx+80,  240, 6, 8))
        nodes.append((6, "Oct\n–Dec", cx+240, 240, 9, 11))
        edges = [(0,1),(0,2),(1,3),(1,4),(2,5),(2,6)]
        return nodes, edges

    def _draw_segtree_idle(self, highlight_range=None):
        self.canvas.delete("all")
        nodes, edges = self._segtree_layout()
        nmap = {n[0]: n for n in nodes}

        for (pid, cid) in edges:
            px,py = nmap[pid][2], nmap[pid][3]
            cx2,cy = nmap[cid][2], nmap[cid][3]
            self.canvas.create_line(px, py+22, cx2, cy-22,
                                   fill=BORDER, width=2)

        for (nid, label, x, y, ms, me) in nodes:
            highlighted = False
            if highlight_range:
                qs, qe = highlight_range
                if ms <= qe and me >= qs:
                    highlighted = True
            fill    = "#2d2a1e" if highlighted else PANEL2
            outline = WARNING   if highlighted else BORDER
            self._rounded_rect(x-46, y-22, x+46, y+22, 10,
                               fill=fill, outline=outline, width=2)
            lines = label.split("\n")
            self.canvas.create_text(x, y-7, text=lines[0],
                fill=TEXT if highlighted else TEXT_DIM,
                font=("Consolas",8,"bold"))
            if len(lines)>1:
                self.canvas.create_text(x, y+7, text=lines[1],
                    fill=TEXT if highlighted else TEXT_DIM,
                    font=("Consolas",8))

    def animate_segtree(self, month_start: int, month_end: int, total_mb: float):
        self.canvas.delete("all")
        self._draw_segtree_idle(highlight_range=(month_start-1, month_end-1))
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        mn = MONTH_NAMES[month_start-1]
        me = MONTH_NAMES[month_end-1]
        self.canvas.create_text(w//2, h-60,
            text=f"Range: {mn} – {me}  |  Total: {total_mb:.2f} MB",
            fill=WARNING, font=("Consolas",10,"bold"))

    # ─── Union-Find ───────────────────────────────────────────────
    def _draw_unionfind_idle(self, orphan_ids=None):
        self.canvas.delete("all")
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        cx, cy = w//2, h//2
        n = 14
        r_layout = min(160, min(w,h)//3)
        r_node   = 20
        orphan_ids = orphan_ids or []

        positions = []
        for i in range(n):
            angle = 2*math.pi*i/n - math.pi/2
            x = cx + r_layout*math.cos(angle)
            y = cy + r_layout*math.sin(angle)
            positions.append((x, y))

        # edges (connected nodes 0–10)
        connected_pairs = [(0,1),(0,2),(1,3),(2,3),(3,4),(4,5),
                           (5,6),(6,7),(7,8),(8,9),(9,10),(0,10)]
        for (a, b) in connected_pairs:
            if a < n and b < n:
                ax, ay = positions[a]
                bx, by = positions[b]
                self.canvas.create_line(ax, ay, bx, by,
                                       fill=SUCCESS+"44", width=2)

        for i, (x, y) in enumerate(positions):
            is_orphan = i in orphan_ids or i >= 11
            fill    = DANGER  if is_orphan else SUCCESS
            outline = "#f85149" if is_orphan else "#238636"
            self.canvas.create_oval(x-r_node, y-r_node,
                                   x+r_node, y+r_node,
                                   fill=fill, outline=outline, width=2)
            label = "⊘" if is_orphan else str(i)
            self.canvas.create_text(x, y, text=label,
                fill=BG if not is_orphan else TEXT,
                font=("Consolas",9,"bold"))

        # legend
        self.canvas.create_oval(20, h-45, 36, h-29, fill=SUCCESS)
        self.canvas.create_text(50, h-37, text="Connected",
            fill=TEXT_DIM, font=("Consolas",9), anchor="w")
        self.canvas.create_oval(130, h-45, 146, h-29, fill=DANGER)
        self.canvas.create_text(160, h-37, text="Orphan",
            fill=TEXT_DIM, font=("Consolas",9), anchor="w")

    def animate_unionfind(self, orphan_count: int):
        orphan_ids = list(range(11, min(11+orphan_count, 14)))
        self._draw_unionfind_idle(orphan_ids=orphan_ids)
        w = max(self.canvas.winfo_width(), 580)
        h = max(self.canvas.winfo_height(), 380)
        self.canvas.create_text(w//2, 30,
            text=f"Orphan Detection — {orphan_count} orphan(s) found",
            fill=DANGER, font=("Consolas",11,"bold"))

    # ── helpers ───────────────────────────────────────────────────
    def _rounded_rect(self, x1, y1, x2, y2, r, **kwargs):
        pts = [x1+r,y1, x2-r,y1, x2,y1, x2,y1+r,
               x2,y2-r, x2,y2, x2-r,y2, x1+r,y2,
               x1,y2, x1,y2-r, x1,y1+r, x1,y1]
        self.canvas.create_polygon(pts, smooth=True, **kwargs)


# ═══════════════════════════════════════════════════════════════════
# 3. FileExplorer
# ═══════════════════════════════════════════════════════════════════
class FileExplorer:
    """Left panel — folder tree and file listing."""

    def __init__(self, parent, on_file_select_cb, on_scan_cb, on_generate_cb,
                 on_index_system_cb=None):
        self.parent = parent
        self.on_file_select = on_file_select_cb
        self.on_scan = on_scan_cb
        self.on_generate = on_generate_cb
        self.on_index_system = on_index_system_cb
        self._all_records = []
        self._indexed_folders = []
        self._folder_nodes = {}
        self._item_lookup = {}
        self._build()

    def _build(self):
        # Header
        hdr = tk.Frame(self.parent, bg=PANEL, height=40)
        hdr.pack(fill="x")
        hdr.pack_propagate(False)
        tk.Label(hdr, text="File Explorer", bg=PANEL, fg=TEXT,
                 font=("Segoe UI", 11, "bold")).pack(side="left", padx=10, pady=8)

        # Buttons
        btn_frame = tk.Frame(self.parent, bg=PANEL, pady=6)
        btn_frame.pack(fill="x", padx=8)

        self._btn(btn_frame, "Scan Folder", self.on_scan)\
            .pack(fill="x", pady=2)
        self._btn(btn_frame, "Generate 1000", self.on_generate)\
            .pack(fill="x", pady=2)

        # Index My System button (prominent, different color)
        idx_btn = tk.Button(
            btn_frame,
            text="Index My System",
            command=self.on_index_system if self.on_index_system else lambda: None,
            bg="#1f2d1f", fg=SUCCESS,
            activebackground="#2a4a2a", activeforeground=SUCCESS,
            relief="flat", font=("Segoe UI", 9, "bold"),
            pady=6, cursor="hand2", bd=0,
            highlightthickness=1, highlightbackground="#238636",
        )
        idx_btn.pack(fill="x", pady=2)
        bind_hover(idx_btn, "#1f2d1f", "#244124", SUCCESS, SUCCESS)

        # Search filter
        sf = tk.Frame(self.parent, bg=PANEL, pady=4)
        sf.pack(fill="x", padx=8)
        tk.Label(sf, text="Filter indexed files", bg=PANEL, fg=TEXT_DIM,
                 font=("Segoe UI", 8)).pack(anchor="w", pady=(0, 2))
        self.filter_var = tk.StringVar()
        self.filter_var.trace_add("write", self._on_filter)
        fe = tk.Entry(sf, textvariable=self.filter_var,
                      bg=PANEL2, fg=TEXT, insertbackground=TEXT,
                      relief="flat", font=("Consolas",9),
                      highlightthickness=1, highlightbackground=BORDER,
                      highlightcolor=ACCENT)
        fe.pack(fill="x", ipady=4)

        # Treeview
        tree_frame = tk.Frame(self.parent, bg=PANEL)
        tree_frame.pack(fill="both", expand=True, padx=4, pady=4)

        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Dark.Treeview",
                        background=BG, foreground=TEXT,
                        fieldbackground=BG, borderwidth=0,
                        font=("Consolas", 9))
        style.configure("Dark.Treeview.Heading",
                        background=PANEL2, foreground=TEXT_DIM,
                        borderwidth=0, font=("Consolas", 9, "bold"))
        style.map("Dark.Treeview",
                  background=[("selected", ACCENT2)],
                  foreground=[("selected", BG)])

        self.tree = ttk.Treeview(tree_frame, style="Dark.Treeview",
                                  columns=("size","modified"),
                                  show="tree headings", selectmode="browse")
        self.tree.heading("#0",       text="Name")
        self.tree.heading("size",     text="Size")
        self.tree.heading("modified", text="Modified")
        self.tree.column("#0",       width=130, minwidth=80)
        self.tree.column("size",     width=70,  minwidth=50)
        self.tree.column("modified", width=60,  minwidth=40)

        vsb = ttk.Scrollbar(tree_frame, orient="vertical",
                            command=self.tree.yview)
        self.tree.configure(yscrollcommand=vsb.set)

        self.tree.pack(side="left", fill="both", expand=True)
        vsb.pack(side="right", fill="y")

        self.tree.bind("<<TreeviewSelect>>", self._on_select)

        # File count label
        self.count_var = tk.StringVar(value="0 files indexed")
        tk.Label(self.parent, textvariable=self.count_var,
                 bg=PANEL, fg=TEXT_DIM, font=("Consolas",8)).pack(pady=4)

        self._show_empty_tree()

    # ── tree operations ───────────────────────────────────────────
    def _show_empty_tree(self):
        self.tree.delete(*self.tree.get_children())
        self._folder_nodes = {}
        self._item_lookup = {}
        self.tree.insert(
            "",
            "end",
            text="No indexed files yet",
            values=("Scan a folder or replay cache", ""),
        )

    def _folder_for_path(self, path: str) -> str:
        norm = os.path.normcase(path)
        for folder in self._indexed_folders:
            if norm.startswith(os.path.normcase(folder)):
                return folder
        return os.path.dirname(path) or path

    def _folder_label(self, folder_path: str, file_count: int) -> str:
        clean = folder_path.rstrip("\\/")
        label = os.path.basename(clean) or clean
        return f"[DIR] {label}"

    def _render_tree(self, records):
        self.tree.delete(*self.tree.get_children())
        self._folder_nodes = {}
        self._item_lookup = {}

        if not records:
            empty = "No files match the current filter" if self._all_records else "No indexed files yet"
            detail = "Try a broader filter" if self._all_records else "Scan a folder or replay cache"
            self.tree.insert("", "end", text=empty, values=(detail, ""))
            total = len(self._all_records)
            if total:
                self.count_var.set(f"0 matching files · {total:,} indexed total")
            else:
                self.count_var.set("0 files indexed")
            return

        grouped = {}
        for record in records[:500]:
            name, path, size_bytes, modified = record
            folder = self._folder_for_path(path)
            grouped.setdefault(folder, []).append((name, path, size_bytes, modified))

        for folder_path, items in sorted(grouped.items(), key=lambda kv: kv[0].lower()):
            folder_node = self.tree.insert(
                "",
                "end",
                text=self._folder_label(folder_path, len(items)),
                open=True,
                values=(f"{len(items)} files", ""),
            )
            self._folder_nodes[folder_path] = folder_node
            for name, path, size_bytes, modified in sorted(items, key=lambda item: item[0].lower()):
                item_id = self.tree.insert(
                    folder_node,
                    "end",
                    text=name,
                    values=(format_bytes(size_bytes), format_timestamp(modified)),
                )
                self._item_lookup[item_id] = (name, path, size_bytes, modified)

        shown = min(len(records), 500)
        total = len(self._all_records)
        if len(records) != total:
            self.count_var.set(f"{len(records):,} matching files · {total:,} indexed total")
        elif total > shown:
            self.count_var.set(f"Showing {shown:,} of {total:,} indexed files")
        else:
            self.count_var.set(f"{total:,} files indexed")

    def load_records(self, file_list, indexed_folders=None):
        """Rebuild the explorer from real cached/indexed files."""
        normalized = []
        for record in file_list:
            if len(record) < 4:
                continue
            name, path, size_bytes, modified = record[:4]
            normalized.append((name, path, int(size_bytes or 0), int(modified or 0)))
        self._all_records = normalized
        self._indexed_folders = sorted(indexed_folders or [], key=len, reverse=True)
        self._render_tree(self._filtered_records())

    def set_file_count(self, count: int):
        self.count_var.set(f"{count:,} files indexed")

    def clear_and_reload(self, file_list):
        """Backward-compatible helper for simple record lists."""
        normalized = []
        for record in file_list:
            if len(record) >= 3:
                name, path, size_kb = record[:3]
                normalized.append((name, path, int(size_kb or 0) * 1024, 0))
        self.load_records(normalized)

    # ── callbacks ─────────────────────────────────────────────────
    def _on_select(self, event):
        sel = self.tree.selection()
        if not sel: return
        record = self._item_lookup.get(sel[0])
        if record:
            self.on_file_select(record[0])

    def _filtered_records(self):
        query = self.filter_var.get().strip().lower()
        if not query:
            return list(self._all_records)
        return [
            record for record in self._all_records
            if query in record[0].lower() or query in record[1].lower()
        ]

    def _on_filter(self, *_):
        self._render_tree(self._filtered_records())

    def _btn(self, parent, text, cmd):
        button = tk.Button(parent, text=text, command=cmd,
                           bg=PANEL2, fg=TEXT, activebackground=ACCENT2,
                           activeforeground=BG, relief="flat",
                           font=("Segoe UI", 9), pady=6,
                           cursor="hand2", bd=0,
                           highlightthickness=1,
                           highlightbackground=BORDER)
        bind_hover(button, PANEL2, "#2b3138", TEXT, TEXT)
        return button


# ═══════════════════════════════════════════════════════════════════
# 4. QueryPanel
# ═══════════════════════════════════════════════════════════════════
class QueryPanel:
    """Right panel — search, results, analytics."""

    def __init__(self, parent, on_search_cb, on_dup_cb,
                 on_orphan_cb, on_analytics_cb, on_bench_cb):
        self.parent = parent
        self.on_search = on_search_cb
        self.on_dup = on_dup_cb
        self.on_orphan = on_orphan_cb
        self.on_analytics = on_analytics_cb
        self.on_bench = on_bench_cb
        self._tooltip_win = None
        self._month_data = {}
        self._build()

    def _build(self):
        self.parent.configure(bg=PANEL)

        # ── Query section ────────────────────────────────────────
        q_frame = tk.Frame(self.parent, bg=PANEL, pady=8)
        q_frame.pack(fill="x", padx=10)

        tk.Label(q_frame, text="Query", bg=PANEL, fg=TEXT,
                 font=("Segoe UI",11,"bold")).pack(anchor="w")

        # Entry
        entry_frame = tk.Frame(q_frame, bg=BORDER, pady=1, padx=1)
        entry_frame.pack(fill="x", pady=4)
        self.search_var = tk.StringVar()
        self.entry = tk.Entry(entry_frame, textvariable=self.search_var,
                              bg=PANEL2, fg=TEXT, insertbackground=ACCENT,
                              relief="flat", font=("Consolas",12),
                              highlightthickness=0)
        self.entry.pack(fill="x", ipady=7, padx=1)
        self.entry.bind("<Return>", lambda e: self.on_search(self.search_var.get()))
        self.entry.focus_set()

        # Quick filters
        filters = tk.Frame(q_frame, bg=PANEL)
        filters.pack(fill="x", pady=2)
        filter_btns = [
            ("Exact",      lambda: self._set_filter("")),
            ("Prefix *",   lambda: self._set_filter("*")),
            ("Size Range", lambda: self._set_filter("size:100-5000")),
            ("Date Range", lambda: self._set_filter("date:jan-mar")),
        ]
        for label, cmd in filter_btns:
            btn = tk.Button(filters, text=label, command=cmd,
                            bg=PANEL2, fg=TEXT_DIM, relief="flat",
                            font=("Segoe UI",8), padx=6, pady=4,
                            activebackground=ACCENT2, activeforeground=BG,
                            cursor="hand2")
            btn.pack(side="left", padx=2)
            bind_hover(btn, PANEL2, "#2b3138", TEXT_DIM, TEXT)

        # Search button
        self.search_btn = tk.Button(q_frame, text="▶  SEARCH",
                                    command=lambda: self.on_search(self.search_var.get()),
                                    bg=ACCENT2, fg=BG, relief="flat",
                                    font=("Segoe UI",10,"bold"), pady=8,
                                    activebackground=ACCENT,
                                    cursor="hand2")
        self.search_btn.pack(fill="x", pady=4)
        bind_hover(self.search_btn, ACCENT2, ACCENT, BG, BG)

        # ── DS badge ─────────────────────────────────────────────
        self._separator()
        ds_frame = tk.Frame(self.parent, bg=PANEL, pady=4)
        ds_frame.pack(fill="x", padx=10)

        tk.Label(ds_frame, text="DS Chosen:", bg=PANEL,
                 fg=TEXT_DIM, font=("Segoe UI",9)).pack(side="left")

        self.ds_badge = tk.Label(ds_frame, text=" B+ Tree ",
                                 bg=ACCENT, fg=BG, padx=8, pady=2,
                                 font=("Segoe UI",9,"bold"),
                                 cursor="hand2")
        self.ds_badge.pack(side="left", padx=6)
        self.ds_badge.bind("<Enter>", self._show_tooltip)
        self.ds_badge.bind("<Leave>", self._hide_tooltip)

        self.ds_reason = tk.Label(self.parent, text="Awaiting first query…",
                                  bg=PANEL, fg=TEXT_DIM,
                                  font=("Consolas",8), wraplength=480,
                                  anchor="w", justify="left")
        self.ds_reason.pack(fill="x", padx=10, pady=2)

        # ── Results ───────────────────────────────────────────────
        self._separator()
        tk.Label(self.parent, text="Results",
                 bg=PANEL, fg=TEXT,
                 font=("Segoe UI",10,"bold")).pack(anchor="w", padx=10)

        res_frame = tk.Frame(self.parent, bg=BORDER, pady=1, padx=1)
        res_frame.pack(fill="both", expand=True, padx=10, pady=4)

        self.results_text = tk.Text(res_frame, bg=BG, fg=TEXT,
                                    font=("Consolas",9), relief="flat",
                                    wrap="word", state="disabled",
                                    insertbackground=ACCENT,
                                    highlightthickness=0)
        vsb = tk.Scrollbar(res_frame, command=self.results_text.yview,
                           bg=PANEL2, troughcolor=BG)
        self.results_text.configure(yscrollcommand=vsb.set)
        self.results_text.pack(side="left", fill="both", expand=True)
        vsb.pack(side="right", fill="y")

        # Colour tags for results text
        self.results_text.tag_configure("found",  foreground=SUCCESS)
        self.results_text.tag_configure("notfnd", foreground=DANGER)
        self.results_text.tag_configure("header", foreground=ACCENT,
                                        font=("Consolas",9,"bold"))
        self.results_text.tag_configure("dim",    foreground=TEXT_DIM)
        self.results_text.tag_configure("warn",   foreground=WARNING)
        self.results_text.tag_configure("hero",   foreground=TEXT,
                                        font=("Segoe UI",11,"bold"))
        self.results_text.tag_configure("soft",   foreground=TEXT_DIM,
                                        font=("Segoe UI",9))

        # ── Analytics bar chart ───────────────────────────────────
        self._separator()
        tk.Label(self.parent, text="Storage Analytics",
                 bg=PANEL, fg=TEXT,
                 font=("Segoe UI",10,"bold")).pack(anchor="w", padx=10)

        self.chart_canvas = tk.Canvas(self.parent, bg=BG, height=190,
                                      highlightthickness=0)
        self.chart_canvas.pack(fill="x", padx=10, pady=4)
        self.chart_canvas.bind("<Configure>", lambda e: self._draw_chart())

        # ── Action buttons ────────────────────────────────────────
        self._separator()
        btn_frame = tk.Frame(self.parent, bg=PANEL, pady=6)
        btn_frame.pack(fill="x", padx=10)

        action_btns = [
            ("Duplicates",  self.on_dup),
            ("Orphans",     self.on_orphan),
            ("Analytics",   self.on_analytics),
            ("Benchmark",   self.on_bench),
        ]
        for i, (label, cmd) in enumerate(action_btns):
            col = i % 2
            row = i // 2
            b = tk.Button(btn_frame, text=label, command=cmd,
                          bg=PANEL2, fg=TEXT, relief="flat",
                          font=("Segoe UI",9), pady=7,
                          activebackground=ACCENT2, activeforeground=BG,
                          cursor="hand2")
            b.grid(row=row, column=col, padx=3, pady=2, sticky="ew")
            bind_hover(b, PANEL2, "#2b3138", TEXT, TEXT)
        btn_frame.columnconfigure(0, weight=1)
        btn_frame.columnconfigure(1, weight=1)
        self.show_welcome_state()

    # ── helpers ───────────────────────────────────────────────────
    def _separator(self):
        tk.Frame(self.parent, bg=BORDER, height=1).pack(fill="x", padx=0, pady=4)

    def _set_filter(self, suffix: str):
        cur = self.search_var.get()
        # strip known suffixes and apply new
        for s in ["*", "size:100-5000", "date:jan-mar"]:
            cur = cur.replace(s, "").strip()
        self.search_var.set(cur + suffix)
        self.entry.icursor("end")
        self.entry.focus_set()

    def set_search_text(self, text: str):
        self.search_var.set(text)
        self.entry.icursor("end")

    # ── public update methods ─────────────────────────────────────
    def set_ds_badge(self, ds_name: str, reason: str):
        label_map = {
            "BPlusTree":    "B+ Tree",
            "Trie":         "Trie",
            "SegmentTree":  "Segment Tree",
            "UnionFind":    "Union-Find",
            "PersistentDS": "Persistent DS",
        }
        label = label_map.get(ds_name, ds_name)
        color = DS_COLORS.get(ds_name, ACCENT)
        self.ds_badge.config(text=f" {label} ", bg=color,
                             fg=BG if ds_name != "PersistentDS" else TEXT)
        self.ds_badge._ds = ds_name  # store for tooltip
        self.ds_reason.config(text=reason)

    def show_result(self, lines: list):
        self.results_text.configure(state="normal")
        self.results_text.delete("1.0", "end")
        for line in lines:
            self._format_result_line(line)
        self.results_text.configure(state="disabled")
        self.results_text.see("end")

    def append_result(self, line: str):
        self.results_text.configure(state="normal")
        self._format_result_line(line)
        self.results_text.configure(state="disabled")
        self.results_text.see("end")

    def clear_results(self):
        self.results_text.configure(state="normal")
        self.results_text.delete("1.0","end")
        self.results_text.configure(state="disabled")

    def show_welcome_state(self):
        self.results_text.configure(state="normal")
        self.results_text.delete("1.0", "end")
        self.results_text.insert("end", "Search the indexed file system\n", "hero")
        self.results_text.insert(
            "end",
            "Use exact filenames, prefixes, or range queries to route work through the right data structure.\n\n",
            "soft",
        )
        self.results_text.insert("end", "Examples\n", "header")
        self.results_text.insert("end", "  report.pdf\n", "dim")
        self.results_text.insert("end", "  bud*\n", "dim")
        self.results_text.insert("end", "  size:100-5000\n", "dim")
        self.results_text.insert("end", "  date:jan-mar\n\n", "dim")
        self.results_text.insert(
            "end",
            "Explorer entries now reflect cached and indexed files instead of demo folders.\n",
            "soft",
        )
        self.results_text.configure(state="disabled")

    def _format_result_line(self, line: str):
        parts = line.split("|")
        t = self.results_text
        if not parts: return

        key = parts[0]
        if key == "RESULT" and len(parts) >= 3:
            status = parts[1]
            if status == "FOUND":
                t.insert("end", f"✓ FOUND  ", "found")
                t.insert("end", f"{parts[2]}\n", "header")
                if len(parts) >= 6:
                    t.insert("end", f"  Path    : {parts[3]}\n", "dim")
                    t.insert("end", f"  Size    : {parts[4]} KB\n", "dim")
                    t.insert("end", f"  DS      : {parts[5]}\n", "dim")
                if len(parts) >= 9:
                    t.insert("end", f"  Traverse: {parts[6]}\n", "dim")
                    t.insert("end", f"  Hops    : {parts[7]}\n", "dim")
                    t.insert("end", f"  Reason  : {parts[8]}\n", "dim")
            elif status == "NOT_FOUND":
                t.insert("end", f"✗ NOT FOUND  ", "notfnd")
                t.insert("end", f"{parts[2] if len(parts)>2 else ''}\n", "header")
                if len(parts) >= 5:
                    t.insert("end", f"  DS    : {parts[3]}\n", "dim")
                    t.insert("end", f"  Hops  : {parts[4]}\n", "dim")
            elif status == "PREFIX":
                names = parts[2].split(",") if len(parts)>2 else []
                t.insert("end", f"✓ PREFIX MATCHES ({len(names)})\n", "found")
                for n in names[:30]:
                    t.insert("end", f"  • {n}\n", "dim")
                if len(names) > 30:
                    t.insert("end", f"  … and {len(names)-30} more\n", "dim")
                if len(parts) >= 4:
                    t.insert("end", f"  Path: {parts[3]}\n", "dim")
            elif status == "SIZE":
                names = parts[2].split(",") if len(parts)>2 else []
                t.insert("end", f"✓ SIZE RANGE ({len(names)} files)\n", "found")
                for n in names[:30]:
                    t.insert("end", f"  • {n}\n", "dim")
        elif key == "DUP":
            if len(parts) >= 5:
                t.insert("end", f"⚠ DUP  {parts[1]}  ", "warn")
                t.insert("end", f"wasted: {parts[4]} KB\n", "dim")
        elif key == "DUP_DONE":
            t.insert("end",
                f"\n── {parts[1] if len(parts)>1 else '?'} group(s) | "
                f"{parts[2] if len(parts)>2 else '?'} MB wasted ──\n", "header")
        elif key == "ORPHAN":
            if len(parts) >= 5:
                t.insert("end", f"👻 ORPHAN  id={parts[1]}  {parts[2]}\n", "notfnd")
                t.insert("end", f"   {parts[3]}  |  {parts[4]} KB\n", "dim")
        elif key == "ORPHAN_DONE":
            t.insert("end",
                f"\n── {parts[1] if len(parts)>1 else '?'} orphan(s)  |  "
                f"{parts[2] if len(parts)>2 else '?'} MB ──\n", "header")
        elif key == "MONTH":
            if len(parts) >= 3:
                bar_len = min(int(float(parts[2])/5), 20)
                bar = "█"*bar_len
                t.insert("end", f"  {parts[1]:<10}  {bar:<20}  {parts[2]} MB\n", "dim")
        elif key == "ANALYTICS_DONE":
            t.insert("end",
                f"\n── Total: {parts[1] if len(parts)>1 else '?'} MB ──\n", "header")
        elif key == "BENCH":
            if len(parts) >= 5:
                t.insert("end", f"  {parts[1]:<22}", "dim")
                t.insert("end", f"  DS={parts[2]}µs  Lin={parts[3]}µs  "
                               f"×{parts[4]}\n", "found")
        elif key == "BENCH_DONE":
            t.insert("end", "── Benchmark complete ──\n", "header")
        elif key == "GENERATED":
            t.insert("end",
                f"✓ Generated {parts[1] if len(parts)>1 else '?'} files\n", "found")
        elif key == "LOADED":
            t.insert("end",
                f"✓ Loaded {parts[1] if len(parts)>1 else '?'} files  "
                f"({int(parts[2])//1048576 if len(parts)>2 else '?'} MB)\n", "found")
        elif key == "ERROR":
            t.insert("end",
                f"✗ ERROR [{parts[1] if len(parts)>1 else '?'}]: "
                f"{parts[2] if len(parts)>2 else ''}\n", "notfnd")
        elif key == "VERSION":
            if len(parts)>=5:
                t.insert("end",
                    f"  v{parts[2]}  {parts[3]} KB  ts={parts[4]}\n","dim")
        else:
            t.insert("end", line+"\n", "dim")

    # ── analytics chart ───────────────────────────────────────────
    def update_chart(self, month_data: dict):
        """month_data: {month_name: mb_float}"""
        self._month_data = month_data
        self._draw_chart()

    def _draw_chart(self):
        c = self.chart_canvas
        c.delete("all")
        w = c.winfo_width() or 480
        h = c.winfo_height() or 190
        if not self._month_data:
            c.create_text(w//2, h//2 - 10, text="Run Analytics to populate chart",
                          fill=TEXT_DIM, font=("Consolas",10))
            c.create_text(w//2, h//2 + 16,
                          text="Monthly storage totals will appear here",
                          fill="#6e7681", font=("Segoe UI",9))
            return

        max_mb = max(self._month_data.values()) or 1
        left_pad = 36
        right_pad = 18
        top_pad = 16
        bottom_pad = 34
        chart_height = max(40, h - top_pad - bottom_pad)
        step = max(18, (w - left_pad - right_pad) // 12)
        bar_w = max(10, step - 8)

        for ratio in (0.25, 0.5, 0.75, 1.0):
            y = top_pad + int(chart_height * (1 - ratio))
            c.create_line(left_pad, y, w - right_pad, y, fill="#20262d", width=1)
            c.create_text(left_pad - 10, y, text=f"{max_mb * ratio:.0f}",
                          fill="#6e7681", font=("Consolas",7))

        for i, month in enumerate(MONTH_NAMES):
            mb = self._month_data.get(month, 0.0)
            bar_h = int((mb / max_mb) * chart_height)
            x = left_pad + i * step
            y_top = top_pad + chart_height - bar_h
            color = ACCENT if month not in ("Jun","Jul","Aug") else WARNING
            c.create_rectangle(x, y_top, x+bar_w, top_pad + chart_height,
                               fill=color, outline="")
            c.create_text(x+bar_w//2, h-14,
                          text=month, fill=TEXT_DIM,
                          font=("Consolas",7))
            if mb > 0:
                c.create_text(x+bar_w//2, y_top-6,
                              text=f"{mb:.0f}", fill=TEXT_DIM,
                              font=("Consolas",6))

    # ── tooltip ───────────────────────────────────────────────────
    def _show_tooltip(self, event):
        self._hide_tooltip(None)
        ds = getattr(self.ds_badge, "_ds", "BPlusTree")
        text = DS_COMPLEXITY.get(ds, "")
        if not text: return
        self._tooltip_win = tw = tk.Toplevel(self.parent)
        tw.wm_overrideredirect(True)
        x = self.ds_badge.winfo_rootx() + 10
        y = self.ds_badge.winfo_rooty() - 35
        tw.wm_geometry(f"+{x}+{y}")
        tk.Label(tw, text=text, bg="#1f2937", fg=TEXT,
                 font=("Consolas",8), padx=8, pady=4,
                 relief="solid", bd=1).pack()

    def _hide_tooltip(self, event):
        if self._tooltip_win:
            self._tooltip_win.destroy()
            self._tooltip_win = None


# ═══════════════════════════════════════════════════════════════════
# 5. FileCache — SQLite persistent index
# ═══════════════════════════════════════════════════════════════════
class FileCache:
    """
    Persists the file index in a SQLite database so the app doesn't
    need to re-scan the filesystem on every restart.

    DB location: ~/.fsm_index/files.db
    """
    DB_DIR  = os.path.join(os.path.expanduser("~"), ".fsm_index")
    DB_PATH = os.path.join(DB_DIR, "files.db")

    def __init__(self):
        os.makedirs(self.DB_DIR, exist_ok=True)
        self._conn = sqlite3.connect(self.DB_PATH, check_same_thread=False)
        self._lock = threading.Lock()
        self._init_schema()

    def _init_schema(self):
        with self._lock:
            c = self._conn.cursor()
            c.executescript("""
                CREATE TABLE IF NOT EXISTS files (
                    id          INTEGER PRIMARY KEY AUTOINCREMENT,
                    name        TEXT NOT NULL,
                    path        TEXT NOT NULL UNIQUE,
                    size_bytes  INTEGER DEFAULT 0,
                    modified    INTEGER DEFAULT 0,
                    extension   TEXT DEFAULT '',
                    indexed_at  INTEGER DEFAULT 0
                );
                CREATE INDEX IF NOT EXISTS idx_name ON files(name);
                CREATE INDEX IF NOT EXISTS idx_ext  ON files(extension);

                CREATE TABLE IF NOT EXISTS indexed_folders (
                    path         TEXT PRIMARY KEY,
                    last_scanned INTEGER DEFAULT 0,
                    file_count   INTEGER DEFAULT 0
                );
            """)
            self._conn.commit()

    # ── write ─────────────────────────────────────────────────────
    def save_scan_results(self, folder_path: str, file_list: list):
        """
        Save scanned file records to SQLite.
        file_list: list of (name, full_path, size_bytes, modified_ts)
        """
        ts_now = int(time.time())
        with self._lock:
            c = self._conn.cursor()
            c.executemany("""
                INSERT OR REPLACE INTO files
                    (name, path, size_bytes, modified, extension, indexed_at)
                VALUES (?, ?, ?, ?, ?, ?)
            """, [
                (name, path, size, mod,
                 name.rsplit(".", 1)[-1].lower() if "." in name else "",
                 ts_now)
                for name, path, size, mod in file_list
            ])
            c.execute("""
                INSERT OR REPLACE INTO indexed_folders (path, last_scanned, file_count)
                VALUES (?, ?, ?)
            """, (folder_path, ts_now, len(file_list)))
            self._conn.commit()

    def save_os_walk_results(self, results: list):
        """
        Save OS-walk (fallback search) results to cache.
        results: list of (name, full_path, size_bytes)
        """
        ts_now = int(time.time())
        with self._lock:
            c = self._conn.cursor()
            c.executemany("""
                INSERT OR IGNORE INTO files
                    (name, path, size_bytes, modified, extension, indexed_at)
                VALUES (?, ?, ?, ?, ?, ?)
            """, [
                (name, path, size, 0,
                 name.rsplit(".", 1)[-1].lower() if "." in name else "",
                 ts_now)
                for name, path, size in results
            ])
            self._conn.commit()

    # ── read ──────────────────────────────────────────────────────
    def get_all_records(self, limit: int = 200_000):
        """Return all file records for C++ batch replay."""
        with self._lock:
            c = self._conn.cursor()
            c.execute(
                "SELECT name, path, size_bytes, modified FROM files LIMIT ?",
                (limit,)
            )
            return c.fetchall()

    def get_indexed_folders(self):
        """Return list of previously indexed folder paths."""
        with self._lock:
            c = self._conn.cursor()
            c.execute("SELECT path, last_scanned, file_count FROM indexed_folders ORDER BY last_scanned DESC")
            return c.fetchall()

    def get_file_count(self):
        with self._lock:
            c = self._conn.cursor()
            c.execute("SELECT COUNT(*) FROM files")
            return c.fetchone()[0]

    def search_local(self, query: str, limit: int = 100):
        """Fast local SQLite search — instant, no C++ needed."""
        with self._lock:
            c = self._conn.cursor()
            c.execute(
                "SELECT name, path, size_bytes, modified FROM files "
                "WHERE name LIKE ? LIMIT ?",
                (f"%{query}%", limit)
            )
            return c.fetchall()

    def get_recent_records(self, limit: int = 500):
        """Return recent records for the explorer UI."""
        with self._lock:
            c = self._conn.cursor()
            c.execute(
                "SELECT name, path, size_bytes, modified FROM files "
                "ORDER BY indexed_at DESC, modified DESC, name ASC LIMIT ?",
                (limit,)
            )
            return c.fetchall()

    def remove_folder(self, folder_path: str):
        """Remove all records that belong to a folder (and the folder entry)."""
        with self._lock:
            c = self._conn.cursor()
            c.execute("DELETE FROM files WHERE path LIKE ?", (folder_path + "%",))
            c.execute("DELETE FROM indexed_folders WHERE path = ?", (folder_path,))
            self._conn.commit()

    def close(self):
        with self._lock:
            self._conn.close()


# ═══════════════════════════════════════════════════════════════════
# 6. FSMApp — main controller
# ═══════════════════════════════════════════════════════════════════
class FSMApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("FSM — Advanced DS Project")
        self.root.geometry("1400x800")
        self.root.configure(bg=BG)
        self.root.minsize(1000, 600)

        self._result_queue = queue.Queue()
        self._file_count   = 0
        self._pending_tag  = None

        # Persistent SQLite index
        self.cache = FileCache()

        # Show loading screen
        self._show_loading()
        self.root.after(400, self._init_bridge)

    # ── loading screen ────────────────────────────────────────────
    def _show_loading(self):
        self._loading = tk.Frame(self.root, bg=BG)
        self._loading.place(relx=0, rely=0, relwidth=1, relheight=1)
        cx = tk.Canvas(self._loading, bg=BG, highlightthickness=0)
        cx.pack(fill="both", expand=True)

        def draw(event=None):
            cx.delete("all")
            w = cx.winfo_width() or 1400
            h = cx.winfo_height() or 800
            cx.create_text(w//2, h//2-60,
                text="FSM", fill=ACCENT,
                font=("Consolas", 72, "bold"))
            cx.create_text(w//2, h//2+10,
                text="File System Directory Manager",
                fill=TEXT, font=("Segoe UI", 18))
            cx.create_text(w//2, h//2+50,
                text="Advanced Data Structures Project",
                fill=TEXT_DIM, font=("Segoe UI", 12))
            cx.create_text(w//2, h//2+100,
                text="Starting C++ engine…",
                fill=TEXT_DIM, font=("Consolas", 10))

        cx.bind("<Configure>", draw)
        self.root.after(50, draw)

    def _hide_loading(self):
        if self._loading:
            self._loading.destroy()
            self._loading = None

    # ── bridge init ───────────────────────────────────────────────
    def _init_bridge(self):
        try:
            self.bridge = SubprocessBridge(self._result_queue)
        except FileNotFoundError as e:
            messagebox.showerror("Binary Not Found", str(e))
            self.root.destroy()
            return

        self._index_queue = []   # list of paths to load sequentially
        self._indexing   = False

        # Build UI then index real system folders
        self.root.after(100, self._build_ui)
        self.root.after(700, self._startup_index)
        self.root.after(100, self._poll_queue)

    def _get_real_user_folders(self):
        """
        Get real Windows user folder paths using the Shell API.
        Handles OneDrive-redirected folders (Desktop, Documents, etc.)
        that os.path.expanduser('~') misses.
        """
        folders = []
        if sys.platform == "win32":
            try:
                import ctypes, ctypes.wintypes
                # Known folder GUIDs
                FOLDERID = {
                    "Desktop":   "{B4BFCC3A-DB2C-424C-B029-7FE99A87C641}",
                    "Documents": "{FDD39AD0-238F-46AF-ADB4-6C85C50FD1EB}",
                    "Downloads": "{374DE290-123F-4565-9164-39C4925E467B}",
                    "Pictures":  "{33E28130-4E1E-4676-835A-98395C3BC3BB}",
                    "Music":     "{4BD8D571-6D19-48D3-BE97-422220080E43}",
                    "Videos":    "{18989B1D-99B5-455B-841C-AB7C74E4DDFC}",
                    "OneDrive":  "{A52BBA46-E9E1-435F-B3D9-28DAA648C0F6}",
                }
                shell32 = ctypes.windll.shell32
                for name, guid in FOLDERID.items():
                    try:
                        buf = ctypes.c_wchar_p()
                        # SHGetKnownFolderPath(rfid, dwFlags, hToken, ppszPath)
                        from ctypes import POINTER, byref
                        import ctypes.wintypes as wt
                        path_ptr = ctypes.c_wchar_p()
                        # Use SHGetFolderPath (older, more compatible API)
                        buf = ctypes.create_unicode_buffer(260)
                        # CSIDL values as fallback
                        CSIDL = {
                            "Desktop":   0x0000,
                            "Documents": 0x0005,
                            "Pictures":  0x0027,
                            "Music":     0x000D,
                            "Videos":    0x000E,
                        }
                        if name in CSIDL:
                            shell32.SHGetFolderPathW(0, CSIDL[name], 0, 0, buf)
                            p = buf.value
                        else:
                            # For Downloads and OneDrive, use registry
                            import winreg
                            if name == "Downloads":
                                key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                                    r"Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders")
                                p = winreg.QueryValueEx(key, "{374DE290-123F-4565-9164-39C4925E467B}")[0]
                            elif name == "OneDrive":
                                key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                                    r"Software\Microsoft\OneDrive")
                                p = winreg.QueryValueEx(key, "UserFolder")[0]
                            else:
                                p = ""
                        if p and os.path.isdir(p):
                            folders.append(p)
                    except Exception:
                        # Fall back to expanduser path
                        home = os.path.expanduser("~")
                        p = os.path.join(home, name)
                        if os.path.isdir(p):
                            folders.append(p)
            except Exception:
                pass
        # Generic fallback for any OS
        if not folders:
            home = os.path.expanduser("~")
            for name in ("Desktop", "Documents", "Downloads", "Pictures", "Music", "Videos", "OneDrive"):
                p = os.path.join(home, name)
                if os.path.isdir(p):
                    folders.append(p)
        return folders

    def _startup_index(self):
        """On startup: replay SQLite cache instantly, then scan any new folders."""
        cached_count = self.cache.get_file_count()
        if cached_count > 0:
            self._log(f"DB cache has {cached_count:,} records — replaying to C++ (no disk scan needed)")
            self._file_count = cached_count
            self._update_title()
            self._refresh_explorer_from_cache()
            self._set_busy("Replaying cached index into the C++ engine...")
            # Replay in background thread so UI stays responsive
            threading.Thread(target=self._replay_from_cache, daemon=True).start()
        else:
            self._log("No cache found — scanning common folders for the first time")
            self._index_queue = self._get_real_user_folders()
            self._log(f"Found {len(self._index_queue)} real folder(s) to index: " +
                      ", ".join(os.path.basename(p) for p in self._index_queue))
            self._set_busy("Scanning common folders for the first launch...")
            self._index_next()

    def _replay_from_cache(self):
        """Send all SQLite records to C++ via LOAD_BATCH — runs in background thread."""
        records = self.cache.get_all_records()
        if not records:
            return
        self._log(f"Sending {len(records):,} cached records to C++ engine…")
        try:
            with self.bridge._lock:
                self.bridge.proc.stdin.write("LOAD_BATCH_BEGIN\n")
                for name, path, size, mod in records:
                    # Sanitise pipe chars
                    safe_name = name.replace("|", "_")
                    safe_path = path.replace("|", "_")
                    line = f"{safe_name}|{safe_path}|{size}|{mod}\n"
                    self.bridge.proc.stdin.write(line)
                self.bridge.proc.stdin.write("LOAD_BATCH_END\n")
                self.bridge.proc.stdin.flush()
            # Read BATCH_DONE response
            response = self.bridge.proc.stdout.readline().rstrip("\n\r")
            parts = response.split("|")
            if parts[0] == "BATCH_DONE":
                count = int(parts[1]) if len(parts) > 1 else 0
                self._result_queue.put(("RESULT", "batch_replay",
                                        [f"BATCH_DONE|{count}"]))
        except Exception as e:
            self._log(f"Batch replay error: {e}")

    def _index_next(self):
        """Send the next LOAD_DIR from the queue (sequential, non-blocking).
        After completion the result is saved to SQLite by _handle_result."""
        while self._index_queue and not os.path.isdir(self._index_queue[0]):
            self._index_queue.pop(0)
        if not self._index_queue:
            self._indexing = False
            self._log("Background indexing complete — results saved to database")
            self._refresh_explorer_from_cache()
            self._clear_busy()
            return
        path = self._index_queue.pop(0)
        self._indexing = True
        self._current_scan_path = path  # remember for SQLite save
        folder_name = os.path.basename(path) or path
        try:
            self.title_var.set(f"FSM — Indexing {folder_name}…")
        except AttributeError:
            pass
        self._log(f"Scanning {path}  ({len(self._index_queue)} remaining) → will save to DB")
        self._set_busy(f"Indexing {folder_name}... {len(self._index_queue)} folder(s) remaining")
        self.bridge.send(f"LOAD_DIR {path}", "startup_load_dir")

    # ── main UI ───────────────────────────────────────────────────
    def _build_ui(self):
        self._hide_loading()

        # Title bar
        title_bar = tk.Frame(self.root, bg=PANEL, height=36)
        title_bar.pack(fill="x")
        title_bar.pack_propagate(False)
        self.title_var = tk.StringVar(value="FSM — Advanced DS Project")
        tk.Label(title_bar, textvariable=self.title_var,
                 bg=PANEL, fg=TEXT, font=("Segoe UI",11,"bold")
                 ).pack(side="left", padx=14, pady=6)
        tk.Label(title_bar, text="B+Tree · Trie · SegTree · UnionFind · PersistentDS",
                 bg=PANEL, fg=TEXT_DIM, font=("Segoe UI",9)
                 ).pack(side="right", padx=14, pady=6)

        # Kbd shortcuts hint
        kb = tk.Label(title_bar, text="Enter=Search  Ctrl+G=Generate",
                      bg=PANEL, fg=TEXT_DIM, font=("Consolas",8))
        kb.pack(side="right", padx=14)

        self._busy_frame = tk.Frame(self.root, bg="#0f1724", height=28)
        self._busy_frame.pack_propagate(False)
        ttk.Style().configure(
            "FSM.Horizontal.TProgressbar",
            troughcolor="#0f1724",
            bordercolor="#0f1724",
            background=ACCENT,
            lightcolor=ACCENT,
            darkcolor=ACCENT2,
        )
        self._busy_label = tk.Label(
            self._busy_frame,
            text="",
            bg="#0f1724",
            fg=TEXT,
            font=("Segoe UI", 9),
            anchor="w",
        )
        self._busy_label.pack(side="left", fill="x", expand=True, padx=(12, 8), pady=4)
        self._busy_bar = ttk.Progressbar(
            self._busy_frame,
            mode="indeterminate",
            style="FSM.Horizontal.TProgressbar",
            length=160,
        )
        self._busy_bar.pack(side="right", padx=12, pady=7)
        self._busy_visible = False

        # Main content area
        content = tk.Frame(self.root, bg=BG)
        content.pack(fill="both", expand=True)

        # ── LEFT PANEL (280px) ────────────────────────────────────
        left = tk.Frame(content, bg=PANEL, width=280)
        left.pack(side="left", fill="y")
        left.pack_propagate(False)

        self.file_explorer = FileExplorer(
            left,
            on_file_select_cb=self._on_file_selected,
            on_scan_cb=self._on_scan_folder,
            on_generate_cb=self._on_generate,
            on_index_system_cb=self._on_index_system,
        )

        # ── CENTER divider ────────────────────────────────────────
        tk.Frame(content, bg=BORDER, width=1).pack(side="left", fill="y")

        # ── CENTER PANEL (flexible) ───────────────────────────────
        center = tk.Frame(content, bg=BG)
        center.pack(side="left", fill="both", expand=True)

        self.anim_canvas = AnimationCanvas(center)
        self.anim_canvas.set_ds("BPlusTree")

        # ── RIGHT divider ─────────────────────────────────────────
        tk.Frame(content, bg=BORDER, width=1).pack(side="left", fill="y")

        # ── RIGHT PANEL (520px) ───────────────────────────────────
        right = tk.Frame(content, bg=PANEL, width=520)
        right.pack(side="right", fill="y")
        right.pack_propagate(False)

        self.query_panel = QueryPanel(
            right,
            on_search_cb=self._on_search,
            on_dup_cb=self._on_duplicates,
            on_orphan_cb=self._on_orphans,
            on_analytics_cb=self._on_analytics,
            on_bench_cb=self._on_benchmark,
        )

        # Keyboard shortcuts
        self.root.bind("<Control-g>", lambda e: self._on_generate())
        self.root.bind("<Control-l>", lambda e: self._toggle_log())

        # ── Activity Log (bottom strip) ───────────────────────
        self._build_activity_log()
        self._toggle_log()
        self._refresh_explorer_from_cache()
        self._log("App started — indexing system folders in background")

    def _set_busy(self, message: str):
        if not hasattr(self, "_busy_frame"):
            return
        self._busy_label.config(text=message)
        if not self._busy_visible:
            self._busy_frame.pack(fill="x", after=self.root.winfo_children()[0])
            self._busy_visible = True
        try:
            self._busy_bar.start(12)
        except tk.TclError:
            pass

    def _clear_busy(self):
        if not hasattr(self, "_busy_frame"):
            return
        try:
            self._busy_bar.stop()
        except tk.TclError:
            pass
        if self._busy_visible:
            self._busy_frame.pack_forget()
            self._busy_visible = False

    def _refresh_explorer_from_cache(self):
        if not hasattr(self, "file_explorer"):
            return
        try:
            records = self.cache.get_recent_records(500)
            folders = [path for path, _ts, _count in self.cache.get_indexed_folders()]
            self.file_explorer.load_records(records, folders)
        except Exception as exc:
            self._log(f"Explorer refresh skipped: {exc}")

    # ── queue polling ─────────────────────────────────────────────
    def _poll_queue(self):
        try:
            while True:
                kind, tag, lines = self._result_queue.get_nowait()
                self._handle_result(tag, lines)
        except queue.Empty:
            pass
        self.root.after(80, self._poll_queue)

    def _handle_result(self, tag: str, lines: list):
        if not lines: return
        first = lines[0]
        parts = first.split("|")

        if tag == "startup_load_dir":
            if parts[0] == "LOADED" and len(parts) > 1:
                try:
                    count = int(parts[1])
                    total_bytes = int(parts[2]) if len(parts) > 2 else 0
                    self._file_count += count
                    self._update_title()
                    self._log(f"Indexed {count:,} file(s)  {total_bytes//1048576} MB  → total {self._file_count:,} — saving to DB")
                    try:
                        self.query_panel.append_result(
                            f"LOADED|{count}|{parts[2] if len(parts)>2 else '0'}")
                    except Exception:
                        pass
                    # Save to SQLite in background — scan the folder we just indexed
                    scan_path = getattr(self, "_current_scan_path", "")
                    if scan_path:
                        threading.Thread(
                            target=self._save_folder_to_cache,
                            args=(scan_path,), daemon=True
                        ).start()
                except ValueError:
                    pass
            elif parts[0] == "ERROR":
                self._log(f"LOAD_DIR error: {parts[2] if len(parts)>2 else '?'}")
            # Load next folder in chain
            self._index_next()
            return

        if tag == "batch_replay":
            if parts[0] == "BATCH_DONE":
                count = int(parts[1]) if len(parts) > 1 else 0
                self._log(f"Cache replay complete — {count:,} records loaded into C++ engine instantly")
                self.anim_canvas.set_status(f"Loaded {count:,} files from database — search is ready")
                self._refresh_explorer_from_cache()
                self._clear_busy()
            return

        if tag.startswith("search_"):
            # Pass original query from tag (tag = "search_<query>")
            original_query = tag[7:] if len(tag) > 7 else ""
            self._log(f"SEARCH '{original_query}' → {parts[1] if len(parts)>1 else '?'}")
            self._handle_search_result(lines, query=original_query)
        elif tag.startswith("prefix_"):
            self._log(f"PREFIX search → {parts[2].count(',') + 1 if len(parts)>2 and parts[2] else 0} match(es)")
            self._handle_prefix_result(lines)
        elif tag == "size_range":
            self._log(f"SIZE_RANGE query → {parts[3] if len(parts)>3 else '?'} result(s)")
            self._handle_size_result(lines)
        elif tag == "date_range":
            self._log(f"DATE_RANGE {parts[2] if len(parts)>2 else '?'}–{parts[3] if len(parts)>3 else '?'} → {parts[1] if len(parts)>1 else '?'} MB")
            self._handle_date_result(lines)
        elif tag == "duplicates":
            dup_count = sum(1 for l in lines if l.startswith("DUP|"))
            self._log(f"DUPLICATES → {dup_count} group(s) found")
            self._handle_duplicates(lines)
        elif tag == "orphans":
            orph_count = sum(1 for l in lines if l.startswith("ORPHAN|"))
            self._log(f"ORPHANS → {orph_count} orphan(s) detected")
            self._handle_orphans(lines)
        elif tag == "analytics":
            self._log("ANALYTICS complete")
            self._handle_analytics(lines)
        elif tag == "benchmark":
            self._log("BENCHMARK complete")
            self._handle_benchmark(lines)
        elif tag == "generate":
            gen_count = parts[1] if len(parts)>1 else '?'
            self._log(f"GENERATE → {gen_count} demo files added")
            self._handle_generate(lines)
        elif tag == "load_dir":
            count = parts[1] if len(parts)>1 else '?'
            self._log(f"LOAD_DIR (manual) → {count} file(s) loaded")
            self._handle_load_dir(lines)
        elif tag == "os_search":
            self._handle_os_search(lines)

    # ── result handlers ───────────────────────────────────────────
    def _handle_search_result(self, lines: list, query: str = ""):
        if not lines: return
        line = lines[0]
        parts = line.split("|")
        self.query_panel.show_result([line])

        if len(parts) >= 2 and parts[1] == "FOUND":
            tpath = parts[6] if len(parts) > 6 else ""
            hops  = int(parts[7]) if len(parts) > 7 else 3
            self.anim_canvas.animate_bptree(tpath, True, hops)
            self.anim_canvas.set_status(f"Found in {hops} hops | O(log n) B+ Tree search")
            self._clear_busy()
        else:
            hops = int(parts[4]) if len(parts) > 4 else 3
            self.anim_canvas.animate_bptree("root->node1->leaf3", False, hops)
            self.anim_canvas.set_status("Not in index — falling back to OS search…")
            # ── Fallback: search the real filesystem ──────────────
            # recover the original query from the tag on the result
            raw_query = query or (parts[2] if len(parts) > 2 else "")
            if raw_query:
                self.query_panel.append_result(
                    f"# Not found in index. Searching real disk for '{raw_query}'…")
                self._set_busy(f"Searching real disk for '{raw_query}'...")
                self._os_search(raw_query)

    def _handle_prefix_result(self, lines: list):
        if not lines: return
        line = lines[0]
        parts = line.split("|")
        self.query_panel.show_result([line])
        prefix_used = getattr(self, "_last_prefix", "")
        names = parts[2].split(",") if len(parts)>2 else []
        tpath = parts[3] if len(parts)>3 else prefix_used
        count = len(names)
        self.anim_canvas.animate_trie(tpath.replace("->",""), count)
        self.anim_canvas.set_status(f"Trie prefix match | {count} file(s) | O(m) lookup")
        self._clear_busy()

    def _handle_size_result(self, lines: list):
        self.query_panel.show_result(lines)
        count = len([l for l in lines if l.startswith("RESULT|SIZE")])
        parts = lines[0].split("|") if lines else []
        cnt = parts[3] if len(parts)>3 else "?"
        self.anim_canvas.animate_segtree(1, 6, 0)
        self.anim_canvas.set_status(f"Size range result | {cnt} file(s) | SegmentTree")
        self._clear_busy()

    def _handle_date_result(self, lines: list):
        self.query_panel.show_result(lines)
        if lines:
            p = lines[0].split("|")
            mb = float(p[1]) if len(p)>1 else 0
            m1 = int(p[2]) if len(p)>2 else 1
            m2 = int(p[3]) if len(p)>3 else 12
            self.anim_canvas.animate_segtree(m1, m2, mb)
            self.anim_canvas.set_status(f"Date range | Total {mb:.2f} MB | SegmentTree")
        self._clear_busy()

    def _handle_duplicates(self, lines: list):
        self.query_panel.show_result(lines)
        dups = [l for l in lines if l.startswith("DUP|")]
        for l in lines:
            if l.startswith("DUP_DONE|"):
                p = l.split("|")
                self.anim_canvas.set_status(
                    f"Duplicates | {p[1] if len(p)>1 else '?'} groups | "
                    f"{p[2] if len(p)>2 else '?'} MB wasted")
        self._clear_busy()

    def _handle_orphans(self, lines: list):
        self.query_panel.show_result(lines)
        orphan_count = len([l for l in lines if l.startswith("ORPHAN|")])
        self.anim_canvas.animate_unionfind(orphan_count)
        self.anim_canvas.set_status(f"Orphan detection | {orphan_count} orphan(s) | UnionFind O(α(n))")
        self._clear_busy()

    def _handle_analytics(self, lines: list):
        self.query_panel.show_result(lines)
        month_data = {}
        for line in lines:
            p = line.split("|")
            if p[0] == "MONTH" and len(p) >= 3:
                try:
                    month_data[p[1]] = float(p[2])
                except ValueError:
                    pass
        self.query_panel.update_chart(month_data)
        total = sum(month_data.values())
        self.anim_canvas.set_status(f"Storage analytics | Total {total:.2f} MB | SegmentTree O(log n)")
        self._clear_busy()

    def _handle_benchmark(self, lines: list):
        self.query_panel.show_result(lines)
        self.anim_canvas.set_status("Benchmark complete | See results panel")
        self._clear_busy()

    def _handle_generate(self, lines: list):
        self.query_panel.show_result(lines)
        if lines:
            p = lines[0].split("|")
            if p[0] == "GENERATED" and len(p) > 1:
                try:
                    count = int(p[1])
                    self._file_count += count
                    self._update_title()
                except ValueError:
                    pass
        self._clear_busy()

    def _handle_load_dir(self, lines: list):
        self.query_panel.show_result(lines)
        if lines:
            p = lines[0].split("|")
            if p[0] == "LOADED" and len(p) > 1:
                try:
                    count = int(p[1])
                    self._file_count += count
                    self._update_title()
                    self.file_explorer.set_file_count(self._file_count)
                except ValueError:
                    pass
        self._clear_busy()

    # ── live OS fallback search ───────────────────────────────────
    def _os_search(self, query: str):
        """Search the real filesystem for files matching the query.
        Runs in a background thread — posts results to the queue."""
        self._log(f"OS fallback search starting for '{query}'…")
        def _worker():
            results = []
            home = os.path.expanduser("~")
            # Search drives: start from home, then expand to full C:\ if needed
            search_roots = [home]
            if sys.platform == "win32":
                search_roots.append("C:\\")
            else:
                search_roots.append("/")

            query_lower = query.lower()
            seen = set()
            for root_dir in search_roots:
                if len(results) >= 200:
                    break
                try:
                    for dirpath, dirnames, filenames in os.walk(root_dir):
                        # Skip known system/noise folders
                        dirnames[:] = [d for d in dirnames
                                       if d not in (
                                           "$Recycle.Bin", "Windows", "System32",
                                           "SysWOW64", "__pycache__", ".git",
                                           "node_modules", "AppData"
                                       )]
                        for fname in filenames:
                            if query_lower in fname.lower():
                                fp = os.path.join(dirpath, fname)
                                if fp not in seen:
                                    seen.add(fp)
                                    try:
                                        sz = os.path.getsize(fp)
                                    except OSError:
                                        sz = 0
                                    results.append((fname, fp, sz))
                                    if len(results) >= 200:
                                        break
                        if len(results) >= 200:
                            break
                except PermissionError:
                    continue
            self._result_queue.put(("RESULT", "os_search",
                                    [(query, results)]))
        threading.Thread(target=_worker, daemon=True).start()

    def _handle_os_search(self, payload):
        """Display OS fallback search results and save to SQLite."""
        if not payload: return
        query, results = payload[0]
        self._log(f"OS search for '{query}' → {len(results)} real file(s) found")
        # Save discovered files to SQLite cache in background
        if results:
            threading.Thread(
                target=self.cache.save_os_walk_results,
                args=(results,), daemon=True
            ).start()
        self.query_panel.clear_results()
        if not results:
            self.query_panel.show_result(
                [f"ERROR|OS Search|No files matching '{query}' found anywhere on system"])
            self.anim_canvas.set_status(f"OS search complete — no match for '{query}'")
            self._refresh_explorer_from_cache()
            self._clear_busy()
            return
        lines = [f"# OS Search results for '{query}' ({len(results)} found)"]
        for fname, fpath, sz in results[:100]:
            kb = sz // 1024
            lines.append(f"RESULT|FOUND|{fname}|{fpath}|{kb}|OS-FileSystem|os-walk|1|Real file found on disk")
        self.query_panel.show_result(lines)
        self.anim_canvas.set_status(
            f"OS search — {len(results)} real file(s) found matching '{query}'")
        self._refresh_explorer_from_cache()
        self._clear_busy()
        # Also send the first result to the C++ index so it's searchable next time
        if results:
            fname, fpath, sz = results[0]
            parent_dir = os.path.dirname(fpath)
            self.bridge.send(f"LOAD_DIR {parent_dir}", "startup_load_dir")

    # ── event handlers ────────────────────────────────────────────
    def _on_search(self, query: str):
        query = query.strip()
        if not query: return

        self.query_panel.clear_results()
        q_lower = query.lower()

        if q_lower == "duplicates":
            self._on_duplicates()
        elif q_lower == "orphans":
            self._on_orphans()
        elif query.endswith("*"):
            prefix = query[:-1]
            self._last_prefix = prefix
            self.anim_canvas.set_ds("Trie")
            self.anim_canvas.show_spinner(f"Prefix search: {prefix}…")
            self._set_busy(f"Running prefix search for '{prefix}'...")
            self.query_panel.set_ds_badge("Trie",
                "Trie provides O(m) prefix lookups where m is the prefix length.")
            self.bridge.send(f"PREFIX {prefix}", f"prefix_{query}")
        elif query.lower().startswith("size:"):
            parts = query[5:].split("-")
            if len(parts) == 2:
                self.anim_canvas.set_ds("SegmentTree")
                self.anim_canvas.show_spinner("Size range search…")
                self._set_busy(f"Running size range query: {parts[0]}-{parts[1]} KB...")
                self.query_panel.set_ds_badge("SegmentTree",
                    "Segment Tree enables O(log n) range queries over file sizes.")
                self.bridge.send(f"SIZE_RANGE {parts[0]} {parts[1]}", "size_range")
        elif query.lower().startswith("date:"):
            rest = query[5:].split("-")
            m_map = {m.lower():str(i+1) for i,m in enumerate(MONTH_NAMES)}
            m1 = m_map.get(rest[0].lower(), "1")
            m2 = m_map.get(rest[-1].lower(), "12") if len(rest)>1 else m1
            self.anim_canvas.set_ds("SegmentTree")
            self.anim_canvas.show_spinner("Date range query…")
            self._set_busy("Running date range analytics query...")
            self.query_panel.set_ds_badge("SegmentTree",
                "Segment Tree enables O(log n) range sum queries over day-indexed storage.")
            self.bridge.send(f"DATE_RANGE {m1} {m2}", "date_range")
        else:
            # Exact search
            self.anim_canvas.set_ds("BPlusTree")
            self.anim_canvas.show_spinner(f"Searching: {query}…")
            self._set_busy(f"Searching index for '{query}'...")
            self.query_panel.set_ds_badge("BPlusTree",
                "B+ Tree allows O(log n) exact key lookup using the filename as the sorted key.")
            self.bridge.send(f"SEARCH {query}", f"search_{query}")

    def _on_file_selected(self, name: str):
        self.query_panel.set_search_text(name)

    def _on_scan_folder(self):
        path = filedialog.askdirectory(title="Select Folder to Scan")
        if not path: return
        self._log(f"Manual scan: {path} → will save to DB")
        self._current_scan_path = path
        self.anim_canvas.show_spinner("Scanning folder…")
        self._set_busy(f"Scanning {os.path.basename(path) or path}...")
        self.query_panel.clear_results()
        self.bridge.send(f"LOAD_DIR {path}", "load_dir")

    def _on_index_system(self):
        """Index all common Windows/home folders for real-file search."""
        home = os.path.expanduser("~")
        common = [
            os.path.join(home, "Desktop"),
            os.path.join(home, "Documents"),
            os.path.join(home, "Downloads"),
            os.path.join(home, "Pictures"),
            os.path.join(home, "Music"),
            os.path.join(home, "Videos"),
            os.path.join(home, "OneDrive"),
            "C:\\Program Files",
            "C:\\Program Files (x86)",
        ]
        self._index_queue = [p for p in common
                             if os.path.isdir(p) and p not in getattr(self, "_indexed_paths", set())]
        self._indexed_paths = getattr(self, "_indexed_paths", set()) | set(self._index_queue)
        self.query_panel.clear_results()
        self.anim_canvas.show_spinner("Indexing system folders…")
        self._set_busy("Indexing your common system folders...")
        self._index_next()

    def _on_generate(self):
        self._log("Generating 1000 demo files…")
        self.anim_canvas.show_spinner("Generating 1000 files…")
        self._set_busy("Generating demo data in the C++ engine...")
        self.query_panel.clear_results()
        self.bridge.send("GENERATE 1000", "generate")

    def _on_duplicates(self):
        self.anim_canvas.set_ds("BPlusTree")
        self.anim_canvas.show_spinner("Finding duplicates…")
        self._set_busy("Scanning indexed files for duplicate checksums...")
        self.query_panel.clear_results()
        self.query_panel.set_ds_badge("BPlusTree",
            "B+ Tree iterates all leaves and groups by checksum to detect duplicates.")
        self.bridge.send("DUPLICATES", "duplicates")

    def _on_orphans(self):
        self.anim_canvas.set_ds("UnionFind")
        self.anim_canvas.show_spinner("Detecting orphans…")
        self._set_busy("Evaluating connectivity to find orphan files...")
        self.query_panel.clear_results()
        self.query_panel.set_ds_badge("UnionFind",
            "Union-Find identifies files not connected to the root directory component.")
        self.bridge.send("ORPHANS", "orphans")

    def _on_analytics(self):
        self.anim_canvas.set_ds("SegmentTree")
        self.anim_canvas.show_spinner("Computing analytics…")
        self._set_busy("Computing monthly storage analytics...")
        self.query_panel.clear_results()
        self.query_panel.set_ds_badge("SegmentTree",
            "Segment Tree provides O(log n) range sum queries for monthly storage analytics.")
        self.bridge.send("ANALYTICS", "analytics")

    def _on_benchmark(self):
        self.anim_canvas.set_ds("BPlusTree")
        self.anim_canvas.show_spinner("Running benchmark…")
        self._set_busy("Benchmarking DS search against linear scan...")
        self.query_panel.clear_results()
        self.bridge.send("BENCHMARK", "benchmark")

    # ── misc helpers + activity log ─────────────────────────────
    def _build_activity_log(self):
        """Build a collapsible activity log strip at the bottom of the window."""
        import time as _time
        self._log_visible = True

        self._log_shell = tk.Frame(self.root, bg=BG)
        self._log_shell.pack(fill="x", side="bottom")

        # Separator
        tk.Frame(self._log_shell, bg=BORDER, height=1).pack(fill="x", side="top")

        # Log container
        self._log_frame = tk.Frame(self._log_shell, bg="#0a0d12", height=120)
        self._log_frame.pack(fill="x", side="bottom")
        self._log_frame.pack_propagate(False)

        # Header bar with toggle
        log_hdr = tk.Frame(self._log_frame, bg="#0a0d12", height=22)
        log_hdr.pack(fill="x")
        log_hdr.pack_propagate(False)

        tk.Label(log_hdr, text="■ Activity Log",
                 bg="#0a0d12", fg=TEXT_DIM,
                 font=("Consolas", 8, "bold")).pack(side="left", padx=8)

        tk.Label(log_hdr, text="Ctrl+L to toggle",
                 bg="#0a0d12", fg="#484f58",
                 font=("Consolas", 7)).pack(side="right", padx=8)

        self._log_clear_btn = tk.Button(
            log_hdr, text="clear",
            bg="#0a0d12", fg="#484f58",
            relief="flat", font=("Consolas",7),
            cursor="hand2", bd=0,
            command=self._clear_log,
            activebackground="#161b22", activeforeground=TEXT_DIM
        )
        self._log_clear_btn.pack(side="right", padx=4)

        # Text widget
        log_body = tk.Frame(self._log_frame, bg="#0a0d12")
        log_body.pack(fill="both", expand=True, padx=4, pady=(0,4))

        self._log_text = tk.Text(
            log_body, bg="#0a0d12", fg="#484f58",
            font=("Consolas", 8), relief="flat",
            wrap="word", state="disabled",
            insertbackground=ACCENT,
            highlightthickness=0, height=5,
        )
        log_vsb = tk.Scrollbar(log_body, command=self._log_text.yview,
                               bg="#0a0d12", troughcolor="#0a0d12", width=6)
        self._log_text.configure(yscrollcommand=log_vsb.set)
        self._log_text.pack(side="left", fill="both", expand=True)
        log_vsb.pack(side="right", fill="y")

        # Tags
        self._log_text.tag_configure("ts",   foreground="#484f58")
        self._log_text.tag_configure("load", foreground="#3fb950")
        self._log_text.tag_configure("srch", foreground="#58a6ff")
        self._log_text.tag_configure("err",  foreground="#f85149")
        self._log_text.tag_configure("info", foreground="#8b949e")
        self._log_text.tag_configure("os",   foreground="#bc8cff")

    def _log(self, message: str):
        """Append a timestamped line to the activity log."""
        import time as _time
        try:
            ts = _time.strftime("%H:%M:%S")
            # Pick tag based on content
            msg_lower = message.lower()
            if "error" in msg_lower:
                tag = "err"
            elif any(k in msg_lower for k in ("load_dir", "indexed", "index")):
                tag = "load"
            elif any(k in msg_lower for k in ("search", "prefix", "found")):
                tag = "srch"
            elif "os " in msg_lower or "real" in msg_lower or "disk" in msg_lower:
                tag = "os"
            else:
                tag = "info"

            self._log_text.configure(state="normal")
            self._log_text.insert("end", f"[{ts}] ", "ts")
            self._log_text.insert("end", message + "\n", tag)
            # Keep last 500 lines only
            lines = int(self._log_text.index("end-1c").split(".")[0])
            if lines > 500:
                self._log_text.delete("1.0", "10.0")
            self._log_text.configure(state="disabled")
            self._log_text.see("end")
        except Exception:
            pass  # log must never crash the app

    def _clear_log(self):
        try:
            self._log_text.configure(state="normal")
            self._log_text.delete("1.0", "end")
            self._log_text.configure(state="disabled")
        except Exception:
            pass

    def _toggle_log(self):
        """Show/hide the activity log strip."""
        if self._log_visible:
            self._log_shell.pack_forget()
            self._log_visible = False
        else:
            self._log_shell.pack(fill="x", side="bottom")
            self._log_visible = True

    def _update_title(self):
        self.title_var.set(f"FSM — {self._file_count:,} files indexed")
        self.root.title(f"FSM — {self._file_count:,} files indexed")
        self.file_explorer.set_file_count(self._file_count)

    def _save_folder_to_cache(self, folder_path: str):
        """Walk folder_path and save all files to SQLite (background thread)."""
        file_list = []
        try:
            for dirpath, dirnames, filenames in os.walk(folder_path):
                dirnames[:] = [d for d in dirnames
                               if d not in ("$Recycle.Bin", "__pycache__",
                                            ".git", "node_modules")]
                for fname in filenames:
                    fp = os.path.join(dirpath, fname)
                    try:
                        stat = os.stat(fp)
                        file_list.append((fname, fp, stat.st_size, int(stat.st_mtime)))
                    except OSError:
                        pass
        except PermissionError:
            pass
        if file_list:
            self.cache.save_scan_results(folder_path, file_list)
            self._log(f"DB saved: {len(file_list):,} records from {os.path.basename(folder_path)}")
            try:
                self.root.after(0, self._refresh_explorer_from_cache)
            except Exception:
                pass

    def on_close(self):
        try:
            self.bridge.shutdown()
        except Exception:
            pass
        try:
            self.cache.close()
        except Exception:
            pass
        self.root.destroy()


# ═══════════════════════════════════════════════════════════════════
# ENTRY POINT
# ═══════════════════════════════════════════════════════════════════
def main():
    root = tk.Tk()
    root.configure(bg=BG)

    # Set DPI awareness on Windows
    try:
        from ctypes import windll
        windll.shcore.SetProcessDpiAwareness(1)
    except Exception:
        pass

    # Font smoothing
    try:
        root.tk.call("tk", "scaling", 1.25)
    except Exception:
        pass

    app = FSMApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    main()
