#include "MixRenderer.h"

#include "../analysis/AudioAnalyzer.h"

#include <cmath>

namespace
{
constexpr int blockSize = 16384;
constexpr double defaultSampleRate = 44100.0;

struct ReaderState
{
    std::unique_ptr<juce::AudioFormatReader> reader;
    const MixTrack* track = nullptr;
};

bool createWriter(const juce::File& file,
                  double sampleRate,
                  unsigned int channels,
                  std::unique_ptr<juce::FileOutputStream>& stream,
                  std::unique_ptr<juce::AudioFormatWriter>& writer,
                  juce::String& error)
{
    stream.reset(file.createOutputStream());
    if (stream == nullptr)
    {
        error = "Não foi possível criar o arquivo de saída.";
        return false;
    }

    juce::WavAudioFormat format;
    writer.reset(format.createWriterFor(stream.get(), sampleRate, channels, 24, {}, 0));
    if (writer == nullptr)
    {
        error = "Não foi possível preparar o WAV de saída.";
        return false;
    }

    stream.release();
    return true;
}
}

double MixRenderer::decibelsToGain(double decibels)
{
    return std::pow(10.0, decibels / 20.0);
}

bool MixRenderer::render(const std::vector<MixTrack>& tracks,
                         const MasterPlan& master,
                         const juce::File& outputFile,
                         juce::String& error) const
{
    if (tracks.empty())
    {
        error = "Importe e analise pelo menos uma faixa antes de renderizar.";
        return false;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::vector<ReaderState> readers;
    readers.reserve(tracks.size());
    double sampleRate = 0.0;
    juce::int64 totalSamples = 0;

    for (const auto& track : tracks)
    {
        if (! track.analysis.valid)
            continue;

        auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(track.sourceFile));
        if (reader == nullptr)
            continue;

        sampleRate = sampleRate > 0.0 ? sampleRate : reader->sampleRate;
        totalSamples = juce::jmax(totalSamples,
                                  static_cast<juce::int64>(reader->lengthInSamples * sampleRate / reader->sampleRate));
        readers.push_back({ std::move(reader), &track });
    }

    if (readers.empty() || totalSamples <= 0)
    {
        error = "Nenhuma faixa analisada pôde ser aberta para renderização.";
        return false;
    }

    if (sampleRate <= 0.0)
        sampleRate = defaultSampleRate;

    auto temporaryFile = outputFile.getSiblingFile(outputFile.getFileNameWithoutExtension() + "-premaster.wav");
    temporaryFile.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    if (! createWriter(temporaryFile, sampleRate, 2, stream, writer, error))
        return false;

    juce::AudioBuffer<float> sourceBuffer(2, blockSize);
    juce::AudioBuffer<float> mixBuffer(2, blockSize);
    juce::int64 position = 0;

    while (position < totalSamples)
    {
        const int samples = static_cast<int>(juce::jmin<juce::int64>(blockSize, totalSamples - position));
        mixBuffer.clear();

        for (auto& state : readers)
        {
            sourceBuffer.clear();
            const auto sourcePosition = static_cast<juce::int64>(position * state.reader->sampleRate / sampleRate);
            const auto sourceSamples = static_cast<int>(juce::jmin<juce::int64>(
                samples * state.reader->sampleRate / sampleRate,
                state.reader->lengthInSamples - sourcePosition));

            if (sourceSamples <= 0)
                continue;

            state.reader->read(&sourceBuffer, 0, sourceSamples, sourcePosition, true, true);

            const auto& plan = state.track->plan;
            const auto gain = static_cast<float>(decibelsToGain(plan.gainDb));
            const auto pan = static_cast<float>(juce::jlimit(-1.0, 1.0, plan.pan));
            const auto leftPan = std::cos((pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f);
            const auto rightPan = std::sin((pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f);

            if (sourceBuffer.getNumChannels() == 1)
            {
                mixBuffer.addFrom(0, 0, sourceBuffer, 0, 0, sourceSamples, gain * leftPan);
                mixBuffer.addFrom(1, 0, sourceBuffer, 0, 0, sourceSamples, gain * rightPan);
            }
            else
            {
                mixBuffer.addFrom(0, 0, sourceBuffer, 0, 0, sourceSamples, gain * leftPan);
                mixBuffer.addFrom(1, 0, sourceBuffer, 1, 0, sourceSamples, gain * rightPan);
            }
        }

        if (! writer->writeFromAudioSampleBuffer(mixBuffer, 0, samples))
        {
            error = "Falha ao gravar o premaster.";
            temporaryFile.deleteFile();
            return false;
        }

        position += samples;
    }

    writer.reset();
    stream.reset();

    AudioAnalyzer analyzer;
    const auto premasterAnalysis = analyzer.analyze(temporaryFile);
    if (! premasterAnalysis.valid)
    {
        error = "A mix foi renderizada, mas não pôde ser analisada para a etapa de masterização.";
        temporaryFile.deleteFile();
        return false;
    }

    const auto requestedGain = master.targetLufs - premasterAnalysis.estimatedLufs;
    const auto peakGain = master.truePeakCeilingDb - premasterAnalysis.peakDb;
    const auto masterGain = juce::jlimit(-12.0, 12.0, juce::jmin(requestedGain, peakGain));
    const auto linearGain = static_cast<float>(decibelsToGain(masterGain));
    const auto ceiling = static_cast<float>(decibelsToGain(master.truePeakCeilingDb));

    std::unique_ptr<juce::AudioFormatReader> premasterReader(formats.createReaderFor(temporaryFile));
    if (premasterReader == nullptr)
    {
        error = "Não foi possível reabrir o premaster para finalizar a masterização.";
        temporaryFile.deleteFile();
        return false;
    }

    outputFile.deleteFile();
    if (! createWriter(outputFile, premasterReader->sampleRate, 2, stream, writer, error))
    {
        temporaryFile.deleteFile();
        return false;
    }

    juce::AudioBuffer<float> masterBuffer(2, blockSize);
    position = 0;
    while (position < premasterReader->lengthInSamples)
    {
        const int samples = static_cast<int>(juce::jmin<juce::int64>(blockSize,
                                                                       premasterReader->lengthInSamples - position));
        masterBuffer.clear();
        premasterReader->read(&masterBuffer, 0, samples, position, true, true);
        for (int channel = 0; channel < masterBuffer.getNumChannels(); ++channel)
        {
            auto* data = masterBuffer.getWritePointer(channel);
            for (int i = 0; i < samples; ++i)
                data[i] = juce::jlimit(-ceiling, ceiling, data[i] * linearGain);
        }

        if (! writer->writeFromAudioSampleBuffer(masterBuffer, 0, samples))
        {
            error = "Falha ao gravar o arquivo masterizado.";
            writer.reset();
            stream.reset();
            outputFile.deleteFile();
            temporaryFile.deleteFile();
            return false;
        }

        position += samples;
    }

    writer.reset();
    stream.reset();
    temporaryFile.deleteFile();
    error.clear();
    return true;
}
