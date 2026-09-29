#include "OpenAiMixService.h"

namespace
{
juce::var getProperty(const juce::var& object, const char* name)
{
    if (auto* dynamicObject = object.getDynamicObject())
        return dynamicObject->getProperty(name);

    return {};
}

double numberProperty(const juce::var& object, const char* name, double fallback)
{
    const auto value = getProperty(object, name);
    return value.isVoid() ? fallback : static_cast<double>(value);
}

juce::String stringProperty(const juce::var& object, const char* name, const juce::String& fallback)
{
    const auto value = getProperty(object, name);
    return value.isVoid() ? fallback : value.toString();
}

juce::String shorten(const juce::String& value, int maximumLength = 480)
{
    const auto trimmed = value.trim();
    if (trimmed.length() <= maximumLength)
        return trimmed;

    return trimmed.substring(0, maximumLength) + juce::String::fromUTF8("…");
}

juce::String cleanJsonText(juce::String value)
{
    value = value.trim();
    if (! value.startsWith("```"))
        return value;

    const auto firstLineBreak = value.indexOfChar('\n');
    if (firstLineBreak >= 0)
        value = value.substring(firstLineBreak + 1);

    if (value.trimEnd().endsWith("```"))
        value = value.trimEnd().dropLastCharacters(3);

    return value.trim();
}

bool boolProperty(const juce::var& object, const char* name, bool fallback)
{
    const auto value = getProperty(object, name);
    return value.isVoid() ? fallback : static_cast<bool>(value);
}

const char* schemaJson()
{
    return R"json({
        "type": "object",
        "properties": {
            "summary": { "type": "string" },
            "master": {
                "type": "object",
                "properties": {
                    "target_lufs": { "type": "number" },
                    "true_peak_ceiling_db": { "type": "number" },
                    "low_cut_hz": { "type": "number" },
                    "high_cut_hz": { "type": "number" },
                    "limiter_threshold_db": { "type": "number" },
                    "rationale": { "type": "string" }
                },
                "required": ["target_lufs", "true_peak_ceiling_db", "low_cut_hz", "high_cut_hz", "limiter_threshold_db", "rationale"],
                "additionalProperties": false
            },
            "tracks": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "index": { "type": "integer" },
                        "gain_db": { "type": "number" },
                        "pan": { "type": "number" },
                        "high_pass_hz": { "type": "number" },
                        "eq_low_gain_db": { "type": "number" },
                        "eq_mid_gain_db": { "type": "number" },
                        "eq_high_gain_db": { "type": "number" },
                        "use_compressor": { "type": "boolean" },
                        "compressor_threshold_db": { "type": "number" },
                        "compressor_ratio": { "type": "number" },
                        "compressor_attack_ms": { "type": "number" },
                        "compressor_release_ms": { "type": "number" },
                        "reverb_send": { "type": "number" },
                        "rationale": { "type": "string" }
                    },
                    "required": ["index", "gain_db", "pan", "high_pass_hz", "eq_low_gain_db", "eq_mid_gain_db", "eq_high_gain_db", "use_compressor", "compressor_threshold_db", "compressor_ratio", "compressor_attack_ms", "compressor_release_ms", "reverb_send", "rationale"],
                    "additionalProperties": false
                }
            }
        },
        "required": ["summary", "master", "tracks"],
        "additionalProperties": false
    })json";
}

juce::String extractApiError(const juce::var& response)
{
    const auto apiError = getProperty(response, "error");
    if (apiError.getDynamicObject() != nullptr)
    {
        auto message = stringProperty(apiError, "message", {});
        const auto type = stringProperty(apiError, "type", {});
        const auto code = stringProperty(apiError, "code", {});

        if (message.isNotEmpty())
        {
            if (type.isNotEmpty())
                message += " [" + type + "]";
            if (code.isNotEmpty())
                message += " (" + code + ")";
            return message;
        }
    }

    if (stringProperty(response, "status", {}) == "incomplete")
    {
        const auto details = getProperty(response, "incomplete_details");
        const auto reason = stringProperty(details, "reason", {});
        return reason.isNotEmpty()
            ? juce::String::fromUTF8("Resposta incompleta: ") + reason
            : juce::String::fromUTF8("Resposta incompleta.");
    }

    return {};
}

