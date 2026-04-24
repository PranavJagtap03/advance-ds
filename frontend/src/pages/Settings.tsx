import React from 'react';
import { BookOpen, Database, Fingerprint, Ghost, Network } from 'lucide-react';

const Settings = () => {
    return (
        <div className="flex flex-col h-full gap-6">
            <header className="mb-4">
                <h1 className="font-h1 text-on-surface">System Help & Glossary</h1>
                <p className="text-on-surface-variant font-body-md mt-1">
                    Understand the data structures powering the internal engine.
                </p>
            </header>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6 overflow-auto custom-scrollbar pb-10">
                
                <div className="bg-surface-container-lowest border border-outline-variant/20 p-6 rounded-xl shadow-sm">
                    <div className="flex items-center gap-3 mb-4">
                        <span className="p-2 bg-primary/10 text-primary rounded-lg"><Database className="w-5 h-5"/></span>
                        <h3 className="font-h3 text-on-surface">B+ Tree (Exact Search)</h3>
                    </div>
                    <p className="text-sm font-sans text-on-surface-variant leading-relaxed">
                        The primary indexing structure mapping exact filenames to explicit physical paths. 
                        It achieves strict <strong className="text-primary font-mono bg-primary/5 px-1 rounded">O(log N)</strong> worst-case traversal 
                        due to aggressive fan-out branching. The web application leverages this for standard queries.
                    </p>
                </div>

                <div className="bg-surface-container-lowest border border-outline-variant/20 p-6 rounded-xl shadow-sm">
                    <div className="flex items-center gap-3 mb-4">
                        <span className="p-2 bg-secondary/10 text-secondary rounded-lg"><Network className="w-5 h-5"/></span>
                        <h3 className="font-h3 text-on-surface">Trie Node (Prefix Discovery)</h3>
                    </div>
                    <p className="text-sm font-sans text-on-surface-variant leading-relaxed">
                        A specialized lexicographic tree mapped parallel to the B+ index. Handles all queries appending an asterisk (*). 
                        Traversal is bounded by <strong className="text-secondary font-mono bg-secondary/5 px-1 rounded">O(L)</strong> where L is word length, rather than the filesystem volume.
                    </p>
                </div>

                <div className="bg-surface-container-lowest border border-outline-variant/20 p-6 rounded-xl shadow-sm">
                    <div className="flex items-center gap-3 mb-4">
                        <span className="p-2 bg-purple-500/10 text-purple-600 rounded-lg"><Fingerprint className="w-5 h-5"/></span>
                        <h3 className="font-h3 text-on-surface">Segment Tree (Aggregations)</h3>
                    </div>
                    <p className="text-sm font-sans text-on-surface-variant leading-relaxed">
                        Divides spatial and temporal bounds to allow constant delta integrations. Used explicitly inside the Storage Analytics module to compute multi-month ranges strictly in <strong className="text-purple-600 font-mono bg-purple-500/5 px-1 rounded">O(log M)</strong> bounding hops.
                    </p>
                </div>

                <div className="bg-surface-container-lowest border border-outline-variant/20 p-6 rounded-xl shadow-sm">
                    <div className="flex items-center gap-3 mb-4">
                        <span className="p-2 bg-error/10 text-error rounded-lg"><Ghost className="w-5 h-5"/></span>
                        <h3 className="font-h3 text-on-surface">Union-Find (Forest Checking)</h3>
                    </div>
                    <p className="text-sm font-sans text-on-surface-variant leading-relaxed">
                        Constructs disjoint-sets associating file blocks to root namespaces. Used in the Orphan Cleanup module to algorithmically detect files disconnected from standard graph traversal, achieving near <strong className="text-error font-mono bg-error/5 px-1 rounded">O(1)</strong> amortized complexity.
                    </p>
                </div>

                <div className="bg-surface-container-lowest border border-outline-variant/20 p-6 rounded-xl shadow-sm md:col-span-2">
                    <div className="flex items-center gap-3 mb-4">
                        <span className="p-2 bg-slate-500/10 text-slate-600 rounded-lg"><BookOpen className="w-5 h-5"/></span>
                        <h3 className="font-h3 text-on-surface">Persistent Immutable Stack (Version Config)</h3>
                    </div>
                    <p className="text-sm font-sans text-on-surface-variant leading-relaxed mb-4">
                        Data modifications push historical tree pointers onto an immutable graph stack. Using the Snapshot command allows for time-travel restoration, effectively returning specific nodes to past path associations instantly without recompiling the B+ tree layout.
                    </p>
                </div>

            </div>
        </div>
    );
};

export default Settings;
