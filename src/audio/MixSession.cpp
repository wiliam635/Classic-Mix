#include "MixSession.h"
#include "../ai/OpenAiMixService.h"

bool MixSession::importFiles(const juce::Array<juce::File>& files, juce::String& error)
{
    bool imported = false;
    for (const auto& file : files)
    {
        if (! file.existsAsFile())
            continue;

        MixTrack track;
        track.name = file.getFileNameWithoutExtension();
        track.sourceFile = file;
        track.analysis.file = file;
        tracks.push_back(std::move(track));
        imported = true;
    }

    if (! imported)
    {
        error = "Nenhuma faixa de áudio válida foi selecionada.";
        return false;
    }

    error.clear();
    return true;
}

void MixSession::clear()
{
    tracks.clear();
}

void MixSession::analyzeAll()
{
    for (auto& track : tracks)
        track.analysis = analyzer.analyze(track.sourceFile);
}

MixPlanner::Result MixSession::createPlan() const
{
    return planner.createPlan(tracks);
}

MixPlanner::Result MixSession::createPlanWithGpt(juce::String& error) const
{
    return planner.createPlanWithGpt(tracks, error);
}

bool MixSession::hasAiApiKey() const
{
    return OpenAiMixService::hasApiKey();
}

bool MixSession::saveAiApiKey(const juce::String& key, juce::String& error) const
{
    return OpenAiMixService::saveApiKey(key, error);
}

bool MixSession::renderMix(const MixPlanner::Result& plan,
                           const juce::File& outputFile,
                           juce::String& error) const
{
    auto renderTracks = tracks;
    size_t planIndex = 0;
    for (auto& track : renderTracks)
    {
        if (track.analysis.valid && planIndex < plan.tracks.size())
            track.plan = plan.tracks[planIndex++];
    }

    return renderer.render(renderTracks, plan.master, outputFile, error);
}
