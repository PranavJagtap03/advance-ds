#include "FileScanner.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

#include <fstream>
#include <cmath>
#include <sstream>
#include <map>
#include <algorithm>

using namespace std;

// ═══════════════════════════════════════════════════════════════════
// Legacy single-level scan (kept for backward compatibility)
// ═══════════════════════════════════════════════════════════════════

vector<ScannedFile> FileScanner::scanDirectory(const string& dirPath) {
    vector<ScannedFile> results;

#ifdef _WIN32
    string pattern = dirPath + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return results;
    do {
        string fname = ffd.cFileName;
        if (fname == "." || fname == "..") continue;
        if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 0x400)) continue;

        ScannedFile sf;
        sf.name = fname;
        sf.path = dirPath + "\\" + fname;
        sf.isDirectory = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (!sf.isDirectory) {
            ULARGE_INTEGER ull;
            ull.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
            sf.mtime = (long)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            sf.size = (long)(((long long)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow);
        } else {
            sf.size = 0;
            sf.mtime = 0;
        }
        sf.trueType = "UNKNOWN";
        sf.entropy = 0.0;
        sf.checksum = "";
        results.push_back(sf);
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return results;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string fname = entry->d_name;
        if (fname == "." || fname == "..") continue;
        string fullPath = dirPath + "/" + fname;
        struct stat st;
        if (lstat(fullPath.c_str(), &st) != 0) continue;

        ScannedFile sf;
        sf.name = fname;
        sf.path = fullPath;
        sf.isDirectory = S_ISDIR(st.st_mode);
        sf.size = sf.isDirectory ? 0 : (long)st.st_size;
        sf.mtime = sf.isDirectory ? 0 : (long)st.st_mtime;
        sf.trueType = "UNKNOWN";
        sf.entropy = 0.0;
        sf.checksum = "";

        if (sf.isDirectory || S_ISREG(st.st_mode)) {
            results.push_back(sf);
        }
    }
    closedir(dir);
#endif

    return results;
}

// ═══════════════════════════════════════════════════════════════════
// Helper: compute FNV-1a checksum, entropy, and trueType from file
// ═══════════════════════════════════════════════════════════════════

static void analyzeFile(ScannedFile& sf) {
    unsigned long long hash = 14695981039346656037ULL;
    auto hashChar = [&](unsigned char c) {
        hash ^= c;
        hash *= 1099511628211ULL;
    };
    auto hashStr = [&](const string& s) {
        for (size_t i = 0; i < s.size(); i++) hashChar((unsigned char)s[i]);
    };

    hashStr(to_string(sf.size));

    ifstream ifs(sf.path, ios::binary);
    if (ifs && sf.size > 0) {
        char buffer[4096];
        ifs.read(buffer, sizeof(buffer));
        size_t bytesRead = (size_t)ifs.gcount();
        if (bytesRead > 0) {
            for (size_t i = 0; i < bytesRead; i++) {
                hashChar((unsigned char)buffer[i]);
            }

            // Entropy
            map<unsigned char, int> freq;
            for (size_t i = 0; i < bytesRead; i++) freq[(unsigned char)buffer[i]]++;
            double ent = 0.0;
            for (map<unsigned char,int>::iterator it = freq.begin(); it != freq.end(); ++it) {
                double p = (double)it->second / bytesRead;
                ent -= p * log2(p);
            }
            sf.entropy = ent;

            // True type detection from magic bytes
            if (bytesRead >= 4) {
                unsigned char b[4];
                for (int i = 0; i < 4; i++) b[i] = (unsigned char)buffer[i];
                if (b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47) sf.trueType = "PNG";
                else if (b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) sf.trueType = "JPEG";
                else if (b[0] == 0x25 && b[1] == 0x50 && b[2] == 0x44 && b[3] == 0x46) sf.trueType = "PDF";
                else if (b[0] == 0x50 && b[1] == 0x4B && b[2] == 0x03 && b[3] == 0x04) sf.trueType = "ZIP/OFFICE";
                else if (b[0] == 0x47 && b[1] == 0x49 && b[2] == 0x46 && b[3] == 0x38) sf.trueType = "GIF";
                else if (b[0] == 0x7F && b[1] == 0x45 && b[2] == 0x4C && b[3] == 0x46) sf.trueType = "ELF";
                else if (b[0] == 0x4D && b[1] == 0x5A) sf.trueType = "EXE/DLL";
                else sf.trueType = "DATA";
            } else {
                sf.trueType = "DATA";
            }
        }
    } else {
        hashStr(sf.path);
        hashStr(to_string(sf.mtime));
        sf.trueType = "UNKNOWN";
        sf.entropy = 0.0;
    }

    stringstream ss;
    ss << hex << hash;
    sf.checksum = ss.str();
}

