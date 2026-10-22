// Adapted from vowelsy's mobile/src/collector/use-recorder.ts for hold-to-record. Frames are built
// exactly as the collector builds them (toFrame, voicedWindow), so samples match its data.
import { File } from 'expo-file-system';
import { useEffect, useRef, useState } from 'react';

import VowelTracker, { type FormantsEvent } from '../../modules/vowel-tracker';
import type { FeatureFrame } from '@/vowelsy/api';

/** Recordings stop on their own after this long */
export const MAX_SECONDS = 5;

/** Shorter holds are treated as accidental taps and thrown away */
export const MIN_SECONDS = 0.25;

export type Recording = {
  audioUri: string;
  duration: number;
  frames: FeatureFrame[];
  /** Frames where the voice was detected */
  voicedFrames: number;
  /** Window around the voiced part, sent as the sample's trim; adjust it in the review app */
  trim: [number, number] | null;
};

/** Padding kept around the voiced region; the review app's "Fit to voice" uses the same */
const TRIM_MARGIN = 0.1;

const round = (t: number) => Math.round(t * 1000) / 1000;

/** From the first voiced frame to the last, plus a margin, or null when nothing was voiced. */
function voicedWindow(frames: FeatureFrame[], duration: number): [number, number] | null {
  const voiced = frames.filter((f) => f.f1 !== null);
  if (!voiced.length) return null;
  const start = Math.max(0, voiced[0].t - TRIM_MARGIN);
  const end = Math.min(duration, voiced[voiced.length - 1].t + TRIM_MARGIN);
  return end > start ? [round(start), round(end)] : null;
}

/** One tracker update as a sample frame (same as the collector's toFrame). */
export function toFrame(event: FormantsEvent, t: number): FeatureFrame {
  if (!event.voicing) return { t, f0: null, f1: null, f2: null, f3: null };
  return { t, f0: event.f0 > 0 ? event.f0 : null, f1: event.f1, f2: event.f2, f3: event.f3 };
}

export type LiveReading = { seconds: number; voiced: boolean };

/**
 * Hold-to-record on top of the running tracker. `start` on press-in, `stop` on release;
 * recordings also stop on their own at MAX_SECONDS, which calls `onAutoStop`.
 */
export function useRecorder(onAutoStop: (recording: Recording | null) => void) {
  const [recording, setRecording] = useState(false);
  const [live, setLive] = useState<LiveReading>({ seconds: 0, voiced: false });
  const frames = useRef<FeatureFrame[]>([]);
  /** Resolves to the WAV URI once the native capture has started */
  const starting = useRef<Promise<string | null> | null>(null);
  const autoStop = useRef(onAutoStop);
  autoStop.current = onAutoStop;

  /** Ends the capture. Only the first call after a start gets the recording; later calls get null. */
  const stop = async (): Promise<Recording | null> => {
    const pending = starting.current;
    if (!pending) return null;
    starting.current = null;
    // Keep this take's frames even if a new take starts (and replaces frames.current) while we wait
    const recorded = frames.current;
    // A quick tap can release before startCapture resolves; wait so the stop applies to it
    const uri = await pending;
    const duration = uri ? await VowelTracker.stopCapture() : -1;
    setRecording(false);
    if (!uri) return null;
    if (duration < MIN_SECONDS) {
      discardAudio(uri);
      return null;
    }
    return {
      audioUri: uri,
      duration,
      frames: recorded,
      voicedFrames: recorded.filter((f) => f.f1 !== null).length,
      trim: voicedWindow(recorded, duration),
    };
  };

  const start = () => {
    if (starting.current) return;
    frames.current = [];
    setLive({ seconds: 0, voiced: false });
    setRecording(true);
    starting.current = VowelTracker.startCapture()
      .then((uri) => uri || null)
      .catch((error) => {
        console.warn('Could not start recording', error);
        setRecording(false);
        return null;
      });
  };

  useEffect(() => {
    if (!recording) return;
    let lastUpdate = 0;
    const subscription = VowelTracker.addListener('onFormants', (event: FormantsEvent) => {
      const t = event.captureTime;
      if (t === null || t === undefined) return;
      frames.current.push(toFrame(event, t));
      if (t - lastUpdate >= 0.1) {
        lastUpdate = t;
        setLive({ seconds: t, voiced: event.voicing });
      }
      if (t >= MAX_SECONDS) {
        stop().then((result) => {
          if (result) autoStop.current(result);
        });
      }
    });
    return () => subscription.remove();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [recording]);

  // Never leave a capture running if the screen goes away mid-recording
  useEffect(
    () => () => {
      VowelTracker.stopCapture().catch(() => {});
    },
    []
  );

  return { recording, live, start, stop };
}

/** Deletes a recording's temporary audio file. */
export function discardAudio(uri: string) {
  try {
    const file = new File(uri);
    if (file.exists) file.delete();
  } catch (error) {
    console.warn('Could not delete recording', error);
  }
}
