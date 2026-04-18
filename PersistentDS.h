#pragma once
#include "FileNode.h"
#include "BPlusTree.h"
#include <map>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace std;

class PersistentDS {
private:
    map<int, vector<FileNode>> history;

public:
    // Saves a new version of the file node into history
    void saveVersion(FileNode& f);
    
    // Retrieves a specific version of a file node
    FileNode getVersion(int fileId, int version);
    
    // Displays the version history of a file
    void showHistory(int fileId);
    
    // Restores an older version into the B+ Tree
    bool rollback(int fileId, int version, BPlusTree& bpt);
    
    // Checks if a file has version history
    bool hasHistory(int fileId);
};
