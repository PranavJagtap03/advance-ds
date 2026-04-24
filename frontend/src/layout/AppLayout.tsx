import { Outlet, NavLink } from 'react-router-dom';
import { useEngine } from '../contexts/EngineContext';
import { 
    FolderSearch, 
    LayoutDashboard, 
    Zap, 
    Link2Off, 
    LineChart, 
    History, 
    Settings as SettingsIcon,
    Database,
    ShieldAlert
} from 'lucide-react';

const AppLayout = () => {
    const { connected, status, progress } = useEngine();

    const navItems = [
        { path: '/', label: 'Overview', icon: LayoutDashboard },
        { path: '/search', label: 'File Search', icon: FolderSearch },
        { path: '/duplicates', label: 'Free Up Space', icon: Zap },
        { path: '/orphans', label: 'Broken Links', icon: Link2Off },
        { path: '/analytics', label: 'Storage Trends', icon: LineChart },
        { path: '/history', label: 'File Timeline', icon: History },
        { path: '/security', label: 'Security Shield', icon: ShieldAlert },
        { path: '/settings', label: 'Scan & Configure', icon: SettingsIcon },
    ];

    return (
        <div className="flex h-screen bg-surface-container-lowest overflow-hidden text-on-surface font-sans">
            {/* Sidebar Navigation */}
            <aside className="w-64 bg-surface flex flex-col border-r border-outline-variant/20 shadow-sm z-20">
                <div className="p-8">
                    <h1 className="text-2xl font-black text-primary tracking-tighter flex items-center gap-2">
                        <Database className="w-7 h-7 text-primary" strokeWidth={3} />
                        DataStruct
                    </h1>
                    <p className="text-[10px] font-black text-outline uppercase tracking-[0.2em] mt-2 opacity-60">Production Engine v1.0</p>
                </div>

                <div className="px-6 py-2">
                    <div className={`flex items-center gap-3 px-4 py-2.5 rounded-2xl border transition-all ${connected ? 'bg-green-50 border-green-200' : 'bg-red-50 border-red-200'}`}>
                        <div className={`w-2 h-2 rounded-full animate-pulse ${connected ? 'bg-green-500 shadow-[0_0_10px_rgba(34,197,94,0.8)]' : 'bg-red-500'}`} />
                        <span className={`text-[10px] font-black uppercase tracking-wider ${connected ? 'text-green-700' : 'text-red-700'}`}>
                            {connected ? 'Engine Online' : 'Engine Offline'}
                        </span>
                    </div>
                </div>

                <nav className="flex-1 px-4 py-8 space-y-2">
                    {navItems.map((item) => (
                        <NavLink
                            key={item.path}
                            to={item.path}
                            className={({ isActive }) =>
                                `flex items-center gap-4 px-5 py-3 rounded-2xl transition-all duration-300 text-sm font-bold ${
                                    isActive
                                        ? 'bg-primary text-on-primary shadow-lg shadow-primary/20 translate-x-1'
                                        : 'text-on-surface-variant hover:bg-primary/5 hover:text-primary'
                                }`
                            }
                        >
                            {({ isActive }) => (
                                <>
                                    <item.icon className="w-[18px] h-[18px]" strokeWidth={isActive ? 3 : 2} />
                                    {item.label}
                                </>
                            )}
                        </NavLink>
                    ))}
                </nav>

                <div className="p-6 border-t border-outline-variant/10">
                    <p className="text-[10px] font-bold text-outline-variant text-center leading-relaxed">
                        &copy; 2026 Antigravity AI<br/>All Data Local-Only
                    </p>
                </div>
            </aside>

            {/* Main Content Area */}
            <main className="flex-1 flex flex-col relative overflow-hidden bg-surface-container-lowest">
                {/* Global Top Progress Loader if Scanning */}
                {status === 'SCANNING' && (
                    <div className="absolute top-0 left-0 right-0 h-1.5 bg-outline-variant/10 z-50 overflow-hidden">
                        <div 
                            className="h-full bg-primary transition-all duration-500 ease-out shadow-[0_0_10px_rgba(79,70,229,0.5)]"
                            style={{ width: `${progress}%` }}
                        />
                    </div>
                )}
                
                <div className="flex-1 overflow-y-auto custom-scrollbar">
                    <div className="max-w-[1400px] mx-auto p-10 h-full">
                        <Outlet />
                    </div>
                </div>
            </main>
        </div>
    );
};

export default AppLayout;
