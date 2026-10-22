// Copyright (c) 2026 Tanner Plauché. All rights reserved.

package com.tannerplauche.voweltracker

/** JNI bindings for the shared C++ tracker (cpp/vowel_tracker.h). */
internal object NativeTracker {
  init {
    System.loadLibrary("voweltracker")
  }

  external fun create(voiceType: Int): Long
  external fun destroy(handle: Long)
  external fun process(handle: Long, samples: FloatArray, count: Int)
  external fun get(handle: Long, out: DoubleArray)
  external fun captureStart(handle: Long, path: String): Int
  external fun captureStop(handle: Long): Double
  external fun setVoiceType(handle: Long, voiceType: Int)
  external fun setVoicingSensitivity(handle: Long, sensitivity: Double)
  external fun setInputGain(handle: Long, gain: Double)
}
