/*
 *  VoiceDetector.cpp
 *  VowelViz
 *
 *  Copyright (c) 2004-2026 Tanner Plauché. All rights reserved.
 */
 // This implementation of a voicing detection algorithm is largely based on that described by
 // Kamran Mustafa in his master's thesis "Robust Formant Tracking for Continuous Speech"
 // The algorithm (specifically the statically defined filter coefficients) assumes an audio sample rate of 12 kHz

#include "VoiceDetector.h"
#include <algorithm>

// HP and LP filters for adult male speech voicing dectection -- cutoff @ 700 Hz
double Bhpf[VDfilterLen] = {0.095058813515697, -1.90117627031394, 18.0611745679824, -108.367047407895, 460.559951483552, -1473.79184474737,
								3684.47961186842, -7368.95922373683, 11974.5587385724, -15966.0783180965, 17562.6861499061, -15966.0783180965,
								11974.5587385724, -7368.95922373683, 3684.47961186842, -1473.79184474737, 460.559951483552, -108.367047407895,
								18.0611745679824, -1.90117627031394, 0.095058813515697};
double Blpf[VDfilterLen] = {2.17435676513405e-16, 4.34871353026811e-15, 4.1312778537547e-14, 2.47876671225282e-13, 1.05347585270745e-12,
								3.37112272866383e-12, 8.42780682165959e-12, 1.68556136433192e-11, 2.73903721703937e-11, 3.65204962271915e-11,
								4.01725458499107e-11, 3.65204962271915e-11, 2.73903721703937e-11, 1.68556136433192e-11, 8.42780682165959e-12,
								3.37112272866383e-12, 1.05347585270745e-12, 2.47876671225282e-13, 4.1312778537547e-14, 4.34871353026811e-15, 2.17435676513405e-16};
// IIR filter coefficients for both filters -- cutoff @ 700 Hz
double mAf[VDfilterLen] = {1.0, -15.3289531703472, 112.054767228983, -519.30191049516, 1710.91632398647, -4259.14356664994, 8311.42496578888,
								-13017.8126715723, 16618.6657728535, -17460.9442139471, 15180.2800385919, -10938.4022891476, 6520.67336063826,
								-3198.09936749963, 1277.78369470347, -409.471663397047, 102.768902260399, -19.4676526672464, 2.61835828791097,
								-0.222931971265494, 0.00903617802701207};

// HP and LP filters for adult female and child speech voicing dectection -- cutoff @ 1120 Hz
double fBhpf[VDfilterLen] = {0.0221471453224131, -0.442942906448262, 4.20795761125849, -25.2477456675509, 107.302919087091, -343.369341078692,
								858.423352696731, -1716.84670539346, 2789.87589626438, -3719.8345283525, 4091.81798118775, -3719.8345283525,
								2789.87589626438, -1716.84670539346, 858.423352696731, -343.369341078692, 107.302919087091, -25.2477456675509,
								4.20795761125849, -0.442942906448262, 0.0221471453224131};
double fBlpf[VDfilterLen] = {8.77186886724289e-13, 1.75437377344858e-11, 1.66665508477615e-10, 9.99993050865689e-10, 4.24997046617918e-09,
								1.35999054917734e-08, 3.39997637294334e-08, 6.79995274588669e-08, 1.10499232120659e-07, 1.47332309494212e-07,
								1.62065540443633e-07, 1.47332309494212e-07, 1.10499232120659e-07, 6.79995274588669e-08, 3.39997637294334e-08,
								1.35999054917734e-08, 4.24997046617918e-09, 9.99993050865689e-10, 1.66665508477615e-10, 1.75437377344858e-11, 8.77186886724289e-13};
// IIR filter coefficients for both filters -- cutoff @ 1120 Hz
double fAf[VDfilterLen] = {1.0, -12.5273034065732, 75.5154575075663, -290.954544097003, 802.895723910473, -1685.53198088335, 2791.28916871929,
								-3731.75628282877, 4088.59404229198, -3705.53001536624, 2792.10483294273, -1751.52487400826, 912.841221825459,
								-392.97436799034, 138.33638512522, -39.1979381482749, 8.7284660892631, -1.47174435954903, 0.176738349182224,
								-0.0134752490501677, 0.000490496045932084};

