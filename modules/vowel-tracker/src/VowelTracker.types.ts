export type FormantsEvent = {
  f1: number;
  f2: number;
  f3: number;
  /** Voice pitch (fundamental frequency) in Hz; 0 when no pitch is detected. */
  f0: number;
  voicing: boolean;
  /** True while an r-colored vowel (/ɝ/) is being produced. */
  rhotic: boolean;
  /** While capturing: seconds of audio recorded up to the end of this update, matching the WAV file. */
  captureTime?: number | null;
};

export type VowelTrackerModuleEvents = {
  onFormants: (event: FormantsEvent) => void;
};

/** 1 = adult male, 2 = adult female, 3 = child (matches FTracker's FTSwitch). */
export type VoiceType = 1 | 2 | 3;
