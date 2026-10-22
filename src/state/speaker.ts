import { useSyncExternalStore } from 'react';

import type { VoiceType } from '../../modules/vowel-tracker';

export type SpeakerSex = 'female' | 'male';

/** The client being recorded. Sent with every sample; sex and age also pick the tracker's voice type. */
export type Speaker = { name: string; sex: SpeakerSex; age: number };

/** Below this age the tracker uses its child settings. */
export const CHILD_AGE_LIMIT = 13;

/**
 * Placeholder until the app has inputs for the client's details. Dev builds only, so a
 * release build never uploads samples under a made-up speaker. Adult female matches
 * vowelsy's default voice type.
 */
const DEV_SPEAKER: Speaker | null = __DEV__ ? { name: 'speechstack-dev', sex: 'female', age: 30 } : null;

let speaker: Speaker | null = DEV_SPEAKER;
const listeners = new Set<() => void>();

export function setSpeaker(next: Speaker | null) {
  speaker = next;
  listeners.forEach((listener) => listener());
}

export function getSpeaker() {
  return speaker;
}

function subscribe(listener: () => void) {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

export function useSpeaker() {
  return useSyncExternalStore(subscribe, () => speaker);
}

/** FTracker's voice type: 3 = child, 1 = adult male, 2 = adult female. */
export function voiceTypeFor(s: Speaker): VoiceType {
  if (s.age < CHILD_AGE_LIMIT) return 3;
  return s.sex === 'male' ? 1 : 2;
}
