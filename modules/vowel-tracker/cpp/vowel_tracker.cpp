//
//  vowel_tracker.cpp
//  VowelViz
//
//  Copyright (c) 2026 Tanner Plauché. All rights reserved.
//

#include "vowel_tracker.h"
#include "FTracker.h"
#include "PitchTracker.h"
#include "RhoticDetector.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>

struct vt_tracker
{
	explicit vt_tracker(int voiceType) : ftracker(voiceType), voiceType(voiceType) {}

	FTracker            ftracker;
	PitchTracker        pitch;
	RhoticDetector      rhotic;
	std::vector<float>  scratch;

	// Settings are written by the UI thread and applied on the audio thread in vt_process
	std::mutex          settingsLock;
	bool                settingsChanged = true;
	int                 voiceType;
	double              voicingSensitivity = 1.0 - kAutoCorrThresholdMale;
	double              inputGain = 1.0;

	std::mutex          resultLock;
	vt_formants         latest = { kTypicalF1, kTypicalF2, kTypicalF3, 0, 0, 0, -1 };

	// Capture state, shared between vt_process (audio thread) and vt_capture_* (any thread)
	std::mutex          captureLock;
	FILE                *capture = nullptr;
	long                capturedSamples = 0;
	std::vector<int16_t> pcm;

	~vt_tracker()
	{
		if(capture)
			fclose(capture);
	}
};

// Writes a canonical 44-byte header for 16-bit mono PCM at VT_SAMPLE_RATE
static void writeWavHeader(FILE *file, long numSamples)
{
	auto u32 = [file](uint32_t v) { uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; fwrite(b, 1, 4, file); };
	auto u16 = [file](uint16_t v) { uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; fwrite(b, 1, 2, file); };
	uint32_t dataBytes = (uint32_t)(numSamples * 2);
	fwrite("RIFF", 1, 4, file);
	u32(36 + dataBytes);
	fwrite("WAVEfmt ", 1, 8, file);
	u32(16);						// fmt chunk size
	u16(1);							// PCM
	u16(1);							// mono
	u32(VT_SAMPLE_RATE);
	u32(VT_SAMPLE_RATE * 2);		// byte rate
	u16(2);							// block align
	u16(16);						// bits per sample
	fwrite("data", 1, 4, file);
	u32(dataBytes);
}

vt_tracker *vt_create(int voiceType)
{
	return new vt_tracker(voiceType);
}

void vt_destroy(vt_tracker *tracker)
{
	delete tracker;
}

static void applySettings(vt_tracker *tracker)
{
	std::lock_guard<std::mutex> lock(tracker->settingsLock);
	if(!tracker->settingsChanged)
		return;

	FTracker &ft = tracker->ftracker;
	ft.FTSwitch = tracker->voiceType;
	ft.vd.FTSwitch = tracker->voiceType;
	ft.vd.autoCorrThreshold = 1.0 - tracker->voicingSensitivity;
	ft.inputGain = tracker->inputGain;
	tracker->settingsChanged = false;
}

void vt_process(vt_tracker *tracker, const float *samples, long numSamples)
{
	applySettings(tracker);

	// ComputeFormants takes a non-const pointer but does not modify the input
	tracker->scratch.assign(samples, samples + numSamples);
	tracker->ftracker.ComputeFormants(tracker->scratch.data(), numSamples, false);
	tracker->pitch.process(samples, numSamples, tracker->ftracker.inputGain);
	FTracker &ft = tracker->ftracker;
	tracker->rhotic.update(ft.currentVoicing != 0, tracker->pitch.currentF0, ft.currentF1, ft.currentF3, ft.FTSwitch);

	double captureTime = -1;
	{
		std::lock_guard<std::mutex> lock(tracker->captureLock);
		if(tracker->capture)
		{
			tracker->pcm.resize(numSamples);
			for(long i=0; i<numSamples; i++)
			{
				float s = samples[i] < -1.0f ? -1.0f : (samples[i] > 1.0f ? 1.0f : samples[i]);
				tracker->pcm[i] = (int16_t)(s * 32767.0f);
			}
			fwrite(tracker->pcm.data(), sizeof(int16_t), numSamples, tracker->capture);	// little-endian on all our targets
			tracker->capturedSamples += numSamples;
			captureTime = (double)tracker->capturedSamples / VT_SAMPLE_RATE;
		}
	}

	std::lock_guard<std::mutex> lock(tracker->resultLock);
	tracker->latest.capture_time = captureTime;
	tracker->latest.f1 = tracker->ftracker.currentF1;
	tracker->latest.f2 = tracker->ftracker.currentF2;
	tracker->latest.f3 = tracker->ftracker.currentF3;
	tracker->latest.f0 = tracker->pitch.currentF0;
	tracker->latest.voicing = tracker->ftracker.currentVoicing;
	tracker->latest.rhotic = tracker->rhotic.rhotic;
}

vt_formants vt_get(vt_tracker *tracker)
{
	std::lock_guard<std::mutex> lock(tracker->resultLock);
	return tracker->latest;
}

int vt_capture_start(vt_tracker *tracker, const char *path)
{
	std::lock_guard<std::mutex> lock(tracker->captureLock);
	if(tracker->capture)
		fclose(tracker->capture);
	tracker->capture = fopen(path, "wb");
	tracker->capturedSamples = 0;
	if(!tracker->capture)
		return -1;
	writeWavHeader(tracker->capture, 0);	// sizes are filled in by vt_capture_stop
	return 0;
}

double vt_capture_stop(vt_tracker *tracker)
{
	std::lock_guard<std::mutex> lock(tracker->captureLock);
	if(!tracker->capture)
		return -1;
	fseek(tracker->capture, 0, SEEK_SET);
	writeWavHeader(tracker->capture, tracker->capturedSamples);
	fclose(tracker->capture);
	tracker->capture = nullptr;
	return (double)tracker->capturedSamples / VT_SAMPLE_RATE;
}

void vt_set_voice_type(vt_tracker *tracker, int voiceType)
{
	if(voiceType < VT_VOICE_MALE || voiceType > VT_VOICE_CHILD)
		return;
	std::lock_guard<std::mutex> lock(tracker->settingsLock);
	tracker->voiceType = voiceType;
	tracker->settingsChanged = true;
}

void vt_set_voicing_sensitivity(vt_tracker *tracker, double sensitivity)
{
	std::lock_guard<std::mutex> lock(tracker->settingsLock);
	tracker->voicingSensitivity = sensitivity;
	tracker->settingsChanged = true;
}

void vt_set_input_gain(vt_tracker *tracker, double gain)
{
	std::lock_guard<std::mutex> lock(tracker->settingsLock);
	tracker->inputGain = gain;
	tracker->settingsChanged = true;
}
