# File System Directory Manager — Project Summary

This document summarizes the development and features of the File System Directory Manager, spanning the high-performance C++ backend, the Python FastAPI middleware, and the modern React/TypeScript frontend.

## 1. Backend Architecture (C++)
The core engine is built in C++ for maximum performance, handling complex data structures to index and query file metadata.

### Data Structures
- **B+ Tree**: Used for exact-match file name lookups (O(log n)).
- **Trie**: Powers prefix-based wildcard searches (e.g., `budget*`) (O(m)).
- **Segment Tree**: Manages range-based queries for file size and creation date analytics (O(log n)).
- **Union-Find**: Detects "orphan" files (files disconnected from a valid directory structure).
- **Persistent DS**: Tracks version history and snapshot rollbacks of file metadata changes.

### Key Backend Features
- **Machine Interface (`--machine`)**: A pipe-delimited communication protocol allowing the middleware to send commands (`SEARCH`, `PREFIX`, `WARNINGS`, `SNAPSHOT`) and receive structured responses.
- **Fast Replay (`LOAD_BATCH`)**: Commands that allow the engine to be populated instantly from a SQLite database, bypassing slow filesystem traversal.
- **Real-time Analytics**: Calculates potential storage waste dynamically.

## 2. Web Frontend (React + Vite)
A professional, modern web application built with React, TypeScript, and Tailwind CSS (via Vite) that serves as the command center for the indexing engine.

### Core Architecture
- **FastAPI Bridge (`api.py`)**: A Python WebSocket server that acts as a secure middleware bridge between the browser's UI and the C++ engine's `stdin`/`stdout`.
- **Engine Context**: A robust React Context (`EngineContext.tsx`) that manages WebSocket connections, state synchronization, and parses engine output streams into React state variables.

### Key Pages
- **Dashboard**: High-level storage overview, indexing controls, and dynamically calculated potential storage waste.
- **Search**: Blazing fast exact-match and prefix lookups.
- **Duplicates & Orphans**: Detailed views for duplicate file clusters and disconnected files detected by Union-Find.
- **Analytics & History**: File distribution visualizations and snapshot rollbacks managed by the Persistent DS.

## 3. Visual & UX Design
- **Dark Mode First**: Beautiful, modern UI using Tailwind CSS variables (`bg-surface`, `text-on-surface`).
- **Responsive Layout**: Sidebar navigation with dynamic content routing.
- **Real-time Banners**: Instant warnings and visual feedback parsed straight from the C++ core.

## 4. How to Run
1. **Build Backend**: `make` (Outputs `fsm.exe`)
2. **Start API Server**: `python api.py`
3. **Start Web UI**: `cd frontend && npm run dev`

---
*Maintained by Antigravity AI*
