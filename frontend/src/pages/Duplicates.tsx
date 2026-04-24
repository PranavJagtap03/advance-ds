import React, { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { Copy, Trash2, Cpu } from 'lucide-react';

interface DupGroup {
    hash: string;
    count: number;
    paths: string[];
}

const Duplicates = () => {
    const { sendCommand, subscribe } = useEngine();
    const [groups, setGroups] = useState<DupGroup[]>([]);
    const [loading, setLoading] = useState(false);
    
    // To buffer the current group paths being built
    const [currentGroupHash, setCurrentGroupHash] = useState<string | null>(null);

    useEffect(() => {
        let activeGroups: DupGroup[] = [];
        let curGroup: DupGroup | null = null;
        
        const unsub = subscribe('duplicates', (line, type) => {
            if (line.startsWith('DUP_DONE')) {
                setGroups([...activeGroups]);
                setLoading(false);
            } 
            else if (line.startsWith('DUP|')) {
                const parts = line.split('|');
                const pathsStr = parts[5] || '';
                activeGroups.push({
                    hash: parts[2] || 'unknown',
                    count: parseInt(parts[4] || '0', 10),
                    paths: pathsStr ? pathsStr.split(',') : []
                });
            }
            else if (line.startsWith('RESULT|NOT_FOUND')) {
                 setLoading(false);
            }
        });

        return () => unsub();
    }, [subscribe]);

    const runScan = () => {
        setGroups([]);
        setLoading(true);
        sendCommand('DUPLICATES', 'duplicates');
    };

    return (
        <div className="flex flex-col h-full gap-6">
            <header className="flex justify-between items-end">
                <div>
                    <h1 className="font-h1 text-on-surface">Duplicate Clusters</h1>
                    <p className="text-on-surface-variant font-body-md mt-1">
                        Identify redundant files matching the exact data characteristics safely via checksum hashing.
                    </p>
                </div>
                <button 
                    onClick={runScan}
                    disabled={loading}
                    className="bg-primary text-on-primary px-4 py-2 rounded-md font-label-md tracking-wider hover:bg-primary-container hover:text-on-primary-container shadow-sm disabled:opacity-50 flex items-center gap-2"
                >
                    <Cpu className="w-4 h-4"/> {loading ? 'ANALYZING...' : 'RUN CLUSTER SCAN'}
                </button>
            </header>

            {!loading && groups.length === 0 && (
                <div className="flex-1 flex items-center justify-center border-2 border-dashed border-outline-variant/30 rounded-xl bg-surface-container-lowest/50">
                    <div className="text-center">
                        <Copy className="mx-auto w-12 h-12 text-outline-variant/50 mb-3" />
                        <h3 className="font-h3 text-on-surface">Zero Clusters Found</h3>
                        <p className="text-sm font-sans text-on-surface-variant">Run a scan to detect duplicate content blocks.</p>
                    </div>
                </div>
            )}

            <div className="flex-1 overflow-auto -mx-2 px-2 custom-scrollbar space-y-6">
                {groups.map((g, i) => (
                    <div key={i} className="bg-surface-container-lowest border border-outline-variant/20 rounded-xl overflow-hidden shadow-sm">
                        <div className="bg-surface-container py-3 px-4 flex justify-between items-center border-b border-outline-variant/10">
                            <div className="flex items-center gap-3">
                                <span className="p-1.5 bg-primary/10 text-primary rounded-md">
                                    <Copy className="w-4 h-4" />
                                </span>
                                <h4 className="font-label-md tracking-widest text-on-surface-variant uppercase">Cluster {g.hash.substring(0,8)}...</h4>
                            </div>
                            <span className="text-xs font-mono font-bold text-on-surface px-2 py-1 bg-surface-variant rounded-md border border-outline-variant/30">
                                {g.paths.length} copies
                            </span>
                        </div>
                        <ul className="divide-y divide-outline-variant/10">
                            {g.paths.map((p, j) => (
                                <li key={j} className="flex justify-between flex-wrap gap-2 items-center px-4 py-3 hover:bg-surface-variant/30 transition-colors">
                                    <span className="text-sm font-sans text-on-surface truncate break-all block flex-1">{p}</span>
                                    <button className="text-error opacity-60 hover:opacity-100 transition-opacity p-2 hover:bg-error-container rounded-md">
                                        <Trash2 className="w-4 h-4" />
                                    </button>
                                </li>
                            ))}
                        </ul>
                    </div>
                ))}
            </div>
        </div>
    );
};

export default Duplicates;
