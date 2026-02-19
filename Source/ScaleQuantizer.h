#pragma once
#include <vector>
#include <string>
#include <map>

class ScaleQuantizer {
public:
    ScaleQuantizer();
    int snapToScale(int midiNote, const std::string& scaleName);
    std::vector<std::string> getScaleNames() const;

private:
    std::map<std::string, std::vector<int>> scales;
};
