import { Component } from 'react';
import type { ErrorInfo, ReactNode } from 'react';
import { AlertTriangle, RotateCcw } from 'lucide-react';

interface Props {
    children: ReactNode;
}

interface State {
    hasError: boolean;
    error: Error | null;
}

class ErrorBoundary extends Component<Props, State> {
    constructor(props: Props) {
        super(props);
        this.state = { hasError: false, error: null };
    }

    static getDerivedStateFromError(error: Error): State {
        return { hasError: true, error };
    }

    componentDidCatch(error: Error, errorInfo: ErrorInfo) {
        console.error('ErrorBoundary caught:', error, errorInfo);
    }

    handleReset = () => {
        this.setState({ hasError: false, error: null });
    };

    render() {
        if (this.state.hasError) {
            return (
                <div className="flex flex-col items-center justify-center h-full gap-6 p-10">
                    <div className="p-6 bg-error/10 rounded-full">
                        <AlertTriangle className="w-12 h-12 text-error" />
                    </div>
                    <div className="text-center">
                        <h2 className="text-xl font-black text-on-surface mb-2">Something went wrong</h2>
                        <p className="text-sm text-on-surface-variant max-w-md">
                            {this.state.error?.message || 'An unexpected error occurred in this panel.'}
                        </p>
                    </div>
                    <button
                        onClick={this.handleReset}
                        className="flex items-center gap-2 bg-primary text-on-primary px-6 py-3 rounded-xl font-bold hover:brightness-110 transition-all shadow-md"
                    >
                        <RotateCcw className="w-4 h-4" />
                        TRY AGAIN
                    </button>
                </div>
            );
        }

        return this.props.children;
    }
}

export default ErrorBoundary;
