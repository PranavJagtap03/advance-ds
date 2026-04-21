# File System Directory Manager — Project Summary

This document summarizes the development and features of the File System Directory Manager, spanning the C++ backend and the Python/Tkinter GUI frontend.

## 1. Backend Architecture (C++)
The core engine is built in C++ for maximum performance, handling complex data structures to index and query file metadata.

### Data Structures
- **B+ Tree**: Used for exact-match file name lookups (O(log n)).
- **Trie**: Powers prefix-based search (e.g., `budget*`) (O(m)).
- **Segment Tree**: Manages range-based queries for file size and creation date (O(log n)).
- **Union-Find**: Detects "orphan" files (files disconnected from a valid directory structure).
- **Persistent DS**: Tracks version history of file metadata changes.

### Key Backend Enhancements
- **Query Analyzer**: A new classification layer that determines the optimal data structure to use based on the user's string query.
- **Machine Interface (`--machine`)**: A pipe-delimited communication protocol allowing the GUI to send commands and receive structured responses.
- **Fast Replay (`LOAD_RECORD` / `LOAD_BATCH`)**: Commands that allow the engine to be populated instantly from a database, bypassing slow filesystem traversal.
- **Path Visualization**: Search methods now return the exact traversal path (e.g., `root->node1->leaf`) to enable UI animations.

## 2. Frontend Application (Python)
A high-performance desktop GUI built using `tkinter` that acts as a visual wrapper for the C++ engine.

### Core Modules
- **Subprocess Bridge**: Manages the C++ process, using background threads and thread-safe queues for non-blocking communication.
- **Animation Canvas**: A central hub that draws real-time visualizations of data structure operations, highlighting traversal hops and results.
- **File Explorer**: A tree-view based panel for navigating folders, scanning real system directories, and generating demo data.
- **Activity Log**: A real-time event monitor (Toggle with `Ctrl+L`) that displays background tasks, C++ communication, and performance metrics.

### Key Features
- **Persistent SQLite Cache**: All scanned files are stored in `~/.fsm_index/files.db`. On restart, the app reloads thousands of files in milliseconds.
- **Auto-System Indexing**: Automatically indexes common user folders (Desktop, Documents, etc.) in the background upon launch.
- **Live OS Fallback**: If a file is not in the indexed database, the app automatically performs a real-time filesystem walk to find it and adds it to the index.
- **Query Badges**: Shows the time complexity (e.g., O(log n)) and reasoning for the data structure chosen for every search.

## 3. Visual & UX Design
- **Dark Mode**: GitHub-inspired color palette (`#0d1117`, `#58a6ff`).
- **Responsive Layout**: Three-panel window (1400x800) with collapsible logs and flexible visualization canvas.
- **Micro-animations**: Smooth highlights and transitions during search operations.

## 4. How to Run
1. **Build Backend**: `g++ -std=c++17 -O2 main.cpp BPlusTree.cpp Trie.cpp UnionFind.cpp SegmentTree.cpp PersistentDS.cpp -o fsm.exe`
2. **Launch App**: `python gui/app.py`

---
*Created by Antigravity AI — April 2026*
