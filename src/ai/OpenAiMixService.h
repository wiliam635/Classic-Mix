#pragma once

#include "MixPlanner.h"

class OpenAiMixService
{
public:
    bool requestPlan(const std::vector<MixTrack>& tracks,
                     MixPlanner::Result& result,
                     juce::String& error) const;

    static juce::String loadApiKey();
    static bool saveApiKey(const juce::String& apiKey, juce::String& error);
    static bool hasApiKey();

    static juce::String loadLocalModel();
    static bool saveLocalModel(const juce::String& model, juce::String& error);
    static bool hasLocalModel();

private:
    static juce::File settingsFile();
    static juce::File localModelFile();
};
