#pragma once
#include "FileNode.h"
#include "BPlusTree.h"
#include "Trie.h"
#include "UnionFind.h"
#include "SegmentTree.h"
#include "PersistentDS.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>

using namespace std;

extern BPlusTree bpt;
extern Trie trie;
extern SegmentTree segTree;
extern PersistentDS pds;
extern UnionFind uf;
extern vector<int> allFileIds;

void generateData(int n) {
    // Setup folder nodes in UnionFind
    uf.addNode(0);                // root
    uf.addNode(1); uf.unite(1, 0); // /documents
    uf.addNode(2); uf.unite(2, 0); // /downloads
    uf.addNode(3); uf.unite(3, 0); // /photos
    uf.addNode(4); uf.unite(4, 0); // /backup

    srand(42); // For reproducible results

    string prefixes[] = {"report", "budget", "presentation", "readme", "notes", "data", "backup", "photo"};
    string extensions[] = {".pdf", ".xlsx", ".txt", ".docx", ".jpg"};
    string folderNames[] = {"", "/documents", "/downloads", "/photos", "/backup"}; // 0 unused, 1-4 match parentId

    cout << "Generating " << n << " files..." << endl;

    for (int i = 0; i < n; i++) {
        int id = i + 10;
        string name = prefixes[rand() % 8] + "_" + to_string(rand() % 999) + extensions[rand() % 5];
        int parentId = (rand() % 4) + 1;
        long size = (rand() % 10000 + 1) * 1024;
        long createdAt = 1700000000 + rand() % 10000000;
        string path = folderNames[parentId] + "/" + name;

        FileNode f(name, path, parentId, size, createdAt);
        f.id = id;
        f.modifiedAt = createdAt;

        int dayIndex = (createdAt / 86400) % 365;

        // Insert into ALL structures
        bpt.insert(name, f);
        trie.insert(name, f.id);
        segTree.addFile(dayIndex, f.size);
        uf.addNode(f.id);
        uf.unite(f.id, f.parentId);  // CRITICAL for orphan detection
        pds.saveVersion(f);
        allFileIds.push_back(f.id);
    }

    cout << "Done! " << n << " files added to all data structures." << endl;
}
