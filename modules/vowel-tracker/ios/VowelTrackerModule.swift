// Copyright (c) 2026 Tanner Plauché. All rights reserved.

import AVFoundation
import ExpoModulesCore

public class VowelTrackerModule: Module {
  private static let sampleRate = Double(VT_SAMPLE_RATE)
  private static let chunkSize = Int(VT_CHUNK_SIZE)

  private let tracker = vt_create(VT_VOICE_FEMALE)
  // Holds about 1 s of input so a stalled consumer can't overflow it
  private let ring = vt_ring_create(96_000)
  private let processingQueue = DispatchQueue(label: "com.tannerplauche.voweltracker.processing", qos: .userInteractive)

  // Replaced on every (re)start: after a route or hardware change, reusing an engine can leave its
  // graph holding the old format, and connecting with a mismatched format aborts the app.
  private var engine = AVAudioEngine()
  private var pendingRestart: DispatchWorkItem?
  private var sinkNode: AVAudioSinkNode?
  private var drainTimer: DispatchSourceTimer?
  private var converter: AVAudioConverter?
  private var inputFormat: AVAudioFormat?
  private var readBuffer = [Float](repeating: 0, count: 96_000)
  private var pending: [Float] = []

  private var wantsRunning = false
  private var soundPlayer: AVAudioPlayer?

  public func definition() -> ModuleDefinition {
    Name("VowelTracker")

    Events("onFormants")

    // start/stop and the notification-driven restarts all run on the main queue so they can't interleave
    AsyncFunction("start") {
      self.wantsRunning = true
      try self.startEngine()
    }.runOnQueue(.main)

    AsyncFunction("stop") {
      self.wantsRunning = false
      self.stopEngine()
    }.runOnQueue(.main)

    Function("setVoiceType") { (voiceType: Int) in
      vt_set_voice_type(self.tracker, Int32(voiceType))
    }

    Function("setVoicingSensitivity") { (sensitivity: Double) in
      vt_set_voicing_sensitivity(self.tracker, sensitivity)
    }

    Function("setInputGain") { (gain: Double) in
      vt_set_input_gain(self.tracker, gain)
    }

    // Records the tracker's input to a new WAV file until stopCapture. Returns the file's URI.
    AsyncFunction("startCapture") { () -> String in
      let directory = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("VowelCollector", isDirectory: true)
      try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
      let url = directory.appendingPathComponent("\(UUID().uuidString).wav")
      guard vt_capture_start(self.tracker, url.path) == 0 else {
        throw CaptureFileException(url.path)
      }
      return url.absoluteString
    }

    /// Finishes the WAV file and returns its duration in seconds (-1 if nothing was recording).
    AsyncFunction("stopCapture") { () -> Double in
      vt_capture_stop(self.tracker)
    }

    // Plays a bundled vowel recording. Uses AVAudioPlayer directly so the shared
    // audio session (and the live input) stays configured the way this module set it up.
    AsyncFunction("playSound") { (uri: String) in
      guard let url = URL(string: uri) else {
        throw InvalidSoundUriException(uri)
      }
      let player = try AVAudioPlayer(contentsOf: url)
      player.prepareToPlay()
      player.play()
      self.soundPlayer = player
    }.runOnQueue(.main)

    OnCreate {
      let center = NotificationCenter.default
      center.addObserver(self, selector: #selector(self.handleInterruption(_:)),
                         name: AVAudioSession.interruptionNotification, object: nil)
      center.addObserver(self, selector: #selector(self.handleConfigurationChange(_:)),
                         name: .AVAudioEngineConfigurationChange, object: nil)
      center.addObserver(self, selector: #selector(self.handleMediaServicesReset(_:)),
                         name: AVAudioSession.mediaServicesWereResetNotification, object: nil)
    }

    OnDestroy {
      NotificationCenter.default.removeObserver(self)
      self.wantsRunning = false
      self.stopEngine()
      self.processingQueue.sync {
        vt_destroy(self.tracker)
        vt_ring_destroy(self.ring)
      }
    }
  }

  // MARK: - Engine

