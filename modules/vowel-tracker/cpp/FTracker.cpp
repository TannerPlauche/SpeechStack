/*
 *  FTracker.cpp
 *  VowelViz
 *
 *  Copyright (c) 2004-2026 Tanner Plauché. All rights reserved.
 */
// FTracker implements the real-time formant estimation algorithm described by Akira Watanabe in his paper
// "Formant Estimation Method Using Inverse-Filter Control" (IEEE Transactions on Speech and Audio Processing, Vol. 9, No. 4, May 2001)
// The algorithm (specifically the statically defined filter coefficients) assumes an input audio sample rate of 48 kHz
// Spectral tilt control is included, but does not seem to be implemented properly--there is some ambiguity in Watanabe's description

#include "FTracker.h"
#include <algorithm>
#include <cstring>

/* Elliptical (IIR) highpass filter taps for fs = 12 kHz w/ cutoff @ 200 Hz */
double ellipHPtaps1[2][elliptaps] =
{
	{ 0.81753487065517, -4.89921991850633, 12.2390772627416, -16.3147844261819, 12.2390772627416, -4.89921991850633, 0.81753487065517},
	{ 1.0, -5.61198406422371, 13.1432497442329, -16.4419040438296, 11.5872068185456, -4.36171124340894, 0.685146387657346}
};

/* Elliptical (IIR) highpass filter taps for fs = 12 kHz w/ cutoff @ 300 Hz */
double ellipHPtaps2[2][elliptaps] =
{
	{ 0.743056283977261, -4.44607487933893, 11.0968456653008, -14.7876541024477, 11.0968456653008, -4.44607487933893, 0.743056283977261},
	{ 1.0, -5.41168145114193, 12.2470495752477, -14.8331334009683, 10.1396467465638, -3.70916951834772, 0.567325479232124}
};

/* Elliptical (IIR) lowpass filter taps for fs = 48 kHz w/ cutoff @ 5800 Hz */
double ellipLPtaps[2][elliptaps] =
{
	{ 0.00532263207750401, 0.000142177398006529, 0.00934454278207163, 0.00253232209250874, 0.00934454278207163, 0.000142177398006527, 0.00532263207750401},
	{ 1.0, -3.93598373022888, 7.1905928589662, -7.57228106043001, 4.80675751958126, -1.73430663570155, 0.277744365762675}
};

// ----------------------------------------------------------------------------
// FTracker
// ----------------------------------------------------------------------------

FTracker::FTracker (int ftswitch)
{
	FTSwitch = ftswitch;
	vd.FTSwitch = ftswitch;
	voicingHist = vd.voicingHist;
	
	inputGain = 1.0;
	
	gNumSeconds = kNUM_SECONDS, sampleRate = 0.25*kSAMPLE_RATE;
	gFramesPerBuffer = kFRAMES_PER_BUFFER, gFmntHistLength = kFmntHistLength, gNumChannels = kNUM_CHANNELS;
	
	formantHist = (double *) calloc(6*gFmntHistLength, sizeof(double));
	fmntLvlHist = (long *) calloc(6*gFmntHistLength, sizeof(long));
	
	int i;
	
	for(i=0; i<gFmntHistLength; i++)
	{
		formantHist[i] = kTypicalF1;
		formantHist[gFmntHistLength + i] = kTypicalF2;
		formantHist[2*gFmntHistLength + i] = kTypicalF3;
		formantHist[3*gFmntHistLength + i] = kTypicalF4;
		formantHist[4*gFmntHistLength + i] = kTypicalF5;
		formantHist[5*gFmntHistLength + i] = kTypicalF6;
	}
	
	formantIdx = 1;
	useVoicingInfo = 1;
	
	long numBytes = (long) ceilf(gNumSeconds * kSAMPLE_RATE)*gNumChannels*sizeof(double);
	formantTime = (double *) calloc(numBytes, 1);
	
	if( formantTime != NULL)
	{
		for(i=0; i<gFmntHistLength; i++)
		{
			formantTime[i] = (double)i*0.01;
		}
	}
	
//	if(sampleRate == 48000.0)
	{
		frameLen = kFrameLength;
		frameInt = kFrameInterval;
	}
	
	InitData();
	
	preemphTaps[0] = 1.0;
	preemphTaps[1] = -1.0;
	spectralTiltControl = false;
	fmntLvlFocus = 2.0;
	
	detectVoice = true;
	
	tau = 0.05;							// time constant (in seconds) for exponential averaging of formant estimates
	updateRate = (double)kFrameInterval/sampleRate;
	timeExpWeight = exp(-updateRate/tau);
}

