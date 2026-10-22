/*
 *  PitchTracker.cpp
 *  VowelViz
 *
 *  Copyright (c) 2026 Tanner Plauché. All rights reserved.
 */

#include "PitchTracker.h"

#include <algorithm>
#include <cmath>

#define kInputRate			48000.0
#define kDecimation			4
#define kAnalysisRate		(kInputRate / kDecimation)		// 12 kHz, enough for voice F0
#define kHistoryLength		2048							// ~43 ms at 48 kHz: two periods of a 60 Hz voice
#define kFrameLength		(kHistoryLength / kDecimation)	// 512 samples at 12 kHz
#define kMinF0				60.0
#define kMaxF0				600.0
#define kYinThreshold		0.15							// de Cheveigné & Kawahara's suggested value
#define kMinRMS				1e-4							// below this the input is treated as silence
#define kSmoothing			3								// median of this many consecutive estimates

static const long kMinLag = (long)(kAnalysisRate / kMaxF0);		// 20
static const long kMaxLag = (long)(kAnalysisRate / kMinF0);		// 200
// Lags run up to kMaxLag + 1 (one past, for interpolation), so frame[j + lag] stays in bounds
static const long kWindow = kFrameLength - kMaxLag - 1;			// 311 samples compared per lag

PitchTracker::PitchTracker ()
	: currentF0(0.0), history(kHistoryLength, 0.0f), frame(kFrameLength), diff(kMaxLag + 2)
{
}

double PitchTracker::process(const float *samples, long numSamples, double inputGain)
{
	// Keep only the most recent kHistoryLength samples
	if(numSamples >= kHistoryLength)
	{
		std::copy(samples + numSamples - kHistoryLength, samples + numSamples, history.begin());
	}
	else
	{
		std::move(history.begin() + numSamples, history.end(), history.begin());
		std::copy(samples, samples + numSamples, history.end() - numSamples);
	}

	// Decimate to 12 kHz by averaging groups of 4 samples (a simple anti-aliasing lowpass)
	double energy = 0.0;
	for(long i=0; i<kFrameLength; i++)
	{
		double sum = 0.0;
		for(long j=0; j<kDecimation; j++)
			sum += history[i*kDecimation + j];
		frame[i] = inputGain * sum / kDecimation;
		energy += frame[i] * frame[i];
	}

	double raw = std::sqrt(energy / kFrameLength) < kMinRMS ? 0.0 : estimate();

	// A median over consecutive estimates removes isolated octave jumps; silence resets it
	if(raw <= 0.0)
	{
		recent.clear();
		currentF0 = 0.0;
		return currentF0;
	}
	recent.push_back(raw);
	if((long)recent.size() > kSmoothing)
		recent.erase(recent.begin());
	std::vector<double> sorted(recent);
	std::sort(sorted.begin(), sorted.end());
	currentF0 = sorted[sorted.size() / 2];
	return currentF0;
}

// YIN steps 2-5: difference function, cumulative mean normalization, absolute threshold,
// and parabolic interpolation around the chosen lag
double PitchTracker::estimate()
{
	long tau, j;

	diff[0] = 0.0;
	for(tau=1; tau<=kMaxLag + 1; tau++)
	{
		double sum = 0.0;
		for(j=0; j<kWindow; j++)
		{
			double delta = frame[j] - frame[j + tau];
			sum += delta * delta;
		}
		diff[tau] = sum;
	}

	double runningSum = 0.0;
	diff[0] = 1.0;
	for(tau=1; tau<=kMaxLag + 1; tau++)
	{
		runningSum += diff[tau];
		diff[tau] = runningSum > 0.0 ? diff[tau] * tau / runningSum : 1.0;
	}

	// First dip below the threshold, then follow it down to its local minimum
	long best = -1;
	for(tau=kMinLag; tau<=kMaxLag; tau++)
	{
		if(diff[tau] < kYinThreshold)
		{
			while(tau + 1 <= kMaxLag && diff[tau + 1] < diff[tau])
				tau++;
			best = tau;
			break;
		}
	}
	if(best < 0)
		return 0.0;		// no clear periodicity: unvoiced or noisy

	double refined = best;
	double below = diff[best - 1], at = diff[best], above = diff[best + 1];
	double curvature = below - 2.0 * at + above;
	if(curvature > 0.0)
		refined += 0.5 * (below - above) / curvature;

	return kAnalysisRate / refined;
}
