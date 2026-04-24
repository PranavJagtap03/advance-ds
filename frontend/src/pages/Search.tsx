import React, { useState, useEffect } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { Search as SearchIcon, FileDigit, Cpu, Timer, ShieldAlert } from 'lucide-react';

interface SearchResult {
    name: string;
    path: string;
    sizeKB: string;
    reason?: string;
    dsName?: string;
}

const Search = () => {
    const { sendCommand, subscribe } = useEngine();
    const [query, setQuery] = useState('');
    const [results, setResults] = useState<SearchResult[]>([]);
    const [lastQueryData, setLastQueryData] = useState<{type: string, ds: string, queryStr: string} | null>(null);

    useEffect(() => {
        const unsub = subscribe('search', (line, type) => {
            if (line.startsWith('RESULT|FOUND')) {
                const parts = line.split('|');
                setResults(prev => [...prev, {
                    name: parts[2] || 'Unknown',
                    path: parts[3] || 'Unknown',
                    sizeKB: parts[4] || '0',
                    dsName: parts[5] || 'B+ Tree'
                }]);
            } else if (line.startsWith('RESULT|PREFIX')) {
                const parts = line.split('|');
                const namesString = parts[2] || '';
                const dsName = parts[5] || 'Trie Node';
                
                if (namesString) {
                    const namesList = namesString.split(',').filter(n => n.trim().length > 0);
                    const parsedResults = namesList.map(n => ({
                        name: n,
                        path: 'Multiple possible paths via Trie',
                        sizeKB: '--',
                        dsName: dsName
                    }));
                    setResults(prev => [...prev, ...parsedResults]);
                }
            } else if (line.startsWith('RESULT|NOT_FOUND')) {
                const parts = line.split('|');
                setResults([{
                    name: parts[2] || 'Missing File',
                    path: '',
                    sizeKB: '0',
                    reason: parts[8] || 'File not present in active tree.',
                    dsName: parts[5] || 'B+ Tree'
                }]);
            }
        });

        return () => unsub();
    }, [subscribe]);

    const executeSearch = (e: React.FormEvent) => {
        e.preventDefault();
        const q = query.trim();
        if (!q) return;

        setResults([]);
        
        if (q.endsWith('*')) {
            setLastQueryData({ type: 'Prefix Match via Trie', ds: 'Trie Node O(L)', queryStr: q });
            sendCommand(`PREFIX ${q.slice(0, -1)}`, 'search');
        } else {
            setLastQueryData({ type: 'Exact Match via B+ Tree', ds: 'B+ Tree O(log N)', queryStr: q });
            sendCommand(`SEARCH ${q}`, 'search');
        }
    };

    return (
        <div className="flex flex-col h-full gap-6">
            <header>
                <h1 className="font-h1 text-on-surface">File Search</h1>
                <p className="text-on-surface-variant font-body-md mt-1">
                    Locate items instantly using algorithmic indexing. Append an asterisk (*) for prefix searches.
                </p>
            </header>

            <form onSubmit={executeSearch} className="flex relative items-center max-w-2xl bg-surface-container-lowest border border-outline-variant/30 rounded-xl overflow-hidden shadow-sm focus-within:ring-2 focus-within:ring-primary/20 transition-all">
                <SearchIcon className="absolute left-4 w-5 h-5 text-outline" />
                <input 
                    type="text" 
                    value={query}
                    onChange={(e) => setQuery(e.target.value)}
                    placeholder="Search by exact file name or prefix*..."
                    className="flex-1 w-full py-4 pl-12 pr-4 bg-transparent outline-none font-sans text-on-surface"
                />
                <button type="submit" className="bg-primary text-on-primary px-6 py-4 font-label-md tracking-wider hover:bg-primary-container hover:text-on-primary-container transition-colors">
                    EXECUTE
                </button>
            </form>

            {lastQueryData && (
                <div className="flex items-center gap-4 text-xs font-mono">
                    <span className="px-2 py-1 bg-surface-container rounded-md flex items-center gap-2 text-on-surface-variant">
                        <Cpu className="w-4 h-4 text-primary" /> {lastQueryData.ds}
                    </span>
                    <span className="px-2 py-1 bg-surface-container rounded-md flex items-center gap-2 text-on-surface-variant">
                        <Timer className="w-4 h-4 text-secondary" /> {lastQueryData.type}
                    </span>
                </div>
            )}

            <div className="flex-1 overflow-auto -mx-2 px-2 custom-scrollbar space-y-3">
                {results.length === 0 && lastQueryData && (
                    <div className="text-sm font-mono text-outline-variant text-center my-12">Waiting for engine response...</div>
                )}
                
                {results.map((r, i) => (
                    <div key={i} className={`bg-surface-container-lowest border ${r.reason ? 'border-error/20' : 'border-outline-variant/20 hover:border-primary/30'} p-4 rounded-xl flex justify-between items-center transition-all bg-white shadow-sm`}>
                        <div className="flex items-center gap-4">
                            <div className={`p-3 rounded-md ${r.reason ? 'bg-error-container text-on-error-container' : 'bg-surface-container text-primary'}`}>
                                {r.reason ? <ShieldAlert className="w-6 h-6" /> : <FileDigit className="w-6 h-6" />}
                            </div>
                            <div>
                                <h4 className="font-h3 text-on-surface">{r.name}</h4>
                                {r.path && <p className="text-xs text-on-surface-variant font-mono mt-1 w-96 truncate" title={r.path}>{r.path}</p>}
                                {r.reason && <p className="text-xs text-error mt-1">{r.reason}</p>}
                            </div>
                        </div>
                        <div className="text-right">
                            <div className="text-xs font-mono text-outline uppercase tracking-tight mb-1">SIZE</div>
                            <div className="font-bold text-on-surface">{r.sizeKB} KB</div>
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};

export default Search;
