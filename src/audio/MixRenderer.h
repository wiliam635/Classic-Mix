#pragma once

#include "TrackModel.h"

class MixRenderer
{
public:
    bool render(const std::vector<MixTrack>& tracks,
                const MasterPlan& master,
                const juce::File& outputFile,
                juce::String& error) const;

private:
    static double decibelsToGain(double decibels);
};