FTracker::~FTracker ()
{
	free(dataBuffB);
	free(dataBuffA);
	free(FIRdataBuff);
	free(toBeFIRFiltered);
	free(formantHist);
	free(fmntLvlHist);
	free(formantTime);
}

void FTracker::InitData()
{
	long		numBytes, i;
	
	free(dataBuffB);
	dataBuffB = (double *) calloc(1, (long)ceilf(kSAMPLE_RATE*gNumSeconds/4 + elliptaps)*sizeof(double));
	
	numBytes = (long)(ceilf(kSAMPLE_RATE*gNumSeconds + elliptaps + kFrameInterval)*sizeof(double));
		
	free(FIRdataBuff);
	FIRdataBuff = (double *) calloc(numBytes, 1);
	
	free(toBeFIRFiltered);
	toBeFIRFiltered = (double *) calloc(numBytes, 1);
	
	free(dataBuffA);
	dataBuffA = (double *) calloc(numBytes, 1);
		
	vd.InitDataBuffer();
		
	typicalFormants[0] = kTypicalF1;
	typicalFormants[1] = kTypicalF2;
	typicalFormants[2] = kTypicalF3;
	typicalFormants[3] = kTypicalF4;
	typicalFormants[4] = kTypicalF5;
	typicalFormants[5] = kTypicalF6;
	
	Formants[0] = typicalFormants[0];
	Formants[1] = typicalFormants[1];
	Formants[2] = typicalFormants[2];
	Formants[3] = typicalFormants[3];
	Formants[4] = typicalFormants[4];
	Formants[5] = typicalFormants[5];
	
	freqCount[0] = 0;
	freqCount[1] = 0;
	freqCount[2] = 0;
	freqCount[3] = 0;
	freqCount[4] = 0;
	freqCount[5] = 0;
	
	Bandwidths[0] = 400.0;
	Bandwidths[1] = 400.0;
	Bandwidths[2] = 400.0;
	Bandwidths[3] = 400.0;
	Bandwidths[4] = 400.0;
	Bandwidths[5] = 400.0;
	
	for(i=0; i<kFrameLength; i++)
	{
		freqs[i] = 0.0;
		W[i] = 0.0;
		zcpts[i] = 0.0;
		localData[i] = 0.0;
		frame1[i] = 0.0;
		frame2[i] = 0.0;
	}
//	for(i=0; i<kMaxVowels; i++)
//	{
//        vowelHist[i] = 0.0;
//        vowelHist[kMaxVowels+i] = 0.0;
//        vowelHist[2*kMaxVowels+i] = 0.0;
//    }
    vd.prevVowelIdx = 0;
    
	for(i=0; i<6; i++)
	{
		rmsValues[i] = 0.0;
		IF0BandWidths[i] = 0.0;
		formantSums[i] = Formants[i];
	}
	InitFilterBuffs();
}

void FTracker::InitFilterBuffs()
{
	long	i;
	
    for(i=0; i<elliptaps; i++)
    {
        u[i] = 0.0;
        FIR_u[i] = 0.0;
        LPIIR_u[i] = 0.0;
        HPIIR_u[i] = 0.0;
        toBeFIRFiltered[i] = 0.0;
		pastFIRData[i] = 0.0;
    }
	
	for(i=0; i<frameInt; i++)
	{
		pastSpeechData[i] = 0.0;
	}
}

void FTracker::syncVoicingDetector()
{
	vd.sampleRate = sampleRate;
	vd.gNumSeconds = gNumSeconds;
//	vd.vowelHist = vowelHist;
}

