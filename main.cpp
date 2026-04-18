#include "BPlusTree.h"
#include "Trie.h"
#include "SegmentTree.h"
#include "PersistentDS.h"
#include "UnionFind.h"
#include "DataGenerator.h"
#include "Features.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>

using namespace std;

// ═══════════════════════════════════
// GLOBALS
// ═══════════════════════════════════
BPlusTree bpt;
Trie trie;
SegmentTree segTree(365);
PersistentDS pds;
UnionFind uf;
vector<int> allFileIds;
int nextFileId = 10000;

// ═══════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════

void printSeparator() {
    cout << string(60, '=') << endl;
}

void printBanner() {
    cout << "============================================" << endl;
    cout << "    FILE SYSTEM DIRECTORY MANAGER v1.0" << endl;
    cout << "    B+Tree | Trie | SegTree | Persistent" << endl;
    cout << "============================================" << endl;
}

void printFileTable(const vector<FileNode>& files) {
    if (files.empty()) {
        cout << "No files found." << endl;
        return;
    }
    
    cout << left << setw(6) << "ID" << " | " << setw(28) << "Name" << " | " 
         << setw(10) << "Size KB" << " | " << setw(28) << "Path" << endl;
    cout << string(80, '-') << endl;
    
    for (const auto& f : files) {
        cout << left << setw(6) << f.id << " | " << setw(28) << f.name << " | " 
             << setw(10) << f.size / 1024 << " | " << setw(28) << f.path << endl;
    }
    cout << "Total: " << files.size() << " file(s) found." << endl;
}

void printMenu() {
    cout << "\n---- MENU ----\n";
    cout << "1.  Add file\n";
    cout << "2.  Delete file\n";
    cout << "3.  Search by name prefix\n";
    cout << "4.  Search by size range (KB)\n";
    cout << "5.  Storage analytics by month\n";
    cout << "6.  Find duplicate files\n";
    cout << "7.  File version history\n";
    cout << "8.  Edit file (creates new version)\n";
    cout << "9.  Rollback file to old version\n";
    cout << "10. Detect orphan files\n";
    cout << "11. Generate 10000 test files\n";
    cout << "12. Run benchmark\n";
    cout << "13. Run stress tests\n";
    cout << "14. Show system info\n";
    cout << "0.  Exit\n";
    cout << "Choice: " << flush;
}

// ═══════════════════════════════════
// FEATURE FUNCTIONS
// ═══════════════════════════════════

void addFile() {
    string name, path;
    long sizeKB;
    int pid;

    cout << "Enter filename: ";
    cin >> name;
    cin.ignore(1000, '\n');
    
    if (name.empty()) {
        cout << "Filename cannot be empty." << endl;
        return;
    }

    cout << "Enter path (e.g. /documents/file.txt): ";
    getline(cin, path);

    cout << "Enter size in KB: ";
    if (!(cin >> sizeKB) || sizeKB <= 0) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid size." << endl;
        return;
    }

    cout << "Enter parent folder ID (1=docs,2=downloads,3=photos,4=backup): ";
    if (!(cin >> pid) || pid < 1 || pid > 4) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid parent folder ID." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    FileNode f(name, path, pid, sizeKB * 1024, time(0));
    f.id = nextFileId++;
    f.modifiedAt = time(0);

    bpt.insert(name, f);
    trie.insert(name, f.id);
    int dayIndex = (f.createdAt / 86400) % 365;
    segTree.addFile(dayIndex, f.size);
    uf.addNode(f.id);
    uf.unite(f.id, f.parentId);
    pds.saveVersion(f);
    allFileIds.push_back(f.id);

    cout << "File added successfully! ID: " << f.id << endl;
}

void deleteFile() {
    string name;
    cout << "Enter filename to delete: ";
    cin >> name;
    cin.ignore(1000, '\n');

    FileNode* f = bpt.search(name);
    if (!f) {
        cout << "File not found: " << name << endl;
        return;
    }

    bpt.remove(name);
    trie.remove(name);
    cout << "Deleted: " << name << endl;
}

void searchPrefix() {
    string prefix;
    cout << "Enter prefix to search: ";
    cin >> prefix;
    cin.ignore(1000, '\n');

    vector<string> names = trie.prefixSearch(prefix);
    if (names.empty()) {
        cout << "No files found with prefix: " << prefix << endl;
        return;
    }

    vector<FileNode> results;
    for (const string& n : names) {
        FileNode* f = bpt.search(n);
        if (f) results.push_back(*f);
    }
    
    printFileTable(results);
}

void searchBySize() {
    long minKB, maxKB;
    cout << "Enter minimum size in KB: ";
    if (!(cin >> minKB) || minKB <= 0) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid minimum size." << endl;
        return;
    }
    cout << "Enter maximum size in KB: ";
    if (!(cin >> maxKB) || maxKB <= 0) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid maximum size." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    if (minKB > maxKB) {
        cout << "Invalid range: min cannot be greater than max." << endl;
        return;
    }

    vector<FileNode> r = bpt.searchBySize(minKB * 1024, maxKB * 1024);
    printFileTable(r);
}

