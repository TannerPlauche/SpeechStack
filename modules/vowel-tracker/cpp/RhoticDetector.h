/*
 *  RhoticDetector.h
 *  VowelViz
 *
 *  Copyright (c) 2026 Tanner Plauché. All rights reserved.
 */
// RhoticDetector flags r-colored vowels (/ɝ/, as in "bird") from the tracker's F1, F3 and pitch.
//
// The defining acoustic cue of an accurate American English /ɹ/ is a lowered F3 (and so a small
// F3-F2 distance). Where F3 falls depends on vocal tract size, so the F3 limit is chosen per speaker
// group: children from the app's Voice setting, and adult men vs women from the speaker's running
// median pitch. An F1 ceiling rejects open back vowels whose F3 the tracker sometimes under-reads.
// Only settled frames count: the tracker's smoothing makes F3 sweep up through low values for the
// first ~100 ms of every voiced sound, which would otherwise look rhotic.
//
// Thresholds were fit to this tracker's output on the Hillenbrand et al. (1995) recordings and
// checked with tools/formant-harness/rhotic.sh, counting a word as flagged if the marker turns on at
// all: /ɝ/ found in 100% of men's, 98% of women's and 93% of children's recordings; other vowels
// falsely flagged 0%, 3.4% (mostly rounded back vowels) and 0.4% of the time.

#ifndef __RHOTICDETECTOR_H_
#define __RHOTICDETECTOR_H_

#include <deque>

class RhoticDetector
{
	public:
		// Call once per tracker update. f0 is 0 when no pitch was found. Returns the smoothed decision.
		bool	update(bool voiced, double f0, double f1, double f3, int voiceType);

		bool	rhotic = false;

	private:
		std::deque<double>	pitchHistory;	// recent pitched F0 values, for the speaker's typical pitch
		std::deque<bool>	recent;			// raw decisions for the most recent voiced updates
		double				previousF3 = 0;	// F3 at the previous voiced update, 0 after silence

		double	speakerPitch() const;
};

#endif
