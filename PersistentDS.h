#pragma once
#include "FileNode.h"
#include "BPlusTree.h"
#include <map>
#include <vector>
#include <iostream>
#include <iomanip>
#include <string>

using namespace std;

class PersistentDS {
private:
    map<int, vector<FileNode>> history;
    string backupDir = ".fsm_backup";

    bool copyFile(const string& source, const string& destination);
    void ensureBackupDir();

public:
    PersistentDS();
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

    // Physical backup logic
    bool backupFile(const FileNode& f);
    bool restoreFile(const FileNode& f, int version);

    // C-6: Check if physical backup file exists on disk
    bool physicalBackupAvailable(int fileId, int version);
};
