Pod::Spec.new do |s|
  s.name           = 'VowelTracker'
  s.version        = '1.0.0'
  s.summary        = 'Live vowel formant tracking for Vowelsy'
  s.description    = 'Captures microphone audio and runs the FTracker formant estimator on it.'
  s.author         = 'Tanner Plauché'
  s.homepage       = 'https://github.com/tannerplauche/vowelviz'
  s.platforms      = {
    :ios => '16.4'
  }
  s.source         = { git: '' }
  s.static_framework = true

  s.dependency 'ExpoModulesCore'

  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
  }

  s.source_files = 'ios/**/*.swift', 'cpp/**/*.{h,cpp}'
  # Only the plain C interface is exposed to Swift; the C++ headers stay private.
  s.public_header_files = 'cpp/vowel_tracker.h'
end