void FTracker::ComputeFormants (float *inData, long numSamples, bool lastBuffer)
{
	double			frame[kFrameLength], framep[kFrameLength], frame4[kFrameLength];//, frame5[kFrameLength];
	double			*HPfiltertaps, *LPfiltertaps;
	long			i, j, k, idx;
//	wxString 		msg;
	
	switch(FTSwitch)
	{
		case 1:
			HPfiltertaps = &ellipHPtaps1[0][0];
			break;
		
		case 2:
		case 3:
			HPfiltertaps = &ellipHPtaps2[0][0];
			break;
		
		default:
			HPfiltertaps = &ellipHPtaps1[0][0];
			break;
	}
	LPfiltertaps = &ellipLPtaps[0][0];
	
	//double	t1, t2, eTime;
	
	// lowpass filter the data to eliminate aliasing before decimation (fs is assumed to be 48 kHz)
	IIRfilterToDouble(LPfiltertaps, elliptaps, LPfiltertaps+elliptaps, elliptaps, inData, &dataBuffA[0], numSamples, LPIIR_u);

	// convert data to a 12 kHz sample rate
	decimate(4, &dataBuffA[0], numSamples, &dataBuffB[0]);
	numSamples = numSamples/4;
	
	for(i=0; i<numSamples; i++)
	{
		dataBuffB[i] = inputGain*dataBuffB[i];
	}
	
	if(useVoicingInfo)//detectVoice)
	{
		syncVoicingDetector();
		vd.detectVoicing(&dataBuffB[0], numSamples, lastBuffer);
	}
	
	// highpass filter the data to reduce the effects of the voice's fundamental frequency on the formant estimate
	IIRfilter(HPfiltertaps, elliptaps, HPfiltertaps+elliptaps, elliptaps, &dataBuffB[0], &dataBuffA[0], numSamples, HPIIR_u);
		
	// pre-emphasis filtering
	filter(&preemphTaps[0], 2, &dataBuffA[0], &dataBuffB[0], numSamples, true);
	
	long ltmp = numSamples - frameInt;
	for(i=0; i<frameInt; i++)
	{
		dataBuffA[i] = pastSpeechData[i];
		pastSpeechData[i] = dataBuffB[ltmp + i];
	}
	for(i=0; i<numSamples; i++)
	{
		dataBuffA[frameInt+i] = dataBuffB[i];
	}
	
	double tempNum = floor((double)(numSamples/kFrameInterval));
	numFrames = (long)tempNum;

	for(i=0; i<numFrames; i++)
	{
		idx = i*frameInt;
		
		if(!useVoicingInfo || (useVoicingInfo && voicingHist[formantIdx]))
		{
			// get an appropriate frame of data from the current pre-emphasized input data
			for(j=0; j<frameLen; j++)
			{
				frame[j] = dataBuffA[idx+j];
			}
					
			switch(FTSwitch)
			{
				case 1:					// adult male
					if(Formants[2] >= 2470.0)
						Bandwidths[2] = 5.0*Formants[2]-12300.0;
					else
						Bandwidths[2] = 50.0;
					// pre-filtering
					basicFilter(&frame[0], &framep[0], 6000.0, Bandwidths[2]);
					basicFilter(&framep[0], &framep[0], Formants[4], Bandwidths[2]);
					basicFilter(&framep[0], &framep[0], Formants[4], Bandwidths[4]);
					break;
				case 2:					// adult female
					if(Formants[2] >= 2470.0)
						Bandwidths[2] = 5.0*Formants[2]-12300.0;
					else
						Bandwidths[2] = 50.0;
					// pre-filtering
					basicFilter(&frame[0], &framep[0], 6000.0, Bandwidths[2]);
					basicFilter(&framep[0], &framep[0], Formants[4], Bandwidths[4]);
					break;
				case 3:					// child
					Bandwidths[2] = 2700.0;
					// pre-filtering
					basicFilter(&frame[0], &framep[0], 6000.0, Bandwidths[2]);
					break;
				default:					// adult male
					if(Formants[2] >= 2470.0)
						Bandwidths[2] = 5.0*Formants[2]-12300.0;
					else
						Bandwidths[2] = 50.0;
					// pre-filtering
					basicFilter(&frame[0], &framep[0], 6000.0, Bandwidths[2]);
					basicFilter(&framep[0], &framep[0], Formants[4], Bandwidths[2]);
					basicFilter(&framep[0], &framep[0], Formants[4], Bandwidths[4]);
					break;
			}
			Bandwidths[2] = 400.0;	// ???
			
			// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			// Large Loop
			// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			for(j=0; j<3; j++)
			{
				//float	f0 = framep[0];//60.0;
				fundamentalBlock(&Formants[2], &Formants[1], j, &framep[0], 3, 0, 1);
				fundamentalBlock(&Formants[1], &Formants[0], j, &framep[0], 3, 2, 0);
				fundamentalBlock(&Formants[3], &Formants[2], j, &framep[0], 1, 0, 2);

				if(spectralTiltControl)
				{
					switch(FTSwitch)
					{
						case 1:					// adult male
							basicFilter(&frame[0], &frame4[0], Formants[5], Bandwidths[5]);
							basicFilter(&frame4[0], &frame4[0], Formants[0], Bandwidths[0]);
							break;
							
						case 2:					// adult female
							basicFilter(&frame[0], &frame4[0], Formants[0], Bandwidths[0]);
							break;
							
						case 3:					// Child
							basicFilter(&frame[0], &frame4[0], Formants[0], Bandwidths[0]);
							break;
							
						default:					// adult male
							basicFilter(&frame[0], &frame4[0], Formants[5], Bandwidths[5]);
							basicFilter(&frame4[0], &frame4[0], Formants[0], Bandwidths[0]);
							break;					
					}
					// spectral tilt control (for fourth filter output when only the first three fundamental blocks are used)
					basicFilter(&frame4[0], &frame4[0], Formants[2], Bandwidths[2]);
					basicFilter(&frame4[0], &frame4[0], Formants[1], Bandwidths[1]);
					rmsValues[3] = rms(&frame4[0], frameLen);
					updateBWs(3);
					// end - spectral tilt control
				}
				sort(&Formants[0], 4);
			}

			for(j=0; j<6; j++)
			{
				formantSums[j] = Formants[j] + timeExpWeight*formantSums[j];
			}
		}
		
		if(useVoicingInfo)				// this conditional could be used above to avoid the formant processing altogether
										// if the voice detection is turned on and the current voicing is false
		{
			formantHist[4*gFmntHistLength+formantIdx] = 100.0*voicingHist[formantIdx];
			if(voicingHist[formantIdx])
			{
    			for(j=0; j<4; j++)
        		{
        			formantHist[j*gFmntHistLength+formantIdx] = formantSums[j]*updateRate/tau;;
        			fmntLvlHist[j*gFmntHistLength+formantIdx] = freqCount[j];
        			freqCount[j] = 0;
        		}
            }
            else
            {
				if(formantIdx == 1)
				{
					for(j=0; j<4; j++)
					{
						formantHist[j*gFmntHistLength] = formantHist[(j+1)*gFmntHistLength - 1];
					}
				}
                
				{
					for(j=0; j<4; j++)
					{
						formantHist[j*gFmntHistLength+formantIdx] = formantHist[j*gFmntHistLength+formantIdx-1];
						fmntLvlHist[j*gFmntHistLength+formantIdx] = freqCount[j];
						freqCount[j] = 0;
					}
				}
            }
            if(vd.vowelIdx > vd.prevVowelIdx)
            {
                for(k=vd.prevVowelIdx; k<vd.vowelIdx; k++)
                {
                    int avgLength = vd.voicingStop[k] - vd.voicingStart[k];
                    if(avgLength > 0)
                    {
//                        for(j=vd.voicingStart[k]; j<vd.voicingStop[k]; j++)
//                        {
//                            vowelHist[k] += formantHist[j];
//                            vowelHist[kMaxVowels + k] += formantHist[gFmntHistLength+j];
//                        }
//                        vowelHist[k] = vowelHist[k]/(double)avgLength;
//                        vowelHist[kMaxVowels + k] = vowelHist[kMaxVowels + k] / (double)avgLength;
//                        vowelHist[2*kMaxVowels + k] = vowelHist[kMaxVowels + k] / vowelHist[k];
//                        msg.Printf(wxT("VOWEL:\n\tF1 = %4.3f\tF2 = %4.3f\n\tF2/F1 = %4.3f \n"), vowelHist[k], vowelHist[kMaxVowels + k], vowelHist[2*kMaxVowels + k]);
//    		            MainTextBox->WriteText(msg);
                    }
                }
                vd.prevVowelIdx = vd.vowelIdx;
            }
			currentVoicing = voicingHist[formantIdx];
		}
		else
		{
            for(j=0; j<4; j++)
    		{
    			formantHist[j*gFmntHistLength+formantIdx] = formantSums[j]*updateRate/tau;;
    			fmntLvlHist[j*gFmntHistLength+formantIdx] = freqCount[j];
    			freqCount[j] = 0;
    		}
			currentVoicing = 1;
        }
		
		currentF1 = formantHist[formantIdx];
		currentF2 = formantHist[gFmntHistLength + formantIdx];
		currentF3 = formantHist[2*gFmntHistLength + formantIdx];
		
		formantIdx++;
		if(formantIdx >= kFmntHistLength)			// Start back at the beginning of the buffers
			formantIdx = 1;
	}
}

