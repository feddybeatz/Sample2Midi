#include "ScaleQuantizer.h"
#include <cmath>
#include <algorithm>

ScaleQuantizer::ScaleQuantizer() {
    scales["C Major"] = {0, 2, 4, 5, 7, 9, 11};
    scales["C Minor"] = {0, 2, 3, 5, 7, 8, 10};
    scales["D Major"] = {2, 4, 6, 7, 9, 11, 1};
    scales["A Minor"] = {9, 11, 0, 2, 4, 5, 7};
    scales["Chromatic"] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
}

int ScaleQuantizer::snapToScale(int midiNote, const std::string& scaleName) {
    if (scales.find(scaleName) == scales.end() || scaleName == "Chromatic")
        return midiNote;

    int octave = midiNote / 12;
    int semitone = midiNote % 12;
    const auto& scaleIntervals = scales[scaleName];

    int nearest = scaleIntervals[0];
    int minDist = 12;

    for (int degree : scaleIntervals) {
        int dist = std::abs(semitone - degree);
        if (dist > 6) dist = 12 - dist; // wrap around

        if (dist < minDist) {
            minDist = dist;
            nearest = degree;
        }
    }

    return octave * 12 + nearest;
}

std::vector<std::string> ScaleQuantizer::getScaleNames() const {
    std::vector<std::string> names;
    for (const auto& pair : scales) names.push_back(pair.first);
    return names;
}
