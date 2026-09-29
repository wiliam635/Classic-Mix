#pragma once

#include "../audio/TrackModel.h"

class MixPlanner
{
public:
    struct Result
    {
        std::vector<TrackMixPlan> tracks;
        MasterPlan master;
        juce::String summary;
        bool usedAi = false;
    };

    Result createPlan(const std::vector<MixTrack>& tracks) const;
};

