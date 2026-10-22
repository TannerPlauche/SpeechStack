/*
 *  VoiceDetector.h
 *  VowelViz
 *
 *  Copyright (c) 2004-2026 Tanner Plauché. All rights reserved.
 */
 
 // This implementation of a voicing detection algorithm is largely based on that described by
 // Kamran Mustafa in his master's thesis "Robust Formant Tracking for Continuous Speech"
 // The algorithm (specifically the statically defined filter coefficients) assumes an input audio sample rate of 48 kHz
 
 // Currently, some of VoiceDetector's instance variables are set directly by other parts of Formant Visualizer (particularly the user-interface).
 // Better encapsulation of the instance variables could be achieved by manipulating them through class methods.


#ifndef __VOICEDETECTOR_H_
#define __VOICEDETECTOR_H_

#include "SpeechAnalysisConstants.h"

#include <math.h>
#include <stdlib.h>

#define kFrameLength				256
#define kFrameInterval				128
#define VDfilterLen					21

#define kVDMaleCutoff				700.0
#define kVDHiMaleBandwidth			5300.0
#define kVDFemaleCutoff				1120.0
#define kVDHiFemaleBandwidth		4880.0

#define kLogRatioMaleVoiced			0.3           // .2
#define kLogRatioMaleUnvoiced		0.1           // .1
#define kLogRatioFemaleVoiced		0.3           // .3
#define kLogRatioFemaleUnvoiced		0.2           // .2

#define kAutoCorrThresholdMale		0.24          // .6
#define kAutoCorrThresholdFemale	0.24          // .25

class VoiceDetector
{
	public:
		VoiceDetector ();
		~VoiceDetector ();
		VoiceDetector (const VoiceDetector &) = delete;
		VoiceDetector &operator= (const VoiceDetector &) = delete;
		
		int			FTSwitch, voicingStart[kMaxVowels], voicingStop[kMaxVowels];
		double		*dataBuff = nullptr, autoCorrThreshold;
		
		void  detectVoicing(double *inData, long numSamples, bool lastBuffer);
		void  InitData();
		void  InitDataBuffer();
		void  InitFilterBuffs();
		double max(double *data, long dataLength);
	
	// these were globals in Formant Visualizer 0.9.3
		long			voiceIdx, *voicingHist = nullptr, vowelIdx = 0, prevVowelIdx;
		double			sampleRate, gNumSeconds;	//, *vowelHist;


	private:
		double			pastSpeechData[kFrameInterval];
		double			logRatio;
		double			*toBeHPIIRFiltered = nullptr, *filteredHP = nullptr;
        double			*toBeLPIIRFiltered = nullptr, *filteredLP = nullptr;
		bool			voiced;
		
		// IIR filter
		// parameters toBeIIRFiltered and filtered allow allocated memory to be supplied to this function, which enables a long stream of data to be processed in chunks
		// as though it had all been processed as a single block
		// 'b' represents the numerator coefficients, and 'a' the denominator coefficients of an IIR filter
		void filter (double *b, long blength, double *a, long alength, double *indata, double *outdata, long datalength, double *toBeIIRFiltered, double *filtered);
		
		// return the rms value of 'indata'
		double rms(double *indata, long dataLength);
		
		// compute the raw, unscaled cross-correlation between 'xData' and 'yData'
		bool xcorr(double *xData, double *yData, double *outData, long numSamples, double threshold);
};


#endif

