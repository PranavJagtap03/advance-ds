#pragma once
#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

// Represents a node in the file system
struct FileNode {
    // Unique identifier for the file/folder
    int id = -1;
    
    // Name of the file/folder
    string name = "";
    
    // Absolute or relative path
    string path = "";
    
    // ID of the parent folder
    int parentId = -1;
    
    // Size of the file in bytes
    long size = 0;
    
    // Creation timestamp (Unix epoch time)
    long createdAt = 0;
    
    // Last modified timestamp (Unix epoch time)
    long modifiedAt = 0;
    
    // Checksum for data integrity
    string checksum = "";
    
    // Flag to determine if node is a folder or file
    bool isFolder = false;
    
    // Version number of the file
    int version = 1;

    // Default constructor
    FileNode() = default;

    // Parameterized constructor
    FileNode(string name, string path, int parentId, long size, long createdAt) 
        : name(name), path(path), parentId(parentId), size(size), createdAt(createdAt), modifiedAt(createdAt) {
        
        // FNV-1a Hash Implementation
        unsigned long long hash = 14695981039346656037ULL;
        auto hashStr = [&](const string& s) {
            for (char c : s) {
                hash ^= (unsigned char)c;
                hash *= 1099511628211ULL;
            }
        };
        
        hashStr(name);
        hashStr(to_string(size));
        hashStr(to_string(createdAt));
        
        stringstream ss;
        ss << hex << hash;
        checksum = ss.str();
    }
};