VoiceDetector::VoiceDetector ()
{
    int i;
	voiced = false;
	
	voiceIdx = 1;
	voicingHist = (long *) calloc(kFmntHistLength, sizeof(long));
	
	sampleRate = kSAMPLE_RATE;
	gNumSeconds = kNUM_SECONDS;
	prevVowelIdx = 0;
	autoCorrThreshold = kAutoCorrThresholdMale;
	
	for(i=0; i<kMaxVowels; i++)
	{
	   voicingStart[i] = voicingStop[i] = 0;
    }
	//voicingHist[0] = 0;
	FTSwitch = 1;
	
	InitData();
}

VoiceDetector::~VoiceDetector ()
{
	free(voicingHist);
	free(dataBuff);
	free(toBeHPIIRFiltered);
	free(filteredHP);
	free(toBeLPIIRFiltered);
	free(filteredLP);
}

void VoiceDetector::InitData()
{
	long		i, numBytes;
	
	InitDataBuffer();
		
	numBytes = (long)(ceilf(kFrameLength + VDfilterLen)*sizeof(double));
	
	free(toBeHPIIRFiltered);
	toBeHPIIRFiltered = (double *) calloc(numBytes, 1);
	free(filteredHP);
	filteredHP = (double *) calloc(numBytes, 1);
	free(toBeLPIIRFiltered);
	toBeLPIIRFiltered = (double *) calloc(numBytes, 1);
	free(filteredLP);
	filteredLP = (double *) calloc(numBytes, 1);
	
	InitFilterBuffs();
	
	for(i=0; i<kFrameInterval; i++)
	{
		pastSpeechData[i] = 0.0;
	}
	for(i=0; i<kMaxVowels; i++)
	{
	   voicingStart[i] = voicingStop[i] = 0;
    }
}

void  VoiceDetector::InitDataBuffer()
{
	long numBytes;

	numBytes = (long)(ceilf(sampleRate*gNumSeconds + VDfilterLen + kFrameInterval)*sizeof(double));
		
	free(dataBuff);
	dataBuff = (double *) calloc(numBytes, 1);
	
	return;
}

void VoiceDetector::InitFilterBuffs()
{
	long	i;
	
	for(i=0; i<VDfilterLen; i++)
	{
		toBeHPIIRFiltered[i] = 0.0;
		toBeLPIIRFiltered[i] = 0.0;
		filteredHP[i] = 0.0;
		filteredLP[i] = 0.0;
	}
}

