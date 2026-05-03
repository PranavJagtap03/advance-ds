#pragma once
#include <string>
#include <vector>
#include <map>

using namespace std;

struct TrieNode {
    // Child nodes
    map<char, TrieNode*> children;
    
    // Flag indicating if this node ends a valid word
    bool isEnd = false;
    
    // Associated file ID
    int fileId = -1;

    // Constructor
    TrieNode() {}
};

class Trie {
private:
    TrieNode* root;

    // Helper to collect all matching strings
    void collectAll(TrieNode* node, string current, vector<string>& result);
    
    // Helper to recursively destroy the trie
    void destroyTrie(TrieNode* node);

    // Helper to recursively remove a word and free empty nodes
    bool removeHelper(TrieNode* node, const string& word, int depth);

public:
    // Initializes root node
    Trie();
    
    // Cleans up memory
    ~Trie();
    
    // Inserts a filename and its associated fileid
    void insert(string filename, int fileId);
    
    // Returns all filenames matching the given prefix
    vector<string> prefixSearch(string prefix);
    
    // Removes a filename from the trie
    void remove(string filename);

    // ── NEW: prefixSearchWithPath(prefix)
    // Returns {matched_filenames, character_path_string}
    // Character path format: "b->u->d->g->e->t"
    pair<vector<string>, string> prefixSearchWithPath(string prefix);
};
