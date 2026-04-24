import React, { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { History as HistoryIcon, Search as SearchIcon, RotateCcw, Save } from 'lucide-react';

interface Version {
    id: string;
    version: string;
    name: string;
    path: string;
    size: string;
    mtime: string;
}

const History = () => {
    const { sendCommand, subscribe } = useEngine();
    const [fileId, setFileId] = useState('');
    const [versions, setVersions] = useState<Version[]>([]);
    const [loading, setLoading] = useState(false);

    useEffect(() => {
        const unsub = subscribe('history', (line, type) => {
            if (line === 'VERSION_DONE') {
                setLoading(false);
            } 
            else if (line.startsWith('RESULT|VERSION')) {
                const parts = line.split('|');
                setVersions(prev => [...prev, {
                    id: parts[2] || '',
                    version: parts[3] || '',
                    name: parts[4] || '',
                    path: parts[5] || '',
                    size: parts[7] || '0',
                    mtime: parts[8] || '0'
                }]);
            }
        });

        return () => unsub();
    }, [subscribe]);

    const executeSearch = (e: React.FormEvent) => {
        e.preventDefault();
        const fid = fileId.trim();
        if (!fid) return;

        setVersions([]);
        setLoading(true);
        sendCommand(`VERSION_HISTORY ${fid}`, 'history');
    };

    const handleRollback = (v: Version) => {
        if(window.confirm(`Are you sure you want to rollback to V${v.version} for this file?`)) {
            sendCommand(`ROLLBACK ${v.id} ${v.version}`, 'history');
            // Re-fetch immediately to show the new timeline after rollback succeeds
            setTimeout(() => {
                setVersions([]);
                sendCommand(`VERSION_HISTORY ${v.id}`, 'history');
            }, 500);
        }
    };

    return (
        <div className="flex flex-col h-full gap-6">
            <header className="flex justify-between items-end">
                <div>
                    <h1 className="font-h1 text-on-surface">Snapshot History</h1>
                    <p className="text-on-surface-variant font-body-md mt-1">
                        Interact directly with the Persistent DS to review mutation timelines or reverse structural edits.
                    </p>
                </div>
            </header>

            <form onSubmit={executeSearch} className="flex relative items-center max-w-sm bg-surface-container-lowest border border-outline-variant/30 rounded-xl overflow-hidden shadow-sm focus-within:ring-2 focus-within:ring-primary/20 transition-all">
                <SearchIcon className="absolute left-4 w-5 h-5 text-outline" />
                <input 
                    type="text" 
                    value={fileId}
                    onChange={(e) => setFileId(e.target.value)}
                    placeholder="Enter absolute File ID..."
                    className="flex-1 w-full py-3 pl-12 pr-4 bg-transparent outline-none font-sans text-on-surface text-sm"
                />
                <button type="submit" className="bg-primary text-on-primary px-4 py-3 font-label-md tracking-wider hover:bg-primary-container hover:text-on-primary-container transition-colors disabled:opacity-50" disabled={loading}>
                    SEARCH
                </button>
            </form>

            <div className="flex-1 bg-surface-container-lowest border border-outline-variant/20 rounded-xl shadow-sm overflow-hidden flex flex-col">
                <div className="bg-surface-container py-3 px-6 border-b border-outline-variant/10 flex items-center gap-3">
                    <HistoryIcon className="w-5 h-5 text-primary" />
                    <h3 className="font-label-md tracking-widest text-on-surface-variant uppercase">Time Series Tracker</h3>
                </div>
                
                <div className="flex-1 overflow-auto custom-scrollbar p-6">
                    {loading && versions.length === 0 && (
                        <div className="text-center text-sm font-mono text-outline-variant py-8">Querying persistent graph...</div>
                    )}
                    {!loading && versions.length === 0 && (
                        <div className="text-center text-sm font-mono text-outline-variant py-8 border-2 border-dashed border-outline-variant/20 rounded-lg">Provide a valid File ID.</div>
                    )}
                    
                    {versions.length > 0 && (
                        <div className="relative border-l-2 border-outline-variant/30 ml-4 space-y-8 pb-4">
                            {versions.map((v, i) => (
                                <div key={i} className="relative pl-6">
                                    <div className="absolute -left-[5px] top-[4px] w-[10px] h-[10px] rounded-full bg-primary ring-4 ring-surface-container-lowest shadow-sm" />
                                    
                                    <div className="bg-surface-container-low border border-outline-variant/20 p-5 rounded-xl hover:border-primary/30 transition-all shadow-[0_2px_10px_rgba(0,0,0,0.02)] group">
                                        <div className="flex justify-between items-start mb-3">
                                            <div className="flex items-center gap-3">
                                                <span className="font-bold font-mono text-primary bg-primary/10 px-2 py-0.5 rounded text-sm tracking-tight">V{v.version}</span>
                                                <h4 className="font-semibold text-on-surface text-sm">{v.name}</h4>
                                            </div>
                                            <button 
                                                onClick={() => handleRollback(v)}
                                                className="opacity-0 group-hover:opacity-100 px-3 py-1 bg-surface border border-outline-variant/30 text-on-surface text-xs font-label-md tracking-wide rounded hover:bg-primary-container hover:border-primary-container hover:text-on-primary-container transition-all flex items-center gap-1"
                                            >
                                                <RotateCcw className="w-3 h-3" /> RESTORE
                                            </button>
                                        </div>
                                        
                                        <div className="grid grid-cols-2 gap-4 text-xs font-mono text-on-surface-variant mt-2 bg-surface-variant/30 p-3 rounded-md">
                                            <div className="flex flex-col gap-1">
                                                <span className="text-[10px] text-outline uppercase tracking-widest">Metadata Size</span>
                                                <span className="text-on-surface">{v.size} Bytes</span>
                                            </div>
                                            <div className="flex flex-col gap-1">
                                                <span className="text-[10px] text-outline uppercase tracking-widest">Global Path Allocation</span>
                                                <span className="truncate text-on-surface">{v.path}</span>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            ))}
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
};

export default History;
