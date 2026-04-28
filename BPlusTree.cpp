#include "BPlusTree.h"
#include <iostream>
#include <queue>
#include <sstream>
#include <algorithm>

// 1. Constructor — root is instantiated as a newly created leaf node.
BPlusTree::BPlusTree() {
    root = new BPlusNode(true);
}

// 2. destroyTree(node) — post-order recursive delete
void BPlusTree::destroyTree(BPlusNode* node) {
    if (!node) return;
    if (!node->isLeaf) {
        // Recursively destroy all children before deleting the node itself
        for (BPlusNode* child : node->children) {
            destroyTree(child);
        }
    }
    delete node; // free memory
}

// 3. Destructor
BPlusTree::~BPlusTree() {
    destroyTree(root);
}

// 4. searchNode(node, key)
FileNode* BPlusTree::searchNode(BPlusNode* node, string key) {
    if (!node) return nullptr;
    
    int i = 0;
    while (i < (int)node->keys.size() && key > node->keys[i]) {
        i++;
    }
    
    if (node->isLeaf) {
        // If leaf: linear scan keys, return &values[i] if found, else nullptr
        if (i < (int)node->keys.size() && node->keys[i] == key) {
            return &(node->values[i]);
        }
        return nullptr;
    } else {
        // If internal: just traverse right, no equality check
        return searchNode(node->children[i], key);
    }
}

// 5. search(key)
FileNode* BPlusTree::search(string key) {
    if (!root) return nullptr;
    return searchNode(root, key);
}

// 6. insertNonFull(node, key, val)
void BPlusTree::insertNonFull(BPlusNode* node, string key, FileNode val) {
    int i = node->keys.size() - 1;
    
    if (node->isLeaf) {
        // Make room for insertion in leaf node
        node->keys.push_back(""); 
        node->values.push_back(FileNode());
        
        // Find correct position, insert key+val while maintaining sorted order
        while (i >= 0 && node->keys[i] > key) {
            node->keys[i + 1] = node->keys[i];
            node->values[i + 1] = node->values[i];
            i--;
        }
        
        node->keys[i + 1] = key;
        node->values[i + 1] = val;
    } else {
        // Internal node logic
        while (i >= 0 && node->keys[i] > key) {
            i--;
        }
        i++; // the proper child index
        
        // If child full, split it first, then recurse
        if ((int)node->children[i]->keys.size() == ORDER - 1) {
            splitChild(node, i, node->children[i]);
            if (key > node->keys[i]) {
                i++;
            }
        }
        insertNonFull(node->children[i], key, val);
    }
}

// 7. splitChild(parent, i, child)
void BPlusTree::splitChild(BPlusNode* parent, int i, BPlusNode* child) {
    BPlusNode* newNode = new BPlusNode(child->isLeaf); // same type as child
    
    if (child->isLeaf) {
        int t = child->keys.size() / 2;
        // Move right half of child's keys and values to new node
        newNode->keys.assign(child->keys.begin() + t, child->keys.end());
        newNode->values.assign(child->values.begin() + t, child->values.end());
        
        child->keys.erase(child->keys.begin() + t, child->keys.end());
        child->values.erase(child->values.begin() + t, child->values.end());
        
        // Set new node's next pointer since it's a leaf
        newNode->next = child->next;
        child->next = newNode;
        
        // Insert new node into parent's children
        parent->children.insert(parent->children.begin() + i + 1, newNode);
        parent->keys.insert(parent->keys.begin() + i, newNode->keys[0]); // keep middle key in both
    } else {
        int t = child->keys.size() / 2;
        // Internal node: Move right half of child's keys and children to new node
        newNode->keys.assign(child->keys.begin() + t + 1, child->keys.end());
        newNode->children.assign(child->children.begin() + t + 1, child->children.end());
        
        string promotedKey = child->keys[t]; 
        
        child->keys.erase(child->keys.begin() + t, child->keys.end());
        child->children.erase(child->children.begin() + t + 1, child->children.end());
        
        // Insert new node into parent's children, promote middle key
        parent->children.insert(parent->children.begin() + i + 1, newNode);
        parent->keys.insert(parent->keys.begin() + i, promotedKey);
    }
}

// 8. insert(key, val) — also maintains sizeIndex (E-1)
void BPlusTree::insert(string key, FileNode val) {
    if (!root) {
        root = new BPlusNode(true);
    }
    
    // If root is full: create new root, old root becomes child, splitChild, insertNonFull
    if ((int)root->keys.size() == ORDER - 1) {
        BPlusNode* oldRoot = root;
        root = new BPlusNode(false);
        root->children.push_back(oldRoot);
        
        splitChild(root, 0, oldRoot);
        insertNonFull(root, key, val);
    } else {
        insertNonFull(root, key, val);
    }

    // E-1: Maintain sorted sizeIndex
    pair<long, string> entry = {val.size, key};
    auto pos = lower_bound(sizeIndex.begin(), sizeIndex.end(), entry);
    sizeIndex.insert(pos, entry);
}

