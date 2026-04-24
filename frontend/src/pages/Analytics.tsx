import { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { TrendingUp, FileWarning, BarChart3, Clock } from 'lucide-react';

interface LargeFile {
    name: string;
    size: number;
    path: string;
}

const Analytics = () => {
    const { sendCommand, subscribe } = useEngine();
    const [monthlyData, setMonthlyData] = useState<{ month: string, size: number }[]>([]);
    const [largeFiles, setLargeFiles] = useState<LargeFile[]>([]);
    const [loading, setLoading] = useState(false);

    useEffect(() => {
        let rawMonths: { month: string, size: number }[] = [];
        let files: LargeFile[] = [];
        
        const unsub = subscribe('analytics', (line) => {
            if (line.startsWith('MONTH|')) {
                const parts = line.split('|');
                rawMonths.push({ month: parts[1], size: parseFloat(parts[2]) });
            } 
            else if (line.startsWith('ANALYTICS_DONE')) {
                setMonthlyData([...rawMonths]);
                setLoading(false);
            }
            else if (line.startsWith('REC|LARGE_FILE|')) {
                const parts = line.split('|');
                files.push({
                    name: parts[2],
                    size: parseInt(parts[3]),
                    path: parts[4]
                });
            }
            else if (line.startsWith('RECLAIM_DONE')) {
                setLargeFiles([...files].sort((a,b) => b.size - a.size).slice(0, 5));
            }
        });

        refresh();
        return () => unsub();
    }, [subscribe, sendCommand]);

    const refresh = () => {
        setLoading(true);
        setMonthlyData([]);
        setLargeFiles([]);
        sendCommand('ANALYTICS', 'analytics');
        sendCommand('RECLAIM', 'analytics');
    };

    const formatMB = (bytes: number) => (bytes / 1048576).toFixed(1) + ' MB';

    return (
        <div className="flex flex-col h-full gap-8">
            <header>
                <h1 className="text-2xl font-bold text-on-surface">Storage Trends</h1>
                <p className="text-on-surface-variant text-sm mt-1">
                    Analyze library growth and identify significant space consumers.
                </p>
            </header>

            <div className="grid grid-cols-1 xl:grid-cols-3 gap-8">
                {/* Growth Chart */}
                <div className="xl:col-span-2 bg-surface-container-lowest p-8 rounded-3xl border border-outline-variant/10 shadow-sm flex flex-col">
                    <div className="flex items-center justify-between mb-8">
                        <h3 className="text-sm font-black text-outline uppercase tracking-widest flex items-center gap-2">
                            <TrendingUp className="text-primary w-4 h-4"/> Library Growth
                        </h3>
                        <p className="text-[10px] font-bold text-on-surface-variant px-3 py-1 bg-surface-container rounded-full">STORAGE ADDED PER MONTH</p>
                    </div>
                    
                    <div className="h-[300px] w-full">
                        <ResponsiveContainer width="100%" height="100%">
                            <AreaChart data={monthlyData}>
                                <defs>
                                    <linearGradient id="growthGradient" x1="0" y1="0" x2="0" y2="1">
                                        <stop offset="5%" stopColor="#4f46e5" stopOpacity={0.1}/>
                                        <stop offset="95%" stopColor="#4f46e5" stopOpacity={0}/>
                                    </linearGradient>
                                </defs>
                                <CartesianGrid strokeDasharray="3 3" stroke="#e5eeff" vertical={false} />
                                <XAxis dataKey="month" axisLine={false} tickLine={false} tick={{fill: '#777587', fontSize: 10, fontWeight: 700}} dy={10} />
                                <YAxis axisLine={false} tickLine={false} tick={{fill: '#777587', fontSize: 10, fontWeight: 700}} dx={-10} tickFormatter={(val) => `${val}MB`} />
                                <Tooltip 
                                    contentStyle={{backgroundColor: '#ffffff', borderRadius: '12px', border: '1px solid #e5eeff', boxShadow: '0 8px 30px rgba(0,0,0,0.08)', padding: '12px'}}
                                    labelStyle={{fontWeight: 800, marginBottom: '4px', fontSize: '12px'}}
                                    itemStyle={{color: '#4f46e5', fontWeight: 700, fontSize: '12px'}}
                                    formatter={(value) => [`${value} MB`, 'Storage Added']}
                                />
                                <Area type="monotone" dataKey="size" stroke="#4f46e5" strokeWidth={4} fillOpacity={1} fill="url(#growthGradient)" />
                            </AreaChart>
                        </ResponsiveContainer>
                    </div>
                </div>

                {/* Largest Files */}
                <div className="bg-surface-container-lowest p-8 rounded-3xl border border-outline-variant/10 shadow-sm flex flex-col gap-6">
                    <h3 className="text-sm font-black text-outline uppercase tracking-widest flex items-center gap-2">
                        <BarChart3 className="text-secondary w-4 h-4"/> Largest Files
                    </h3>
                    <div className="flex-1 space-y-4">
                        {largeFiles.length === 0 ? (
                            <div className="text-center py-12 text-outline-variant text-sm">No large files indexed.</div>
                        ) : (
                            largeFiles.map((file, i) => (
                                <div key={i} className="flex flex-col gap-1 p-3 rounded-xl hover:bg-surface-container transition-colors group">
                                    <div className="flex justify-between items-center">
                                        <p className="font-bold text-xs text-on-surface truncate pr-4">{file.name}</p>
                                        <p className="text-xs font-black text-primary">{formatMB(file.size)}</p>
                                    </div>
                                    <p className="text-[10px] text-on-surface-variant font-mono truncate opacity-60 group-hover:opacity-100 transition-opacity">{file.path}</p>
                                    <div className="w-full h-1 bg-surface-container-high rounded-full mt-2 overflow-hidden shadow-inner">
                                        <div style={{ width: `${(file.size / largeFiles[0].size) * 100}%` }} className="h-full bg-secondary/40 rounded-full" />
                                    </div>
                                </div>
                            ))
                        )}
                    </div>
                </div>
            </div>

            {/* Distribution Placeholder */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
                <div className="bg-surface-container-low p-6 rounded-2xl border border-outline-variant/10 flex items-center gap-6">
                    <div className="p-4 bg-primary/10 text-primary rounded-2xl">
                        <Clock className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-[10px] font-black text-outline uppercase tracking-widest mb-1">Average File Age</p>
                        <h4 className="text-xl font-black text-on-surface">1.2 Years</h4>
                    </div>
                </div>
                <div className="bg-surface-container-low p-6 rounded-2xl border border-outline-variant/10 flex items-center gap-6">
                    <div className="p-4 bg-secondary/10 text-secondary rounded-2xl">
                        <FileWarning className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-[10px] font-black text-outline uppercase tracking-widest mb-1">Obsolete Storage</p>
                        <h4 className="text-xl font-black text-on-surface">15% of library</h4>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default Analytics;
