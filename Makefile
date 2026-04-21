CXX      = g++
CXXFLAGS = -std=c++17 -O2

SRCS = main.cpp BPlusTree.cpp Trie.cpp UnionFind.cpp SegmentTree.cpp PersistentDS.cpp

# Platform-specific binary name
ifeq ($(OS),Windows_NT)
    TARGET = fsm.exe
    RUN    = fsm.exe
else
    TARGET = fsm
    RUN    = ./fsm
endif

# Headers listed as dependencies so a header change triggers rebuild
HDRS = BPlusTree.h Trie.h SegmentTree.h PersistentDS.h UnionFind.h \
       FileNode.h Features.h DataGenerator.h QueryAnalyzer.h

# ── Default target ──────────────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

# ── Run in machine mode ──────────────────────────────────────────────
run: $(TARGET)
	$(RUN) --machine

# ── Run in normal menu mode ──────────────────────────────────────────
demo: $(TARGET)
	$(RUN)

# ── Remove build artefacts ───────────────────────────────────────────
clean:
ifeq ($(OS),Windows_NT)
	del /f /q fsm.exe *.o 2>nul || true
else
	rm -f fsm *.o
endif
