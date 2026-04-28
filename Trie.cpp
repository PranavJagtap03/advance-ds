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

// 7. removeHelper(node, word, depth) — returns true if caller should delete this node
bool Trie::removeHelper(TrieNode* node, const string& word, int depth) {
    if (!node) return false;

    if (depth == (int)word.size()) {
        if (node->isEnd) {
            node->isEnd = false;
            node->fileId = -1;
        }
        // Node is deletable if it has no children
        return node->children.empty();
    }

    char c = word[depth];
    if (node->children.find(c) == node->children.end()) {
        return false; // Not found
    }

    bool shouldDelete = removeHelper(node->children[c], word, depth + 1);
    if (shouldDelete) {
        delete node->children[c];
        node->children.erase(c);
        // This node is also deletable if it has no children and is not an end
        return !node->isEnd && node->children.empty();
    }
    return false;
}

// 8. remove(filename) — public wrapper
void Trie::remove(string filename) {
    if (!root) return;
    removeHelper(root, filename, 0);
}

// ─────────────────────────────────────────────────────────────────────
// 8. prefixSearchWithPath(prefix) — new additive method
//    Walks the trie character by character, building a path string
//    like "b->u->d->g->e->t", then collects all matches.
//    Returns {matched_filenames, character_path_string}.
// ─────────────────────────────────────────────────────────────────────
pair<vector<string>, string> Trie::prefixSearchWithPath(string prefix) {
    vector<string> matched;
    string path;

    if (!root || prefix.empty()) {
        // Empty prefix: collect everything, path is just "(root)"
        path = "(root)";
        collectAll(root, "", matched);
        return {matched, path};
    }

    TrieNode* curr = root;

    // Walk trie following prefix chars, building the path
    for (int i = 0; i < (int)prefix.size(); i++) {
        char c = prefix[i];
        if (curr->children.find(c) == curr->children.end()) {
            // Character not found — path shows how far we got
            if (!path.empty()) path += "->";
            path += string(1, c) + "(NOT_FOUND)";
            return {matched, path}; // empty matched
        }
        if (!path.empty()) path += "->";
        path += string(1, c);
        curr = curr->children[c];
    }

    // Collect all filenames reachable from the node we landed on
    collectAll(curr, prefix, matched);

    return {matched, path};
}
