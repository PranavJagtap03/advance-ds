import React, { useState } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { FolderPlus, Fingerprint, Database, AlertCircle } from 'lucide-react';

const Dashboard = () => {
    const { status, scanCount, sendCommand } = useEngine();
    const [scanPath, setScanPath] = useState('');

    const handleScan = () => {
        if(scanPath) sendCommand(`LOAD_DIR ${scanPath}`, 'explorer');
    };

    return (
        <div className="flex flex-col gap-6">
            <header className="mb-4">
                <h1 className="font-h1 text-on-surface">System Overview</h1>
                <p className="text-on-surface-variant font-body-md mt-2">Manage and inspect your core file system data structures.</p>
            </header>

            {/* Scan Action Card */}
            <div className="bg-surface-container-lowest p-6 rounded-xl shadow-[0_8px_30px_rgba(79,70,229,0.06)] border border-outline-variant/10">
                <h3 className="font-h3 text-on-surface mb-2 flex items-center gap-2">
                    <FolderPlus className="text-primary w-5 h-5"/> Index Configuration
                </h3>
                <p className="text-sm text-on-surface-variant mb-4">Provide an absolute path to a local directory to begin building the B+ Tree and Trie index topologies.</p>
                
                <div className="flex gap-4">
                    <input 
                        type="text" 
                        value={scanPath}
                        onChange={(e) => setScanPath(e.target.value)}
                        placeholder="e.g. C:\Users\HP\Documents"
                        className="flex-1 bg-surface border border-outline-variant/30 rounded-md px-4 py-2 text-sm focus:outline-none focus:border-primary focus:ring-2 focus:ring-primary/20 transition-all font-mono"
                    />
                    <button 
                        onClick={handleScan}
                        disabled={status === 'SCANNING'}
                        className="bg-primary text-on-primary px-6 py-2 rounded-md font-label-md tracking-wider hover:bg-primary-container hover:text-on-primary-container transition-all shadow-[0_4px_14px_rgba(79,70,229,0.3)] disabled:opacity-50"
                    >
                        {status === 'SCANNING' ? 'INDEXING...' : 'BEGIN SCAN'}
                    </button>
                </div>
            </div>

            {/* Stats Grid */}
            <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
                <div className="bg-surface-container-lowest p-6 rounded-xl shadow-[0_8px_30px_rgba(79,70,229,0.06)] border border-outline-variant/10 hover:shadow-[0_8px_30px_rgba(79,70,229,0.12)] transition-shadow cursor-default flex flex-col justify-between">
                    <div>
                        <div className="flex items-center justify-between mb-4">
                            <span className="p-2 bg-surface-container rounded-md inline-block">
                                <Database className="w-5 h-5 text-secondary" />
                            </span>
                            <span className="text-[10px] font-mono text-outline uppercase tracking-widest">B+ Tree Size</span>
                        </div>
                        <h4 className="text-3xl font-bold text-on-surface">{scanCount > 0 ? scanCount : '--'}</h4>
                    </div>
                    <p className="text-xs text-on-surface-variant mt-4">Indexed nodes in primary B+ topology.</p>
                </div>
                
                <div className="bg-surface-container-lowest p-6 rounded-xl shadow-[0_8px_30px_rgba(79,70,229,0.06)] border border-outline-variant/10 hover:shadow-[0_8px_30px_rgba(79,70,229,0.12)] transition-shadow flex flex-col justify-between">
                    <div>
                        <div className="flex items-center justify-between mb-4">
                            <span className="p-2 bg-surface-container rounded-md inline-block">
                                <Fingerprint className="w-5 h-5 text-secondary" />
                            </span>
                            <span className="text-[10px] font-mono text-outline uppercase tracking-widest">Data Routing</span>
                        </div>
                        <h4 className="text-3xl font-bold text-on-surface text-secondary">O(log N)</h4>
                    </div>
                    <p className="text-xs text-on-surface-variant mt-4">Search complexity achieved.</p>
                </div>

                <div className="bg-surface-container-lowest p-6 rounded-xl shadow-[0_8px_30px_rgba(79,70,229,0.06)] border border-outline-variant/10 hover:shadow-[0_8px_30px_rgba(79,70,229,0.12)] transition-shadow flex flex-col justify-between">
                    <div>
                         <div className="flex items-center justify-between mb-4">
                            <span className="p-2 bg-error-container rounded-md inline-block">
                                <AlertCircle className="w-5 h-5 text-error" />
                            </span>
                            <span className="text-[10px] font-mono text-outline uppercase tracking-widest">Cache Status</span>
                        </div>
                        <h4 className="text-xl font-bold text-on-surface">
                             L3 SQLite Active
                        </h4>
                    </div>
                    <p className="text-xs text-on-surface-variant mt-4">Persistent store linked.</p>
                </div>
            </div>
        </div>
    );
};
export default Dashboard;
