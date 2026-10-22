import { useSyncExternalStore } from 'react';

import { FEATURE_VERSION } from '../../modules/vowel-tracker';
import type { WordEntry } from '@/data';
import { discardAudio, type Recording } from '@/recording/use-recorder';
import type { Speaker } from '@/state/speaker';
import { createRating, createSample, uploadAudio } from './api';
import { RATER_ID, RATING_CONFIDENCE, SERVER_URL } from './config';

export type UploadStatus = 'queued' | 'uploading' | 'done' | 'failed';

export type UploadItem = {
  key: string;
  /** The practice round this came from (see SessionResult.roundId) */
  roundId: string;
  sessionId: string;
  word: WordEntry;
  correct: boolean;
  isRetry: boolean;
  recording: Recording;
  speaker: Speaker;
  status: UploadStatus;
  error?: string;
  // Set as each step succeeds, so a retry resumes instead of creating a duplicate sample
  audioKey?: string;
  sampleId?: string;
};

let items: UploadItem[] = [];
let running = false;
const listeners = new Set<() => void>();

function emit() {
  items = [...items];
  listeners.forEach((listener) => listener());
}

function patch(key: string, changes: Partial<UploadItem>) {
  items = items.map((item) => (item.key === key ? { ...item, ...changes } : item));
  emit();
}

/**
 * The server has no fields for the word or session, so they ride along in the rating's notes.
 * The `source` tag lets SpeechStack samples be told apart from collector samples.
 */
function ratingNotes(item: UploadItem) {
  return JSON.stringify({
    source: 'speechstack',
    session_id: item.sessionId,
    round_id: item.roundId,
    word_id: item.word.id,
    word: item.word.word,
    ipa: item.word.ipa,
    sound: item.word.sound,
    position: item.word.position,
    retry_round: item.isRetry,
  });
}

async function upload(item: UploadItem) {
  let { audioKey, sampleId } = item;
  if (!audioKey) {
    audioKey = await uploadAudio(SERVER_URL, item.recording.audioUri);
    patch(item.key, { audioKey });
  }
  if (!sampleId) {
    sampleId = await createSample(SERVER_URL, {
      phoneme: item.word.targetIpa,
      speaker_name: item.speaker.name,
      speaker_sex: item.speaker.sex,
      speaker_age: item.speaker.age,
      feature_version: FEATURE_VERSION,
      frames: item.recording.frames,
      audio_key: audioKey,
      trim_start: item.recording.trim?.[0] ?? null,
      trim_end: item.recording.trim?.[1] ?? null,
    });
    patch(item.key, { sampleId });
  }
  await createRating(SERVER_URL, sampleId, {
    rater_id: RATER_ID,
    correct: item.correct,
    confidence: RATING_CONFIDENCE,
    notes: ratingNotes(item),
  });
}

/** Works through queued items one at a time. */
async function drain() {
  if (running) return;
  running = true;
  try {
    for (;;) {
      const next = items.find((item) => item.status === 'queued');
      if (!next) break;
      patch(next.key, { status: 'uploading', error: undefined });
      try {
        await upload(items.find((item) => item.key === next.key)!);
        patch(next.key, { status: 'done' });
        discardAudio(next.recording.audioUri);
      } catch (error) {
        patch(next.key, { status: 'failed', error: error instanceof Error ? error.message : String(error) });
      }
    }
  } finally {
    running = false;
  }
}

export function enqueueUploads(newItems: Omit<UploadItem, 'key' | 'status'>[]) {
  if (!newItems.length) return;
  items = [
    ...items,
    ...newItems.map((item) => ({ ...item, key: `${item.roundId}:${item.word.id}`, status: 'queued' as const })),
  ];
  emit();
  drain();
}

export function retryFailed(roundId: string) {
  items = items.map((item) =>
    item.roundId === roundId && item.status === 'failed' ? { ...item, status: 'queued' as const } : item
  );
  emit();
  drain();
}

function subscribe(listener: () => void) {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

export function useUploads(roundId: string | undefined) {
  const all = useSyncExternalStore(subscribe, () => items);
  return roundId ? all.filter((item) => item.roundId === roundId) : [];
}
