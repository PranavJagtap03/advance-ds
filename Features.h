#pragma once
#include "FileNode.h"
#include "BPlusTree.h"
#include "Trie.h"
#include "UnionFind.h"
#include "SegmentTree.h"
#include "PersistentDS.h"
#include "DataGenerator.h"
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>
#include <cmath>

using namespace std;

extern BPlusTree bpt;
extern Trie trie;
extern SegmentTree segTree;
extern PersistentDS pds;
extern UnionFind uf;
extern vector<int> allFileIds;

// ─────────────────────────
// INTELLIGENCE HELPERS
// ─────────────────────────

inline double calculateEntropy(const char* data, size_t len) {
    if (len == 0) return 0.0;
    map<unsigned char, int> freq;
    for (size_t i = 0; i < len; i++) freq[(unsigned char)data[i]]++;
    
    double entropy = 0.0;
    for (auto const& pair : freq) {
        double p = (double)pair.second / len;
        entropy -= p * log2(p);
    }
    return entropy;
}

inline string getTrueFileType(const char* data, size_t len) {
    if (len < 4) return "UNKNOWN";
    unsigned char b[4];
    for(int i=0; i<4; i++) b[i] = (unsigned char)data[i];

    if (b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47) return "PNG";
    if (b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) return "JPEG";
    if (b[0] == 0x25 && b[1] == 0x50 && b[2] == 0x44 && b[3] == 0x46) return "PDF";
    if (b[0] == 0x50 && b[1] == 0x4B && b[2] == 0x03 && b[3] == 0x04) return "ZIP/OFFICE";
    if (b[0] == 0x47 && b[1] == 0x49 && b[2] == 0x46 && b[3] == 0x38) return "GIF";
    if (b[0] == 0x7F && b[1] == 0x45 && b[2] == 0x4C && b[3] == 0x46) return "ELF";
    if (b[0] == 0x4D && b[1] == 0x5A) return "EXE/DLL";
    
    return "DATA";
}

// ─────────────────────────
// FUNCTION 1: findDuplicates()
// ─────────────────────────
inline void findDuplicates() {
    vector<FileNode> all = bpt.getAllLeaves();
    if (all.empty()) {
        cout << "No files in system." << endl;
        return;
    }

    map<string, vector<FileNode>> groups;
    for (const auto& file : all) {
        groups[file.checksum].push_back(file);
    }

    int duplicateGroups = 0;
    double totalWastedMB = 0;

    for (const auto& pair : groups) {
        if (pair.second.size() > 1) {
            duplicateGroups++;
            const auto& files = pair.second;
            cout << "DUPLICATE FOUND: " << files[0].name << endl;
            
            long singleSize = files[0].size;
            long wastedKB = (files.size() - 1) * singleSize / 1024;
            totalWastedMB += wastedKB / 1024.0;

            for (const auto& file : files) {
                cout << "  Location: " << file.path << "  Size: " << file.size / 1024 << " KB" << endl;
            }
            cout << "  Wasted: " << wastedKB << " KB" << endl;
            cout << string(50, '-') << endl;
        }
    }

    if (duplicateGroups > 0) {
        cout << duplicateGroups << " duplicate groups found. Total wasted: " << fixed << setprecision(2) << totalWastedMB << " MB" << endl;
    } else {
        cout << "No duplicate files found." << endl;
    }
}

