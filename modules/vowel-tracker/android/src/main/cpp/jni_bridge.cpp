//
//  jni_bridge.cpp
//  VowelViz
//
//  Copyright (c) 2026 Tanner Plauché. All rights reserved.
//
//  JNI entry points for com.tannerplauche.voweltracker.NativeTracker
//

#include <jni.h>

#include "vowel_tracker.h"

static vt_tracker *fromHandle(jlong handle)
{
	return reinterpret_cast<vt_tracker *>(handle);
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_create(JNIEnv *, jobject, jint voiceType)
{
	return reinterpret_cast<jlong>(vt_create(voiceType));
}

JNIEXPORT void JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_destroy(JNIEnv *, jobject, jlong handle)
{
	vt_destroy(fromHandle(handle));
}

JNIEXPORT void JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_process(JNIEnv *env, jobject, jlong handle, jfloatArray samples, jint count)
{
	auto *data = static_cast<float *>(env->GetPrimitiveArrayCritical(samples, nullptr));
	if(data == nullptr)
		return;
	vt_process(fromHandle(handle), data, count);
	env->ReleasePrimitiveArrayCritical(samples, data, JNI_ABORT);
}

// Writes [f1, f2, f3, f0, voicing, rhotic, captureTime] into 'out'
JNIEXPORT void JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_get(JNIEnv *env, jobject, jlong handle, jdoubleArray out)
{
	vt_formants formants = vt_get(fromHandle(handle));
	jdouble values[7] = { formants.f1, formants.f2, formants.f3, formants.f0, (jdouble)formants.voicing, (jdouble)formants.rhotic, formants.capture_time };
	env->SetDoubleArrayRegion(out, 0, 7, values);
}

JNIEXPORT jint JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_captureStart(JNIEnv *env, jobject, jlong handle, jstring path)
{
	const char *chars = env->GetStringUTFChars(path, nullptr);
	int result = vt_capture_start(fromHandle(handle), chars);
	env->ReleaseStringUTFChars(path, chars);
	return result;
}

JNIEXPORT jdouble JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_captureStop(JNIEnv *, jobject, jlong handle)
{
	return vt_capture_stop(fromHandle(handle));
}

JNIEXPORT void JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_setVoiceType(JNIEnv *, jobject, jlong handle, jint voiceType)
{
	vt_set_voice_type(fromHandle(handle), voiceType);
}

JNIEXPORT void JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_setVoicingSensitivity(JNIEnv *, jobject, jlong handle, jdouble sensitivity)
{
	vt_set_voicing_sensitivity(fromHandle(handle), sensitivity);
}

JNIEXPORT void JNICALL
Java_com_tannerplauche_voweltracker_NativeTracker_setInputGain(JNIEnv *, jobject, jlong handle, jdouble gain)
{
	vt_set_input_gain(fromHandle(handle), gain);
}

}
