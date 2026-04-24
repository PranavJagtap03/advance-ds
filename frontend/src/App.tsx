import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { EngineProvider } from './contexts/EngineContext';
import AppLayout from './layout/AppLayout';

// Pages
import Dashboard from './pages/Dashboard';
import Search from './pages/Search';
import Duplicates from './pages/Duplicates';
import Orphans from './pages/Orphans';
import Analytics from './pages/Analytics';
import History from './pages/History';
import Settings from './pages/Settings';
import Security from './pages/Security';

const App = () => {
    return (
        <EngineProvider>
            <BrowserRouter>
                <Routes>
                    <Route path="/" element={<AppLayout />}>
                        <Route index element={<Dashboard />} />
                        <Route path="search" element={<Search />} />
                        <Route path="duplicates" element={<Duplicates />} />
                        <Route path="orphans" element={<Orphans />} />
                        <Route path="analytics" element={<Analytics />} />
                        <Route path="history" element={<History />} />
                        <Route path="security" element={<Security />} />
                        <Route path="settings" element={<Settings />} />
                    </Route>
                </Routes>
            </BrowserRouter>
        </EngineProvider>
    );
};

export default App;