juce::String extractRefusal(const juce::var& response)
{
    const auto output = getProperty(response, "output");
    const auto* outputArray = output.getArray();
    if (outputArray == nullptr)
        return {};

    for (const auto& outputItem : *outputArray)
    {
        const auto content = getProperty(outputItem, "content");
        const auto* contentArray = content.getArray();
        if (contentArray == nullptr)
            continue;

        for (const auto& contentItem : *contentArray)
        {
            if (stringProperty(contentItem, "type", {}) == "refusal")
            {
                const auto refusal = stringProperty(contentItem, "refusal", {});
                return refusal.isNotEmpty()
                    ? refusal
                    : juce::String::fromUTF8("O GPT recusou gerar o plano.");
            }
        }
    }

    return {};
}

juce::String extractOutputText(const juce::var& response)
{
    auto text = stringProperty(response, "output_text", {});
    if (text.isNotEmpty())
        return text;

    const auto output = getProperty(response, "output");
    const auto* outputArray = output.getArray();
    if (outputArray == nullptr)
        return {};

    juce::String combinedText;

    for (const auto& outputItem : *outputArray)
    {
        const auto content = getProperty(outputItem, "content");
        const auto* contentArray = content.getArray();
        if (contentArray == nullptr)
            continue;

        for (const auto& contentItem : *contentArray)
        {
            if (stringProperty(contentItem, "type", {}) == "output_text")
                combinedText += stringProperty(contentItem, "text", {});

            // Some SDKs expose the parsed Structured Output alongside the text.
            // Accept it as a fallback so the app is not tied to one response shape.
            const auto parsed = getProperty(contentItem, "parsed");
            if (! parsed.isVoid() && parsed.getDynamicObject() != nullptr)
                return juce::JSON::toString(parsed, false);
        }
    }

    return combinedText;
}

juce::var buildMetrics(const std::vector<MixTrack>& tracks)
{
    juce::Array<juce::var> metrics;
    int planIndex = 0;
    for (const auto& track : tracks)
    {
        if (! track.analysis.valid)
            continue;

        auto* item = new juce::DynamicObject();
        item->setProperty("index", planIndex++);
        item->setProperty("name", track.name);
        item->setProperty("duration_seconds", track.analysis.durationSeconds);
        item->setProperty("sample_rate", track.analysis.sampleRate);
        item->setProperty("channels", track.analysis.channels);
        item->setProperty("peak_db", track.analysis.peakDb);
        item->setProperty("rms_db", track.analysis.rmsDb);
        item->setProperty("estimated_lufs", track.analysis.estimatedLufs);
        item->setProperty("crest_factor_db", track.analysis.crestFactorDb);
        item->setProperty("dc_offset_db", track.analysis.dcOffsetDb);
        item->setProperty("silence_ratio", track.analysis.silenceRatio);
        item->setProperty("low_energy_ratio", track.analysis.lowEnergyRatio);
        item->setProperty("mid_energy_ratio", track.analysis.midEnergyRatio);
        item->setProperty("high_energy_ratio", track.analysis.highEnergyRatio);
        item->setProperty("clipped_samples", track.analysis.clippedSamples);
        metrics.add(juce::var(item));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("tracks", juce::var(metrics));
    root->setProperty("constraints", "Return one conservative plan per input track. Do not invent tracks. Keep all values inside the schema ranges implied by the task.");
    return juce::var(root);
}
}

juce::File OpenAiMixService::settingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Classic Mix")
        .getChildFile("gpt-api-key.txt");
}

juce::File OpenAiMixService::localModelFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Classic Mix")
        .getChildFile("local-model.txt");
}

juce::String OpenAiMixService::loadApiKey()
{
    const auto environmentKey = juce::SystemStats::getEnvironmentVariable("OPENAI_API_KEY", {}).trim();
    if (environmentKey.isNotEmpty())
        return environmentKey;

    const auto file = settingsFile();
    return file.existsAsFile() ? file.loadFileAsString().trim() : juce::String{};
}

bool OpenAiMixService::saveApiKey(const juce::String& apiKey, juce::String& error)
{
    const auto file = settingsFile();
    if (! file.getParentDirectory().createDirectory())
    {
        error = juce::String::fromUTF8("Não foi possível criar a pasta local de configuração do GPT.");
        return false;
    }

    const auto trimmed = apiKey.trim();
    if (trimmed.isEmpty())
    {
        file.deleteFile();
        error.clear();
        return true;
    }

    if (! file.replaceWithText(trimmed + "\n"))
    {
        error = juce::String::fromUTF8("Não foi possível salvar a chave do GPT localmente.");
        return false;
    }

    error.clear();
    return true;
}