// ─────────────────────────
// FUNCTION 2: storageAnalytics()
// ─────────────────────────
inline void storageAnalytics() {
    string monthNames[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    int startDays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int endDays[12]   = {30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364};

    cout << "=== STORAGE ANALYTICS (by month) ===" << endl;
    cout << left << setw(12) << "Month" << " | " << setw(15) << "Storage Added" << " | Bar chart" << endl;
    cout << string(40, '-') << endl;

    double totalMB = 0;
    for (int i = 0; i < 12; i++) {
        long bytes = segTree.queryRange(startDays[i], endDays[i]);
        double mb = bytes / 1048576.0;
        totalMB += mb;
        int barLen = min((int)(mb / 10), 30);
        
        cout << left << setw(12) << monthNames[i] << " | " 
             << setw(12) << fixed << setprecision(2) << mb << " MB | ";
        cout << "[";
        for (int b = 0; b < barLen; b++) cout << "=";
        cout << "]" << endl;
    }
    cout << "Total Storage: " << fixed << setprecision(2) << totalMB << " MB" << endl;
}

// ─────────────────────────
// FUNCTION 3: benchmark()
// ─────────────────────────
inline void benchmark() {
    using namespace std::chrono;
    
    vector<FileNode> all = bpt.getAllLeaves();
    if (all.empty()) {
        cout << "Tree empty! Generating 10000 files for benchmark..." << endl;
        generateData(10000);
        all = bpt.getAllLeaves();
    } else {
        cout << "Running benchmark on " << all.size() << " files..." << endl;
    }

    if (all.empty()) return;

    // TEST 1 — Search
    auto start = high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        bpt.search(all[rand() % all.size()].name);
    }
    auto end = high_resolution_clock::now();
    long bptSearchTime = duration_cast<microseconds>(end - start).count();

    start = high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        string searchName = all[rand() % all.size()].name;
        for (const auto& f : all) {
            if (f.name == searchName) break;
        }
    }
    end = high_resolution_clock::now();
    long linSearchTime = duration_cast<microseconds>(end - start).count();

    // TEST 2 — Range search
    start = high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        bpt.rangeSearch("a", "m");
    }
    end = high_resolution_clock::now();
    long bptRangeTime = duration_cast<microseconds>(end - start).count();

    start = high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        vector<FileNode> results;
        for (const auto& f : all) {
            if (f.name >= "a" && f.name <= "m") {
                results.push_back(f);
            }
        }
    }
    end = high_resolution_clock::now();
    long linRangeTime = duration_cast<microseconds>(end - start).count();

    // TEST 3 — Prefix search
    start = high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        trie.prefixSearch("rep");
    }
    end = high_resolution_clock::now();
    long triePrefixTime = duration_cast<microseconds>(end - start).count();

    start = high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        vector<FileNode> results;
        for (const auto& f : all) {
            if (f.name.substr(0, 3) == "rep") {
                results.push_back(f);
            }
        }
    }
    end = high_resolution_clock::now();
    long linPrefixTime = duration_cast<microseconds>(end - start).count();

    // Print results table
    cout << "\nOperation          | DS(µs)  | Linear(µs) | Speedup" << endl;
    cout << "-------------------|---------|------------|--------" << endl;
    
    auto printRow = [](string op, long dsTime, long linTime) {
        double speedup = dsTime > 0 ? (double)linTime / dsTime : 0.0;
        cout << left << setw(18) << op << " | " 
             << setw(7) << dsTime << " | " 
             << setw(10) << linTime << " | " 
             << fixed << setprecision(2) << speedup << "x" << endl;
    };

    printRow("Search (1000 ops)", bptSearchTime, linSearchTime);
    printRow("Range (100 ops)", bptRangeTime, linRangeTime);
    printRow("Prefix (1000 ops)", triePrefixTime, linPrefixTime);
    
    cout << "Benchmark complete!" << endl;
}

// ─────────────────────────
// FUNCTION 4: stressTest()
// ─────────────────────────
inline void stressTest() {
    int passedCount = 0;

    // Test 1 — BPlusTree
    int bptErrors = 0;
    vector<string> names(50000);
    for (int i = 0; i < 50000; i++) {
        FileNode f;
        f.name = "stress_" + to_string(i);
        bpt.insert(f.name, f);
        names[i] = f.name;
    }
    for (int i = 0; i < 1000; i++) {
        if (bpt.search(names[rand() % 50000]) == nullptr) bptErrors++;
    }
    for (int i = 0; i < 5000; i++) {
        string delName = names[rand() % 50000];
        bpt.remove(delName);
        if (bpt.search(delName) != nullptr) bptErrors++; 
    }
    
    if (bptErrors == 0) {
        cout << "B+ Tree: PASSED" << endl;
        passedCount++;
    } else {
        cout << "FAILED: " << bptErrors << " errors in B+ Tree" << endl;
    }

    // Test 2 — Trie
    Trie testTrie;
    for (int i = 0; i < 10000; i++) {
        string name = (i % 2 == 0 ? "rep_" : "other_") + to_string(i);
        testTrie.insert(name, i);
    }
    vector<string> res = testTrie.prefixSearch("rep");
    bool trieFailed = false;
    if (res.size() != 5000) trieFailed = true;
    for (const string& s : res) {
        if (s.substr(0, 3) != "rep") trieFailed = true;
    }
    
    if (!trieFailed) {
        cout << "Trie: PASSED" << endl;
        passedCount++;
    } else {
        cout << "FAILED: Trie" << endl;
    }

    // Test 3 — UnionFind
    UnionFind testUF;
    for (int i = 0; i < 1000; i++) {
        testUF.addNode(i);
        testUF.unite(i, 0);
    }
    vector<int> allNodes;
    for (int i = 0; i < 1000; i++) allNodes.push_back(i);
    int baselineOrphans = testUF.findAllOrphans(0, allNodes).size();
    
    for (int i = 1000; i < 1005; i++) {
        testUF.addNode(i);
        allNodes.push_back(i);
    }
    
    int currentOrphans = testUF.findAllOrphans(0, allNodes).size() - baselineOrphans;
    if (currentOrphans == 5) {
        cout << "UnionFind: PASSED" << endl;
        passedCount++;
    } else {
        cout << "FAILED: UnionFind" << endl;
    }

    // Test 4 — PersistentDS
    PersistentDS testPds;
    FileNode verNode;
    verNode.id = 999;
    for (int i = 1; i <= 5; i++) {
        verNode.size = i * 1000;
        testPds.saveVersion(verNode);
    }
    BPlusTree dummyBPT; // Create a temporary BPT to pass for rollback
    testPds.rollback(999, 3, dummyBPT); 
    FileNode rolledNode = testPds.getVersion(999, 3);
    
    if (rolledNode.size == 3000) {
        cout << "PersistentDS: PASSED" << endl;
        passedCount++;
    } else {
        cout << "FAILED: PersistentDS" << endl;
    }

    if (passedCount == 4) {
        cout << "ALL STRESS TESTS PASSED!" << endl;
    }
}
