// Copyright (c) 2026 Tanner Plauché. All rights reserved.

package com.tannerplauche.voweltracker

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioRecord
import android.media.MediaPlayer
import android.media.MediaRecorder
import android.net.Uri
import android.util.Log
import androidx.core.content.ContextCompat
import expo.modules.kotlin.Promise
import expo.modules.kotlin.exception.CodedException
import expo.modules.kotlin.exception.Exceptions
import expo.modules.kotlin.functions.Queues
import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition
import kotlin.concurrent.thread

private const val TAG = "VowelTracker"
private const val SAMPLE_RATE = 48000
private const val CHUNK_SIZE = 1024
private const val VOICE_FEMALE = 2

class VowelTrackerModule : Module() {
  private val handle = NativeTracker.create(VOICE_FEMALE)

  @Volatile private var running = false
  private var recordThread: Thread? = null
  private var soundPlayer: MediaPlayer? = null

  private val context: Context
    get() = appContext.reactContext ?: throw Exceptions.ReactContextLost()

  override fun definition() = ModuleDefinition {
    Name("VowelTracker")

    Events("onFormants")

    AsyncFunction("start") {
      startRecording()
    }

    AsyncFunction("stop") {
      stopRecording()
    }

    Function("setVoiceType") { voiceType: Int ->
      NativeTracker.setVoiceType(handle, voiceType)
    }

    Function("setVoicingSensitivity") { sensitivity: Double ->
      NativeTracker.setVoicingSensitivity(handle, sensitivity)
    }

    Function("setInputGain") { gain: Double ->
      NativeTracker.setInputGain(handle, gain)
    }

    // Records the tracker's input to a new WAV file until stopCapture. Returns the file's URI.
    AsyncFunction("startCapture") {
      val directory = java.io.File(context.cacheDir, "VowelCollector").apply { mkdirs() }
      val file = java.io.File(directory, "${java.util.UUID.randomUUID()}.wav")
      if (NativeTracker.captureStart(handle, file.absolutePath) != 0) {
        throw CaptureFileException(file.absolutePath)
      }
      Uri.fromFile(file).toString()
    }

    // Finishes the WAV file and returns its duration in seconds (-1 if nothing was recording)
    AsyncFunction("stopCapture") {
      NativeTracker.captureStop(handle)
    }

    AsyncFunction("playSound") { uri: String, promise: Promise ->
      soundPlayer?.release()
      val player = MediaPlayer()
      player.setAudioAttributes(
        AudioAttributes.Builder()
          .setUsage(AudioAttributes.USAGE_MEDIA)
          .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
          .build()
      )
      try {
        player.setDataSource(context, Uri.parse(uri))
        player.prepare()
      } catch (e: Exception) {
        player.release()
        promise.reject(InvalidSoundUriException(uri, e))
        return@AsyncFunction
      }
      player.setOnCompletionListener {
        it.release()
        if (soundPlayer === it) soundPlayer = null
      }
      player.start()
      soundPlayer = player
      promise.resolve(null)
    }.runOnQueue(Queues.MAIN)

    OnDestroy {
      stopRecording()
      soundPlayer?.release()
      soundPlayer = null
      NativeTracker.destroy(handle)
    }
  }

  @Synchronized
  private fun startRecording() {
    if (running) return
    if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
      throw MicrophonePermissionException()
    }

    val source = pickAudioSource()
    val minBuffer = AudioRecord.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT)
    val record = AudioRecord(
      source,
      SAMPLE_RATE,
      AudioFormat.CHANNEL_IN_MONO,
      AudioFormat.ENCODING_PCM_FLOAT,
      maxOf(minBuffer, CHUNK_SIZE * 4 * Float.SIZE_BYTES)
    )
    if (record.state != AudioRecord.STATE_INITIALIZED) {
      record.release()
      throw NoAudioInputException()
    }
    Log.i(TAG, "Recording from source $source at ${record.sampleRate} Hz")

    record.startRecording()
    running = true
    recordThread = thread(name = "VowelTrackerInput", priority = Thread.MAX_PRIORITY) {
      val chunk = FloatArray(CHUNK_SIZE)
      val formants = DoubleArray(7)
      var filled = 0
      while (running) {
        val read = record.read(chunk, filled, CHUNK_SIZE - filled, AudioRecord.READ_BLOCKING)
        if (read < 0) {
          Log.e(TAG, "AudioRecord.read failed: $read")
          break
        }
        filled += read
        if (filled < CHUNK_SIZE) continue
        filled = 0

        NativeTracker.process(handle, chunk, CHUNK_SIZE)
        NativeTracker.get(handle, formants)
        sendEvent(
          "onFormants",
          mapOf(
            "f1" to formants[0],
            "f2" to formants[1],
            "f3" to formants[2],
            "f0" to formants[3],
            "voicing" to (formants[4] != 0.0),
            "rhotic" to (formants[5] != 0.0),
            "captureTime" to formants[6].takeIf { it >= 0 }
          )
        )
      }
      record.stop()
      record.release()
    }
  }

  @Synchronized
  private fun stopRecording() {
    running = false
    recordThread?.join()
    recordThread = null
  }

  // UNPROCESSED skips the device's noise suppression and gain control, which would distort formants.
  // VOICE_RECOGNITION is the next-cleanest source and is available everywhere.
  private fun pickAudioSource(): Int {
    val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    val supportsUnprocessed = audioManager.getProperty(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED) == "true"
    return if (supportsUnprocessed) MediaRecorder.AudioSource.UNPROCESSED else MediaRecorder.AudioSource.VOICE_RECOGNITION
  }
}

internal class MicrophonePermissionException :
  CodedException("Microphone permission has not been granted")

internal class NoAudioInputException :
  CodedException("No audio input is available")

internal class CaptureFileException(path: String) :
  CodedException("Could not create the recording file: $path")

internal class InvalidSoundUriException(uri: String, cause: Throwable) :
  CodedException("Could not play sound: $uri", cause)
