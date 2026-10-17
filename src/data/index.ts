import type { Position, WordEntry } from './types';
import { SOUNDS, WORDS } from './wordlists.generated';

export type { Position, SoundInfo, WordEntry } from './types';
export { SOUNDS, WORDS };

export const POSITIONS: { id: Position; label: string; hint: string }[] = [
  { id: 'initial', label: 'Initial', hint: 'Beginning of the word' },
  { id: 'medial', label: 'Medial', hint: 'Middle of the word' },
  { id: 'final', label: 'Final', hint: 'End of the word' },
];

const wordsById = new Map(WORDS.map((w) => [w.id, w]));

export function getSound(id: string) {
  return SOUNDS.find((s) => s.id === id);
}

export function getWord(id: string) {
  return wordsById.get(id);
}

export function getWords(sound: string, position: Position | 'all', picturesOnly: boolean): WordEntry[] {
  return WORDS.filter(
    (w) => w.sound === sound && (position === 'all' || w.position === position) && (!picturesOnly || w.image),
  );
}

export function positionLabel(position: string) {
  return POSITIONS.find((p) => p.id === position)?.label ?? 'All positions';
}

/** IPA phonemes for display, e.g. "/θ/ /ð/". */
export function phonemeText(phonemes: string[]) {
  return phonemes.map((p) => `/${p}/`).join(' ');
}

export function shuffle<T>(items: T[]): T[] {
  const a = [...items];
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}
