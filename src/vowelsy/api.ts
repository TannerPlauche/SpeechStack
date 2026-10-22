// Client for the Vowelsy server (/Users/tannerplauche/source/vowelsy, src/vowelsy_server/).
// Ported from vowelsy's mobile/src/collector/api.ts; keep the request shapes in sync with its schemas.py.
// Endpoints used: POST /audio, POST /samples, POST /samples/{id}/ratings.

import { File } from 'expo-file-system';

import type { SpeakerSex } from '@/state/speaker';

/** One analysis frame, matching the server's FeatureFrame. Unvoiced values are null. */
export type FeatureFrame = {
  t: number;
  f0: number | null;
  f1: number | null;
  f2: number | null;
  f3: number | null;
};

export type SampleCreate = {
  phoneme: string;
  speaker_name: string;
  speaker_sex: SpeakerSex;
  speaker_age: number;
  feature_version: string;
  frames: FeatureFrame[];
  audio_key?: string | null;
  /** Seconds on the frames' clock; both null means the whole clip */
  trim_start?: number | null;
  trim_end?: number | null;
};

export type RatingCreate = {
  rater_id: string;
  correct: boolean;
  /** 0..1 */
  confidence: number;
  notes?: string | null;
};

const TIMEOUT_MS = 15000;

export function joinUrl(base: string, path: string) {
  return base.replace(/\/+$/, '') + path;
}

async function request(url: string, init: RequestInit = {}, timeoutMs = TIMEOUT_MS) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, {
      ...init,
      // ngrok's free tier answers some requests with an HTML warning page unless this is set
      headers: { 'ngrok-skip-browser-warning': 'true', ...(init.headers as Record<string, string>) },
      signal: controller.signal,
    });
    if (!response.ok) {
      let detail = `${response.status}`;
      try {
        const body = await response.json();
        if (typeof body?.detail === 'string') detail = `${response.status}: ${body.detail}`;
      } catch {
        // Not JSON; the status code is enough
      }
      throw new Error(`Server error ${detail}`);
    }
    return response;
  } catch (error) {
    if (controller.signal.aborted) throw new Error('The server did not respond');
    throw error;
  } finally {
    clearTimeout(timer);
  }
}

/** Uploads a WAV recording and returns its audio_key. */
export async function uploadAudio(serverUrl: string, fileUri: string) {
  const form = new FormData();
  // expo-file-system's File is a Blob, which fetch can send directly as a multipart part
  form.append('file', new File(fileUri), 'sample.wav');
  const response = await request(joinUrl(serverUrl, '/audio'), { method: 'POST', body: form });
  const body = await response.json();
  return body.audio_key as string;
}

/** Saves a sample and returns its id. */
export async function createSample(serverUrl: string, sample: SampleCreate) {
  const response = await request(joinUrl(serverUrl, '/samples'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(sample),
  });
  const body = (await response.json()) as { id: string };
  return body.id;
}

export async function createRating(serverUrl: string, sampleId: string, rating: RatingCreate) {
  await request(joinUrl(serverUrl, `/samples/${encodeURIComponent(sampleId)}/ratings`), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(rating),
  });
}
