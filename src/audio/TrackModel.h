#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

struct TrackAnalysis
{
    juce::File file;
    double durationSeconds = 0.0;
    double sampleRate = 0.0;
    int channels = 0;
    double peakDb = -100.0;
    double rmsDb = -100.0;
    double estimatedLufs = -100.0;
    double crestFactorDb = 0.0;
    double dcOffsetDb = -100.0;
    double silenceRatio = 0.0;
    double lowEnergyRatio = 0.0;
    double midEnergyRatio = 0.0;
    double highEnergyRatio = 0.0;
    int clippedSamples = 0;
    bool valid = false;
    juce::String error;
};

struct EqBandPlan
{
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 0.707;
};

struct TrackMixPlan
{
    double gainDb = 0.0;
    double pan = 0.0;
    double highPassHz = 20.0;
    std::array<EqBandPlan, 3> eq;
    bool useCompressor = false;
    double compressorThresholdDb = -18.0;
    double compressorRatio = 2.0;
    double compressorAttackMs = 20.0;
    double compressorReleaseMs = 120.0;
    double reverbSend = 0.0;
    juce::String rationale;
};

struct MasterPlan
{
    double targetLufs = -14.0;
    double truePeakCeilingDb = -1.0;
    double lowCutHz = 20.0;
    double highCutHz = 20000.0;
    double limiterThresholdDb = -1.0;
    juce::String rationale;
};

struct MixTrack
{
    juce::String name;
    juce::File sourceFile;
    TrackAnalysis analysis;
    TrackMixPlan plan;
    bool selected = false;
};
