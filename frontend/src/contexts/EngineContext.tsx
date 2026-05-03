import { createContext, useContext, useEffect, useState, useRef, useCallback } from 'react';
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
    bytes?: number;
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
    sessionScanned: number;
    currentFolder: {
        path: string;
        filesFound: number;
        status: 'entering' | 'done';
    } | null;
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
    
    // Existing — DB-confirmed count only
    const [scanCount, setScanCount] = useState(0);
    
    // NEW — files found in the current scan session (resets on new scan)
    const [sessionScanned, setSessionScanned] = useState(0);
    
    // NEW — current folder being scanned
    const [currentFolder, setCurrentFolder] = useState<{
        path: string;
        filesFound: number;
        status: 'entering' | 'done';
    } | null>(null);

    const [diskInfo, setDiskInfo] = useState<DiskInfo | null>(null);
    const [warnings, setWarnings] = useState<Warning[]>([]);
    
    const wsRef = useRef<WebSocket | null>(null);
    const subscribersRef = useRef<Map<string, Set<(line: string, eventType: string) => void>>>(new Map());
    const connIdRef = useRef<string>(crypto.randomUUID());
    const genRef = useRef<number>(0);
    const reconnectTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
    const isConnectingRef = useRef<boolean>(false);
    const pendingCommandsRef = useRef<{cmd: string, tag: string}[]>([]);
    const backoffDelayRef = useRef<number>(1000);

    // Buffered scan count refs to avoid re-renders on every progress event
    const scanCountRef = useRef<number>(0);
    const sessionScannedRef = useRef<number>(0);
    const lastUIUpdateRef = useRef<number>(0);

    const clearWarnings = () => setWarnings([]);

    useEffect(() => {
        genRef.current++;
        const currentGen = genRef.current;
        let localWs: WebSocket | null = null;

        const connect = () => {
            if (isConnectingRef.current) return;
            if (wsRef.current?.readyState === WebSocket.OPEN || wsRef.current?.readyState === WebSocket.CONNECTING) return;
            isConnectingRef.current = true;

            const ws = new WebSocket('ws://localhost:8000/ws');
            localWs = ws;
            
            ws.onopen = () => {
                isConnectingRef.current = false;
                setConnected(true);
                backoffDelayRef.current = 1000;
                if (ws.readyState === WebSocket.OPEN) {
                    console.log('[WS] Connection established. Sending initial status requests.');
                    ws.send(JSON.stringify({ command: 'STATUS', tag: 'global_status', conn_id: connIdRef.current }));
                    ws.send(JSON.stringify({ command: 'DISK_INFO', tag: 'dashboard', conn_id: connIdRef.current }));
                    setTimeout(() => {
                        const pending = pendingCommandsRef.current.splice(0);
                        if (pending.length > 0) console.log(`[WS] Flushing ${pending.length} pending commands.`);
                        for (const p of pending) {
                            if (ws.readyState === WebSocket.OPEN) {
                                ws.send(JSON.stringify({ command: p.cmd, tag: p.tag, conn_id: connIdRef.current }));
                            }
                        }
                    }, 200);
                }
            };
            
            ws.onclose = () => {
                isConnectingRef.current = false;
                setConnected(false);
                if (reconnectTimerRef.current) clearTimeout(reconnectTimerRef.current);
                const delay = backoffDelayRef.current;
                reconnectTimerRef.current = setTimeout(connect, delay);
                backoffDelayRef.current = Math.min(delay * 2, 30000);
            };
            
            ws.onmessage = (event) => {
                if (genRef.current !== currentGen) {
                    return;
                }

                try {
                    const message: WSMessage = JSON.parse(event.data);
                    const { event: evType, data } = message;
                    const { tag, line } = data;
                    
                    console.log(`[WS RECV] [${evType}] [${tag}] -> ${line}`);

                    // Handle fatal engine errors
                    if (evType === 'engine_fatal') {
                        setStatus('ERROR');
                        return;
                    }

                    if (evType === 'engine_progress' || evType === 'engine_stream') {
                        // ── RULE: sessionScanned is ONLY updated by these events
                        if (line.startsWith('SCAN_PROGRESS') || line.startsWith('INDEXING')) {
                            const parts = line.split('|');
                            const count = parseInt(parts[1], 10);
                            if (!isNaN(count)) {
                                sessionScannedRef.current = Math.max(sessionScannedRef.current, count);
                                const now = Date.now();
                                if (now - lastUIUpdateRef.current > 500) {
                                    setSessionScanned(sessionScannedRef.current);
                                    lastUIUpdateRef.current = now;
                                }
                            }
                        }

                        // ── RULE: scanCount (DB count) is ONLY updated by these events
                        if (line.startsWith('LOADED') || 
                            line.startsWith('BATCH_DONE') || 
                            line.startsWith('INDEX_STATUS')) {
                            const parts = line.split('|');
                            const count = parseInt(parts[1], 10);
                            if (!isNaN(count)) {
                                setScanCount(count);
                                // Only sync sessionScanned from DB count if NOT actively scanning
                                // This prevents DB batch writes from dropping the live counter
                                setStatus((currentStatus) => {
                                    if (currentStatus !== 'SCANNING') {
                                        setSessionScanned(count);
                                        sessionScannedRef.current = count;
                                    }
                                    return currentStatus;
                                });
                            }
                        }

                        // ── RULE: currentFolder is ONLY updated by SCAN_FOLDER
                        if (line.startsWith('SCAN_FOLDER')) {
                            const parts = line.split('|');
                            if (parts.length >= 3) {
                                const folderPath = parts[1];
                                const folderStatus = parts[2];
                                if (folderStatus === 'ENTERING') {
                                    setCurrentFolder({
                                        path: folderPath,
                                        filesFound: 0,
                                        status: 'entering'
                                    });
                                } else {
                                    const count = parseInt(folderStatus, 10);
                                    setCurrentFolder({
                                        path: folderPath,
                                        filesFound: isNaN(count) ? 0 : count,
                                        status: 'done'
                                    });
                                }
                            }
                        }

                        // ── RULE: SCAN_DONE syncs everything
                        if (line.startsWith('SCAN_DONE')) {
                            const parts = line.split('|');
                            const total = parseInt(parts[1], 10);
                            const finalCount = isNaN(total) ? sessionScannedRef.current : total;
                            setSessionScanned(finalCount);
                            setScanCount(finalCount);
                            setCurrentFolder(null);
                            setStatus('IDLE');
                            setProgress(100);
                            setTimeout(() => setProgress(0), 1500);
                            
                            if (wsRef.current?.readyState === WebSocket.OPEN) {
                                wsRef.current.send(JSON.stringify({ command: 'DISK_INFO', tag: 'dashboard', conn_id: connIdRef.current }));
                                wsRef.current.send(JSON.stringify({ command: 'WARNINGS', tag: 'dashboard', conn_id: connIdRef.current }));
                            }
                        }

                        // Additional UI/Status handling
                        if (line.startsWith('LOADED') || line.startsWith('BATCH_DONE') || line.startsWith('INDEX_STATUS')) {
                             setStatus((prev) => {
                                 if(prev === 'SCANNING' && line.startsWith('INDEX_STATUS')) return prev; // Don't interrupt scanning for a generic status
                                 if(line.startsWith('LOADED') || line.startsWith('BATCH_DONE')){
                                    setProgress(100);
                                    setTimeout(() => setProgress(0), 1000);
                                    return 'IDLE';
                                 }
                                 return prev;
                             });
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
                            setWarnings(prev => [...prev, { 
                                type: parts[1], 
                                message: parts[2],
                                bytes: parts[3] ? parseInt(parts[3]) : undefined
                            }]);
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
                } catch (e) {
                    console.error('[WS ERROR] Failed to parse message:', event.data, e);
                }
            };
            
            wsRef.current = ws;
        };
        
        connect();
        return () => {
            genRef.current++;
            if (reconnectTimerRef.current) clearTimeout(reconnectTimerRef.current);
            if (localWs) {
                localWs.onclose = null;
                localWs.close();
            }
            isConnectingRef.current = false;
        };
    }, []);

    const sendCommand = useCallback((cmd: string, tag: string = 'api_req') => {
        if (cmd.startsWith('LOAD_DIR') || 
            cmd.startsWith('SCAN_PARALLEL') || 
            cmd.startsWith('SCAN_INCREMENTAL')) {
            setSessionScanned(0);
            sessionScannedRef.current = 0;
            setCurrentFolder(null);
            setStatus('SCANNING');
            lastUIUpdateRef.current = 0;
        }

        console.log(`[WS SEND] [${tag}] -> ${cmd}`);
        if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
            wsRef.current.send(JSON.stringify({ command: cmd, tag, conn_id: connIdRef.current }));
        } else {
            console.warn(`[WS PENDING] Engine not ready, queuing: ${cmd}`);
            pendingCommandsRef.current.push({ cmd, tag });
        }
    }, []);

    const subscribe = useCallback((tag: string, callback: (line: string, eventType: string) => void) => {
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
    }, []);

    return (
        <EngineContext.Provider value={{
            status, connected, progress, scanCount, sessionScanned, currentFolder, diskInfo, warnings, sendCommand, subscribe, clearWarnings
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
