#pragma once

#include "TrackModel.h"
#include "../analysis/AudioAnalyzer.h"
#include "../ai/MixPlanner.h"
#include "MixRenderer.h"

class MixSession
{
public:
    bool importFiles(const juce::Array<juce::File>& files, juce::String& error);
    void clear();
    void analyzeAll();
    MixPlanner::Result createPlan() const;
    MixPlanner::Result createPlanWithGpt(juce::String& error) const;
    bool renderMix(const MixPlanner::Result& plan, const juce::File& outputFile, juce::String& error) const;
    bool hasAiApiKey() const;
    bool saveAiApiKey(const juce::String& key, juce::String& error) const;

    const std::vector<MixTrack>& getTracks() const noexcept { return tracks; }

private:
    std::vector<MixTrack> tracks;
    AudioAnalyzer analyzer;
    MixPlanner planner;
    MixRenderer renderer;
};
