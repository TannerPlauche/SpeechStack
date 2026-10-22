/*
 *  FTracker.h
 *  VowelViz
 *
 *  Copyright (c) 2004-2026 Tanner Plauché. All rights reserved.
 */
// FTracker implements the real-time formant estimation algorithm described by Akira Watanabe in his paper
// "Formant Estimation Method Using Inverse-Filter Control" (IEEE Transactions on Speech and Audio Processing, Vol. 9, No. 4, May 2001)
// The algorithm (specifically the statically defined filter coefficients) assumes an input audio sample rate of 48 kHz
// Spectral tilt control is included, but does not seem to be implemented properly--there is some ambiguity in Watanabe's description

// Currently, some of FTracker's instance variables are set directly by other parts of Formant Visualizer (particularly the user-interface).
// Better encapsulation of the instance variables could be achieved by manipulating them through class methods.
 
#ifndef __FTRACKER_H_
#define __FTRACKER_H_

#include "VoiceDetector.h"

#include <math.h>

#define elliptaps				7
#define kTypicalF1              750.0
#define kTypicalF2              1400.0
#define kTypicalF3              2000.0
#define kTypicalF4              3000.0
#define kTypicalF5              5200.0
#define kTypicalF6              5500.0

class FTracker
{
	public:
		FTracker (int ftswitch);
		~FTracker ();
		FTracker (const FTracker &) = delete;
		FTracker &operator= (const FTracker &) = delete;
		
		int			FTSwitch;
		long		numFrames;
		double		sampleRate, fmntLvlFocus, inputGain;
		double		*dataBuffB = nullptr, *dataBuffA = nullptr, typicalFormants[6];
		bool        spectralTiltControl;
		bool		detectVoice;
		VoiceDetector	vd;
		
		double	currentF1;
		double	currentF2;
		double	currentF3;
		long	currentVoicing;
	
		void syncVoicingDetector();
		void ComputeFormants(float *inData, long numSamples, bool lastBuffer);
		void InitData();
		void InitFilterBuffs();
		
	// these were globals in Formant Visualizer 0.9.3
		long		formantIdx, useVoicingInfo, *voicingHist;
		double		*formantHist = nullptr, /*vowelHist[3*kMaxVowels],*/ *formantTime = nullptr, gNumSeconds;
		long		*fmntLvlHist = nullptr, gFramesPerBuffer, gNumChannels, gFmntHistLength;

	private:		
		double		Formants[6], formantSums[6], Bandwidths[6], preemphTaps[2], pastSpeechData[kFrameInterval];
		double		*toBeFIRFiltered = nullptr, *FIRdataBuff = nullptr, pastFIRData[elliptaps];
		double		u[elliptaps], FIR_u[elliptaps], rmsValues[6], IF0BandWidths[6];
		double      HPIIR_u[elliptaps];
        double      LPIIR_u[elliptaps];
        long		freqCount[6], frameLen, frameInt;
				
	    double      freqs[kFrameLength], W[kFrameLength], zcpts[kFrameLength], localData[kFrameLength], frame1[kFrameLength], frame2[kFrameLength];
	    
		double		tau, updateRate, timeExpWeight;
	
		// IIR filter
		// parameter IIRu enables a long stream of data to be processed in chunks
        // as though it had all been processed as a single block
        // 'b' represents the numerator coefficients, and 'a' the denominator coefficients of an IIR filter
		void IIRfilterToDouble (double *b, long blength, double *a, long alength, float *indata, double *outdata, long datalength, double *IIRu);
		void IIRfilter (double *b, long blength, double *a, long alength, double *indata, double *outdata, long datalength, double *IIRu);
		
		// FIR filter
		// 'usePastData' may be used to indicate that data in past calls to this function will be used to offer continuity of the filter
        // allowing a long stream of data to be processed in chunks as though it had all been processed as a single block
        // if 'usePastData' is false, 'indata' is assumed to be zero before and after the current block of length datalength
		void filter (double *b, long blength, double *indata, double *outdata, long datalength, bool usePastData);
		
		// basicFilter implements the basic filter described by Watanabe
		void basicFilter (double *indata, double *outdata, double frequency, double bandwidth);		
		
		// decimate assumes anti-aliasing filtering has already been performed on 'indata'
		void decimate (long N, double *indata, long datalength, double *outdata);
		
		// Update the bandwidths of the spectral tilt control filters
		void updateBWs (long numBWs);
		
		double rms(double *indata, long dataLength);
		
		// Perform the fundamental block processing as described by Watanabe
		void fundamentalBlock (double *Fb, double *Fa, int loop, double *indata, int b, int a, int k);
		
		// Estimate the mean frequency of the 'indata'
		double frq (double *indata, int loop, int formant, int step, double oldFreq);
		
		// obtains a frequency distribution based on the zero-crossing points in *indata
		// returns the number of frequencies in the distribution
		long zerocross (double *freqDist, double *indata, int formant);
		
		void sort (double *indata, long datalength);
		
		// recursively sorts the values in the *indata array in ascending order between the "left" and "right" entries
		void q_sort (double *indata, long left, long right);
		
		// linear interpolation function
		double interpl(double x1, double x2, double y1, double y2, double xpt);
};


#endif

