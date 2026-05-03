import { useState, useEffect, useRef } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { Ghost, Link2Off, Trash2, FolderSync, AlertCircle } from 'lucide-react';

interface BrokenLink {
    id: number;
    name: string;
    path: string;
    sizeKB: number;
}

const Orphans = () => {
    const { sendCommand, subscribe } = useEngine();
    const [links, setLinks] = useState<BrokenLink[]>([]);
    const [totalWastedMB, setTotalWastedMB] = useState(0);
    const [loading, setLoading] = useState(false);
    // E-8: Generation counter to discard stale subscriber callbacks
    const genRef = useRef(0);
    // Inline two-step confirmation for delete-all (replaces confirm())
    const [pendingDeleteAll, setPendingDeleteAll] = useState(false);

    useEffect(() => {
        loadOrphans();
    }, []);

    const loadOrphans = async () => {
        setLoading(true);
        setLinks([]);
        setPendingDeleteAll(false);
        try {
            const res = await fetch('http://localhost:8000/api/orphans');
            const data = await res.json();
            setLinks(data.files.map((f: any) => ({
                id: f.id,
                name: f.name,
                path: f.path,
                sizeKB: f.sizeKB
            })));
            setTotalWastedMB(data.total_wasted_mb);
        } catch(e) {
            console.error('Failed to load orphans', e);
        } finally {
            setLoading(false);
        }
    };

    const refresh = () => { loadOrphans(); };

    const handleDelete = (id: number) => {
        sendCommand(`DELETE ${id}`, 'orphans');
        setLinks(prev => prev.filter(l => l.id !== id));
    };

    const handleDeleteAllClick = () => {
        setPendingDeleteAll(true);
    };

    const confirmDeleteAll = () => {
        sendCommand('DELETE_ORPHANS', 'orphans');
        setLinks([]);
        setPendingDeleteAll(false);
    };

    const cancelDeleteAll = () => {
        setPendingDeleteAll(false);
    };

    return (
        <div className="flex flex-col h-full gap-6">
            <header className="flex justify-between items-start">
                <div>
                    <h1 className="text-2xl font-bold text-on-surface">Broken File Links</h1>
                    <p className="text-on-surface-variant text-sm mt-1">
                        Files whose parent folders no longer exist or are disconnected from the scan root.
                    </p>
                </div>
                <div className="flex gap-3">
                    {links.length > 0 && !pendingDeleteAll && (
                        <button 
                            onClick={handleDeleteAllClick}
                            className="bg-error text-on-error px-6 py-2.5 rounded-xl font-bold hover:brightness-110 active:scale-95 transition-all shadow-md flex items-center gap-2"
                        >
                            <Trash2 className="w-4 h-4" />
                            DELETE ALL
                        </button>
                    )}
                    <button 
                        onClick={refresh}
                        disabled={loading}
                        className="bg-primary text-on-primary px-6 py-2.5 rounded-xl font-bold hover:brightness-110 active:scale-95 transition-all shadow-md disabled:opacity-50 flex items-center gap-2"
                    >
                        <FolderSync className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
                        {loading ? 'VALIDATING...' : 'CHECK LINKS'}
                    </button>
                </div>
            </header>

            {/* Inline two-step delete-all confirmation */}
            {pendingDeleteAll && (
                <div className="bg-error/5 border border-error/20 p-5 rounded-2xl flex items-center justify-between shadow-sm">
                    <p className="text-sm font-bold text-error">
                        Delete all {links.length} orphan files permanently? This cannot be undone.
                    </p>
                    <div className="flex gap-2">
                        <button onClick={cancelDeleteAll} className="px-4 py-2 rounded-xl text-xs font-bold bg-surface-container-high border border-outline-variant/20">CANCEL</button>
                        <button onClick={confirmDeleteAll} className="px-4 py-2 rounded-xl text-xs font-bold text-on-error bg-error hover:brightness-110 shadow-sm">CONFIRM DELETE ALL</button>
                    </div>
                </div>
            )}

            {links.length > 0 ? (
                <>
                    <div className="bg-error/5 border border-error/20 p-6 rounded-2xl flex items-center justify-between shadow-sm">
                        <div className="flex items-center gap-4 text-error">
                            <AlertCircle className="w-8 h-8" />
                            <div>
                                <h3 className="text-lg font-black">{links.length} Broken Links Found</h3>
                                <p className="text-sm opacity-80 font-medium">These files occupy space but aren't accessible via your normal folder structure.</p>
                            </div>
                        </div>
                        <div className="text-right">
                            <p className="text-[10px] font-bold text-outline uppercase tracking-widest">Total Wasted</p>
                            <p className="text-2xl font-black text-on-surface">{totalWastedMB.toFixed(1)} MB</p>
                        </div>
                    </div>

                    <div className="flex-1 overflow-auto space-y-3 custom-scrollbar pr-2">
                        {links.map((link) => (
                            <div key={link.id} className="bg-surface-container-lowest border border-outline-variant/10 p-5 rounded-2xl flex justify-between items-center hover:border-error/20 transition-all shadow-sm group">
                                <div className="flex items-center gap-5 overflow-hidden">
                                    <div className="p-3 bg-surface-container rounded-xl text-outline group-hover:text-error transition-colors">
                                        <Ghost className="w-6 h-6" />
                                    </div>
                                    <div className="overflow-hidden">
                                        <h4 className="font-bold text-on-surface truncate">{link.name}</h4>
                                        <p className="text-xs text-on-surface-variant font-mono mt-1 truncate max-w-lg opacity-60 group-hover:opacity-100 transition-opacity">{link.path}</p>
                                    </div>
                                </div>
                                <div className="flex items-center gap-6">
                                    <div className="text-right hidden md:block">
                                        <p className="text-[10px] font-bold text-outline uppercase">Size</p>
                                        <p className="text-xs font-black text-on-surface-variant">{link.sizeKB} KB</p>
                                    </div>
                                    <div className="flex gap-2">
                                        <button 
                                            onClick={() => setLinks(prev => prev.filter(l => l.id !== link.id))}
                                            className="text-xs font-bold text-primary hover:underline px-3 py-1.5"
                                        >IGNORE</button>
                                        <button 
                                            onClick={() => handleDelete(link.id)}
                                            className="text-xs font-bold text-on-error bg-error hover:brightness-110 px-4 py-1.5 rounded-lg transition-all shadow-sm"
                                        >
                                            DELETE
                                        </button>
                                    </div>
                                </div>
                            </div>
                        ))}
                    </div>
                </>
            ) : !loading && (
                <div className="flex-1 flex flex-col items-center justify-center border-2 border-dashed border-outline-variant/20 rounded-3xl bg-surface-container-lowest/30">
                    <div className="p-6 bg-primary/5 rounded-full mb-4">
                        <Link2Off className="w-12 h-12 text-outline-variant" />
                    </div>
                    <h3 className="text-xl font-bold text-on-surface">No Broken Links</h3>
                    <p className="text-sm text-on-surface-variant mt-2 max-w-xs text-center">Your file hierarchy is perfectly connected. No orphaned files discovered.</p>
                </div>
            )}
        </div>
    );
};

export default Orphans;
