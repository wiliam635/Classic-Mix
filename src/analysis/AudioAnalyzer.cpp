#include "AudioAnalyzer.h"

#include <cmath>

double AudioAnalyzer::toDb(double linear)
{
    return linear > 0.0000001 ? 20.0 * std::log10(linear) : -100.0;
}

TrackAnalysis AudioAnalyzer::analyze(const juce::File& file) const
{
    TrackAnalysis result;
    result.file = file;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
    {
        result.error = juce::String::fromUTF8("Formato de áudio não reconhecido");
        return result;
    }

    result.sampleRate = reader->sampleRate;
    result.channels = static_cast<int>(reader->numChannels);
    result.durationSeconds = reader->sampleRate > 0.0
        ? static_cast<double>(reader->lengthInSamples) / reader->sampleRate
        : 0.0;

    if (reader->lengthInSamples <= 0 || reader->numChannels == 0)
    {
        result.error = juce::String::fromUTF8("Arquivo sem áudio");
        return result;
    }

    constexpr int blockSize = 16384;
    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), blockSize);
    juce::AudioBuffer<float> mono(1, blockSize);
    juce::int64 position = 0;
    long double sumSquares = 0.0;
    long double sumSamples = 0.0;
    long double lowEnergy = 0.0;
    long double midEnergy = 0.0;
    long double highEnergy = 0.0;
    juce::int64 sampleCount = 0;
    juce::int64 silentSamples = 0;
    double maxAbs = 0.0;
    juce::dsp::FFT fft(11);
    juce::dsp::WindowingFunction<float> window(fft.getSize(), juce::dsp::WindowingFunction<float>::hann);

    while (position < reader->lengthInSamples)
    {
        const int samples = static_cast<int>(juce::jmin<juce::int64>(blockSize, reader->lengthInSamples - position));
        buffer.clear();
        reader->read(&buffer, 0, samples, position, true, true);

        mono.clear();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            mono.addFrom(0, 0, buffer, channel, 0, samples, 1.0 / buffer.getNumChannels());

        for (int i = 0; i < samples; ++i)
        {
            const double value = mono.getSample(0, i);
            const double absolute = std::abs(value);
            maxAbs = juce::jmax(maxAbs, absolute);
            sumSquares += value * value;
            sumSamples += value;
            ++sampleCount;

            if (absolute < 0.000316) // approximately -70 dBFS
                ++silentSamples;
            if (absolute >= 0.999)
                ++result.clippedSamples;
        }

        juce::HeapBlock<float> fftData(2 * fft.getSize(), true);
        const int fftSamples = juce::jmin(samples, fft.getSize());
        for (int i = 0; i < fftSamples; ++i)
            fftData[i] = mono.getSample(0, i);
        window.multiplyWithWindowingTable(fftData.getData(), fft.getSize());
        fft.performFrequencyOnlyForwardTransform(fftData.getData());
        for (int bin = 1; bin < fft.getSize() / 2; ++bin)
        {
            const double frequency = static_cast<double>(bin) * reader->sampleRate / fft.getSize();
            const double energy = static_cast<double>(fftData[bin]) * fftData[bin];
            if (frequency < 180.0)
                lowEnergy += energy;
            else if (frequency < 4000.0)
                midEnergy += energy;
            else
                highEnergy += energy;
        }

        position += samples;
    }

    const double meanSquare = static_cast<double>(sumSquares / juce::jmax<juce::int64>(1, sampleCount));
    const double rms = std::sqrt(meanSquare);
    const double mean = static_cast<double>(sumSamples / juce::jmax<juce::int64>(1, sampleCount));
    const double peak = toDb(maxAbs);
    const double totalEnergy = lowEnergy + midEnergy + highEnergy;

    result.peakDb = peak;
    result.rmsDb = toDb(rms);
    result.estimatedLufs = result.rmsDb - 0.691; // RMS-based estimate; BS.1770 is added in the render engine.
    result.crestFactorDb = toDb(peak > -99.0 ? std::pow(10.0, peak / 20.0) / juce::jmax(0.0000001, rms) : 0.0);
    result.dcOffsetDb = toDb(std::abs(mean));
    result.silenceRatio = static_cast<double>(silentSamples) / juce::jmax<juce::int64>(1, sampleCount);
    result.lowEnergyRatio = totalEnergy > 0.0 ? static_cast<double>(lowEnergy / totalEnergy) : 0.0;
    result.midEnergyRatio = totalEnergy > 0.0 ? static_cast<double>(midEnergy / totalEnergy) : 0.0;
    result.highEnergyRatio = totalEnergy > 0.0 ? static_cast<double>(highEnergy / totalEnergy) : 0.0;
    result.valid = true;
    return result;
}
