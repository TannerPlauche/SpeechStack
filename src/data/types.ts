import type { ImageRequireSource } from 'react-native';

export type Position = 'initial' | 'medial' | 'final';

export type SoundInfo = {
  id: string;
  label: string;
  phonemes: string[];
  notes: string | null;
};

export type WordEntry = {
  id: string;
  sound: string;
  position: Position;
  word: string;
  ipa: string;
  targetIpa: string;
  variant: string | null;
  cue: string | null;
  /** Character range of the target sound in `word`, when known. */
  targetSpan: [number, number] | null;
  image: ImageRequireSource | null;
};