// IIR filter
// parameter IIRu enables a long stream of data to be processed in chunks
// as though it had all been processed as a single block
// 'b' represents the numerator coefficients, and 'a' the denominator coefficients of an IIR filter
void FTracker::IIRfilter (double *b, long blength, double *a, long alength, double *indata, double *outdata, long datalength, double *IIRu)
{
	long		i, j, filterOrder = std::max(blength, alength)-1, filterOrderMinus1;
	
	filterOrderMinus1 = filterOrder-1;
	
    for(i=0; i<datalength; i++)
    {
        outdata[i] = b[0]*indata[i]	+ IIRu[0];
        for(j=1; j<filterOrder; j++)
        {
            IIRu[j-1] = b[j]*indata[i] - a[j]*outdata[i] + IIRu[j];
        }
        IIRu[filterOrderMinus1] = b[filterOrder]*indata[i] - a[filterOrder]*outdata[i];
    }
}

// Same as IIRFilter, but with single-precision (32-bit float) input data
void FTracker::IIRfilterToDouble (double *b, long blength, double *a, long alength, float *indata, double *outdata, long datalength, double *IIRu)
{
	long		i, j, filterOrder = std::max(blength, alength)-1, filterOrderMinus1;
	
	filterOrderMinus1 = filterOrder-1;
	
    for(i=0; i<datalength; i++)
    {
        outdata[i] = b[0]*indata[i]	+ IIRu[0];
        for(j=1; j<filterOrder; j++)
        {
            IIRu[j-1] = b[j]*indata[i] - a[j]*outdata[i] + IIRu[j];
        }
        IIRu[filterOrderMinus1] = b[filterOrder]*indata[i] - a[filterOrder]*outdata[i];
    }
}


