/*
 *  RhoticDetector.cpp
 *  VowelViz
 *
 *  Copyright (c) 2026 Tanner Plauché. All rights reserved.
 */

#include "RhoticDetector.h"

#include <algorithm>
#include <cmath>
#include <vector>

#define kPitchHistory			150		// ~3 s of pitched updates (~47 per second)
#define kAdultPitchSplit		165.0	// Hz; below this an adult speaker is treated as male
#define kWindow					5		// smoothing window of voiced updates (~100 ms)
#define kOnVotes				3		// raw detections in the window needed to turn on
#define kOffVotes				1		// at or below this many it turns off (hysteresis)
#define kMaxF3Step				60.0	// Hz between updates; larger changes mean the formants are still moving

struct RhoticLimits { double maxF3; double maxF1; };

static const RhoticLimits kMale		= { 2050.0, 600.0 };
static const RhoticLimits kFemale	= { 2225.0, 650.0 };
static const RhoticLimits kChild	= { 2600.0, 700.0 };

bool RhoticDetector::update(bool voiced, double f0, double f1, double f3, int voiceType)
{
	if(f0 > 0.0)
	{
		pitchHistory.push_back(f0);
		if(pitchHistory.size() > kPitchHistory)
			pitchHistory.pop_front();
	}

	// Silence ends any r-colored stretch
	if(!voiced)
	{
		recent.clear();
		previousF3 = 0;
		rhotic = false;
		return rhotic;
	}

	bool settled = previousF3 > 0 && std::fabs(f3 - previousF3) < kMaxF3Step;
	previousF3 = f3;

	RhoticLimits limits;
	if(voiceType == 3)
		limits = kChild;
	else
		limits = speakerPitch() < kAdultPitchSplit ? kMale : kFemale;

	recent.push_back(settled && f3 < limits.maxF3 && f1 < limits.maxF1);
	if(recent.size() > kWindow)
		recent.pop_front();

	long votes = std::count(recent.begin(), recent.end(), true);
	if(!rhotic && votes >= kOnVotes)
		rhotic = true;
	else if(rhotic && votes <= kOffVotes)
		rhotic = false;
	return rhotic;
}

// Median of recent pitch values; before any pitch is heard, assume the higher (female) range
double RhoticDetector::speakerPitch() const
{
	if(pitchHistory.empty())
		return kAdultPitchSplit;
	std::vector<double> sorted(pitchHistory.begin(), pitchHistory.end());
	std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
	return sorted[sorted.size() / 2];
}