void VoiceDetector::detectVoicing(double *inData, long numSamples, bool lastBuffer)
{
	long		i, j, idx, numFrames;
	long		ltmp = numSamples - kFrameInterval;
	double		*HPB, *HPA, *LPB, *LPA;
	double		lowBandwidth, highBandwidth, logRatioVoiced, logRatioUnvoiced;
	double		inFrame[kFrameLength], outLoFrame[kFrameLength], xcorrFrame[kFrameLength];
	double       HIoutFrame[kFrameLength];
	bool		anyLargeValue, tempVoiced;
//	wxString    msg;
	
	voicingHist[0] = 0;
	
	//peak = MAX(peak,max(inData, numSamples));
	
	for(i=0; i<kFrameInterval; i++)
	{
		dataBuff[i] = pastSpeechData[i];
		pastSpeechData[i] = inData[ltmp + i];//*peak;
	}
	for(i=0; i<numSamples; i++)
	{
		dataBuff[kFrameInterval+i] = inData[i];//*peak;
	}
	
	double tempNum = floor((double)(numSamples/kFrameInterval));
	numFrames = (long)tempNum;
	
	// Setup the relevant variables for the appropriate age/gender of the speaker
	switch(FTSwitch)
	{
		case 1:					// adult male
			LPB = Blpf;
			LPA = mAf;
			HPB = Bhpf;
			HPA = mAf;
			lowBandwidth = kVDMaleCutoff;
			highBandwidth = kVDHiMaleBandwidth;
			logRatioVoiced = kLogRatioMaleVoiced;
			logRatioUnvoiced = kLogRatioMaleUnvoiced;
//			autoCorrThreshold = kAutoCorrThresholdMale;
			break;
		
		case 2:					// adult female
			LPB = fBlpf;
			LPA = fAf;
			HPB = fBhpf;
			HPA = fAf;
			lowBandwidth = kVDFemaleCutoff;
			highBandwidth = kVDHiFemaleBandwidth;
			logRatioVoiced = kLogRatioFemaleVoiced;
			logRatioUnvoiced = kLogRatioFemaleUnvoiced;
//			autoCorrThreshold = kAutoCorrThresholdFemale;
			break;
		
		case 3:					// Child
			LPB = fBlpf;
			LPA = fAf;
			HPB = fBhpf;
			HPA = fAf;
			lowBandwidth = kVDFemaleCutoff;
			highBandwidth = kVDHiFemaleBandwidth;
			logRatioVoiced = kLogRatioFemaleVoiced;
			logRatioUnvoiced = kLogRatioFemaleUnvoiced;
//			autoCorrThreshold = kAutoCorrThresholdFemale;
			break;
			
		default:				// adult male
			LPB = Blpf;
			LPA = mAf;
			HPB = Bhpf;
			HPA = mAf;
			lowBandwidth = kVDMaleCutoff;
			highBandwidth = kVDHiMaleBandwidth;
			logRatioVoiced = kLogRatioMaleVoiced;
			logRatioUnvoiced = kLogRatioMaleUnvoiced;
//			autoCorrThreshold = kAutoCorrThresholdMale;
			break;
	}
		
	for(i=0; i<numFrames; i++)
	{
		idx = i*kFrameInterval;
		// get an appropriate frame of data from the current input data
		for(j=0; j<kFrameLength; j++)
		{
			inFrame[j] = dataBuff[idx+j];
		}
		
        // high-pass filter
		filter(HPB, VDfilterLen, HPA, VDfilterLen, inFrame, &HIoutFrame[0], kFrameLength, toBeHPIIRFiltered, filteredHP);
		
		// low-pass filter
		filter(LPB, VDfilterLen, LPA, VDfilterLen, inFrame, &outLoFrame[0], kFrameLength, toBeLPIIRFiltered, filteredLP);
	    
		// compute log ratio of the rms output of low-pass filter to high-pass filter
		logRatio = log10( (rms(outLoFrame, kFrameLength)/lowBandwidth) / (rms(HIoutFrame, kFrameLength)/highBandwidth));
		
		// determine voicing based on logRatio
		tempVoiced = voiced;
		if(!voiced && logRatio > logRatioVoiced)
			tempVoiced = true;
		else if(voiced && logRatio < logRatioUnvoiced)
			tempVoiced = false;
		// else -- voiced should not change
			    
		// compute (one-sided) autocorrelation function
		// anyLargeValue = xcorr(inFrame, inFrame, xcorrFrame, kFrameLength, autoCorrThreshold);
		anyLargeValue = xcorr(outLoFrame, outLoFrame, xcorrFrame, kFrameLength, autoCorrThreshold);
				
		// final voicing decision
		if(fabs(xcorrFrame[0]) > 0.0 && anyLargeValue && tempVoiced)
		{
			tempVoiced = true;
			voicingHist[voiceIdx] = 1;
		}
		else
		{
			tempVoiced = false;
			voicingHist[voiceIdx] = 0;
		}
		
		if(voiced && !tempVoiced)
		{
            if(voicingStart[vowelIdx])
            {
                voicingStop[vowelIdx] = voiceIdx;
	            vowelIdx++;
				if(vowelIdx >= kMaxVowels)
				{	
					vowelIdx = kMaxVowels-1;
					prevVowelIdx--;
					for(j=0; j<kMaxVowels-1; j++)
					{
						voicingStart[j] = voicingStart[j+1];
						voicingStop[j] = voicingStop[j+1];
//						vowelHist[j] = vowelHist[j+1];
					}
//					for(j=kMaxVowels; j<2*kMaxVowels-1; j++)
//					{
//						vowelHist[j] = vowelHist[j+1];
//						vowelHist[kMaxVowels + j] = vowelHist[kMaxVowels + j+1];
//					}
				}
            }
        }
        else if(!voiced && tempVoiced)
        {
            voicingStart[vowelIdx] = voiceIdx;
        }
        else if(lastBuffer && (i == (numFrames-1)))
        {
            if(voicingStart[vowelIdx])
            {
                voicingStop[vowelIdx] = voiceIdx;
	            vowelIdx++;
				if(vowelIdx >= kMaxVowels)
				{	
					vowelIdx = kMaxVowels-1;
					prevVowelIdx--;
					for(j=0; j<kMaxVowels-1; j++)
					{
						voicingStart[j] = voicingStart[j+1];
						voicingStop[j] = voicingStop[j+1];
//						vowelHist[j] = vowelHist[j+1];
					}
//					for(j=kMaxVowels; j<2*kMaxVowels-1; j++)
//					{
//						vowelHist[j] = vowelHist[j+1];
//						vowelHist[kMaxVowels + j] = vowelHist[kMaxVowels + j+1];
//					}
				}
            }
        }
        voiced = tempVoiced;
		voiceIdx++;
		if(voiceIdx >= kFmntHistLength)			// Start back at the beginning of the buffers
			voiceIdx = 1;
	}
}