// FIR filter
// 'usePastData' may be used to indicate that data in past calls to this function will be used to offer continuity of the filter
// allowing a long stream of data to be processed in chunks as though it had all been processed as a single block
// if 'usePastData' is false, 'indata' is assumed to be zero before and after the current block of length datalength
void FTracker::filter (double *b, long blength, double *indata, double *outdata, long datalength, bool usePastData)
{
	double		FIRtemp;
	long		i, j, filterOrder = blength-1;
	
	if(usePastData)
	{
		for(i=0; i<filterOrder; i++)
		{
			toBeFIRFiltered[i] = pastFIRData[i];
		}
		
		memcpy(&toBeFIRFiltered[filterOrder], indata, datalength*sizeof(double));
//		for(i=0; i<datalength; i++)
//		{
//			toBeFIRFiltered[i+filterOrder] = indata[i];
//		}
		
		for(i=0; i<datalength; i++)
		{
			FIRtemp = 0.0;
			for(j=0; j<blength; j++)
			{
				FIRtemp += b[j]*toBeFIRFiltered[i+filterOrder-j];
			}
			outdata[i] = FIRtemp;
		}
	
		for(i=0; i< filterOrder; i++)
		{
			pastFIRData[i] = toBeFIRFiltered[i+datalength];
		}
	}
	else
	{
		for(i=0; i<filterOrder; i++)
		{
			FIRdataBuff[i] = 0.0;
		}
		
		memcpy(&FIRdataBuff[filterOrder], indata, datalength*sizeof(double));
//		for(i=0; i<datalength; i++)
//		{
//			FIRdataBuff[i+filterOrder] = indata[i];
//		}
		
		for(i=0; i<datalength; i++)
		{
			FIRtemp = 0.0;
			for(j=0; j<blength; j++)
			{
				FIRtemp += b[j]*FIRdataBuff[i+filterOrder-j];
			}
			outdata[i] = FIRtemp;
		}
	}
}

