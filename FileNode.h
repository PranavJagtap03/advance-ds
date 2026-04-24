#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cmath>
#include <map>

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

    // Intelligence Data
    double entropy = 0.0;
    string trueType = "UNKNOWN";

    // Default constructor
    FileNode() = default;

    // Parameterized constructor
    FileNode(string name, string path, int parentId, long size, long createdAt, bool deepAnalysis = false) 
        : name(name), path(path), parentId(parentId), size(size), createdAt(createdAt), modifiedAt(createdAt) {
        
        // FNV-1a Hash Implementation
        unsigned long long hash = 14695981039346656037ULL;
        auto hashStr = [&](const string& s) {
            for (char c : s) {
                hash ^= (unsigned char)c;
                hash *= 1099511628211ULL;
            }
        };

        auto hashData = [&](const char* data, size_t len) {
            for (size_t i = 0; i < len; i++) {
                hash ^= (unsigned char)data[i];
                hash *= 1099511628211ULL;
            }
        };

        auto calcEnt = [&](const char* data, size_t len) {
            if (len == 0) return 0.0;
            map<unsigned char, int> freq;
            for (size_t i = 0; i < len; i++) freq[(unsigned char)data[i]]++;
            double ent = 0.0;
            for (auto const& pair : freq) {
                double p = (double)pair.second / len;
                ent -= p * log2(p);
            }
            return ent;
        };

        auto getTType = [&](const char* data, size_t len) -> string {
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
        };
        
        // Hash metadata first
        hashStr(to_string(size));
        
        // Hash first 4KB of content if deep analysis is enabled
        if (deepAnalysis && size > 0) {
            ifstream ifs(path, ios::binary);
            if (ifs) {
                char buffer[4096];
                ifs.read(buffer, sizeof(buffer));
                size_t bytesRead = (size_t)ifs.gcount();
                if (bytesRead > 0) {
                    hashData(buffer, bytesRead);
                    entropy = calcEnt(buffer, bytesRead);
                    trueType = getTType(buffer, bytesRead);
                }
            }
        } else if (!deepAnalysis) {
            // Faster path: just use metadata
            hashStr(name);
            hashStr(to_string(createdAt));
        } else {
            // Folder or empty file
            hashStr(name);
        }
        
        stringstream ss;
        ss << hex << hash;
        checksum = ss.str();
    }
};