bool OpenAiMixService::hasApiKey()
{
    return loadApiKey().isNotEmpty();
}

juce::String OpenAiMixService::loadLocalModel()
{
    const auto environmentModel = juce::SystemStats::getEnvironmentVariable("CLASSIC_MIX_LOCAL_MODEL", {}).trim();
    if (environmentModel.isNotEmpty())
        return environmentModel;

    const auto file = localModelFile();
    if (file.existsAsFile())
    {
        const auto configuredModel = file.loadFileAsString().trim();
        if (configuredModel.isNotEmpty())
            return configuredModel;
    }

    return "llama3.2:3b";
}

bool OpenAiMixService::saveLocalModel(const juce::String& model, juce::String& error)
{
    const auto file = localModelFile();
    if (! file.getParentDirectory().createDirectory())
    {
        error = juce::String::fromUTF8("Não foi possível criar a pasta de configuração da IA local.");
        return false;
    }

    const auto trimmed = model.trim();
    if (trimmed.isEmpty())
    {
        file.deleteFile();
        error.clear();
        return true;
    }

    if (! file.replaceWithText(trimmed + "\n"))
    {
        error = juce::String::fromUTF8("Não foi possível salvar o modelo local.");
        return false;
    }

    error.clear();
    return true;
}

bool OpenAiMixService::hasLocalModel()
{
    return loadLocalModel().isNotEmpty();
}

