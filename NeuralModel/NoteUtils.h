//
// Created by Damien Ronssin on 14.03.23.
//

#ifndef NN_NOTEUTILS_H
#define NN_NOTEUTILS_H

#include <cmath>
#include <string>
#include <vector>

namespace NoteUtils {

/**
 * Return closest midi note number to frequency
 * @param hz Input frequency
 * @return Closest midi note number
 */
static inline int hzToMidi(float hz) {
  return (int)std::round(12.0f * std::log2(hz / 440.0f) + 69.0f);
}

/**
 * Compute frequency in Hz corresponding to given midi note
 * @param inMidiNote Midi note number
 * @return Frequency in Hz
 */
static inline float midiToHz(float inMidiNote) {
  return 440.0f * std::pow(2.0f, (inMidiNote - 69.0f) / 12.0f);
}

} // namespace NoteUtils

#endif // NN_NOTEUTILS_H
