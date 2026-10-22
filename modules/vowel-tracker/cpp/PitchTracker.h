/*
 *  PitchTracker.h
 *  VowelViz
 *
 *  Copyright (c) 2026 Tanner Plauché. All rights reserved.
 */
// PitchTracker estimates the fundamental frequency (F0) of the voice with the YIN algorithm
// (de Cheveigné & Kawahara, "YIN, a fundamental frequency estimator for speech and music",
// JASA 111(4), 2002). Input is 48 kHz audio; analysis runs at 12 kHz on the most recent ~43 ms.

#ifndef __PITCHTRACKER_H_
#define __PITCHTRACKER_H_

#include <vector>

class PitchTracker
{
	public:
		PitchTracker ();

		// Appends 48 kHz samples and re-estimates. Returns the smoothed F0 in Hz, or 0 when no pitch is found.
		double	process(const float *samples, long numSamples, double inputGain);

		double	currentF0;

	private:
		std::vector<float>	history;		// most recent kHistoryLength input samples (48 kHz)
		std::vector<double>	frame;			// decimated analysis frame (12 kHz)
		std::vector<double>	diff;			// YIN difference function, then its cumulative mean normalized form
		std::vector<double>	recent;			// last few raw estimates, for median smoothing

		double	estimate();
};

#endif
