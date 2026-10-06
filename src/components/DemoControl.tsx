import React, { useState, useEffect } from 'react';
import { Power, CheckCircle2, AlertCircle, RefreshCw, Smartphone, Radio } from 'lucide-react';
import { cn } from '../utils/cn';

interface DemoControlProps {
  apiBaseUrl?: string;
}

export const DemoControl: React.FC<DemoControlProps> = ({ apiBaseUrl }) => {
  // Determine API base URL dynamically
  const defaultApiUrl = apiBaseUrl || 
    (typeof window !== 'undefined' && window.location.hostname 
      ? `http://${window.location.hostname}:8000` 
      : 'http://localhost:8000');

  const [apiUrl, setApiUrl] = useState<string>(defaultApiUrl);
  const [isOn, setIsOn] = useState<boolean | null>(null);
  const [loading, setLoading] = useState<boolean>(false);
  const [statusMsg, setStatusMsg] = useState<string>('');
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  const [lastSync, setLastSync] = useState<Date | null>(null);

  // Fetch current demo state
  const fetchState = async (url: string = apiUrl) => {
    try {
      const res = await fetch(`${url}/api/demo/state`, {
        method: 'GET',
        headers: { 'Accept': 'application/json' },
      });
      if (!res.ok) {
        throw new Error(`HTTP ${res.status}: ${res.statusText}`);
      }
      const data = await res.json();
      setIsOn(data.on);
      setErrorMsg(null);
      setLastSync(new Date());
    } catch (err: any) {
      setErrorMsg(`Cannot reach backend at ${url}. ${err.message || ''}`);
    }
  };

  // Toggle state to ON or OFF
  const setDemoState = async (targetOn: boolean) => {
    setLoading(true);
    setErrorMsg(null);
    setStatusMsg(`Sending ${targetOn ? 'ON' : 'OFF'} command...`);
    try {
      const res = await fetch(`${apiUrl}/api/demo/state`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Accept': 'application/json',
        },
        body: JSON.stringify({ on: targetOn }),
      });

      if (!res.ok) {
        throw new Error(`HTTP ${res.status}: ${res.statusText}`);
      }

      const data = await res.json();
      setIsOn(data.on);
      setStatusMsg(`Demo bulb state successfully set to ${data.state}!`);
      setLastSync(new Date());
      setTimeout(() => setStatusMsg(''), 3000);
    } catch (err: any) {
      setErrorMsg(`Failed to set state: ${err.message || 'Network error'}`);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchState(apiUrl);
    // Poll state every 2 seconds to keep multiple browser tabs / mobile synced
    const interval = setInterval(() => {
      fetchState(apiUrl);
    }, 2000);
    return () => clearInterval(interval);
  }, [apiUrl]);

  return (
    <div className="max-w-md mx-auto w-full space-y-6">
      {/* Card Header */}
      <div className="glass-card p-6 sm:p-8 text-center relative overflow-hidden shadow-xl border border-white/60">
        <div className="flex justify-center mb-3">
          <div className={cn(
            "p-3.5 rounded-2xl shadow-lg border transition-all duration-500",
            isOn 
              ? "bg-gradient-to-br from-emerald-400 to-green-600 text-white border-green-300 ring-4 ring-green-400/30 animate-pulse" 
              : "bg-gradient-to-br from-slate-700 to-slate-900 text-slate-300 border-slate-600 ring-2 ring-slate-800/20"
          )}>
            <Power className="h-8 w-8" />
          </div>
        </div>

        <h2 className="text-xl font-black bg-gradient-to-r from-brand-primary to-brand-accent bg-clip-text text-transparent">
          EnergyGuard
        </h2>
        <h3 className="text-2xl font-black text-brand-text mt-0.5 tracking-tight">
          Demo Control
        </h3>
        <p className="text-xs text-brand-text-muted mt-1 font-semibold">
          Software mock control for 9W LED bulb presentation demo
        </p>

        {/* Current State Pill */}
        <div className="mt-6 mb-6">
          <div className="text-xs font-bold uppercase tracking-wider text-brand-text-muted mb-2">
            Current Demo State
          </div>
          <div className="inline-flex items-center justify-center">
            <span className={cn(
              "px-6 py-2 rounded-full text-base font-black tracking-wide uppercase transition-all duration-300 flex items-center gap-2 shadow-sm border",
              isOn === true
                ? "bg-emerald-500 text-white border-emerald-400 shadow-emerald-500/30 shadow-lg ring-4 ring-emerald-500/20"
                : isOn === false
                ? "bg-slate-800 text-slate-200 border-slate-700 shadow-sm"
                : "bg-amber-100 text-amber-800 border-amber-300 animate-pulse"
            )}>
              <span className={cn(
                "h-3 w-3 rounded-full",
                isOn === true ? "bg-white animate-ping" : isOn === false ? "bg-slate-400" : "bg-amber-500"
              )} />
              {isOn === true ? "STATE: ON" : isOn === false ? "STATE: OFF" : "CONNECTING..."}
            </span>
          </div>
        </div>

        {/* Big Touch-Friendly Action Buttons */}
        <div className="grid grid-cols-2 gap-4 mt-6">
          {/* ON BUTTON */}
          <button
            type="button"
            onClick={() => setDemoState(true)}
            disabled={loading}
            className={cn(
              "py-4 px-6 rounded-2xl font-black text-lg transition-all duration-200 flex flex-col items-center justify-center gap-1 shadow-md active:scale-95 disabled:opacity-50",
              isOn === true
                ? "bg-emerald-500 text-white shadow-emerald-500/40 ring-4 ring-emerald-400/40 border-2 border-emerald-300 scale-105"
                : "bg-white text-emerald-700 hover:bg-emerald-50 border-2 border-emerald-500/30 hover:border-emerald-500"
            )}
          >
            <div className="flex items-center gap-1.5">
              <Power className="h-5 w-5" />
              <span>ON</span>
            </div>
            <span className="text-[10px] font-bold opacity-80 uppercase tracking-widest">
              ~9W Load
            </span>
          </button>

          {/* OFF BUTTON */}
          <button
            type="button"
            onClick={() => setDemoState(false)}
            disabled={loading}
            className={cn(
              "py-4 px-6 rounded-2xl font-black text-lg transition-all duration-200 flex flex-col items-center justify-center gap-1 shadow-md active:scale-95 disabled:opacity-50",
              isOn === false
                ? "bg-slate-900 text-white shadow-slate-900/40 ring-4 ring-slate-700/40 border-2 border-slate-600 scale-105"
                : "bg-white text-slate-700 hover:bg-slate-100 border-2 border-slate-300 hover:border-slate-400"
            )}
          >
            <div className="flex items-center gap-1.5">
              <Power className="h-5 w-5 opacity-60" />
              <span>OFF</span>
            </div>
            <span className="text-[10px] font-bold opacity-80 uppercase tracking-widest">
              0W Standby
            </span>
          </button>
        </div>

        {/* Status or Success Message */}
        {statusMsg && (
          <div className="mt-4 p-2.5 bg-emerald-50 text-emerald-800 border border-emerald-200 rounded-xl text-xs font-bold flex items-center justify-center gap-1.5 animate-fade-in">
            <CheckCircle2 className="h-4 w-4 text-emerald-600" />
            {statusMsg}
          </div>
        )}

        {/* Error / Connection Alert */}
        {errorMsg && (
          <div className="mt-4 p-3 bg-red-50 text-red-700 border border-red-200 rounded-xl text-xs font-semibold flex items-start gap-2 text-left">
            <AlertCircle className="h-4 w-4 text-red-500 flex-shrink-0 mt-0.5" />
            <div className="flex-1">
              <p className="font-bold">Connection Warning</p>
              <p className="text-[11px] opacity-90">{errorMsg}</p>
            </div>
          </div>
        )}

        {/* Expected Electrical Output Guide */}
        <div className="mt-6 pt-4 border-t border-black/5 text-left text-xs space-y-2 bg-black/[0.02] p-3 rounded-xl">
          <div className="font-black text-brand-text/70 flex items-center gap-1.5">
            <Radio className="h-3.5 w-3.5 text-brand-primary" /> ESP32 Broadcast Target:
          </div>
          <div className="grid grid-cols-2 gap-2 text-[11px] font-medium text-brand-text/80">
            <div className="p-2 rounded-lg bg-white/70 border border-black/5">
              <div className="font-bold text-emerald-700">When ON:</div>
              <div>⚡ 228-230 V</div>
              <div>🔌 0.033-0.043 A</div>
              <div>💡 ~7-10 W</div>
            </div>
            <div className="p-2 rounded-lg bg-white/70 border border-black/5">
              <div className="font-bold text-slate-700">When OFF:</div>
              <div>⚡ 228-230 V</div>
              <div>🔌 0.0000 A</div>
              <div>💡 0.00 W</div>
            </div>
          </div>
        </div>
      </div>

      {/* Backend API Configuration Helper */}
      <div className="glass-card p-4 text-xs font-medium space-y-2 border border-brand-primary/10">
        <div className="flex justify-between items-center text-brand-text/60 font-bold">
          <span className="flex items-center gap-1.5">
            <Smartphone className="h-3.5 w-3.5 text-brand-primary" /> Backend Endpoint:
          </span>
          <button
            onClick={() => fetchState(apiUrl)}
            className="hover:text-brand-primary flex items-center gap-1 text-[10px] uppercase font-black"
          >
            <RefreshCw className={cn("h-3 w-3", loading && "animate-spin")} /> Refresh
          </button>
        </div>
        <div className="flex gap-2">
          <input
            type="text"
            value={apiUrl}
            onChange={(e) => setApiUrl(e.target.value.trim())}
            placeholder="http://10.47.59.1:8000"
            className="flex-1 bg-white/80 border border-brand-secondary rounded-lg px-2.5 py-1.5 font-mono text-xs text-brand-text outline-none focus:ring-1 focus:ring-brand-primary"
          />
          <button
            onClick={() => fetchState(apiUrl)}
            className="px-3 py-1.5 bg-brand-primary text-white rounded-lg font-bold hover:bg-brand-primary/90 text-xs shadow-sm"
          >
            Set
          </button>
        </div>
        {lastSync && (
          <div className="text-[10px] text-brand-text/40 text-right">
            Synced: {lastSync.toLocaleTimeString()}
          </div>
        )}
      </div>
    </div>
  );
};