// basicFilter implements the basic filter described by Watanabe
void FTracker::basicFilter (double *indata, double *outdata, double frequency, double bandwidth)
{
	double		f = frequency/sampleRate, b = bandwidth/sampleRate;
	double		alpha, beta, gamma, coeffs[3];
	
	alpha = exp(-TwoPI*b);
	beta = -2.0*exp(-PI*b)*cos(TwoPI*f);
	gamma = 1.0/(1.0 + alpha + fabs(beta));
	
	coeffs[0] = gamma;
	coeffs[1] = gamma*beta;
	coeffs[2] = gamma*alpha;
	
	filter(&coeffs[0], 3, indata, outdata, frameLen, false);
}

// decimate assumes anti-aliasing filtering has already been performed on 'indata'
void FTracker::decimate (long N, double *indata, long datalength, double *outdata)
{
	long i, len = datalength/N;
	
	for (i=0; i<len; i++)
	{
		outdata[i] = indata[i*N];
	}
}

// Update the bandwidths of the spectral tilt control filters
void FTracker::updateBWs (long numBWs)
{
	long   i;
	double  R;
	for(i=0; i<numBWs; i++)
	{
        R = 20*log10(rmsValues[i+1]/rmsValues[i]);
        if(R < -20.)
            R = -20.;
        IF0BandWidths[i] = 100*R + 2200;
    }
}

double FTracker::rms(double *indata, long dataLength)
{
    double   rms = 0.;
    long    i;
    
    for(i=0; i<dataLength; i++)
        rms += indata[i]*indata[i];
    
    rms = sqrt(rms/(double)dataLength);
    
    return rms;
}

// Perform the fundamental block processing as described by Watanabe
void FTracker::fundamentalBlock (double *Fb, double *Fa, int loop, double *indata, int b, int a, int k)
{
	int			i;
	double		f1, f2;
	
	basicFilter(indata, localData, Formants[b], Bandwidths[b]);
	basicFilter(localData, localData, Formants[a], Bandwidths[a]);
	
	// spectral tilt control
	if(spectralTiltControl)
	{
    	rmsValues[k] = rms(localData, frameLen);
    	basicFilter(localData, localData, 60, IF0BandWidths[k]);
    }
	// end - spectral tilt control
	
	f1 = Formants[k];
	f2 = Formants[k+1];
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Small Loop
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	for(i=0; i<3; i++)
	{
		basicFilter(localData, &frame1[0], f1, Bandwidths[k]);
		f2 = frq(&frame1[0], loop, k+1, i, f2);
		
		basicFilter(localData, &frame2[0], f2, Bandwidths[k+1]);
		f1 = frq(&frame2[0], loop, k, i, f1);
		
		if(f2<f1)
		{
			double tmp = f1;
			f1 = f2;
			f2 = tmp;
		}
	}
	if(f2<6000.0)
	{
		*Fb = f2;
	}
	else
	{
//		MainTextBox->WriteText(wxT("f2 >= 6000!!!"));	// debug
	}
	if(f1<6000.0)
	{
		*Fa = f1;
	}
	else
	{
//		MainTextBox->WriteText(wxT("f1 >= 6000!!!"));	// debug
	}
}

