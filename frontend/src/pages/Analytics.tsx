import React, { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, AreaChart, Area } from 'recharts';
import { Layers, Activity, Cpu } from 'lucide-react';

const Analytics = () => {
    const { sendCommand, subscribe } = useEngine();
    const [monthlyData, setMonthlyData] = useState<{ month: string, size: number }[]>([]);
    const [rangeResult, setRangeResult] = useState<{size: string, complexity: string, hops: string} | null>(null);
    const [startMonth, setStartMonth] = useState('1');
    const [endMonth, setEndMonth] = useState('12');

    useEffect(() => {
        let rawDays: number[] = new Array(366).fill(0);
        
        const unsub = subscribe('analytics', (line, type) => {
            if (line.startsWith('RESULT|ANALYTICS')) {
                const parts = line.split('|');
                const day = parseInt(parts[2], 10);
                const size = parseInt(parts[3], 10);
                if (day > 0 && day <= 366) {
                    rawDays[day-1] = size;
                }
            } else if (line === 'ANALYTICS_DONE') {
                // Group into 12 months for chart
                const formatted = [];
                const monthNames = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
                for (let m = 0; m < 12; m++) {
                    const block = rawDays.slice(m * 30, (m + 1) * 30);
                    const sum = block.reduce((a,b) => a+b, 0);
                    formatted.push({ month: monthNames[m], size: sum });
                }
                setMonthlyData(formatted);
            }
            else if (line.startsWith('RESULT|DATE_RANGE')) {
                const parts = line.split('|');
                setRangeResult({
                    size: parts[4] || '0',
                    complexity: 'Segment Tree O(log M)',
                    hops: parts[7] || '0'
                });
            }
        });

        // Fetch overall analytics mapping on mount
        sendCommand('ANALYTICS', 'analytics');

        return () => unsub();
    }, [subscribe, sendCommand]);

    const calculateDelta = () => {
        setRangeResult(null);
        sendCommand(`DATE_RANGE ${startMonth} ${endMonth}`, 'analytics');
    };

    return (
        <div className="flex flex-col h-full gap-6">
            <header className="flex justify-between items-end">
                <div>
                    <h1 className="font-h1 text-on-surface">Storage Segment Analytics</h1>
                    <p className="text-on-surface-variant font-body-md mt-1">
                        Visualize file system ingestion across temporal domains resolved via Segment Trees.
                    </p>
                </div>
                <button 
                    onClick={() => sendCommand('ANALYTICS', 'analytics')}
                    className="bg-surface-container border border-outline-variant/30 text-on-surface px-4 py-2 rounded-md font-label-md tracking-wider hover:bg-surface-variant shadow-sm flex items-center gap-2"
                >
                    <Activity className="w-4 h-4"/> REFRESH SERIES
                </button>
            </header>

            <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                
                {/* Chart Block */}
                <div className="lg:col-span-2 bg-surface-container-lowest p-6 rounded-xl border border-outline-variant/20 shadow-sm flex flex-col">
                    <h3 className="font-h3 text-on-surface mb-6 flex items-center gap-2">
                        <Layers className="text-primary w-5 h-5"/> Temporal Growth Nodes
                    </h3>
                    
                    <div className="flex-1 min-h-[300px]">
                        <ResponsiveContainer width="100%" height="100%">
                            <AreaChart data={monthlyData}>
                                <defs>
                                    <linearGradient id="colorSize" x1="0" y1="0" x2="0" y2="1">
                                    <stop offset="5%" stopColor="#4f46e5" stopOpacity={0.3}/>
                                    <stop offset="95%" stopColor="#4f46e5" stopOpacity={0}/>
                                    </linearGradient>
                                </defs>
                                <CartesianGrid strokeDasharray="3 3" stroke="#e5eeff" vertical={false} />
                                <XAxis dataKey="month" axisLine={false} tickLine={false} tick={{fill: '#777587', fontSize: 12}} dy={10} />
                                <YAxis axisLine={false} tickLine={false} tick={{fill: '#777587', fontSize: 12}} dx={-10} tickFormatter={(val) => `${val/1024}MB`} />
                                <Tooltip 
                                    contentStyle={{backgroundColor: '#ffffff', borderRadius: '8px', border: '1px solid #e5eeff', boxShadow: '0 4px 15px rgba(0,0,0,0.05)'}}
                                    itemStyle={{color: '#4f46e5', fontWeight: 600, fontFamily: '"Public Sans", sans-serif'}}
                                    formatter={(value) => [`${value} KB`, 'Volume']}
                                />
                                <Area type="monotone" dataKey="size" stroke="#4f46e5" strokeWidth={3} fillOpacity={1} fill="url(#colorSize)" />
                            </AreaChart>
                        </ResponsiveContainer>
                    </div>
                </div>

                {/* Range Query Tool */}
                <div className="bg-surface-container-lowest p-6 rounded-xl border border-outline-variant/20 shadow-sm flex flex-col gap-4">
                    <h3 className="font-h3 text-on-surface mb-2 flex items-center gap-2">
                        <Cpu className="text-secondary w-5 h-5"/> Range Extractor
                    </h3>
                    <p className="text-xs text-on-surface-variant font-sans">
                        Query the segment tree to compute aggregations in strict logarithmic time O(log M).
                    </p>

                    <div className="grid grid-cols-2 gap-4 mt-2">
                        <div className="flex flex-col gap-1">
                            <label className="text-[10px] font-mono text-outline uppercase tracking-widest">Start Interval</label>
                            <select value={startMonth} onChange={e => setStartMonth(e.target.value)} className="bg-surface-container border border-outline-variant/30 rounded-md p-2 text-sm text-primary font-mono outline-none focus:border-primary">
                                {["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"].map((m, i) => <option key={i+1} value={i+1}>{m}</option>)}
                            </select>
                        </div>
                        <div className="flex flex-col gap-1">
                            <label className="text-[10px] font-mono text-outline uppercase tracking-widest">End Interval</label>
                            <select value={endMonth} onChange={e => setEndMonth(e.target.value)} className="bg-surface-container border border-outline-variant/30 rounded-md p-2 text-sm text-primary font-mono outline-none focus:border-primary">
                                {["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"].map((m, i) => <option key={i+1} value={i+1}>{m}</option>)}
                            </select>
                        </div>
                    </div>

                    <button 
                        onClick={calculateDelta}
                        className="w-full bg-secondary text-on-secondary py-2 rounded-md font-label-md tracking-wider hover:bg-secondary-container hover:text-on-secondary-container transition-colors mt-2"
                    >
                        PULL AGGREGATE DELTA
                    </button>

                    {rangeResult && (
                        <div className="mt-4 p-4 bg-surface-container-low border border-primary/20 rounded-lg">
                            <div className="text-xs font-mono text-outline uppercase tracking-tight mb-1">Compute Resolution</div>
                            <div className="text-2xl font-bold text-on-surface">{rangeResult.size} KB</div>
                            
                            <div className="flex items-center gap-3 mt-3 pt-3 border-t border-outline-variant/20">
                                <div className="text-[10px] font-mono text-on-surface-variant"><span className="text-primary font-bold">ROUTE: </span>{rangeResult.complexity}</div>
                                <div className="text-[10px] font-mono text-on-surface-variant"><span className="text-primary font-bold">HOPS: </span>{rangeResult.hops}</div>
                            </div>
                        </div>
                    )}
                </div>

            </div>
        </div>
    );
};

export default Analytics;
