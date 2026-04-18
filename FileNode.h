#pragma once
#include <string>

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
        checksum = to_string(size) + name.substr(0, min(3, (int)name.size()));
    }
};
