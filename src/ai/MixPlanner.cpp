#include "MixPlanner.h"
#include "OpenAiMixService.h"

MixPlanner::Result MixPlanner::createPlan(const std::vector<MixTrack>& tracks) const
{
    Result result;
    result.master.rationale = "Master inicial conservadora: alvo -14 LUFS e teto true peak de -1 dBTP.";

    for (const auto& track : tracks)
    {
        TrackMixPlan plan;
        const auto& analysis = track.analysis;

        if (! analysis.valid)
            continue;

        // Safe baseline while the provider-backed AI planner is not configured.
        plan.gainDb = juce::jlimit(-6.0, 6.0, -18.0 - analysis.estimatedLufs);
        plan.highPassHz = analysis.lowEnergyRatio > 0.55 ? 35.0 : 20.0;
        plan.eq[0] = { 120.0, analysis.lowEnergyRatio > 0.60 ? -1.5 : 0.0, 0.8 };
        plan.eq[1] = { 1200.0, analysis.midEnergyRatio < 0.20 ? 1.0 : 0.0, 0.9 };
        plan.eq[2] = { 8000.0, analysis.highEnergyRatio < 0.08 ? 0.8 : 0.0, 0.7 };
        plan.useCompressor = analysis.crestFactorDb > 12.0;
        plan.compressorThresholdDb = -18.0;
        plan.compressorRatio = 2.0;
        plan.compressorAttackMs = 20.0;
        plan.compressorReleaseMs = 120.0;
        plan.rationale = "Plano de segurança baseado nas métricas locais; o provedor de IA poderá refiná-lo.";
        result.tracks.push_back(plan);
    }

    result.summary = tracks.empty()
        ? "Importe stems para gerar a primeira análise."
        : "Análise concluída. Este é um plano inicial conservador; conecte um provedor de IA para refinamento musical.";
    return result;
}

MixPlanner::Result MixPlanner::createPlanWithGpt(const std::vector<MixTrack>& tracks, juce::String& error) const
{
    auto result = createPlan(tracks);
    OpenAiMixService service;
    if (! service.requestPlan(tracks, result, error))
        return result;

    return result;
}
