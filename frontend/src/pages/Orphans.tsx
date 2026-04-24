import React, { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { Ghost, Link2Off, Trash2, Cpu } from 'lucide-react';

interface Orphan {
    name: string;
    path: string;
}

const Orphans = () => {
    const { sendCommand, subscribe } = useEngine();
    const [orphans, setOrphans] = useState<Orphan[]>([]);
    const [loading, setLoading] = useState(false);

    useEffect(() => {
        let activeOrphans: Orphan[] = [];
        
        const unsub = subscribe('orphans', (line, type) => {
            if (line.startsWith('ORPHAN_DONE')) {
                setOrphans([...activeOrphans]);
                setLoading(false);
            } 
            else if (line.startsWith('ORPHAN|')) {
                const parts = line.split('|');
                activeOrphans.push({
                    name: parts[2] || 'unknown',
                    path: parts[3] || 'unknown path'
                });
            }
        });

        return () => unsub();
    }, [subscribe]);

    const runScan = () => {
        setOrphans([]);
        setLoading(true);
        sendCommand('ORPHANS', 'orphans');
    };

    return (
        <div className="flex flex-col h-full gap-6">
            <header className="flex justify-between items-end">
                <div>
                    <h1 className="font-h1 text-on-surface">Orphan Node Cleanup</h1>
                    <p className="text-on-surface-variant font-body-md mt-1">
                        Detect disconnected entity blocks floating outside the main Union-Find forest pathing.
                    </p>
                </div>
                <button 
                    onClick={runScan}
                    disabled={loading}
                    className="bg-primary text-on-primary px-4 py-2 rounded-md font-label-md tracking-wider hover:bg-primary-container hover:text-on-primary-container shadow-sm disabled:opacity-50 flex items-center gap-2"
                >
                    <Cpu className="w-4 h-4"/> {loading ? 'TRAVERSING DFS...' : 'RUN PATH VALIDATION'}
                </button>
            </header>

            {!loading && orphans.length === 0 && (
                <div className="flex-1 flex items-center justify-center border-2 border-dashed border-outline-variant/30 rounded-xl bg-surface-container-lowest/50">
                    <div className="text-center">
                        <Ghost className="mx-auto w-12 h-12 text-outline-variant/50 mb-3" />
                        <h3 className="font-h3 text-on-surface">Forest is Connected</h3>
                        <p className="text-sm font-sans text-on-surface-variant">No disconnected or orphaned nodes discovered via Union-Find algorithm.</p>
                    </div>
                </div>
            )}

            {orphans.length > 0 && (
                <div className="bg-error-container p-4 rounded-xl border border-error/20 flex gap-3 text-on-error-container mb-2">
                    <Link2Off className="w-5 h-5 opacity-80" />
                    <div className="text-sm shadow-sm">
                        <b>{orphans.length} Unlinked Entities Discovered.</b> These files exist within the indexed structure but have no valid connectivity to the root directory pathing, rendering them invisible to traditional traversal methods.
                    </div>
                </div>
            )}

            <div className="flex-1 overflow-auto -mx-2 px-2 custom-scrollbar space-y-3">
                {orphans.map((o, i) => (
                    <div key={i} className="bg-surface-container-lowest border border-outline-variant/20 hover:border-error/30 p-4 rounded-xl flex justify-between items-center transition-all shadow-sm">
                        <div className="flex items-center gap-4">
                            <div className="p-3 bg-surface-container rounded-md text-error/80">
                                <Ghost className="w-6 h-6" />
                            </div>
                            <div>
                                <h4 className="font-h3 text-on-surface">{o.name}</h4>
                                <p className="text-xs text-on-surface-variant font-mono mt-1 break-all">{o.path}</p>
                            </div>
                        </div>
                        <div className="text-right">
                            <button className="text-error bg-error/10 hover:bg-error hover:text-on-error px-4 py-2 font-label-md tracking-wider rounded-md transition-colors flex items-center gap-2">
                                <Trash2 className="w-4 h-4"/> PRUNE
                            </button>
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};

export default Orphans;