  private func startEngine() throws {
    guard Self.hasRecordPermission() else {
      throw MicrophonePermissionException()
    }
    stopEngine()

    let session = AVAudioSession.sharedInstance()
    // Default mode (not .voiceChat) matches the original app's RemoteIO setup the tracker was tuned with.
    try session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker])
    try? session.setPreferredSampleRate(Self.sampleRate)
    try? session.setPreferredIOBufferDuration(Double(Self.chunkSize) / Self.sampleRate)
    try session.setActive(true)

    engine = AVAudioEngine()
    let input = engine.inputNode
    // Connect at the hardware's current input format; a stale format would make connect() abort
    let hardware = input.inputFormat(forBus: 0)
    guard hardware.sampleRate > 0, hardware.channelCount > 0,
          let format = AVAudioFormat(standardFormatWithSampleRate: hardware.sampleRate, channels: hardware.channelCount) else {
      throw NoAudioInputException()
    }
    inputFormat = format

    // The sink receives audio on the real-time thread; copy channel 0 into the ring and return.
    let ring = self.ring
    let sink = AVAudioSinkNode { _, frameCount, audioBufferList in
      let buffers = UnsafeMutableAudioBufferListPointer(UnsafeMutablePointer(mutating: audioBufferList))
      guard let first = buffers.first, let data = first.mData else {
        return noErr
      }
      let stride = max(Int(first.mNumberChannels), 1)
      vt_ring_write(ring, data.assumingMemoryBound(to: Float.self), Int(frameCount), stride)
      return noErr
    }
    engine.attach(sink)
    engine.connect(input, to: sink, format: format)
    sinkNode = sink

    // Hardware that can't run at 48 kHz (e.g. some external mics) gets resampled before tracking.
    let monoInput = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: format.sampleRate, channels: 1, interleaved: false)!
    let trackerFormat = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: Self.sampleRate, channels: 1, interleaved: false)!
    converter = format.sampleRate == Self.sampleRate ? nil : AVAudioConverter(from: monoInput, to: trackerFormat)

    processingQueue.sync {
      vt_ring_clear(self.ring)
      self.pending.removeAll(keepingCapacity: true)
    }

    engine.prepare()
    try engine.start()

    let timer = DispatchSource.makeTimerSource(queue: processingQueue)
    timer.schedule(deadline: .now(), repeating: .milliseconds(10))
    timer.setEventHandler { [weak self] in self?.drain() }
    timer.resume()
    drainTimer = timer
  }

  private func stopEngine() {
    drainTimer?.cancel()
    drainTimer = nil
    if engine.isRunning {
      engine.stop()
    }
    if let sink = sinkNode {
      engine.disconnectNodeInput(sink)
      engine.detach(sink)
      sinkNode = nil
    }
  }

  // Runs on processingQueue
  private func drain() {
    let count = readBuffer.withUnsafeMutableBufferPointer { buffer in
      vt_ring_read(ring, buffer.baseAddress, buffer.count)
    }
    if count == 0 {
      return
    }

    if let converter, let inputFormat {
      pending.append(contentsOf: resample(count: count, with: converter, inputRate: inputFormat.sampleRate))
    } else {
      pending.append(contentsOf: readBuffer[0..<count])
    }

    var offset = 0
    while pending.count - offset >= Self.chunkSize {
      pending.withUnsafeBufferPointer { buffer in
        vt_process(tracker, buffer.baseAddress! + offset, Self.chunkSize)
      }
      offset += Self.chunkSize

      let formants = vt_get(tracker)
      sendEvent("onFormants", [
        "f1": formants.f1,
        "f2": formants.f2,
        "f3": formants.f3,
        "f0": formants.f0,
        "voicing": formants.voicing != 0,
        "rhotic": formants.rhotic != 0,
        "captureTime": formants.capture_time >= 0 ? formants.capture_time : nil
      ])
    }
    pending.removeFirst(offset)
  }

  private func resample(count: Int, with converter: AVAudioConverter, inputRate: Double) -> [Float] {
    let inFormat = converter.inputFormat
    guard let inBuffer = AVAudioPCMBuffer(pcmFormat: inFormat, frameCapacity: AVAudioFrameCount(count)) else {
      return []
    }
    inBuffer.frameLength = AVAudioFrameCount(count)
    readBuffer.withUnsafeBufferPointer { source in
      inBuffer.floatChannelData![0].update(from: source.baseAddress!, count: count)
    }

    let capacity = AVAudioFrameCount(Double(count) * Self.sampleRate / inputRate) + 64
    guard let outBuffer = AVAudioPCMBuffer(pcmFormat: converter.outputFormat, frameCapacity: capacity) else {
      return []
    }
    var supplied = false
    var error: NSError?
    converter.convert(to: outBuffer, error: &error) { _, status in
      if supplied {
        status.pointee = .noDataNow
        return nil
      }
      supplied = true
      status.pointee = .haveData
      return inBuffer
    }
    return Array(UnsafeBufferPointer(start: outBuffer.floatChannelData![0], count: Int(outBuffer.frameLength)))
  }

  private static func hasRecordPermission() -> Bool {
    if #available(iOS 17.0, *) {
      return AVAudioApplication.shared.recordPermission == .granted
    }
    return AVAudioSession.sharedInstance().recordPermission == .granted
  }

  // MARK: - Notifications

  @objc private func handleInterruption(_ notification: Notification) {
    guard let raw = notification.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
          let type = AVAudioSession.InterruptionType(rawValue: raw),
          type == .ended else {
      return
    }
    restartIfWanted()
  }

  // Sent when the input route or hardware format changes (headset plugged in, etc.)
  @objc private func handleConfigurationChange(_ notification: Notification) {
    // Only the current engine matters; replaced engines can still post on their way out
    guard notification.object as AnyObject? === engine else {
      return
    }
    restartIfWanted()
  }

  @objc private func handleMediaServicesReset(_ notification: Notification) {
    restartIfWanted()
  }

  // Route and configuration changes often arrive in bursts while the hardware settles, so restart
  // once, shortly after the last one
  private func restartIfWanted() {
    DispatchQueue.main.async {
      self.pendingRestart?.cancel()
      let work = DispatchWorkItem { [weak self] in
        guard let self, self.wantsRunning else {
          return
        }
        do {
          try self.startEngine()
        } catch {
          log.error("VowelTracker: could not restart audio input: \(error)")
        }
      }
      self.pendingRestart = work
      DispatchQueue.main.asyncAfter(deadline: .now() + 0.3, execute: work)
    }
  }
}

final class MicrophonePermissionException: Exception, @unchecked Sendable {
  override var reason: String {
    "Microphone permission has not been granted"
  }
}

final class NoAudioInputException: Exception, @unchecked Sendable {
  override var reason: String {
    "No audio input is available"
  }
}

final class CaptureFileException: GenericException<String>, @unchecked Sendable {
  override var reason: String {
    "Could not create the recording file: \(param)"
  }
}

final class InvalidSoundUriException: GenericException<String>, @unchecked Sendable {
  override var reason: String {
    "Invalid sound URI: \(param)"
  }
}
