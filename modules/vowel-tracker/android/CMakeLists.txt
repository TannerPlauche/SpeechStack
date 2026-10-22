cmake_minimum_required(VERSION 3.22.1)
project(voweltracker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHARED_CPP ${CMAKE_CURRENT_SOURCE_DIR}/../cpp)

add_library(voweltracker SHARED
  ${SHARED_CPP}/FTracker.cpp
  ${SHARED_CPP}/PitchTracker.cpp
  ${SHARED_CPP}/RhoticDetector.cpp
  ${SHARED_CPP}/VoiceDetector.cpp
  ${SHARED_CPP}/vowel_tracker.cpp
  ${SHARED_CPP}/vt_ring.cpp
  src/main/cpp/jni_bridge.cpp
)

target_include_directories(voweltracker PRIVATE ${SHARED_CPP})
target_link_libraries(voweltracker log)
