import { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { History as HistoryIcon, Search as SearchIcon, RotateCcw, Camera, ArrowRightLeft, FileClock, HardDrive } from 'lucide-react';

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
    const [activeTab, setActiveTab] = useState<'files' | 'snapshots'>('files');

    useEffect(() => {
        const unsub = subscribe('history', (line) => {
            if (line.startsWith('VERSION_DONE')) setLoading(false);
            else if (line.startsWith('RESULT|VERSION')) {
                const parts = line.split('|');
                setVersions(prev => [...prev, {
                    id: parts[2], version: parts[3], name: parts[4], path: parts[5], size: parts[7], mtime: parts[8]
                }]);
            }
        });
        return () => unsub();
    }, [subscribe]);

    const executeSearch = (e: React.FormEvent) => {
        e.preventDefault();
        if (!fileId.trim()) return;
        setVersions([]);
        setLoading(true);
        sendCommand(`VERSION_HISTORY ${fileId}`, 'history');
    };

    const handleRollback = (v: Version) => {
        if(window.confirm(`Restore file to version ${v.version}?`)) {
            sendCommand(`ROLLBACK ${v.id} ${v.version}`, 'history');
            setTimeout(() => { setVersions([]); sendCommand(`VERSION_HISTORY ${v.id}`, 'history'); }, 500);
        }
    };

    const formatBytes = (b: string) => {
        const bytes = parseInt(b);
        if (bytes < 1024) return bytes + ' B';
        return (bytes / 1024).toFixed(1) + ' KB';
    };

    return (
        <div className="flex flex-col h-full gap-8">
            <header className="flex justify-between items-start">
                <div>
                    <h1 className="text-2xl font-bold text-on-surface">File Timeline</h1>
                    <p className="text-on-surface-variant text-sm mt-1">Review modification history and restore previous file states.</p>
                </div>
                <div className="flex bg-surface-container-low p-1 rounded-xl border border-outline-variant/20 shadow-inner">
                    <button 
                        onClick={() => setActiveTab('files')}
                        className={`px-4 py-1.5 rounded-lg text-xs font-bold transition-all ${activeTab === 'files' ? 'bg-surface-container-lowest text-primary shadow-sm' : 'text-on-surface-variant hover:text-on-surface'}`}
                    >
                        FILE VERSIONS
                    </button>
                    <button 
                        onClick={() => setActiveTab('snapshots')}
                        className={`px-4 py-1.5 rounded-lg text-xs font-bold transition-all ${activeTab === 'snapshots' ? 'bg-surface-container-lowest text-primary shadow-sm' : 'text-on-surface-variant hover:text-on-surface'}`}
                    >
                        DIRECTORY SNAPSHOTS
                    </button>
                </div>
            </header>

            {activeTab === 'files' ? (
                <>
                    <form onSubmit={executeSearch} className="flex gap-3 max-w-md">
                        <div className="flex-1 relative group">
                            <SearchIcon className="absolute left-4 top-1/2 -translate-y-1/2 w-4 h-4 text-outline transition-colors group-focus-within:text-primary" />
                            <input 
                                type="text" 
                                value={fileId}
                                onChange={(e) => setFileId(e.target.value)}
                                placeholder="Enter File ID..."
                                className="w-full bg-surface-container-lowest border border-outline-variant/30 rounded-xl py-3 pl-11 pr-4 outline-none focus:ring-2 focus:ring-primary/20 transition-all shadow-sm text-sm font-mono"
                            />
                        </div>
                        <button type="submit" className="bg-primary text-on-primary px-6 rounded-xl font-bold hover:brightness-110 active:scale-95 transition-all shadow-md">
                            FIND
                        </button>
                    </form>

                    <div className="flex-1 bg-surface-container-lowest border border-outline-variant/10 rounded-3xl overflow-hidden shadow-sm flex flex-col">
                        <div className="bg-surface-container-low px-8 py-4 border-b border-outline-variant/5 flex items-center gap-3">
                            <FileClock className="w-5 h-5 text-primary" />
                            <h3 className="text-xs font-black text-outline uppercase tracking-widest">Version Timeline</h3>
                        </div>
                        
                        <div className="flex-1 overflow-auto custom-scrollbar p-8">
                            {loading && <div className="text-center py-20 text-outline animate-pulse font-medium">Retrieving timeline...</div>}
                            {!loading && versions.length === 0 && (
                                <div className="flex flex-col items-center justify-center py-20 opacity-40">
                                    <HistoryIcon className="w-12 h-12 mb-4" />
                                    <p className="text-sm font-bold">Search for a File ID to see its timeline.</p>
                                </div>
                            )}
                            
                            {versions.length > 0 && (
                                <div className="relative border-l-2 border-outline-variant/30 ml-4 space-y-10">
                                    {versions.map((v, i) => (
                                        <div key={i} className="relative pl-10">
                                            <div className="absolute -left-[9px] top-1 w-4 h-4 rounded-full bg-primary border-4 border-surface-container-lowest shadow-sm" />
                                            <div className="bg-surface-container-low/50 border border-outline-variant/10 p-6 rounded-2xl hover:border-primary/20 transition-all group max-w-2xl shadow-sm">
                                                <div className="flex justify-between items-start mb-6">
                                                    <div className="flex items-center gap-3">
                                                        <span className="font-black text-[10px] text-primary bg-primary/10 px-2 py-0.5 rounded-full tracking-widest uppercase">Version {v.version}</span>
                                                        <h4 className="font-bold text-on-surface">{v.name}</h4>
                                                    </div>
                                                    <button 
                                                        onClick={() => handleRollback(v)}
                                                        className="opacity-0 group-hover:opacity-100 bg-surface border border-outline-variant/30 text-on-surface px-3 py-1 rounded-lg text-[10px] font-black hover:bg-primary hover:text-on-primary transition-all flex items-center gap-2"
                                                    >
                                                        <RotateCcw className="w-3 h-3" /> RESTORE THIS STATE
                                                    </button>
                                                </div>
                                                <div className="grid grid-cols-2 gap-8">
                                                    <div>
                                                        <p className="text-[10px] font-black text-outline uppercase tracking-widest mb-1.5">Storage Impact</p>
                                                        <p className="text-sm font-bold text-on-surface-variant">{formatBytes(v.size)}</p>
                                                    </div>
                                                    <div>
                                                        <p className="text-[10px] font-black text-outline uppercase tracking-widest mb-1.5">File Location</p>
                                                        <p className="text-sm font-mono text-on-surface-variant truncate opacity-80" title={v.path}>{v.path}</p>
                                                    </div>
                                                </div>
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            )}
                        </div>
                    </div>
                </>
            ) : (
                <div className="flex-1 flex flex-col gap-8">
                    <div className="bg-primary/5 border border-primary/20 rounded-3xl p-10 flex items-center justify-between shadow-sm">
                        <div className="flex items-center gap-6">
                            <div className="p-4 bg-primary text-on-primary rounded-2xl shadow-lg">
                                <Camera className="w-8 h-8" />
                            </div>
                            <div>
                                <h3 className="text-xl font-black text-on-surface">Library Snapshot</h3>
                                <p className="text-sm text-on-surface-variant font-medium mt-1">Capture the current state of your indexed folders for future comparison.</p>
                            </div>
                        </div>
                        <button className="bg-primary text-on-primary px-8 py-3 rounded-2xl font-bold hover:brightness-110 shadow-lg active:scale-95 transition-all">
                            TAKE SNAPSHOT
                        </button>
                    </div>

                    <div className="flex flex-col gap-4">
                        <h3 className="text-sm font-black text-outline uppercase tracking-widest flex items-center gap-2">
                            <HardDrive className="w-4 h-4" /> Snapshot History
                        </h3>
                        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                            <div className="bg-surface-container-lowest border border-outline-variant/10 p-6 rounded-2xl opacity-40 border-dashed">
                                <p className="text-center py-4 font-bold text-outline-variant text-sm">Snapshots are not yet persistent. Run a scan first.</p>
                            </div>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
};

export default History;
