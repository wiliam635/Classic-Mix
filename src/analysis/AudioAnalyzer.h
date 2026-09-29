#pragma once

#include "../audio/TrackModel.h"

class AudioAnalyzer
{
public:
    TrackAnalysis analyze(const juce::File& file) const;

private:
    static double toDb(double linear);
};

