import { createContext, useContext, useMemo, useState, type ReactNode } from 'react';

export type CardResult = { id: string; correct: boolean };

export type SessionResult = {
  sound: string;
  position: string;
  /** True when this round only re-practiced previously missed words. */
  isRetry: boolean;
  results: CardResult[];
};

type SessionContextValue = {
  lastSession: SessionResult | null;
  setLastSession: (s: SessionResult) => void;
};

const SessionContext = createContext<SessionContextValue | null>(null);

export function SessionProvider({ children }: { children: ReactNode }) {
  const [lastSession, setLastSession] = useState<SessionResult | null>(null);
  const value = useMemo(() => ({ lastSession, setLastSession }), [lastSession]);
  return <SessionContext.Provider value={value}>{children}</SessionContext.Provider>;
}

export function useSession() {
  const ctx = useContext(SessionContext);
  if (!ctx) throw new Error('useSession must be used inside SessionProvider');
  return ctx;
}