// Pipe-safe encoding for SCAN_FOLDER paths
static string safePath(const string& s) {
    string r = s;
    for (size_t i = 0; i < r.length(); i++) if (r[i] == '|') r[i] = '\x01';
    return r;
}

// ═══════════════════════════════════════════════════════════════════
// Recursive helper — emits SCAN_FOLDER on enter/exit of each dir
// ═══════════════════════════════════════════════════════════════════

void FileScanner::scanRecursiveHelper(
    const string& dirPath,
    int depth,
    vector<ScannedFile>& results,
    int& fileCount,
    function<void(int)> progressCallback,
    long lastScanTime,
    vector<ImmediateLine>* immediateLines
) {
    if (depth >= MAX_DEPTH) return;

    // Emit SCAN_FOLDER|<path>|ENTERING
    if (immediateLines) {
        ImmediateLine il;
        il.content = "SCAN_FOLDER|" + safePath(dirPath) + "|ENTERING";
        immediateLines->push_back(il);
    }

    int dirFileCount = 0; // Files found directly in THIS directory

#ifdef _WIN32
    string pattern = dirPath + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        string fname = ffd.cFileName;
        if (fname == "." || fname == "..") continue;
        if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        string fullPath = dirPath + "\\" + fname;

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scanRecursiveHelper(fullPath, depth + 1, results, fileCount, progressCallback, lastScanTime, immediateLines);
        } else {
            ULARGE_INTEGER ull;
            ull.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
            long mtime = (long)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            long fileSize = (long)(((long long)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow);

            if (lastScanTime > 0 && mtime <= lastScanTime) continue;

            ScannedFile sf;
            sf.name = fname;
            sf.path = fullPath;
            sf.size = fileSize;
            sf.mtime = mtime;
            sf.isDirectory = false;
            sf.trueType = "UNKNOWN";
            sf.entropy = 0.0;
            sf.checksum = "";

            analyzeFile(sf);
            results.push_back(sf);
            dirFileCount++;

            fileCount++;
            if (progressCallback && fileCount % PROGRESS_INTERVAL == 0) {
                progressCallback(fileCount);
            }
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string fname = entry->d_name;
        if (fname == "." || fname == "..") continue;
        if (fname[0] == '.') continue;

        string fullPath = dirPath + "/" + fname;
        struct stat st;
        if (lstat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) continue;

        if (S_ISDIR(st.st_mode)) {
            scanRecursiveHelper(fullPath, depth + 1, results, fileCount, progressCallback, lastScanTime, immediateLines);
        } else if (S_ISREG(st.st_mode)) {
            long mtime = (long)st.st_mtime;
            if (lastScanTime > 0 && mtime <= lastScanTime) continue;

            ScannedFile sf;
            sf.name = fname;
            sf.path = fullPath;
            sf.size = (long)st.st_size;
            sf.mtime = mtime;
            sf.isDirectory = false;
            sf.trueType = "UNKNOWN";
            sf.entropy = 0.0;
            sf.checksum = "";

            analyzeFile(sf);
            results.push_back(sf);
            dirFileCount++;

            fileCount++;
            if (progressCallback && fileCount % PROGRESS_INTERVAL == 0) {
                progressCallback(fileCount);
            }
        }
    }
    closedir(dir);
#endif

    // Emit SCAN_FOLDER|<path>|<count> only if non-empty
    if (immediateLines && dirFileCount > 0) {
        ImmediateLine il;
        il.content = "SCAN_FOLDER|" + safePath(dirPath) + "|" + to_string(dirFileCount);
        immediateLines->push_back(il);
    }
}

// ═══════════════════════════════════════════════════════════════════
// Main recursive scan entry point
// ═══════════════════════════════════════════════════════════════════

vector<ScannedFile> FileScanner::scanDirectoryRecursive(
    const string& dirPath,
    int /* threadCount (unused, kept for interface compat) */,
    function<void(int)> progressCallback,
    long lastScanTime,
    vector<ImmediateLine>* immediateLines
) {
    vector<ScannedFile> results;
    int fileCount = 0;

    // Reserve space to reduce reallocations for large scans
    results.reserve(100000);

    scanRecursiveHelper(dirPath, 0, results, fileCount, progressCallback, lastScanTime, immediateLines);

    return results;
}
