#include "Trie.h"

// 1. Constructor
Trie::Trie() {
    root = new TrieNode();
}

// 2. destroyTrie(node)
void Trie::destroyTrie(TrieNode* node) {
    // If node is null return immediately
    if (!node) return;
    
    // Recursively delete all children
    for (auto const& pair : node->children) {
        destroyTrie(pair.second);
    }
    // Then delete node
    delete node;
}

// 3. Destructor
Trie::~Trie() {
    destroyTrie(root);
}

// 4. insert(filename, fileId)
void Trie::insert(string filename, int fileId) {
    if (!root) return;
    TrieNode* curr = root;
    
    // Walk/create nodes for each char in filename
    for (char c : filename) {
        if (curr->children.find(c) == curr->children.end()) {
            curr->children[c] = new TrieNode();
        }
        curr = curr->children[c];
    }
    
    // Mark last node
    curr->isEnd = true;
    curr->fileId = fileId;
}

// 5. collectAll(node, current, result)
void Trie::collectAll(TrieNode* node, string current, vector<string>& result) {
    if (!node) return;
    
    // If node->isEnd: push current to result
    if (node->isEnd) {
        result.push_back(current);
    }
    
    // For each child in node->children: recursively call collectAll
    for (auto const& pair : node->children) {
        collectAll(pair.second, current + pair.first, result);
    }
}

// 6. prefixSearch(prefix)
vector<string> Trie::prefixSearch(string prefix) {
    vector<string> result;
    if (!root) return result;
    
    TrieNode* curr = root;
    // Walk trie following prefix chars
    for (char c : prefix) {
        // If any char missing: return empty vector (no crash)
        if (curr->children.find(c) == curr->children.end()) {
            return result; 
        }
        curr = curr->children[c];
    }
    
    // Call collectAll from reached node with prefix as current string
    collectAll(curr, prefix, result);
    return result; 
}

// 7. remove(filename)
void Trie::remove(string filename) {
    if (!root) return;
    TrieNode* curr = root;
    
    // Walk to last node of filename
    for (char c : filename) {
        if (curr->children.find(c) == curr->children.end()) {
            return; // Not found: return silently (no crash, no error message)
        }
        curr = curr->children[c];
    }
    
    // If found: set isEnd=false, fileId=-1
    if (curr->isEnd) {
        curr->isEnd = false;
        curr->fileId = -1;
    }
}
