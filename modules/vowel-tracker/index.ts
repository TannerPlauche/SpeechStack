export { default } from './src/VowelTrackerModule';
export * from './src/VowelTracker.types';

/**
 * Version of the measurement pipeline (FTracker formants, YIN pitch, 48 kHz audio in 1024-sample
 * chunks) recorded with collected samples. Bump it whenever the tracker's output changes.
 */
export const FEATURE_VERSION = '1.0.0';
