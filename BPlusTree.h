#pragma once
#include "FileNode.h"
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

const int ORDER = 4;

struct BPlusNode {
    // True if node is a leaf
    bool isLeaf;
    
    // Keys used for routing
    vector<string> keys;
    
    // File nodes stored in leaves
    vector<FileNode> values; // leaf nodes only
    
    // Child pointers for internal nodes
    vector<BPlusNode*> children; // internal nodes only
    
    // Pointer to next leaf node for linked list property
    BPlusNode* next; // leaf linked list

    // Constructor
    BPlusNode(bool isLeaf) : isLeaf(isLeaf), next(nullptr) {}
};

class BPlusTree {
private:
    BPlusNode* root;

    // E-1: Secondary index sorted by (size, key) for O(log n) size range queries
    vector<pair<long, string>> sizeIndex;

    // Helper to split a child node
    void splitChild(BPlusNode* parent, int i, BPlusNode* child);
    
    // Helper to insert a key-value pair into a non-full node
    void insertNonFull(BPlusNode* node, string key, FileNode val);
    
    // Helper to recursively search for a node
    FileNode* searchNode(BPlusNode* node, string key);
    
    // Helper to collect all leaves
    void collectLeaves(BPlusNode* node, vector<FileNode>& result);
    
    // Helper to destroy the tree
    void destroyTree(BPlusNode* node);

    // C-3: Recursive remove helper with underflow propagation
    void removeHelper(BPlusNode* node, BPlusNode* parent, int childIndex, string key, bool& underflow);

    // Helper: search node and build traversal path (recursive)
    // nodeIdx is an incrementing counter for naming internal nodes
    FileNode* searchNodeWithPath(BPlusNode* node, string key,
                                  string& path, int& hops,
                                  int& nodeCounter);

public:
    // Constructor initializes root
    BPlusTree();
    
    // Destructor to free memory
    ~BPlusTree();
    
    // Inserts a file node with the given key
    void insert(string key, FileNode val);
    
    // Searches for a file node by key
    FileNode* search(string key);
    
    // Removes a key from the tree (C-3: with underflow propagation + root collapse)
    void remove(string key);
    
    // Performs a range search between k1 and k2
    vector<FileNode> rangeSearch(string k1, string k2);
    
    // E-1: O(log n) size range search using secondary sizeIndex
    vector<FileNode> searchBySize(long minSize, long maxSize);
    
    // Gets all leaf nodes in the tree
    vector<FileNode> getAllLeaves();
    
    // Gets the total number of files in the tree
    int getSize();
    
    // Displays the tree structure visually
    void display();

    // ── Returns the found FileNode* plus a human-readable traversal path.
    // Path format: "root->nodeN->...->leafM  [H hops]"
    pair<FileNode*, string> searchWithPath(string key);

    // ── Range search that also returns traversal path per result.
    // Each pair: {FileNode, path_string}
    vector<pair<FileNode*, string>> rangeSearchWithPath(string k1, string k2);
};
