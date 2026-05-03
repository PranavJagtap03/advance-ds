# DataStruct FSM — File System Monitor

> A high-performance file system management and analytics platform built with a custom C++ engine powered by advanced data structures, exposed through a Python REST/WebSocket API, and visualized through a modern React dashboard.

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Data Structures Used](#data-structures-used)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Running the Application](#running-the-application)
- [API Reference](#api-reference)
- [Tech Stack](#tech-stack)
- [License](#license)

---

## Overview

**DataStruct FSM** is a semester-long course project that demonstrates the practical application of advanced data structures in a real-world file system management scenario. The system can index hundreds of thousands of files, detect duplicates, identify suspicious files, analyze storage patterns, and support instant file searches — all in real time.

This project was built as part of the **Advanced Data Structures** course (2nd Semester).

---

## Features

### 🔍 File Search
- **Exact Search** — O(log n) B+ Tree lookup by file path
- **Prefix Search** — Trie-based instant prefix matching (e.g., `report*`)
- **Regex Search** — Full pattern matching using `std::regex`
- **FTS5 Turbo Search** — SQLite Full-Text Search for millisecond results across millions of files

### 📊 Analytics Dashboard
- Monthly storage growth visualization (Area Chart)
- File type distribution breakdown (Pie Chart)
- Top 10 largest files listing

### 🗂️ Duplicate Detection
- Identifies duplicate files by name and size
- Shows total reclaimable disk space
- Group-based view with file paths

### 🔐 Security Analysis
- Detects high-entropy files (potentially encrypted or suspicious)
- Lists suspicious executables and scripts

### 🧹 Orphan File Manager
- Finds broken symlinks and orphaned temporary files
- Shows total wasted disk space
- One-click delete all orphans

### 📡 Real-Time Scanning
- Multi-threaded parallel directory scanner (8 threads)
- Incremental scanning (only processes changed files)
- Live progress updates via WebSocket

### 🕑 Version History & Snapshots
- Persistent version tracking using a custom Persistent Data Structure
- File rollback support
- Snapshot diffing between two states

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    React Frontend                        │
│  Dashboard │ Search │ Duplicates │ Security │ Analytics  │
└───────────────────────┬─────────────────────────────────┘
                        │  HTTP REST + WebSocket
┌───────────────────────▼─────────────────────────────────┐
│              Python FastAPI Middleware (api.py)           │
│   REST endpoints │ WebSocket hub │ SQLite FileCache       │
└───────────────────────┬─────────────────────────────────┘
                        │  stdin/stdout pipe
┌───────────────────────▼─────────────────────────────────┐
│              C++ FSM Engine (fsm.exe)                    │
│  B+ Tree │ Trie │ Segment Tree │ Union-Find │ PDS         │
└─────────────────────────────────────────────────────────┘
```

- **C++ Engine** — The core engine processes all commands. It maintains in-memory data structures and communicates with the Python layer via stdin/stdout pipes.
- **Python Middleware** — Bridges the C++ engine to the web world. Exposes REST API endpoints for analytics and a WebSocket endpoint for real-time updates.
- **SQLite Database** — A WAL-mode SQLite database (`files_web.db`) serves as the persistent file index. FTS5 virtual tables enable instant full-text search.
- **React Frontend** — A modern dashboard that loads analytics via REST and receives live scan progress via WebSocket.

---

## Data Structures Used

| Data Structure | Location | Purpose |
|---|---|---|
| **B+ Tree** | `src/core/BPlusTree.cpp` | O(log n) file search and range queries by file path |
| **Trie** | `src/core/Trie.cpp` | Prefix search for filenames; efficient auto-complete |
| **Segment Tree** | `src/core/SegmentTree.cpp` | Range size queries and storage analytics |
| **Union-Find (DSU)** | `src/core/UnionFind.cpp` | Grouping duplicate and related files into clusters |
| **Persistent DS (PDS)** | `src/core/PersistentDS.cpp` | Immutable version history for file rollback |
| **FTS5 (SQLite)** | `api.py` | Full-text search index for sub-millisecond queries |

---

## Project Structure

```
advance-ds/
├── src/
│   ├── core/               # Core data structures
│   │   ├── BPlusTree.cpp   # B+ Tree implementation
│   │   ├── Trie.cpp        # Trie implementation
│   │   ├── SegmentTree.cpp # Segment Tree implementation
│   │   ├── UnionFind.cpp   # Union-Find (DSU) implementation
│   │   ├── PersistentDS.cpp# Persistent data structure
│   │   └── FileNode.h      # Core file node definition
│   ├── engine/             # File system engine
│   │   └── FileSystemEngine.cpp
│   ├── commands/           # Command processor (stdin handler)
│   │   └── CommandProcessor.cpp
│   ├── analytics/          # Analytics engine
│   │   └── AnalyticsEngine.cpp
│   ├── scanner/            # Multi-threaded file scanner
│   │   └── FileScanner.cpp
│   └── snapshots/          # Snapshot manager
│       └── SnapshotManager.cpp
├── include/
│   └── fsm.h               # Public header
├── frontend/               # React + TypeScript frontend
│   ├── src/
│   │   ├── pages/          # Dashboard, Search, Duplicates, etc.
│   │   ├── contexts/       # EngineContext (WebSocket hub)
│   │   └── layout/         # App shell and navigation
│   └── package.json
├── api.py                  # Python FastAPI middleware
├── main.cpp                # C++ entry point
├── Makefile                # Build configuration
└── .gitignore
```

---

## Getting Started

### Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| `g++` | 11+ | Compile the C++ engine |
| `Python` | 3.10+ | Run the FastAPI backend |
| `Node.js` | 18+ | Run the React frontend |
| `pip` | latest | Install Python dependencies |

### Installation

**1. Clone the repository:**
```bash
git clone https://github.com/PranavJagtap03/advance-ds.git
cd advance-ds
```

**2. Build the C++ engine:**
```bash
make
# This produces fsm.exe on Windows, or fsm on Linux/macOS
```

**3. Install Python dependencies:**
```bash
pip install fastapi uvicorn
```

**4. Install frontend dependencies:**
```bash
cd frontend
npm install
cd ..
```

---

## Running the Application

You need to run **two servers** simultaneously:

**Terminal 1 — Start the Python API backend:**
```bash
python api.py
# Runs on http://localhost:8000
```

**Terminal 2 — Start the React frontend:**
```bash
cd frontend
npm run dev
# Runs on http://localhost:5173
```

Then open your browser and navigate to **http://localhost:5173**.

> **Note:** The C++ engine (`fsm.exe`) is automatically launched by `api.py` as a child process. Do not run it manually.

---

## API Reference

### REST Endpoints

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/stats` | Total indexed files and storage usage |
| `GET` | `/api/duplicates` | List of duplicate file groups |
| `GET` | `/api/suspicious` | List of high-entropy/suspicious files |
| `GET` | `/api/orphans` | List of orphaned/broken link files |
| `GET` | `/api/analytics` | Monthly growth, largest files, type distribution |
| `GET` | `/api/reclaim` | Reclaimable disk space breakdown |
| `POST` | `/scan/incremental` | Trigger an incremental directory scan |

### WebSocket

Connect to `ws://localhost:8000/ws` for real-time events:

| Event | Description |
|---|---|
| `engine_stream` | Live scan progress, SCAN_FOLDER, SCAN_DONE |
| `engine_progress` | SCAN_PROGRESS counter updates |
| `engine_fatal` | Engine crash or pipe error |

**Commands you can send over WebSocket:**
```json
{ "command": "SCAN_PARALLEL C:/Users/YourName/Documents", "tag": "dashboard", "conn_id": "uuid" }
{ "command": "SEARCH myfile.txt", "tag": "search", "conn_id": "uuid" }
{ "command": "PREFIX report", "tag": "search", "conn_id": "uuid" }
{ "command": "REGEX .*\\.exe$", "tag": "search", "conn_id": "uuid" }
{ "command": "DISK_INFO", "tag": "dashboard", "conn_id": "uuid" }
```

---

## Tech Stack

**Backend / Engine**
- C++17 — Core data structures and file scanning engine
- Python 3 + FastAPI — REST API and WebSocket middleware
- SQLite (WAL + FTS5) — Persistent file index with full-text search
- Uvicorn — ASGI server for FastAPI

**Frontend**
- React 18 + TypeScript — Component-based UI
- Vite — Lightning-fast dev server and bundler
- Recharts — Area, Pie, and Bar chart components
- Lucide React — Icon library

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

*Built with ❤️ as a Data Structures course project.*