bool OpenAiMixService::requestPlan(const std::vector<MixTrack>& tracks,
                                   MixPlanner::Result& result,
                                   juce::String& error) const
{
    const auto model = loadLocalModel();
    auto* requestObject = new juce::DynamicObject();
    requestObject->setProperty("model", model);
    requestObject->setProperty("system",
        juce::String::fromUTF8("Você é um engenheiro de mixagem e masterização. "
                               "Responda somente com um objeto JSON válido conforme o schema. "
                               "Faça decisões conservadoras, preserve dinâmica, não invente instrumentos "
                               "e explique brevemente cada decisão."));
    requestObject->setProperty("prompt",
        juce::String::fromUTF8("Analise as métricas dos stems abaixo. As faixas originais permanecem no computador. "
                               "Retorne um plano por faixa, sem criar faixas novas.\n\n") +
        juce::JSON::toString(buildMetrics(tracks), false));
    requestObject->setProperty("format", juce::JSON::parse(schemaJson()));
    requestObject->setProperty("stream", false);
    requestObject->setProperty("think", false);
    requestObject->setProperty("keep_alive", 0);

    auto* optionsObject = new juce::DynamicObject();
    optionsObject->setProperty("temperature", 0.1);
    requestObject->setProperty("options", juce::var(optionsObject));

    const auto payload = juce::JSON::toString(juce::var(requestObject), false);
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
        .withHttpRequestCmd("POST")
        .withExtraHeaders("Content-Type: application/json\r\n")
        .withConnectionTimeoutMs(120000)
        .withStatusCode(&statusCode);

    juce::URL url("http://127.0.0.1:11434/api/generate");
    std::unique_ptr<juce::InputStream> stream(url.withPOSTData(payload).createInputStream(options));
    if (stream == nullptr)
    {
        error = juce::String::fromUTF8("O Ollama não respondeu. Instale-o, abra o aplicativo Ollama e baixe o modelo ")
              + model + juce::String::fromUTF8(" (HTTP ") + juce::String(statusCode) + ").";
        return false;
    }

    const auto responseText = stream->readEntireStreamAsString();
    const auto response = juce::JSON::parse(responseText);

    if (response.isVoid())
    {
        error = juce::String::fromUTF8("A IA local devolveu uma resposta inválida (HTTP ")
              + juce::String(statusCode) + ").";
        const auto detail = shorten(responseText);
        if (detail.isNotEmpty())
            error += " " + detail;
        return false;
    }

    const auto localError = getProperty(response, "error");
    if (! localError.isVoid())
    {
        error = juce::String::fromUTF8("O Ollama não conseguiu gerar o plano: ")
              + shorten(localError.toString());
        return false;
    }

    const auto outputText = cleanJsonText(stringProperty(response, "response", {}));
    if (outputText.isEmpty())
    {
        error = juce::String::fromUTF8("A IA local não retornou um plano estruturado (HTTP ")
              + juce::String(statusCode) + "). Verifique se o modelo suporta saída JSON.";
        return false;
    }

    const auto plan = juce::JSON::parse(outputText);
    if (plan.isVoid() || plan.getDynamicObject() == nullptr)
    {
        error = juce::String::fromUTF8("A resposta da IA local não pôde ser interpretada como JSON.");
        return false;
    }

    const auto master = getProperty(plan, "master");
    result.master.targetLufs = juce::jlimit(-24.0, -6.0, numberProperty(master, "target_lufs", -14.0));
    result.master.truePeakCeilingDb = juce::jlimit(-3.0, -0.1, numberProperty(master, "true_peak_ceiling_db", -1.0));
    result.master.lowCutHz = juce::jlimit(10.0, 120.0, numberProperty(master, "low_cut_hz", 20.0));
    result.master.highCutHz = juce::jlimit(12000.0, 22000.0, numberProperty(master, "high_cut_hz", 20000.0));
    result.master.limiterThresholdDb = juce::jlimit(-12.0, 0.0, numberProperty(master, "limiter_threshold_db", -1.0));
    result.master.rationale = stringProperty(master, "rationale", juce::String::fromUTF8("Master definido pela IA local."));

    const auto trackPlans = getProperty(plan, "tracks");
    const auto* trackPlanArray = trackPlans.getArray();
    if (trackPlanArray == nullptr || trackPlanArray->isEmpty())
    {
        error = juce::String::fromUTF8("A IA local não retornou planos para as faixas.");
        return false;
    }

    for (const auto& item : *trackPlanArray)
    {
        const auto index = static_cast<int>(numberProperty(item, "index", -1));
        if (index < 0 || index >= static_cast<int>(result.tracks.size()))
            continue;

        auto& trackPlan = result.tracks[static_cast<size_t>(index)];
        trackPlan.gainDb = juce::jlimit(-12.0, 12.0, numberProperty(item, "gain_db", trackPlan.gainDb));
        trackPlan.pan = juce::jlimit(-1.0, 1.0, numberProperty(item, "pan", trackPlan.pan));
        trackPlan.highPassHz = juce::jlimit(20.0, 500.0, numberProperty(item, "high_pass_hz", trackPlan.highPassHz));
        trackPlan.eq[0].gainDb = juce::jlimit(-6.0, 6.0, numberProperty(item, "eq_low_gain_db", trackPlan.eq[0].gainDb));
        trackPlan.eq[1].gainDb = juce::jlimit(-6.0, 6.0, numberProperty(item, "eq_mid_gain_db", trackPlan.eq[1].gainDb));
        trackPlan.eq[2].gainDb = juce::jlimit(-6.0, 6.0, numberProperty(item, "eq_high_gain_db", trackPlan.eq[2].gainDb));
        trackPlan.useCompressor = boolProperty(item, "use_compressor", trackPlan.useCompressor);
        trackPlan.compressorThresholdDb = juce::jlimit(-40.0, 0.0, numberProperty(item, "compressor_threshold_db", trackPlan.compressorThresholdDb));
        trackPlan.compressorRatio = juce::jlimit(1.0, 20.0, numberProperty(item, "compressor_ratio", trackPlan.compressorRatio));
        trackPlan.compressorAttackMs = juce::jlimit(0.1, 200.0, numberProperty(item, "compressor_attack_ms", trackPlan.compressorAttackMs));
        trackPlan.compressorReleaseMs = juce::jlimit(5.0, 1000.0, numberProperty(item, "compressor_release_ms", trackPlan.compressorReleaseMs));
        trackPlan.reverbSend = juce::jlimit(0.0, 1.0, numberProperty(item, "reverb_send", trackPlan.reverbSend));
        trackPlan.rationale = stringProperty(item, "rationale", juce::String::fromUTF8("Plano definido pela IA local."));
    }

    result.summary = juce::String::fromUTF8("A IA local criou o plano de mix: ")
                   + stringProperty(plan, "summary", juce::String::fromUTF8("plano estruturado recebido."));
    result.usedAi = true;
    error.clear();
    return true;
}