// 9. collectLeaves(node, result)
void BPlusTree::collectLeaves(BPlusNode* node, vector<FileNode>& result) {
    if (!node) return;
    
    BPlusNode* curr = node;
    // Traverse down to the leftmost leaf
    while (!curr->isLeaf) {
        if (curr->children.empty()) return;
        curr = curr->children[0];
    }
    
    // Walk through linked list using "next" pointer, collecting all values
    while (curr) {
        for (const auto& val : curr->values) {
            result.push_back(val);
        }
        curr = curr->next;
    }
}

// 10. getAllLeaves()
vector<FileNode> BPlusTree::getAllLeaves() {
    vector<FileNode> result;
    collectLeaves(root, result); // Create result, collect, and return
    return result;
}

// 11. rangeSearch(k1, k2)
vector<FileNode> BPlusTree::rangeSearch(string k1, string k2) {
    vector<FileNode> result;
    if (!root) return result;
    
    BPlusNode* curr = root;
    // Find the leaf that would contain k1
    while (!curr->isLeaf) {
        int i = 0;
        while (i < (int)curr->keys.size() && k1 > curr->keys[i]) i++;
        curr = curr->children[i];
    }
    
    // Walk the leaf linked list, collecting FileNodes between k1 and k2
    while (curr) {
        for (int i = 0; i < (int)curr->keys.size(); i++) {
            if (curr->keys[i] >= k1 && curr->keys[i] <= k2) {
                result.push_back(curr->values[i]);
            } else if (curr->keys[i] > k2) {
                // Keys are ordered, so if key > k2, we stop immediately
                return result; 
            }
        }
        curr = curr->next;
    }
    return result;
}

// 12. searchBySize(minSize, maxSize) — E-1: O(log n) using sizeIndex
vector<FileNode> BPlusTree::searchBySize(long minSize, long maxSize) {
    vector<FileNode> result;
    
    auto lo = lower_bound(sizeIndex.begin(), sizeIndex.end(), make_pair(minSize, string("")));
    for (auto it = lo; it != sizeIndex.end() && it->first <= maxSize; ++it) {
        FileNode* fn = search(it->second);
        if (fn) result.push_back(*fn);
    }
    return result;
}

// 13. getSize()
int BPlusTree::getSize() {
    return getAllLeaves().size();
}

// 14. removeHelper — C-3: recursive underflow propagation
void BPlusTree::removeHelper(BPlusNode* node, BPlusNode* parent, int childIndex, string key, bool& underflow) {
    if (!node) return;

    if (node->isLeaf) {
        // Find and remove the key from the leaf
        bool found = false;
        for (int i = 0; i < (int)node->keys.size(); i++) {
            if (node->keys[i] == key) {
                node->keys.erase(node->keys.begin() + i);
                node->values.erase(node->values.begin() + i);
                found = true;
                break;
            }
        }
        if (!found) return;

        int minKeys = (ORDER - 1) / 2;
        if (node != root && (int)node->keys.size() < minKeys) {
            underflow = true;
        }
        return;
    }

    // Internal node: find the child to recurse into
    int i = 0;
    while (i < (int)node->keys.size() && key > node->keys[i]) i++;

    bool childUnderflow = false;
    removeHelper(node->children[i], node, i, key, childUnderflow);

    if (!childUnderflow) return;

    // Handle underflow in child at index i
    BPlusNode* child = node->children[i];
    int minKeys = (ORDER - 1) / 2;
    BPlusNode* leftSib = (i > 0) ? node->children[i - 1] : nullptr;
    BPlusNode* rightSib = (i < (int)node->children.size() - 1) ? node->children[i + 1] : nullptr;

    if (leftSib && (int)leftSib->keys.size() > minKeys) {
        // Borrow from left sibling
        child->keys.insert(child->keys.begin(), leftSib->keys.back());
        child->values.insert(child->values.begin(), leftSib->values.back());
        leftSib->keys.pop_back();
        leftSib->values.pop_back();
        node->keys[i - 1] = child->keys[0];
    } else if (rightSib && (int)rightSib->keys.size() > minKeys) {
        // Borrow from right sibling — M-2: update parent key AFTER erase
        child->keys.push_back(rightSib->keys.front());
        child->values.push_back(rightSib->values.front());
        rightSib->keys.erase(rightSib->keys.begin());
        rightSib->values.erase(rightSib->values.begin());
        node->keys[i] = rightSib->keys[0]; // M-2: correctly uses new first key
    } else if (leftSib) {
        // Merge with left sibling
        leftSib->keys.insert(leftSib->keys.end(), child->keys.begin(), child->keys.end());
        leftSib->values.insert(leftSib->values.end(), child->values.begin(), child->values.end());
        leftSib->next = child->next;
        node->keys.erase(node->keys.begin() + i - 1);
        node->children.erase(node->children.begin() + i);
        delete child;
    } else if (rightSib) {
        // Merge with right sibling
        child->keys.insert(child->keys.end(), rightSib->keys.begin(), rightSib->keys.end());
        child->values.insert(child->values.end(), rightSib->values.begin(), rightSib->values.end());
        child->next = rightSib->next;
        node->keys.erase(node->keys.begin() + i);
        node->children.erase(node->children.begin() + i + 1);
        delete rightSib;
    }

    // Propagate underflow upward
    if (node != root && (int)node->keys.size() < minKeys) {
        underflow = true;
    }
}

