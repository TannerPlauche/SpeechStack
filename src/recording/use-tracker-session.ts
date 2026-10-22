// Ported from vowelsy's mobile/src/use-tracker-session.ts.
import { requestRecordingPermissionsAsync } from 'expo-audio';
import { useEffect, useRef, useState } from 'react';
import { AppState, Platform } from 'react-native';

import VowelTracker from '../../modules/vowel-tracker';

export type TrackingStatus = 'starting' | 'running' | 'permission-denied' | 'error' | 'unavailable';

/**
 * Runs live microphone tracking while the calling screen is mounted and the app is in the
 * foreground. Recordings are captured from this running input. `retry` tries again after an
 * error, e.g. once a microphone becomes available.
 */
export function useTrackerSession() {
  const [status, setStatus] = useState<TrackingStatus>(Platform.OS === 'web' ? 'unavailable' : 'starting');
  const startRef = useRef<() => Promise<void>>(async () => {});

  useEffect(() => {
    // The tracker is native-only; the web build gets a no-op module
    if (Platform.OS === 'web') return;
    let cancelled = false;

    const start = async () => {
      const permission = await requestRecordingPermissionsAsync();
      if (cancelled) return;
      if (!permission.granted) {
        setStatus('permission-denied');
        return;
      }
      try {
        await VowelTracker.start();
        if (!cancelled) setStatus('running');
      } catch (error) {
        console.warn('Could not start live tracking', error);
        if (!cancelled) setStatus('error');
      }
    };

    startRef.current = start;
    start();
    const appState = AppState.addEventListener('change', (state) => {
      if (state === 'active') {
        start();
      } else if (state === 'background') {
        VowelTracker.stop();
      }
    });

    return () => {
      cancelled = true;
      appState.remove();
      VowelTracker.stop();
    };
  }, []);

  const retry = () => {
    setStatus('starting');
    startRef.current();
  };

  return { status, retry };
}
