#pragma once

#include <JuceHeader.h>
#include "../audio/MixSession.h"

class MainComponent final : public juce::Component,
                            private juce::Button::Listener,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TrackListModel;
    class PlanJob;

    void buttonClicked(juce::Button*) override;
    void timerCallback() override;
    void refreshTrackList();
    void setStatus(const juce::String&);
    void configureGpt();

    juce::TextButton importButton { "Importar stems" };
    juce::TextButton analyzeButton { "Analisar e criar mix" };
    juce::TextButton renderButton { "Renderizar WAV" };
    juce::TextButton configureButton { "Configurar GPT" };
    juce::TextButton clearButton { "Limpar" };
    juce::Label title;
    juce::Label subtitle;
    juce::Label status;
    juce::ListBox trackList;
    std::unique_ptr<TrackListModel> trackModel;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Label planLabel;
    juce::String pendingStatus;
    MixSession session;
    MixPlanner::Result lastPlan;
    juce::ThreadPool analysisPool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
