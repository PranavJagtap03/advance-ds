import React from 'react';
import { Outlet, NavLink } from 'react-router-dom';
import { useEngine } from '../contexts/EngineContext';
import { 
    FolderSearch, 
    LayoutDashboard, 
    Copy, 
    Ghost, 
    LineChart, 
    History, 
    Settings,
    Activity
} from 'lucide-react';

const AppLayout = () => {
    const { connected, status, progress } = useEngine();

    const navItems = [
        { path: '/', label: 'Overview', icon: LayoutDashboard },
        { path: '/search', label: 'File Search', icon: FolderSearch },
        { path: '/duplicates', label: 'Duplicates map', icon: Copy },
        { path: '/orphans', label: 'Orphan Nodes', icon: Ghost },
        { path: '/analytics', label: 'Storage Analytics', icon: LineChart },
        { path: '/history', label: 'Version Control', icon: History },
        { path: '/settings', label: 'System Help', icon: Settings },
    ];

    return (
        <div className="flex h-screen bg-background overflow-hidden text-on-surface">
            {/* Sidebar Navigation */}
            <aside className="w-64 bg-surface flex flex-col border-r border-outline-variant/30">
                <div className="p-6">
                    <h1 className="text-xl font-bold text-primary tracking-tight flex items-center gap-2">
                        <Activity className="w-6 h-6 text-primary" />
                        Serene FS
                    </h1>
                    <p className="text-xs text-on-surface-variant mt-1 font-medium">Academic Index Explorer</p>
                </div>

                <div className="px-4 py-2">
                    <div className="flex items-center gap-2 px-2 py-2 rounded-md bg-surface-container-low border border-outline-variant/20">
                        <div className={`w-2 h-2 rounded-full ${connected ? 'bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]' : 'bg-red-500'}`} />
                        <span className="text-xs font-semibold text-on-surface-variant flex-1">
                            {connected ? 'Engine Online' : 'Engine Offline'}
                        </span>
                    </div>
                </div>

                <nav className="flex-1 px-4 py-4 space-y-1">
                    {navItems.map((item) => (
                        <NavLink
                            key={item.path}
                            to={item.path}
                            className={({ isActive }) =>
                                `flex items-center gap-3 px-3 py-2.5 rounded-md transition-all duration-200 text-sm font-medium ${
                                    isActive
                                        ? 'bg-primary-container text-on-primary-container shadow-sm'
                                        : 'text-on-surface-variant hover:bg-surface-variant hover:text-on-surface'
                                }`
                            }
                        >
                            <item.icon className="w-[18px] h-[18px]" />
                            {item.label}
                        </NavLink>
                    ))}
                </nav>
            </aside>

            {/* Main Content Area */}
            <main className="flex-1 flex flex-col relative overflow-hidden">
                {/* Global Top Progress Loader if Scanning */}
                {status === 'SCANNING' && (
                    <div className="absolute top-0 left-0 right-0 h-1 bg-surface-variant z-50">
                        <div 
                            className="h-full bg-primary transition-all duration-300 ease-out"
                            style={{ width: `${progress}%` }}
                        />
                    </div>
                )}
                
                <div className="flex-1 overflow-y-auto custom-scrollbar">
                    <div className="max-w-[1280px] mx-auto p-8 h-full">
                        <Outlet />
                    </div>
                </div>
            </main>
        </div>
    );
};

export default AppLayout;
