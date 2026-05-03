CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Isrc/core -Isrc/engine -Isrc/commands -Isrc/analytics -Isrc/snapshots -Isrc/scanner

# ── Source files ────────────────────────────────────────────────────
CORE_SRCS = src/core/BPlusTree.cpp \
            src/core/Trie.cpp \
            src/core/UnionFind.cpp \
            src/core/SegmentTree.cpp \
            src/core/PersistentDS.cpp

MOD_SRCS  = src/engine/FileSystemEngine.cpp \
            src/commands/CommandProcessor.cpp \
            src/analytics/AnalyticsEngine.cpp \
            src/snapshots/SnapshotManager.cpp \
            src/scanner/FileScanner.cpp

SRCS = main.cpp $(CORE_SRCS) $(MOD_SRCS)

# ── Platform-specific binary name ───────────────────────────────────
ifeq ($(OS),Windows_NT)
    TARGET = fsm.exe
    RUN    = fsm.exe
else
    TARGET = fsm
    RUN    = ./fsm
endif

# ── Headers (for rebuild detection) ────────────────────────────────
CORE_HDRS = src/core/BPlusTree.h src/core/Trie.h src/core/SegmentTree.h \
            src/core/PersistentDS.h src/core/UnionFind.h src/core/FileNode.h \
            src/core/Utils.h

MOD_HDRS  = src/engine/FileSystemEngine.h \
            src/commands/CommandProcessor.h \
            src/analytics/AnalyticsEngine.h \
            src/snapshots/SnapshotManager.h \
            src/scanner/FileScanner.h

HDRS = $(CORE_HDRS) $(MOD_HDRS)

# ── Default target ──────────────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
ifeq ($(OS),Windows_NT)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)
else
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) -lpthread
endif

# ── Run in machine mode ────────────────────────────────────────────
run: $(TARGET)
	$(RUN) --machine

# ── Remove build artefacts ─────────────────────────────────────────
clean:
ifeq ($(OS),Windows_NT)
	del /f /q fsm.exe *.o 2>nul || true
else
	rm -f fsm *.o
endif
