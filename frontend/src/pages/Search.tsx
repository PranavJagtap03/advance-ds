import React, { useState, useEffect, useMemo } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { Search as SearchIcon, FileText, ImageIcon, FileCode, Film, File, Filter, X } from 'lucide-react';

interface SearchResult {
    name: string;
    path: string;
    sizeKB: string;
    modified?: string;
    type: string;
}

const EXTENSION_MAP: Record<string, string> = {
    'jpg': 'Images', 'jpeg': 'Images', 'png': 'Images', 'gif': 'Images', 'svg': 'Images',
    'pdf': 'Documents', 'doc': 'Documents', 'docx': 'Documents', 'txt': 'Documents', 'md': 'Documents',
    'mp4': 'Videos', 'mkv': 'Videos', 'avi': 'Videos', 'mov': 'Videos',
    'cpp': 'Code', 'h': 'Code', 'py': 'Code', 'js': 'Code', 'ts': 'Code', 'tsx': 'Code', 'html': 'Code', 'css': 'Code'
};

const Search = () => {
    const { sendCommand, subscribe } = useEngine();
    const [query, setQuery] = useState('');
    const [results, setResults] = useState<SearchResult[]>([]);
    const [activeFilter, setActiveFilter] = useState('All');
    const [searching, setSearching] = useState(false);
    const [useRegex, setUseRegex] = useState(false);

    useEffect(() => {
        const unsub = subscribe('search', (line) => {
            if (line.startsWith('RESULT|FOUND')) {
                const parts = line.split('|');
                const name = parts[2] || '';
                const ext = name.split('.').pop()?.toLowerCase() || '';
                setResults(prev => [...prev, {
                    name,
                    path: parts[3] || '',
                    sizeKB: parts[4] || '0',
                    type: EXTENSION_MAP[ext] || 'Other'
                }]);
                setSearching(false);
            } else if (line.startsWith('RESULT|PREFIX')) {
                const parts = line.split('|');
                const namesString = parts[2] || '';
                if (namesString) {
                    const namesList = namesString.split(',').filter(n => n.trim().length > 0);
                    const parsedResults = namesList.map(n => {
                        const ext = n.split('.').pop()?.toLowerCase() || '';
                        return {
                            name: n,
                            path: 'Multiple paths indexed',
                            sizeKB: '--',
                            type: EXTENSION_MAP[ext] || 'Other'
                        };
                    });
                    setResults(prev => [...prev, ...parsedResults]);
                }
                setSearching(false);
            } else if (line.startsWith('RESULT|NOT_FOUND')) {
                setSearching(false);
            }
        });
        return () => unsub();
    }, [subscribe]);

    const executeSearch = (e: React.FormEvent) => {
        e.preventDefault();
        const q = query.trim();
        if (!q) return;
        setResults([]);
        setSearching(true);
        
        if (useRegex) {
            sendCommand(`REGEX ${q}`, 'search');
        } else if (q.endsWith('*')) {
            sendCommand(`PREFIX ${q.slice(0, -1)}`, 'search');
        } else {
            sendCommand(`SEARCH ${q}`, 'search');
        }
    };

    const filteredResults = useMemo(() => {
        if (activeFilter === 'All') return results;
        return results.filter(r => r.type === activeFilter);
    }, [results, activeFilter]);

    const getIcon = (type: string) => {
        switch(type) {
            case 'Images': return <ImageIcon className="w-5 h-5 text-blue-500" />;
            case 'Videos': return <Film className="w-5 h-5 text-purple-500" />;
            case 'Code': return <FileCode className="w-5 h-5 text-orange-500" />;
            case 'Documents': return <FileText className="w-5 h-5 text-green-500" />;
            default: return <File className="w-5 h-5 text-slate-400" />;
        }
    };

    return (
        <div className="flex flex-col h-full gap-6">
            <header>
                <h1 className="font-h1 text-on-surface text-2xl font-bold">File Search</h1>
                <p className="text-on-surface-variant text-sm mt-1">Locate any file instantly. Use * for prefix matches, or enable REGEX for pattern search.</p>
            </header>

            <div className="flex flex-col gap-4 max-w-4xl">
                <form onSubmit={executeSearch} className="flex gap-3">
                    <div className="flex-1 relative group">
                        <SearchIcon className="absolute left-4 top-1/2 -translate-y-1/2 w-5 h-5 text-outline transition-colors group-focus-within:text-primary" />
                        <input 
                            type="text" 
                            value={query}
                            onChange={(e) => setQuery(e.target.value)}
                            placeholder={useRegex ? "Enter regex pattern (e.g. .*\\.exe)..." : "Enter filename..."}
                            className="w-full bg-surface-container-lowest border border-outline-variant/30 rounded-2xl py-4 pl-12 pr-4 outline-none focus:ring-2 focus:ring-primary/20 focus:border-primary transition-all shadow-sm"
                        />
                        <div className="absolute right-4 top-1/2 -translate-y-1/2 flex items-center gap-2">
                            {query && (
                                <button type="button" onClick={() => setQuery('')} className="p-1 hover:bg-surface-container rounded-full">
                                    <X className="w-4 h-4 text-outline" />
                                </button>
                            )}
                            <button 
                                type="button" 
                                onClick={() => setUseRegex(!useRegex)}
                                className={`px-3 py-1.5 rounded-xl text-[10px] font-black transition-all border ${useRegex ? 'bg-secondary text-on-secondary border-secondary shadow-md' : 'bg-surface-container-high text-on-surface-variant border-outline-variant/20 hover:border-outline-variant'}`}
                            >
                                REGEX
                            </button>
                        </div>
                    </div>
                    <button type="submit" className="bg-primary text-on-primary px-8 rounded-2xl font-bold hover:brightness-110 active:scale-95 transition-all shadow-lg">
                        SEARCH
                    </button>
                </form>

                <div className="flex items-center gap-2 overflow-x-auto pb-2 no-scrollbar">
                    <Filter className="w-4 h-4 text-outline-variant mr-2" />
                    {['All', 'Images', 'Documents', 'Videos', 'Code', 'Other'].map(f => (
                        <button 
                            key={f}
                            onClick={() => setActiveFilter(f)}
                            className={`px-4 py-1.5 rounded-full text-xs font-bold transition-all whitespace-nowrap border ${activeFilter === f ? 'bg-primary text-on-primary border-primary shadow-md' : 'bg-surface-container-low text-on-surface-variant border-outline-variant/20 hover:border-outline-variant'}`}
                        >
                            {f}
                        </button>
                    ))}
                </div>
            </div>

            <div className="flex-1 overflow-auto -mx-2 px-2 space-y-3 custom-scrollbar">
                {searching && <div className="text-center py-12 text-outline-variant font-medium animate-pulse">Searching library...</div>}
                {!searching && results.length === 0 && query && <div className="text-center py-12 text-outline-variant">No files found matching your search.</div>}
                
                {filteredResults.map((r, i) => (
                    <div key={i} className="bg-surface-container-lowest border border-outline-variant/10 p-5 rounded-2xl flex justify-between items-center hover:border-primary/20 hover:shadow-md transition-all group">
                        <div className="flex items-center gap-5 overflow-hidden">
                            <div className="p-3 bg-surface-container-high rounded-xl group-hover:scale-110 transition-transform shadow-sm">
                                {getIcon(r.type)}
                            </div>
                            <div className="overflow-hidden">
                                <h4 className="font-bold text-on-surface truncate">{r.name}</h4>
                                <p className="text-xs text-on-surface-variant font-mono mt-1 truncate max-w-lg opacity-70 group-hover:opacity-100 transition-opacity">{r.path}</p>
                            </div>
                        </div>
                        <div className="flex items-center gap-8 text-right flex-shrink-0">
                            <div className="hidden md:block">
                                <p className="text-[10px] font-bold text-outline uppercase tracking-wider mb-0.5">TYPE</p>
                                <p className="text-xs font-bold text-on-surface-variant">{r.type}</p>
                            </div>
                            <div>
                                <p className="text-[10px] font-bold text-outline uppercase tracking-wider mb-0.5">SIZE</p>
                                <p className="text-sm font-black text-on-surface">{r.sizeKB} {r.sizeKB !== '--' && 'KB'}</p>
                            </div>
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};

export default Search;
