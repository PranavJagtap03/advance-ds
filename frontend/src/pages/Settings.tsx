import { useState } from 'react';
import { useEngine } from '../contexts/EngineContext';
import { FolderPlus, RefreshCw, Trash2, Info, HardDrive, Shield } from 'lucide-react';

const Settings = () => {
    const { scanCount, diskInfo, sendCommand } = useEngine();
    const [newPath, setNewPath] = useState('');

    const handleAdd = () => {
        if(newPath) {
            sendCommand(`LOAD_DIR ${newPath}`, 'explorer');
            setNewPath('');
        }
    };

    return (
        <div className="flex flex-col h-full gap-8">
            <header>
                <h1 className="text-2xl font-bold text-on-surface">Scan & Configure</h1>
                <p className="text-on-surface-variant text-sm mt-1">Manage your indexed libraries and system preferences.</p>
            </header>

            <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
                {/* Folder Management */}
                <div className="lg:col-span-2 flex flex-col gap-6">
                    <section className="bg-surface-container-lowest border border-outline-variant/10 rounded-3xl p-8 shadow-sm">
                        <h3 className="text-sm font-black text-outline uppercase tracking-widest flex items-center gap-2 mb-6">
                            <FolderPlus className="w-4 h-4 text-primary" /> Add New Library
                        </h3>
                        <div className="flex gap-3">
                            <input 
                                type="text" 
                                value={newPath}
                                onChange={(e) => setNewPath(e.target.value)}
                                placeholder="Absolute path to folder..."
                                className="flex-1 bg-surface border border-outline-variant/30 rounded-xl px-4 py-3 text-sm focus:ring-2 focus:ring-primary/20 outline-none transition-all shadow-inner"
                            />
                            <button 
                                onClick={handleAdd}
                                className="bg-primary text-on-primary px-8 rounded-xl font-bold hover:brightness-110 active:scale-95 transition-all shadow-md"
                            >
                                INDEX FOLDER
                            </button>
                        </div>
                        <p className="text-[10px] text-on-surface-variant mt-4 font-medium flex items-center gap-1.5 opacity-60">
                            <Info className="w-3 h-3" /> Indexing large folders may take a few minutes depending on your disk speed.
                        </p>
                    </section>

                    <section className="flex flex-col gap-4">
                        <h3 className="text-sm font-black text-outline uppercase tracking-widest flex items-center gap-2 px-2">
                            <HardDrive className="w-4 h-4 text-secondary" /> Indexed Folders
                        </h3>
                        <div className="space-y-3">
                            <div className="bg-surface-container-low border border-outline-variant/10 p-5 rounded-2xl flex justify-between items-center shadow-sm">
                                <div className="flex items-center gap-4 overflow-hidden">
                                    <div className="p-3 bg-surface-container-highest rounded-xl text-primary">
                                        <HardDrive className="w-5 h-5" />
                                    </div>
                                    <div className="overflow-hidden">
                                        <h4 className="font-bold text-sm text-on-surface truncate">Local User Library</h4>
                                        <p className="text-[10px] text-on-surface-variant font-mono truncate opacity-70">C:\Users\HP\Documents</p>
                                    </div>
                                </div>
                                <div className="flex items-center gap-4">
                                    <span className="text-[10px] font-black text-outline bg-outline-variant/20 px-2 py-0.5 rounded-full uppercase">Active</span>
                                    <div className="flex gap-1">
                                        <button className="p-2 text-outline hover:text-primary transition-colors">
                                            <RefreshCw className="w-4 h-4" />
                                        </button>
                                        <button className="p-2 text-outline hover:text-error transition-colors">
                                            <Trash2 className="w-4 h-4" />
                                        </button>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </section>
                </div>

                {/* System Info Sidebar */}
                <div className="flex flex-col gap-6">
                    <section className="bg-surface-container-lowest border border-outline-variant/10 rounded-3xl p-8 shadow-sm">
                        <h3 className="text-sm font-black text-outline uppercase tracking-widest mb-6">System Health</h3>
                        <div className="space-y-6">
                            <div className="flex items-start gap-4">
                                <Shield className="w-5 h-5 text-green-500 mt-1" />
                                <div>
                                    <p className="text-xs font-black text-on-surface">Engine Status</p>
                                    <p className="text-[10px] text-on-surface-variant mt-0.5">Core subprocess is healthy and responding to queries.</p>
                                </div>
                            </div>
                            <div className="pt-4 border-t border-outline-variant/10">
                                <div className="flex justify-between items-center mb-1">
                                    <span className="text-[10px] font-black text-outline uppercase">Library Depth</span>
                                    <span className="text-xs font-bold text-on-surface">Optimal</span>
                                </div>
                                <p className="text-[10px] text-on-surface-variant leading-relaxed">Search responsiveness is currently under 10ms for most queries.</p>
                            </div>
                        </div>
                    </section>

                    <section className="bg-primary text-on-primary rounded-3xl p-8 shadow-xl">
                        <h3 className="text-[10px] font-black uppercase tracking-[0.2em] opacity-80 mb-4 text-primary-container">Quick Statistics</h3>
                        <div className="space-y-6">
                            <div>
                                <p className="text-3xl font-black">{scanCount.toLocaleString()}</p>
                                <p className="text-[10px] font-bold opacity-80 uppercase tracking-wider mt-1 text-primary-container">Files Indexed</p>
                            </div>
                            <div>
                                <p className="text-3xl font-black">{diskInfo ? (diskInfo.indexed / 1048576).toFixed(1) : '--'}<span className="text-sm font-bold ml-1 opacity-80">MB</span></p>
                                <p className="text-[10px] font-bold opacity-80 uppercase tracking-wider mt-1 text-primary-container">Database Size</p>
                            </div>
                        </div>
                    </section>
                </div>
            </div>
        </div>
    );
};

export default Settings;
