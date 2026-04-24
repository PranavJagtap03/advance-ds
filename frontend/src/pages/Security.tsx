import { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { ShieldAlert, AlertTriangle, Trash2, FileWarning, ShieldCheck } from 'lucide-react';

interface SuspiciousFile {
    name: string;
    path: string;
    entropy: string;
}

const Security = () => {
    const { sendCommand, subscribe, status } = useEngine();
    const [suspiciousFiles, setSuspiciousFiles] = useState<SuspiciousFile[]>([]);
    const [loading, setLoading] = useState(false);

    useEffect(() => {
        const unsub = subscribe('security', (line) => {
            if (line.startsWith('SUSPICIOUS|')) {
                const parts = line.split('|');
                setSuspiciousFiles(prev => [...prev, {
                    name: parts[1],
                    path: parts[2],
                    entropy: parts[3]
                }]);
            } else if (line.startsWith('SUSPICIOUS_DONE')) {
                setLoading(false);
            }
        });

        refresh();
        return () => unsub();
    }, [subscribe]);

    const refresh = () => {
        setLoading(true);
        setSuspiciousFiles([]);
        sendCommand('SUSPICIOUS', 'security');
    };

    return (
        <div className="flex flex-col h-full gap-8">
            <header className="flex justify-between items-start">
                <div>
                    <h1 className="text-2xl font-bold text-on-surface flex items-center gap-2">
                        <ShieldAlert className="text-amber-500 w-7 h-7" /> Security Analysis
                    </h1>
                    <p className="text-on-surface-variant text-sm mt-1">Identify potentially encrypted or high-entropy files that could represent security risks.</p>
                </div>
                <button 
                    onClick={refresh}
                    disabled={loading || status === 'SCANNING'}
                    className="bg-primary text-on-primary px-6 py-2.5 rounded-xl font-bold hover:brightness-110 transition-all shadow-md disabled:opacity-50"
                >
                    {loading ? 'ANALYZING...' : 'RUN SECURITY SCAN'}
                </button>
            </header>

            {suspiciousFiles.length > 0 ? (
                <div className="bg-amber-500/5 border border-amber-500/20 rounded-2xl p-4 flex items-center gap-4 text-amber-700">
                    <AlertTriangle className="w-6 h-6 flex-shrink-0" />
                    <p className="text-sm font-medium">
                        Found {suspiciousFiles.length} high-entropy files. While these can be compressed archives or encrypted volumes, they are also a common characteristic of ransomware-encrypted payloads.
                    </p>
                </div>
            ) : !loading && (
                <div className="bg-green-500/5 border border-green-500/20 rounded-2xl p-8 flex flex-col items-center justify-center text-center">
                    <ShieldCheck className="w-12 h-12 text-green-500 mb-4" />
                    <h3 className="text-lg font-bold text-on-surface">No Threats Detected</h3>
                    <p className="text-on-surface-variant text-sm max-w-md mt-2">All indexed files appear to have normal entropy levels. No suspicious encrypted patterns were identified.</p>
                </div>
            )}

            <div className="flex-1 overflow-auto space-y-3 custom-scrollbar pr-2">
                {suspiciousFiles.map((file, i) => (
                    <div key={i} className="bg-surface-container-lowest border border-outline-variant/10 p-5 rounded-2xl flex justify-between items-center group hover:border-amber-500/30 transition-all">
                        <div className="flex items-center gap-5 overflow-hidden">
                            <div className="p-3 bg-amber-500/10 text-amber-600 rounded-xl">
                                <FileWarning className="w-6 h-6" />
                            </div>
                            <div className="overflow-hidden">
                                <h4 className="font-bold text-on-surface truncate">{file.name}</h4>
                                <p className="text-xs text-on-surface-variant font-mono mt-1 truncate max-w-2xl opacity-70 group-hover:opacity-100 transition-opacity">{file.path}</p>
                            </div>
                        </div>
                        <div className="flex items-center gap-8 text-right flex-shrink-0">
                            <div>
                                <p className="text-[10px] font-bold text-outline uppercase tracking-wider mb-0.5">ENTROPY</p>
                                <p className="text-sm font-black text-amber-600">{file.entropy} <span className="text-[10px] text-outline font-normal">bits</span></p>
                            </div>
                            <button className="p-2 text-outline-variant hover:text-error transition-colors">
                                <Trash2 className="w-5 h-5" />
                            </button>
                        </div>
                    </div>
                ))}
                
                {loading && suspiciousFiles.length === 0 && (
                    <div className="text-center py-20 text-outline-variant italic animate-pulse">
                        Performing deep entropy analysis on indexed files...
                    </div>
                )}
            </div>
        </div>
    );
};

export default Security;