// IIR filter
void VoiceDetector::filter (double *b, long blength, double *a, long alength, double *indata, double *outdata, long datalength, double *toBeIIRFiltered, double *filtered)
{
	double		FIRtemp, IIRtemp;
	long		i, j, filterOrder = std::max(blength, alength)-1;
	
	for(i=0; i<datalength; i++)
	{
		toBeIIRFiltered[i+filterOrder] = indata[i];
	}
	
	for(i=filterOrder; i<filterOrder+datalength; i++)
	{
		FIRtemp = IIRtemp = 0.0;
		for(j=0; j<blength; j++)
		{
			FIRtemp += b[j]*toBeIIRFiltered[i-j];
		}
		for(j=1; j<alength; j++)
		{
			IIRtemp += a[j]*filtered[i-j];
		}
		filtered[i] = FIRtemp - IIRtemp;
		outdata[i-filterOrder] = (double)filtered[i];
	}
	
	for(i=0; i< filterOrder; i++)
	{
		toBeIIRFiltered[i] = toBeIIRFiltered[i+datalength];
		filtered[i] = filtered[i+datalength];
	}
}

double VoiceDetector::rms(double *indata, long dataLength)
{
    double   rms = 0.;
    long    i;
    
    for(i=0; i<dataLength; i++)
        rms += indata[i]*indata[i];
    
    rms = sqrt(rms/(double)dataLength);
    
    return rms;
}

// compute the raw, unscaled cross-correlation between 'xData' and 'yData'
bool VoiceDetector::xcorr(double *xData, double *yData, double *outData, long numSamples, double threshold)
{
	long		m, n;
	double       trigger;
//	wxString    msg;
	
	outData[0] = 0.0;
	for(n=0; n<numSamples; n++)
	{
		outData[0] += xData[n]*yData[n];
	}
	if(outData[0] < 0.2)  // If the speech is unvoiced, the maximnum of the autocorrelation will be small
	{
	    return false;
    }
	
	trigger = threshold*outData[0];

	for(m=1; m<numSamples; m++)
	{
		outData[m] = 0.0;
		for(n=0; n<numSamples-m; n++)
		{
			outData[m] += xData[n+m]*yData[n];
		}
		if(m > 10 && outData[m] >= trigger)
		{
            return true;
        }
	}
	return false;
}

double VoiceDetector::max(double *data, long dataLength)
{
    long    i;
    double   max = 0.0;
    
    for(i=0; i<dataLength; i++)
		max = std::max(max, data[i]);
    return max;
}