// Estimate the mean frequency of the basic filter output
double FTracker::frq (double *indata, int loop, int formant, int step, double fm)
{
	//long		len = frameLen;
	double		w, tmp1, tmp2, dl, dh, fl, fh, newFreq = 0.;
	long		i;
	
	// obtain the zero-crossing frequency distribution
	long numFreqs = zerocross(&freqs[0], indata, formant);
	
	// initial estimate of the mean frequency from the power spectrum
	if(step == 0)
	{
		tmp1 = tmp2 = 0.0;
		for(i=1; i<frameLen; i++)
		{
			tmp1 += indata[i]*indata[i-1];
			tmp2 += indata[i]*indata[i];
		}
		if(tmp2 == 0.0)
			return fm;		// all the data is zero, so there's nothing more to do here
		w = sampleRate*acos(tmp1/tmp2);
		fm = w/TwoPI;
	}
	
	// implementation of the triangle-shaped weighting function for the zero-crossing frequency distribution
	if(formant == 0)
	{
		switch(FTSwitch)
		{
			case 1:						// adult male
				dl = 0.3*fm + 140.0;
				dh = dl;
				break;
			case 2:						// adult female
				dl = 0.3*fm + 240.0;
				dh = 0.3*fm + 340.0;
				break;
			case 3:						// Child
				dl = 0.3*fm + 340.0;
				dh = 0.3*fm + 440.0;
				break;
			default:						// adult male
				dl = 0.3*fm + 140.0;
				dh = dl;
				break;
		}
	}
	else if(formant < 3)
	{
		dl = 0.63*fm + 174.0;
		dh = dl;
	}
	else
	{
		dl = 0.84*fm + 230.0;
		dh = dl;
	}
	
	if(loop == 0)
	{
		if(step == 1)
		{
			dl = 0.75*dl;
			dh = 0.75*dh;
		}
		else if(step == 2)
		{
			dl = 0.5*dl;
			dh = 0.5*dh;
		}
	}
	else if(loop == 1)
	{
		if(step == 1)
		{
			dl = 0.7*dl;
			dh = 0.7*dh;
		}
		else if(step == 2)
		{
			dl = 0.4*dl;
			dh = 0.4*dh;
		}
	}
	else if(loop >= 2)
	{
		if(step == 1)
		{
			dl = 0.65*dl;
			dh = 0.65*dh;
		}
		else if(step == 2)
		{
			dl = 0.3*dl;
			dh = 0.3*dh;
		}
	}
	
	fl = fm - dl;
	fh = fm + dh;

	tmp1 = tmp2 = 0.0;	
	for(i=0; i<numFreqs; i++)
	{
		if(freqs[i] < fl)
			W[i] = 0.0;
		else if(freqs[i] < fm)
		{
			W[i] = (freqs[i] - fl)/dl;
			if((freqs[i] > (fm-dl/fmntLvlFocus)) && (loop >= 2))
				freqCount[formant] += 1;
		}
		else if(freqs[i] < fh)
		{
			W[i] = (fh - freqs[i])/dh;
			if((freqs[i] < (fm+dh/fmntLvlFocus)) && (loop >= 2))
				freqCount[formant] += 1;
		}
		else
			W[i] = 0.0;
			
		tmp1 += W[i];				// sum up the values in the weighting function
		tmp2 += W[i]*freqs[i];		// apply the weighting function to the frequency distribution
	}
	if (tmp1 == 0.0)  /* error!!! The weighting function should be nonzero, and we don't want to divide by zero! */
		return fm;
	else if(tmp2/tmp1 < 6000.0)
		newFreq = tmp2/tmp1;
	else
	{
//		MainTextBox->WriteText(wxString::Format(wxT("frq:  tmp1=%f, tmp2=%f"), tmp1, tmp2));	// debug
	}
	
	return newFreq;
}

