import { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { Copy, Trash2, Zap, Download, FileWarning } from 'lucide-react';

interface ReclaimAction {
    type: 'DUPLICATES' | 'OLD_FILES' | 'LARGE_FILE';
    label: string;
    size: number;
    path?: string;
    id?: number;
}

interface DuplicateGroup {
    name: string;
    hash: string;
    sizeKB: number;
    paths: { path: string; id: number }[];
}

const Duplicates = () => {
    const { sendCommand, subscribe } = useEngine();
    const [reclaimActions, setReclaimActions] = useState<ReclaimAction[]>([]);
    const [duplicateGroups, setDuplicateGroups] = useState<DuplicateGroup[]>([]);
    const [totalReclaimable, setTotalReclaimable] = useState(0);
    const [loading, setLoading] = useState(false);

    useEffect(() => {
        const unsub = subscribe('duplicates', (line) => {
            if (line.startsWith('REC|')) {
                const parts = line.split('|');
                const type = parts[1] as any;
                if (type === 'LARGE_FILE') {
                    setReclaimActions(prev => [...prev, {
                        type,
                        label: parts[2],
                        size: parseInt(parts[3]),
                        path: parts[4]
                    }]);
                } else {
                    setReclaimActions(prev => [...prev, {
                        type,
                        label: type === 'DUPLICATES' ? 'Duplicate Content' : 'Files older than 1 year',
                        size: parseInt(parts[2])
                    }]);
                }
            }
            if (line.startsWith('RECLAIM_DONE')) {
                const total = parseInt(line.split('|')[1]);
                setTotalReclaimable(total);
                setLoading(false);
            }
            if (line.startsWith('DUP|')) {
                const parts = line.split('|');
                // E-6: Parse path:id format
                const rawPaths = parts[3].split(',').filter(p => p);
                const parsedPaths = rawPaths.map(entry => {
                    const colonIdx = entry.lastIndexOf(':');
                    if (colonIdx > 0) {
                        return { path: entry.substring(0, colonIdx), id: parseInt(entry.substring(colonIdx + 1)) };
                    }
                    return { path: entry, id: -1 };
                });
                setDuplicateGroups(prev => [...prev, {
                    name: parts[1],
                    hash: parts[2],
                    paths: parsedPaths,
                    sizeKB: parseInt(parts[4])
                }]);
            }
        });

        // Initial fetch
        refresh();
        return () => unsub();
    }, [subscribe]);

    // E-6: Delete a specific duplicate file
    const handleDeleteDup = (fileId: number, groupIdx: number) => {
        if (fileId < 0) return;
        if (confirm('Delete this duplicate file permanently?')) {
            sendCommand(`DELETE ${fileId}`, 'duplicates');
            setDuplicateGroups(prev => prev.map((g, i) => 
                i === groupIdx ? { ...g, paths: g.paths.filter(p => p.id !== fileId) } : g
            ).filter(g => g.paths.length > 1));
        }
    };

    const refresh = () => {
        setLoading(true);
        setReclaimActions([]);
        setDuplicateGroups([]);
        sendCommand('RECLAIM', 'duplicates');
        sendCommand('DUPLICATES', 'duplicates');
    };

    const downloadCSV = () => {
        const headers = ["Name", "Hash", "Size (KB)", "Path"];
        const rows = duplicateGroups.flatMap(g => 
            g.paths.map(p => [g.name, g.hash, g.sizeKB.toString(), p.path])
        );
        
        const csvContent = [headers, ...rows].map(e => e.join(",")).join("\n");
        const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.setAttribute("href", url);
        link.setAttribute("download", `duplicates_${new Date().toISOString().split('T')[0]}.csv`);
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    };

    const formatMB = (bytes: number) => (bytes / 1048576).toFixed(1) + ' MB';

    return (
        <div className="flex flex-col h-full gap-8">
            <header className="flex justify-between items-start">
                <div>
                    <h1 className="text-2xl font-bold text-on-surface">Free Up Space</h1>
                    <p className="text-on-surface-variant text-sm mt-1">Optimize your storage by removing redundant or obsolete files.</p>
                </div>
                <div className="flex gap-3">
                    <button 
                        onClick={downloadCSV}
                        disabled={duplicateGroups.length === 0}
                        className="flex items-center gap-2 bg-surface-container-high text-on-surface px-4 py-2.5 rounded-xl font-bold hover:bg-surface-container-highest transition-all border border-outline-variant/20 disabled:opacity-50"
                    >
                        <Download className="w-4 h-4" />
                        EXPORT CSV
                    </button>
                    <button 
                        onClick={refresh}
                        disabled={loading}
                        className="flex items-center gap-2 bg-primary text-on-primary px-5 py-2.5 rounded-xl font-bold hover:brightness-110 transition-all shadow-md disabled:opacity-50"
                    >
                        <Zap className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
                        {loading ? 'ANALYZING...' : 'REFRESH ADVISOR'}
                    </button>
                </div>
            </header>

            {/* Reclaim advisor summary */}
            <div className="bg-primary/5 border border-primary/20 rounded-3xl p-8 flex flex-col md:flex-row items-center gap-8 shadow-sm">
                <div className="flex-1 text-center md:text-left">
                    <p className="text-xs font-black text-primary uppercase tracking-widest mb-2">Total Reclaimable Space</p>
                    <h2 className="text-5xl font-black text-on-surface">
                        {formatMB(totalReclaimable)}
                    </h2>
                    <p className="text-on-surface-variant mt-4 text-sm leading-relaxed max-w-md">
                        We've identified potential savings across duplicates and old files. Cleaning these up will improve your system performance.
                    </p>
                </div>
            </div>

            <div className="grid grid-cols-1 lg:grid-cols-2 gap-8 flex-1 overflow-hidden">
                {/* Advisor List */}
                <section className="flex flex-col gap-4 overflow-hidden">
                    <h3 className="text-sm font-black text-outline uppercase tracking-widest flex items-center gap-2">
                        <Zap className="w-4 h-4 text-primary" /> Reclaim Advisor
                    </h3>
                    <div className="flex-1 overflow-auto space-y-3 custom-scrollbar pr-2">
                        {reclaimActions.filter(a => a.type === 'LARGE_FILE').map((a, i) => (
                            <div key={i} className="bg-surface-container-lowest border border-outline-variant/10 p-4 rounded-2xl flex justify-between items-center group hover:border-primary/20 transition-all">
                                <div className="flex items-center gap-4 overflow-hidden">
                                    <div className="p-3 bg-error/5 text-error rounded-xl">
                                        <FileWarning className="w-5 h-5" />
                                    </div>
                                    <div className="overflow-hidden">
                                        <p className="text-xs font-bold text-error uppercase mb-0.5 tracking-tight">Large File Found</p>
                                        <h4 className="font-bold text-sm text-on-surface truncate">{a.label}</h4>
                                        <p className="text-[10px] text-on-surface-variant font-mono truncate">{a.path}</p>
                                    </div>
                                </div>
                                <div className="text-right flex-shrink-0 ml-4">
                                    <p className="text-sm font-black text-on-surface">{formatMB(a.size)}</p>
                                    <button className="text-[10px] font-black text-error hover:underline mt-1">DELETE</button>
                                </div>
                            </div>
                        ))}
                    </div>
                </section>

                {/* Duplicate Groups */}
                <section className="flex flex-col gap-4 overflow-hidden">
                    <h3 className="text-sm font-black text-outline uppercase tracking-widest flex items-center gap-2">
                        <Copy className="w-4 h-4 text-secondary" /> Duplicate Groups
                    </h3>
                    <div className="flex-1 overflow-auto space-y-4 custom-scrollbar pr-2">
                        {duplicateGroups.map((g, i) => (
                            <div key={i} className="bg-surface-container-lowest border border-outline-variant/10 rounded-2xl overflow-hidden shadow-sm hover:shadow-md transition-all">
                                <div className="bg-surface-container-low px-4 py-3 flex justify-between items-center border-b border-outline-variant/5">
                                    <div className="flex items-center gap-3">
                                        <h4 className="text-xs font-black text-on-surface truncate max-w-[200px]">{g.name}</h4>
                                        <span className="text-[10px] font-mono bg-outline-variant/20 px-1.5 py-0.5 rounded text-outline">{g.hash.substring(0,6)}</span>
                                    </div>
                                    <span className="text-[10px] font-black text-secondary bg-secondary/10 px-2 py-0.5 rounded-full">{g.paths.length} COPIES</span>
                                </div>
                                <div className="divide-y divide-outline-variant/5">
                                    {g.paths.map((p, j) => (
                                        <div key={j} className="px-4 py-2.5 flex justify-between items-center group/item hover:bg-primary/5 transition-colors">
                                            <p className="text-[10px] text-on-surface-variant font-mono truncate flex-1">{p.path}</p>
                                            <button 
                                                onClick={() => handleDeleteDup(p.id, i)}
                                                className="p-1.5 text-outline-variant hover:text-error transition-colors opacity-0 group-hover/item:opacity-100"
                                            >
                                                <Trash2 className="w-3.5 h-3.5" />
                                            </button>
                                        </div>
                                    ))}
                                </div>
                            </div>
                        ))}
                    </div>
                </section>
            </div>
        </div>
    );
};

export default Duplicates;