void showVersionHistory() {
    int fid;
    cout << "Enter file ID: ";
    if (!(cin >> fid)) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid format." << endl;
        return;
    }
    cin.ignore(1000, '\n');
    
    pds.showHistory(fid);
}

void editFile() {
    string name;
    cout << "Enter filename to edit: ";
    cin >> name;
    cin.ignore(1000, '\n');

    FileNode* f = bpt.search(name);
    if (!f) {
        cout << "File not found" << endl;
        return;
    }

    cout << "Current Details - Name: " << f->name << " Size: " << f->size / 1024 
         << " KB Path: " << f->path << " Version: " << f->version << endl;

    cout << "Enter new size in KB (0 to keep same): ";
    long newKB;
    if (!(cin >> newKB)) {
        cin.clear(); cin.ignore(1000, '\n');
        newKB = 0;
    } else {
        cin.ignore(1000, '\n');
    }

    cout << "Enter new path (press Enter to keep same): ";
    string newPath;
    getline(cin, newPath);

    FileNode updatedNode = *f;
    if (newKB > 0) {
        updatedNode.size = newKB * 1024;
        updatedNode.checksum = to_string(updatedNode.size) + updatedNode.name.substr(0, min(3, (int)updatedNode.name.size()));
    }
    if (!newPath.empty()) {
        updatedNode.path = newPath;
    }
    updatedNode.modifiedAt = time(0);
    pds.saveVersion(updatedNode);

    bpt.remove(name);
    bpt.insert(updatedNode.name, updatedNode);

    cout << "File updated! Version " << updatedNode.version << " saved." << endl;
}

void rollbackFile() {
    int fid;
    cout << "Enter file ID: ";
    if (!(cin >> fid)) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid input." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    if (!pds.hasHistory(fid)) {
        cout << "No history for this ID" << endl;
        return;
    }
    pds.showHistory(fid);

    int ver;
    cout << "Enter version number to rollback to: ";
    if (!(cin >> ver)) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid version input." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    pds.rollback(fid, ver, bpt);
}

void detectOrphans() {
    vector<int> orphans = uf.findAllOrphans(0, allFileIds);
    if (orphans.empty()) {
        cout << "No orphan files found. All files connected." << endl;
        return;
    }

    cout << "ORPHAN FILES DETECTED:" << endl;
    long totalSize = 0;
    vector<FileNode> allLeaves = bpt.getAllLeaves();

    for (int id : orphans) {
        for (const auto& file : allLeaves) {
            if (file.id == id) {
                cout << "  ID:" << file.id << " Name:" << file.name << " Size:" << file.size / 1024 << " KB Path:" << file.path << endl;
                totalSize += file.size;
                break;
            }
        }
    }
    cout << "Total: " << orphans.size() << " orphans, " << fixed << setprecision(2) << (totalSize / 1048576.0) << " MB potentially unrecoverable" << endl;
}

void showSystemInfo() {
    cout << "=== SYSTEM INFO ===" << endl;
    cout << "Total files: " << bpt.getSize() << endl;
    cout << "Total indexed names (Trie): " << allFileIds.size() << endl;
    cout << "Storage tracked: " << fixed << setprecision(2) << (segTree.queryRange(0, 364) / 1048576.0) << " MB" << endl;
    
    int filesWithHistory = 0;
    for (int id : allFileIds) {
        if (pds.hasHistory(id)) filesWithHistory++;
    }
    cout << "Files with version history: " << filesWithHistory << endl;
}

// ═══════════════════════════════════
// MAIN FUNCTION
// ═══════════════════════════════════
int main() {
    printBanner();
    cout << "Welcome! Type 11 to generate test data first." << endl;
    
    int choice = -1;
    do {
        printMenu();
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "Invalid input. Enter a number." << endl;
            continue;
        }
        cin.ignore(1000, '\n'); // clear newline after valid integer input
        cout << endl; // space for readability
        
        switch(choice) {
            case 1:  addFile(); break;
            case 2:  deleteFile(); break;
            case 3:  searchPrefix(); break;
            case 4:  searchBySize(); break;
            case 5:  storageAnalytics(); break;
            case 6:  findDuplicates(); break;
            case 7:  showVersionHistory(); break;
            case 8:  editFile(); break;
            case 9:  rollbackFile(); break;
            case 10: detectOrphans(); break;
            case 11: generateData(10000); break;
            case 12: benchmark(); break;
            case 13: stressTest(); break;
            case 14: showSystemInfo(); break;
            case 0:  cout << "Goodbye!" << endl; break;
            default: cout << "Invalid option. Try again." << endl;
        }
    } while(choice != 0);

    return 0;
}
