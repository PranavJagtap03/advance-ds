import { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { HardDrive, AlertTriangle, ShieldCheck, Database, LayoutGrid, PieChart as PieChartIcon } from 'lucide-react';
import { PieChart, Pie, Cell, ResponsiveContainer, Tooltip } from 'recharts';

const COLORS = ['#4f46e5', '#10b981', '#f59e0b', '#ef4444', '#8b5cf6', '#ec4899'];

const Dashboard = () => {
    const { status, scanCount, diskInfo, warnings, sendCommand, subscribe, clearWarnings } = useEngine();
    const [scanPath, setScanPath] = useState('');
    const [typeData, setTypeData] = useState<{ name: string, value: number }[]>([]);
    const [suspiciousCount, setSuspiciousCount] = useState(0);

    useEffect(() => {
        let rawTypes: { name: string, value: number }[] = [];
        const unsub = subscribe('dashboard', (line) => {
            if (line.startsWith('TYPE|')) {
                const parts = line.split('|');
                rawTypes.push({ name: parts[1], value: parseInt(parts[2]) });
            } else if (line.startsWith('TYPE_DONE')) {
                setTypeData([...rawTypes]);
            } else if (line.startsWith('SUSPICIOUS_DONE|')) {
                setSuspiciousCount(parseInt(line.split('|')[1]));
            }
        });

        if (status === 'IDLE' && scanCount > 0) {
            sendCommand('TYPE_DISTRIBUTION', 'dashboard');
            sendCommand('SUSPICIOUS', 'dashboard');
        }

        return () => unsub();
    }, [subscribe, status, scanCount, sendCommand]);

    const handleScan = () => {
        if(scanPath) {
            clearWarnings();
            sendCommand(`LOAD_DIR ${scanPath}`, 'explorer');
        }
    };

    const formatBytes = (bytes: number) => {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    };

    const calculateWaste = () => {
        if (!warnings || warnings.length === 0) return '0 MB';
        let total = 0;
        warnings.forEach(w => {
            const match = w.message.match(/(\d+)\s+MB/);
            if (match) total += parseInt(match[1]);
        });
        return total > 0 ? `${total} MB` : '0 MB';
    };

    return (
        <div className="flex flex-col gap-6">
            <header className="mb-4">
                <h1 className="font-h1 text-on-surface">Storage Overview</h1>
                <p className="text-on-surface-variant font-body-md mt-2">Monitor system capacity, scan progress, and storage health.</p>
            </header>

            {/* Security Alerts */}
            {suspiciousCount > 0 && (
                <div className="bg-amber-500/10 border border-amber-500/20 text-amber-600 p-4 rounded-xl flex items-center gap-4 shadow-sm animate-pulse">
                    <ShieldCheck className="w-6 h-6 flex-shrink-0" />
                    <div className="flex-1">
                        <p className="font-bold text-sm uppercase">Security Alert</p>
                        <p className="text-sm opacity-90">{suspiciousCount} high-entropy files detected. These may be encrypted or represent a security risk.</p>
                    </div>
                </div>
            )}

            {/* Smart Warnings */}
            {warnings.length > 0 && (
                <div className="flex flex-col gap-3">
                    {warnings.map((warn, idx) => (
                        <div key={idx} className="bg-error-container text-on-error-container p-4 rounded-lg flex items-center gap-4 shadow-sm border border-error/20">
                            <AlertTriangle className="w-6 h-6 flex-shrink-0" />
                            <div className="flex-1">
                                <p className="font-bold text-sm uppercase tracking-wider">{warn.type}</p>
                                <p className="text-sm opacity-90">{warn.message}</p>
                            </div>
                            <button onClick={clearWarnings} className="text-xs font-bold hover:underline">DISMISS</button>
                        </div>
                    ))}
                </div>
            )}

            {/* Main Scan Action & Type Distribution */}
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                <div className="lg:col-span-2 bg-surface-container-lowest p-8 rounded-2xl shadow-xl border border-outline-variant/10">
                    <div className="flex flex-col md:flex-row items-center gap-8">
                        <div className="flex-1">
                            <h3 className="text-xl font-bold text-on-surface mb-2 flex items-center gap-2">
                                <ShieldCheck className="text-primary w-6 h-6"/> Local Indexing
                            </h3>
                            <p className="text-on-surface-variant mb-6 text-sm leading-relaxed">
                                Analyze your local storage to identify duplicates, large folders, and recovery opportunities. 
                                Data remains strictly on your device.
                            </p>
                            <div className="flex gap-3">
                                <input 
                                    type="text" 
                                    value={scanPath}
                                    onChange={(e) => setScanPath(e.target.value)}
                                    placeholder="Enter folder path"
                                    className="flex-1 bg-surface border border-outline-variant/30 rounded-xl px-4 py-3 text-sm focus:outline-none focus:ring-2 focus:ring-primary/20 transition-all shadow-inner"
                                />
                                <button 
                                    onClick={handleScan}
                                    disabled={status === 'SCANNING'}
                                    className="bg-primary text-on-primary px-8 py-3 rounded-xl font-bold tracking-tight hover:brightness-110 active:scale-95 transition-all shadow-lg disabled:opacity-50 disabled:scale-100"
                                >
                                    {status === 'SCANNING' ? 'ANALYZING...' : 'ANALYZE'}
                                </button>
                            </div>
                        </div>
                        
                        {diskInfo && (
                            <div className="w-full md:w-64 bg-surface p-5 rounded-xl border border-outline-variant/20 shadow-sm">
                                <div className="flex items-center justify-between mb-3 text-xs font-bold text-outline">
                                    <span>DISK CAPACITY</span>
                                    <HardDrive className="w-4 h-4" />
                                </div>
                                <div className="relative pt-1">
                                    <div className="flex mb-2 items-center justify-between">
                                        <span className="text-2xl font-black text-primary">
                                            {Math.round((1 - diskInfo.free / diskInfo.total) * 100)}%
                                        </span>
                                        <span className="text-xs font-bold text-on-surface-variant">Used</span>
                                    </div>
                                    <div className="overflow-hidden h-3 mb-4 flex rounded-full bg-outline-variant/20 shadow-inner">
                                        <div style={{ width: `${(1 - diskInfo.free / diskInfo.total) * 100}%` }} className="bg-primary transition-all duration-1000"></div>
                                    </div>
                                    <div className="flex justify-between text-[10px] font-bold text-on-surface-variant uppercase tracking-tighter">
                                        <span>FREE: {formatBytes(diskInfo.free)}</span>
                                        <span>TOTAL: {formatBytes(diskInfo.total)}</span>
                                    </div>
                                </div>
                            </div>
                        )}
                    </div>
                </div>

                <div className="bg-surface-container-lowest p-6 rounded-2xl border border-outline-variant/10 shadow-lg flex flex-col">
                    <h3 className="text-xs font-black text-outline uppercase tracking-widest mb-4 flex items-center gap-2">
                        <PieChartIcon className="text-secondary w-4 h-4"/> Content Types
                    </h3>
                    <div className="flex-1 min-h-[160px]">
                        {typeData.length > 0 ? (
                            <ResponsiveContainer width="100%" height="100%">
                                <PieChart>
                                    <Pie
                                        data={typeData}
                                        innerRadius={45}
                                        outerRadius={65}
                                        paddingAngle={5}
                                        dataKey="value"
                                    >
                                        {typeData.map((_, index) => (
                                            <Cell key={`cell-${index}`} fill={COLORS[index % COLORS.length]} />
                                        ))}
                                    </Pie>
                                    <Tooltip 
                                        formatter={(value: any) => formatBytes(value)}
                                        contentStyle={{borderRadius: '12px', border: 'none', boxShadow: '0 4px 12px rgba(0,0,0,0.1)'}}
                                    />
                                </PieChart>
                            </ResponsiveContainer>
                        ) : (
                            <div className="h-full flex items-center justify-center text-outline-variant text-xs italic">
                                Scan a folder to see distribution
                            </div>
                        )}
                    </div>
                </div>
            </div>

            {/* Metrics Grid */}
            <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
                <div className="bg-surface-container-lowest p-6 rounded-xl border border-outline-variant/10 shadow-md flex items-center gap-5 hover:border-primary/30 transition-colors">
                    <div className="p-4 bg-primary/10 rounded-full text-primary">
                        <Database className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-xs font-bold text-on-surface-variant uppercase tracking-widest mb-1">Indexed Files</p>
                        <h4 className="text-2xl font-black text-on-surface">{scanCount.toLocaleString()}</h4>
                    </div>
                </div>

                <div className="bg-surface-container-lowest p-6 rounded-xl border border-outline-variant/10 shadow-md flex items-center gap-5 hover:border-primary/30 transition-colors">
                    <div className="p-4 bg-secondary/10 rounded-full text-secondary">
                        <LayoutGrid className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-xs font-bold text-on-surface-variant uppercase tracking-widest mb-1">Library Size</p>
                        <h4 className="text-2xl font-black text-on-surface">{diskInfo ? formatBytes(diskInfo.indexed) : '--'}</h4>
                    </div>
                </div>

                <div className="bg-surface-container-lowest p-6 rounded-xl border border-outline-variant/10 shadow-md flex items-center gap-5 hover:border-primary/30 transition-colors">
                    <div className="p-4 bg-error/10 rounded-full text-error">
                        <AlertTriangle className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-xs font-bold text-on-surface-variant uppercase tracking-widest mb-1">Potential Waste</p>
                        <h4 className="text-2xl font-black text-on-surface">{calculateWaste()}</h4>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default Dashboard;
