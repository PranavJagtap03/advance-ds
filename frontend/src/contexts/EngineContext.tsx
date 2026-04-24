import { createContext, useContext, useEffect, useState, useRef } from 'react';
import type { ReactNode } from 'react';

export type EngineState = 'IDLE' | 'SCANNING' | 'ERROR';

export interface DiskInfo {
    total: number;
    free: number;
    indexed: number;
}

export interface Warning {
    type: string;
    message: string;
}

export interface WSMessage {
    event: string;
    data: { tag: string; line: string };
}

interface EngineContextType {
    status: EngineState;
    connected: boolean;
    progress: number;
    scanCount: number;
    diskInfo: DiskInfo | null;
    warnings: Warning[];
    sendCommand: (cmd: string, tag?: string) => void;
    subscribe: (tag: string, callback: (line: string, eventType: string) => void) => () => void;
    clearWarnings: () => void;
}

const EngineContext = createContext<EngineContextType | undefined>(undefined);

export const EngineProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
    const [connected, setConnected] = useState(false);
    const [status, setStatus] = useState<EngineState>('IDLE');
    const [progress, setProgress] = useState(0);
    const [scanCount, setScanCount] = useState(0);
    const [diskInfo, setDiskInfo] = useState<DiskInfo | null>(null);
    const [warnings, setWarnings] = useState<Warning[]>([]);
    
    const wsRef = useRef<WebSocket | null>(null);
    const subscribersRef = useRef<Map<string, Set<(line: string, eventType: string) => void>>>(new Map());

    const clearWarnings = () => setWarnings([]);

    useEffect(() => {
        const connect = () => {
            const ws = new WebSocket('ws://localhost:8000/ws');
            
            ws.onopen = () => {
                setConnected(true);
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ command: 'STATUS', tag: 'global_status' }));
                    ws.send(JSON.stringify({ command: 'DISK_INFO', tag: 'dashboard' }));
                }
            };
            
            ws.onclose = () => {
                setConnected(false);
                setTimeout(connect, 3000); 
            };
            
            ws.onmessage = (event) => {
                const message: WSMessage = JSON.parse(event.data);
                const { event: evType, data } = message;
                const { tag, line } = data;

                if (evType === 'engine_progress' && line.startsWith('INDEXING')) {
                    setStatus('SCANNING');
                    const parts = line.split('|');
                    const count = parseInt(parts[1], 10);
                    if (!isNaN(count)) setScanCount(count);
                    setProgress(p => Math.min(p + (Math.random() * 3), 90));
                }
                
                if (evType === 'engine_stream') {
                    if (line.startsWith('LOADED') || line.startsWith('BATCH_DONE') || line.startsWith('INDEX_STATUS')) {
                        setStatus('IDLE');
                        setProgress(100);
                        const parts = line.split('|');
                        const count = parseInt(parts[1], 10);
                        if (!isNaN(count)) setScanCount(count);
                        
                        // Request updates after scan
                        if (wsRef.current?.readyState === WebSocket.OPEN) {
                            wsRef.current.send(JSON.stringify({ command: 'DISK_INFO', tag: 'dashboard' }));
                            wsRef.current.send(JSON.stringify({ command: 'WARNINGS', tag: 'dashboard' }));
                        }
                        setTimeout(() => setProgress(0), 1000);
                    }

                    if (line.startsWith('DISK_INFO')) {
                        const parts = line.split('|');
                        setDiskInfo({
                            total: parseInt(parts[1]),
                            free: parseInt(parts[2]),
                            indexed: parseInt(parts[3])
                        });
                    }

                    if (line.startsWith('WARN|')) {
                        const parts = line.split('|');
                        setWarnings(prev => [...prev, { type: parts[1], message: parts[2] }]);
                    }

                    if (line.startsWith('ERROR')) {
                        setStatus('ERROR');
                        setProgress(100);
                        setTimeout(() => { setStatus('IDLE'); setProgress(0); }, 5000);
                    }
                }

                const tagSubs = subscribersRef.current.get(tag);
                if (tagSubs) tagSubs.forEach(cb => cb(line, evType));
                
                const globalSubs = subscribersRef.current.get('GLOBAL');
                if (globalSubs) globalSubs.forEach(cb => cb(line, evType));
            };
            
            wsRef.current = ws;
        };
        
        connect();
        return () => { if (wsRef.current) wsRef.current.close(); };
    }, []);

    const sendCommand = (cmd: string, tag: string = 'api_req') => {
        if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
            wsRef.current.send(JSON.stringify({ command: cmd, tag }));
        }
    };

    const subscribe = (tag: string, callback: (line: string, eventType: string) => void) => {
        const subs = subscribersRef.current;
        if (!subs.has(tag)) subs.set(tag, new Set());
        subs.get(tag)!.add(callback);
        return () => {
            const tagSet = subs.get(tag);
            if (tagSet) {
                tagSet.delete(callback);
                if (tagSet.size === 0) subs.delete(tag);
            }
        };
    };

    return (
        <EngineContext.Provider value={{
            status, connected, progress, scanCount, diskInfo, warnings, sendCommand, subscribe, clearWarnings
        }}>
            {children}
        </EngineContext.Provider>
    );
};

export const useEngine = () => {
    const context = useContext(EngineContext);
    if (!context) throw new Error("useEngine must be used within EngineProvider");
    return context;
};
