import { createContext, useContext, useMemo, useState, type ReactNode } from 'react';

import type { Recording } from '@/recording/use-recorder';

export type CardResult = { id: string; correct: boolean };

export type SessionResult = {
  /** One practice round (one pass through a deck) */
  roundId: string;
  /** Shared by a round and the "practice missed" / "practice again" rounds that follow it */
  sessionId: string;
  sound: string;
  position: string;
  /** True when this round only re-practiced previously missed words. */
  isRetry: boolean;
  results: CardResult[];
  /** The last take recorded for each card, by word id */
  recordings: Record<string, Recording>;
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

export function newId() {
  return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 8)}`;
}
