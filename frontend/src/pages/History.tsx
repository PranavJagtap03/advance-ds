import { useState, useEffect, useRef } from 'react';
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
    // E-2: Inline rollback status
    const [rollbackStatus, setRollbackStatus] = useState<string | null>(null);
    // E-2: Inline two-step confirmation (replaces confirm())
    const [pendingRollback, setPendingRollback] = useState<number | null>(null);
    // P-2: Ref to track current fileId for subscriber closure
    const fileIdRef = useRef(fileId);

    // P-2: Keep ref in sync with state
    useEffect(() => { fileIdRef.current = fileId; }, [fileId]);

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
            // E-2: Show inline status for rollback results
            if (line.startsWith('ROLLBACK_OK')) {
                const parts = line.split('|');
                const suffix = parts[3] || '';
                if (suffix === 'METADATA_ONLY') {
                    setRollbackStatus('Rollback successful (metadata only — no physical backup was available)');
                } else {
                    setRollbackStatus('Rollback successful — file fully restored!');
                }
                fetchHistory();
                setTimeout(() => setRollbackStatus(null), 5000);
            }
            if (line.startsWith('ROLLBACK_ERROR')) {
                setRollbackStatus('Rollback failed: ' + (line.split('|')[1] || 'unknown error'));
                setTimeout(() => setRollbackStatus(null), 5000);
            }
        });
        return () => unsub();
    }, [subscribe]); // P-2: Removed fileId from deps to prevent re-subscribe on keystroke

    // P-2: Use ref so subscriber closure always has current value
    const fetchHistory = () => {
        if (!fileIdRef.current) return;
        setLoading(true);
        setVersions([]);
        sendCommand(`VERSION_HISTORY ${fileIdRef.current}`, 'history');
    };

    // E-2: First click — show inline confirmation card
    const handleRollback = (ver: number) => {
        setPendingRollback(ver);
    };

    // E-2: Confirm click — actually perform rollback
    const confirmRollback = () => {
        if (pendingRollback === null) return;
        setRollbackStatus(null);
        sendCommand(`ROLLBACK ${fileId} ${pendingRollback}`, 'history');
        setPendingRollback(null);
    };

    // E-2: Cancel click — dismiss confirmation
    const cancelRollback = () => {
        setPendingRollback(null);
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

            {/* E-2: Inline rollback status */}
            {rollbackStatus && (
                <div className={`p-4 rounded-2xl text-sm font-bold flex items-center gap-3 ${rollbackStatus.includes('failed') ? 'bg-error/10 text-error border border-error/20' : 'bg-green-50 text-green-700 border border-green-200'}`}>
                    {rollbackStatus}
                    <button onClick={() => setRollbackStatus(null)} className="ml-auto text-xs opacity-60 hover:opacity-100">DISMISS</button>
                </div>
            )}

            {/* E-2: Inline two-step confirmation card */}
            {pendingRollback !== null && (
                <div className="bg-amber-50 border border-amber-300 p-5 rounded-2xl flex items-center justify-between shadow-sm animate-in">
                    <div className="flex items-center gap-3">
                        <RotateCcw className="w-5 h-5 text-amber-600" />
                        <p className="text-sm font-bold text-amber-800">Confirm rollback to v{pendingRollback}? Current file will be overwritten.</p>
                    </div>
                    <div className="flex gap-2">
                        <button onClick={cancelRollback} className="px-4 py-2 rounded-xl text-xs font-bold text-on-surface-variant bg-surface-container-high hover:bg-surface-container-highest transition-all border border-outline-variant/20">CANCEL</button>
                        <button onClick={confirmRollback} className="px-4 py-2 rounded-xl text-xs font-bold text-on-primary bg-primary hover:brightness-110 transition-all shadow-md">CONFIRM</button>
                    </div>
                </div>
            )}

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