// 15. remove(key) — C-3: with root collapse
void BPlusTree::remove(string key) {
    if (!root) return;

    bool underflow = false;
    removeHelper(root, nullptr, -1, key, underflow);

    // Root collapse: if root is internal with 0 keys and 1 child, replace root
    if (!root->isLeaf && root->keys.empty() && !root->children.empty()) {
        BPlusNode* newRoot = root->children[0];
        delete root;
        root = newRoot;
    }

    // E-1: Remove from sizeIndex
    FileNode* check = search(key);
    if (!check) {
        // Key was removed — find and erase from sizeIndex by key match
        for (auto it = sizeIndex.begin(); it != sizeIndex.end(); ++it) {
            if (it->second == key) {
                sizeIndex.erase(it);
                break;
            }
        }
    }
}

// 16. display()
void BPlusTree::display() {
    if (!root || (root->isLeaf && root->keys.empty())) return;
    
    queue<BPlusNode*> q;
    q.push(root);
    int level = 0;
    
    // Level-order traversal using queue
    while (!q.empty()) {
        int size = q.size();
        cout << "Level " << level << ": ";
        
        for (int count = 0; count < size; count++) {
            BPlusNode* curr = q.front();
            q.pop();
            
            // Print keys properly formatted inside brackets
            cout << "[";
            for (int i = 0; i < (int)curr->keys.size(); i++) {
                cout << curr->keys[i];
                if (i != (int)curr->keys.size() - 1) cout << ", ";
            }
            cout << "]";
            
            // Separator between nodes at same level
            if (count < size - 1) cout << " | ";
            
            // Add internal children into cycle
            if (!curr->isLeaf) {
                for (BPlusNode* child : curr->children) {
                    q.push(child);
                }
            }
        }
        cout << endl; // next line for new depth
        level++;
    }
}

// ─────────────────────────────────────────────────────────────────────
// 17. searchNodeWithPath — private recursive helper
// ─────────────────────────────────────────────────────────────────────
FileNode* BPlusTree::searchNodeWithPath(BPlusNode* node, string key,
                                         string& path, int& hops,
                                         int& nodeCounter) {
    if (!node) return nullptr;

    // Label this node
    string label;
    if (node == root) {
        label = "root";
    } else if (node->isLeaf) {
        label = "leaf" + to_string(nodeCounter++);
    } else {
        label = "node" + to_string(nodeCounter++);
    }

    // Append to path
    if (!path.empty()) path += "->";
    path += label;

    int i = 0;
    while (i < (int)node->keys.size() && key > node->keys[i]) {
        i++;
    }

    if (node->isLeaf) {
        hops++;
        if (i < (int)node->keys.size() && node->keys[i] == key) {
            return &(node->values[i]);
        }
        return nullptr;
    } else {
        hops++;
        return searchNodeWithPath(node->children[i], key, path, hops, nodeCounter);
    }
}

// ─────────────────────────────────────────────────────────────────────
// 18. searchWithPath(key) — public
// ─────────────────────────────────────────────────────────────────────
pair<FileNode*, string> BPlusTree::searchWithPath(string key) {
    if (!root) return {nullptr, "empty_tree"};

    string path;
    int hops = 0;
    int nodeCounter = 1;

    FileNode* result = searchNodeWithPath(root, key, path, hops, nodeCounter);

    path += "  [" + to_string(hops) + " hop" + (hops == 1 ? "" : "s") + "]";

    return {result, path};
}

// ─────────────────────────────────────────────────────────────────────
// 19. rangeSearchWithPath(k1, k2) — public
// ─────────────────────────────────────────────────────────────────────
vector<pair<FileNode*, string>> BPlusTree::rangeSearchWithPath(string k1, string k2) {
    vector<pair<FileNode*, string>> result;
    if (!root) return result;

    string path;
    int hops = 0;
    int nodeCounter = 1;

    BPlusNode* curr = root;

    while (!curr->isLeaf) {
        string label = (curr == root) ? "root" : "node" + to_string(nodeCounter++);
        if (!path.empty()) path += "->";
        path += label;

        int i = 0;
        while (i < (int)curr->keys.size() && k1 > curr->keys[i]) i++;
        curr = curr->children[i];
        hops++;
    }

    string leafLabel = "leaf" + to_string(nodeCounter++);
    if (!path.empty()) path += "->";
    path += leafLabel;
    hops++;

    string basePath = path + "  [" + to_string(hops) + " hop" + (hops == 1 ? "" : "s") + "]";

    while (curr) {
        for (int i = 0; i < (int)curr->keys.size(); i++) {
            if (curr->keys[i] >= k1 && curr->keys[i] <= k2) {
                result.push_back({&(curr->values[i]), basePath});
            } else if (curr->keys[i] > k2) {
                return result;
            }
        }
        curr = curr->next;
    }
    return result;
}
