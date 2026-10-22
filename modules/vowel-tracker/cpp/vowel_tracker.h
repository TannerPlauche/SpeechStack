//
//  vowel_tracker.h
//  VowelViz
//
//  Copyright (c) 2026 Tanner Plauché. All rights reserved.
//
//  Plain C interface to FTracker so it can be called from Swift and from JNI.
//  Input audio must be mono, 32-bit float, 48 kHz.
//

#ifndef VOWEL_TRACKER_H
#define VOWEL_TRACKER_H

#ifdef __cplusplus
extern "C" {
#endif

#define VT_SAMPLE_RATE      48000
#define VT_CHUNK_SIZE       1024    // samples per vt_process call used by the native audio layers

#define VT_VOICE_MALE       1
#define VT_VOICE_FEMALE     2
#define VT_VOICE_CHILD      3

typedef struct vt_tracker vt_tracker;

typedef struct vt_formants
{
	double  f1;
	double  f2;
	double  f3;
	double  f0;         // voice pitch in Hz, 0 when no pitch is detected
	long    voicing;    // 1 when the most recent frame was voiced
	long    rhotic;     // 1 while an r-colored vowel (/ɝ/) is being produced
	double  capture_time; // seconds of audio captured up to the end of this update, or -1 when not capturing
} vt_formants;

vt_tracker *vt_create(int voiceType);
void        vt_destroy(vt_tracker *tracker);

// Called from the audio thread. Settings changed since the last call are applied first.
void        vt_process(vt_tracker *tracker, const float *samples, long numSamples);
vt_formants vt_get(vt_tracker *tracker);

// Safe to call from any thread
void        vt_set_voice_type(vt_tracker *tracker, int voiceType);
void        vt_set_voicing_sensitivity(vt_tracker *tracker, double sensitivity);   // 0..1
void        vt_set_input_gain(vt_tracker *tracker, double gain);

// Audio capture: while active, every sample passed to vt_process is also written to a 48 kHz mono
// 16-bit WAV file, so the recording matches the tracker's measurements exactly (see capture_time).
// Safe to call from any thread. vt_capture_start returns 0 on success, -1 if the file can't be
// opened; starting again replaces an unfinished capture. vt_capture_stop finalizes the file and
// returns its duration in seconds, or -1 if no capture was running.
int         vt_capture_start(vt_tracker *tracker, const char *path);
double      vt_capture_stop(vt_tracker *tracker);

// Single-producer/single-consumer sample queue. vt_ring_write is safe to call from a
// real-time audio callback (no locks or allocation); vt_ring_read from one other thread.
typedef struct vt_ring vt_ring;

vt_ring    *vt_ring_create(long capacity);
void        vt_ring_destroy(vt_ring *ring);
// Copies every 'stride'-th sample of 'samples' (e.g. channel 0 of interleaved audio). Drops what doesn't fit.
long        vt_ring_write(vt_ring *ring, const float *samples, long numFrames, long stride);
long        vt_ring_read(vt_ring *ring, float *dest, long maxSamples);
void        vt_ring_clear(vt_ring *ring);

#ifdef __cplusplus
}
#endif

#endif
