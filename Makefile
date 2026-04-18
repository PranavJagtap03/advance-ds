all:
	g++ -std=c++17 -O2 main.cpp BPlusTree.cpp Trie.cpp UnionFind.cpp SegmentTree.cpp PersistentDS.cpp -o fsm

run: all
	./fsm

clean:
	rm -f fsm *.o
