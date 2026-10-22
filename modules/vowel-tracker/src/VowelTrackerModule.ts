import { NativeModule, requireNativeModule } from 'expo';

import { VoiceType, VowelTrackerModuleEvents } from './VowelTracker.types';

declare class VowelTrackerModule extends NativeModule<VowelTrackerModuleEvents> {
  /** Starts live microphone input. Rejects if microphone permission hasn't been granted. */
  start(): Promise<void>;
  stop(): Promise<void>;
  setVoiceType(voiceType: VoiceType): void;
  /** 0..1; higher accepts quieter or less periodic sound as voiced. */
  setVoicingSensitivity(sensitivity: number): void;
  /** Software gain applied to the input before tracking (1..128). */
  setInputGain(gain: number): void;
  /**
   * Starts recording the tracker's input (48 kHz mono 16-bit WAV) and returns the file's URI.
   * Updates then carry `captureTime`, so measurements line up with the recording.
   */
  startCapture(): Promise<string>;
  /** Finishes the recording; resolves to its duration in seconds, or -1 if none was running. */
  stopCapture(): Promise<number>;
  /** Plays a local audio file (file:// URI) without disturbing the live input. */
  playSound(uri: string): Promise<void>;
}

export default requireNativeModule<VowelTrackerModule>('VowelTracker');