// obtains a frequency distribution based on the zero-crossing points in *indata
// returns the number of frequencies in the distribution
long FTracker::zerocross (double *freqDist, double *indata, int formant)
{
	long		i, numFreqs, zcIdx, numPts;
	double		dT = 1.0/sampleRate, pt;//, zcpts[frameLen];
	
	if(indata[0] == 0.0)
		zcpts[0] = 0.0;
	else
		zcpts[0] = -1.0;
	
	zcIdx = 1;
		
	for(i=1; i<frameLen; i++)
	{
		if(indata[i] == 0.0)
		{
			zcpts[zcIdx] = (double)i*dT;
			zcIdx++;
		}
		else if (indata[i-1] != 0.0)
		{
			if(indata[i-1] < 0.0 && indata[i] > 0.0)
			{
				pt = interpl(indata[i-1], indata[i], (double)(i-1)*dT, (double)i*dT, 0.0);
				zcpts[zcIdx] = pt;
				zcIdx++;
			}
			else if(indata[i-1] > 0.0 && indata[i] < 0.0)
			{
				pt = interpl(indata[i], indata[i-1], (double)i*dT, (double)(i-1)*dT, 0.0);
				zcpts[zcIdx] = pt;
				zcIdx++;
			}
		}
	}
	
	numPts = zcIdx;
	zcIdx = 1;
	
	int			startIdx = 2;
	double		intvl;
	if(zcpts[0] == -1.0)
	{
		startIdx = 3;
		numPts--;
	}
		
	if(formant == 0)
	{
		intvl = (zcpts[startIdx-1] - zcpts[startIdx-2]);
		if(intvl > 1e-5)
			freqDist[0] = 0.5/intvl;
		else
			freqDist[0] = 1.2e4;
		
		for(i=startIdx; i<numPts; i++)
		{
			intvl = zcpts[i] - zcpts[i-1];
			if(intvl > 1e-5)
				freqDist[zcIdx] = 0.5/intvl;
			else
				freqDist[0] = 1.2e4;
			zcIdx++;
		}
	}
	else if(numPts >= 3)
	{
		intvl = (zcpts[startIdx-1] - zcpts[startIdx-3]);
		if(intvl > 1e-5)
			freqDist[0] = 1.0/intvl;
		else
			freqDist[0] = 1.2e4;
		
		for(i=startIdx; i<numPts; i++)
		{
			intvl = zcpts[i] - zcpts[i-2];
			if(intvl > 1e-5)
				freqDist[zcIdx] = 1.0/intvl;
			else
				freqDist[0] = 1.2e4;
			zcIdx++;
		}
	}
	
	numFreqs = zcIdx;
	sort(freqDist, numFreqs);
	
	return numFreqs;
}

void FTracker::sort (double *indata, long datalength)
{
	std::sort(indata, indata + datalength);		// In-place sort in ascending order
}

// recursively sorts the values in the *indata array in ascending order between the "left" and "right" entries
void FTracker::q_sort (double *indata, long left, long right)
{
	long		l_hold, r_hold, pvt;
	double		pivot;

	l_hold = left;
	r_hold = right;
	pivot = indata[left];
	while (left < right)
	{
		while ((indata[right] >= pivot) && (left < right))
		right--;
		if (left != right)
		{
			indata[left] = indata[right];
			left++;
		}
		while ((indata[left] <= pivot) && (left < right))
		left++;
		if (left != right)
		{
			indata[right] = indata[left];
			right--;
		}
	}
	indata[left] = pivot;
	pvt = left;
	left = l_hold;
	right = r_hold;
	if (left < pvt)
		q_sort(indata, left, pvt-1);
	if (right > pvt)
		q_sort(indata, pvt+1, right);
}

// linear interpolation function
double FTracker::interpl(double x1, double x2, double y1, double y2, double xpt)
{
	double		ypt, m, b;		// y = mx + b

	m = (y2 - y1)/(x2 - x1);
	b = y2 - m*x2;
	
	ypt = m*xpt + b;
	
	return ypt;
}


