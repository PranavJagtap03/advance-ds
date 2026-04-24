import { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { History as HistoryIcon, RotateCcw, Camera, FileClock } from 'lucide-react';

interface Version {
    fid: number;
    ver: number;
    name: string;
    path: string;
    parentId: number;
    size: number;
    modified: number;
}

const History = () => {
    const { sendCommand, subscribe } = useEngine();
    const [fileId, setFileId] = useState('');
    const [versions, setVersions] = useState<Version[]>([]);
    const [loading, setLoading] = useState(false);

    useEffect(() => {
        const unsub = subscribe('history', (line) => {
            if (line.startsWith('RESULT|VERSION')) {
                const parts = line.split('|');
                setVersions(prev => [...prev, {
                    fid: parseInt(parts[2]),
                    ver: parseInt(parts[3]),
                    name: parts[4],
                    path: parts[5],
                    parentId: parseInt(parts[6]),
                    size: parseInt(parts[7]),
                    modified: parseInt(parts[8])
                }]);
            }
            if (line.startsWith('VERSION_DONE')) {
                setLoading(false);
            }
            if (line.startsWith('ROLLBACK_OK')) {
                alert('Rollback successful!');
                fetchHistory();
            }
        });
        return () => unsub();
    }, [subscribe, fileId]);

    const fetchHistory = () => {
        if (!fileId) return;
        setLoading(true);
        setVersions([]);
        sendCommand(`VERSION_HISTORY ${fileId}`, 'history');
    };

    const handleRollback = (ver: number) => {
        if (confirm(`Roll back to version ${ver}? Current file will be overwritten.`)) {
            sendCommand(`ROLLBACK ${fileId} ${ver}`, 'history');
        }
    };

    const handleSnapshot = () => {
        sendCommand('SNAPSHOT', 'global');
    };

    return (
        <div className="flex flex-col h-full gap-8">
            <header className="flex justify-between items-start">
                <div>
                    <h1 className="text-2xl font-bold text-on-surface">File Timeline</h1>
                    <p className="text-on-surface-variant text-sm mt-1">Track metadata changes and restore previous file states.</p>
                </div>
                <button 
                    onClick={handleSnapshot}
                    className="flex items-center gap-2 bg-secondary text-on-secondary px-5 py-2.5 rounded-xl font-bold hover:brightness-110 transition-all shadow-md"
                >
                    <Camera className="w-4 h-4" />
                    CREATE SYSTEM SNAPSHOT
                </button>
            </header>

            <div className="bg-surface-container-lowest border border-outline-variant/10 p-6 rounded-3xl shadow-sm flex flex-col md:flex-row gap-4 items-center">
                <div className="flex-1 relative w-full">
                    <HistoryIcon className="absolute left-4 top-1/2 -translate-y-1/2 w-5 h-5 text-outline" />
                    <input 
                        type="number" 
                        value={fileId}
                        onChange={(e) => setFileId(e.target.value)}
                        placeholder="Enter File ID (e.g. 10001)"
                        className="w-full bg-surface border border-outline-variant/30 rounded-2xl py-3 pl-12 pr-4 outline-none focus:ring-2 focus:ring-primary/20 transition-all"
                    />
                </div>
                <button 
                    onClick={fetchHistory}
                    disabled={loading || !fileId}
                    className="bg-primary text-on-primary px-8 py-3 rounded-2xl font-bold hover:brightness-110 disabled:opacity-50 transition-all shadow-md w-full md:w-auto"
                >
                    VIEW HISTORY
                </button>
            </div>

            <div className="flex-1 overflow-auto space-y-4 custom-scrollbar pr-2">
                {versions.length === 0 ? (
                    <div className="h-full flex flex-col items-center justify-center text-center opacity-40">
                        <FileClock className="w-16 h-16 mb-4" />
                        <p className="text-sm font-medium max-w-xs">Enter a File ID above to see its modification history and available restore points.</p>
                    </div>
                ) : (
                    versions.map((v) => (
                        <div key={v.ver} className="bg-surface-container-lowest border border-outline-variant/10 p-6 rounded-2xl flex justify-between items-center hover:border-primary/20 transition-all shadow-sm group">
                            <div className="flex items-center gap-6">
                                <div className="w-12 h-12 bg-primary/5 text-primary rounded-full flex items-center justify-center font-black text-lg">
                                    {v.ver}
                                </div>
                                <div>
                                    <h4 className="font-bold text-on-surface">{v.name}</h4>
                                    <p className="text-xs text-on-surface-variant font-mono mt-1 opacity-70 truncate max-w-xl">{v.path}</p>
                                    <div className="flex gap-4 mt-3">
                                        <div className="text-[10px] font-bold text-outline uppercase">Size: <span className="text-on-surface">{(v.size/1024).toFixed(1)} KB</span></div>
                                        <div className="text-[10px] font-bold text-outline uppercase">Modified: <span className="text-on-surface">{new Date(v.modified * 1000).toLocaleString()}</span></div>
                                    </div>
                                </div>
                            </div>
                            <button 
                                onClick={() => handleRollback(v.ver)}
                                className="flex items-center gap-2 bg-primary/10 text-primary px-4 py-2 rounded-xl font-bold hover:bg-primary hover:text-on-primary transition-all opacity-0 group-hover:opacity-100"
                            >
                                <RotateCcw className="w-4 h-4" />
                                RESTORE
                            </button>
                        </div>
                    ))
                )}
            </div>
        </div>
    );
};

export default History;
