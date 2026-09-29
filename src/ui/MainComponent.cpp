#include "MainComponent.h"

class MainComponent::TrackListModel final : public juce::ListBoxModel
{
public:
    explicit TrackListModel(const std::vector<MixTrack>& tracks) : tracks(tracks) {}

    int getNumRows() override { return static_cast<int>(tracks.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= getNumRows())
            return;

        g.setColour(selected ? juce::Colour(0xff294b63) : juce::Colour(0xff17232d));
        g.fillRoundedRectangle(4.0f, 3.0f, static_cast<float>(width - 8), static_cast<float>(height - 6), 5.0f);
        g.setColour(juce::Colour(0xffe9f1f5));
        g.setFont(15.0f);
        g.drawText(tracks[static_cast<size_t>(row)].name, 16, 0, width - 32, height, juce::Justification::centredLeft);

        const auto& analysis = tracks[static_cast<size_t>(row)].analysis;
        if (analysis.valid)
        {
            g.setColour(juce::Colour(0xff9eb1bd));
            g.setFont(12.0f);
            g.drawText(juce::String(analysis.estimatedLufs, 1) + " LUFS  |  peak " + juce::String(analysis.peakDb, 1) + " dB",
                       width - 230, 0, 210, height, juce::Justification::centredRight);
        }
    }

private:
    const std::vector<MixTrack>& tracks;
};

MainComponent::MainComponent()
{
    setOpaque(true);
    title.setText("Classic Mix", juce::dontSendNotification);
    title.setFont(juce::Font(28.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f6f8));
    addAndMakeVisible(title);

    subtitle.setText("Mixagem e masterização assistidas por IA", juce::dontSendNotification);
    subtitle.setFont(juce::Font(14.0f));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff9eb1bd));
    addAndMakeVisible(subtitle);

    for (auto* button : { &importButton, &analyzeButton, &renderButton, &clearButton })
    {
        button->addListener(this);
        addAndMakeVisible(button);
    }
    analyzeButton.setEnabled(false);
    renderButton.setEnabled(false);

    status.setText("Importe os stems para começar.", juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colour(0xffb8c7cf));
    addAndMakeVisible(status);

    planLabel.setText("O plano da IA aparecerá aqui depois da análise.", juce::dontSendNotification);
    planLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc5d4db));
    planLabel.setJustificationType(juce::Justification::topLeft);
    planLabel.setMinimumHorizontalScale(0.5f);
    addAndMakeVisible(planLabel);

    trackList.setRowHeight(52);
    addAndMakeVisible(trackList);
    startTimerHz(5);
}

MainComponent::~MainComponent()
{
    stopTimer();
    trackList.setModel(nullptr);
    trackModel.reset();
    for (auto* button : { &importButton, &analyzeButton, &renderButton, &clearButton })
        button->removeListener(this);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e171d));
    g.setColour(juce::Colour(0xff15252e));
    g.fillRoundedRectangle(24.0f, 24.0f, static_cast<float>(getWidth() - 48), 82.0f, 12.0f);
    g.setColour(juce::Colour(0xff13212a));
    g.fillRoundedRectangle(24.0f, 122.0f, static_cast<float>(getWidth() - 48), static_cast<float>(getHeight() - 146), 12.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(24);
    auto header = area.removeFromTop(82).reduced(18, 12);
    title.setBounds(header.removeFromTop(34));
    subtitle.setBounds(header.removeFromTop(22));

    auto content = area.reduced(18);
    auto controls = content.removeFromTop(42);
    importButton.setBounds(controls.removeFromLeft(150));
    analyzeButton.setBounds(controls.removeFromLeft(190).withTrimmedLeft(10));
    renderButton.setBounds(controls.removeFromLeft(150).withTrimmedLeft(20));
    clearButton.setBounds(controls.removeFromLeft(90).withTrimmedLeft(20));
    status.setBounds(controls.withTrimmedLeft(18));

    auto bottom = content.removeFromBottom(96);
    planLabel.setBounds(bottom.reduced(8));
    trackList.setBounds(content.reduced(8));
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &importButton)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Importar stems", juce::File{}, "*.wav;*.aiff;*.flac;*.mp3");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectMultipleItems,
                                 [this](const juce::FileChooser& chooser)
        {
            const auto files = chooser.getResults();
            fileChooser.reset();
            if (files.isEmpty())
                return;

            juce::String error;
            if (session.importFiles(files, error))
            {
                refreshTrackList();
                analyzeButton.setEnabled(true);
                renderButton.setEnabled(false);
                setStatus(juce::String(session.getTracks().size()) + " stem(s) importado(s). Clique em Analisar.");
            }
            else
            {
                setStatus(error);
            }
        });
    }
    else if (button == &analyzeButton)
    {
        setStatus("Analisando áudio localmente...");
        analyzeButton.setEnabled(false);
        juce::Timer::callAfterDelay(10, [this]
        {
            session.analyzeAll();
            lastPlan = session.createPlan();
            refreshTrackList();
            planLabel.setText(lastPlan.summary + "\n\n" + lastPlan.master.rationale, juce::dontSendNotification);
            setStatus("Análise concluída. Plano inicial criado sem alterar os arquivos originais.");
            analyzeButton.setEnabled(true);
            renderButton.setEnabled(! lastPlan.tracks.empty());
        });
    }
    else if (button == &renderButton)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Salvar mix",
                                                           juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                                               .getChildFile("Classic Mix.wav"),
                                                           "*.wav");
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode,
                                 [this](const juce::FileChooser& chooser)
        {
            const auto output = chooser.getResult();
            fileChooser.reset();
            if (output == juce::File{})
                return;

            juce::String error;
            setStatus("Renderizando mix e masterização inicial...");
            renderButton.setEnabled(false);
            juce::Timer::callAfterDelay(10, [this, output, error]() mutable
            {
                if (session.renderMix(lastPlan, output, error))
                {
                    planLabel.setText(lastPlan.summary + "\n\nArquivo criado: " + output.getFullPathName(),
                                      juce::dontSendNotification);
                    setStatus("Mix renderizada e masterizada. Os stems originais continuam intactos.");
                }
                else
                {
                    setStatus(error);
                }
                renderButton.setEnabled(! lastPlan.tracks.empty());
            });
        });
    }
    else if (button == &clearButton)
    {
        session.clear();
        lastPlan = {};
        refreshTrackList();
        planLabel.setText("O plano da IA aparecerá aqui depois da análise.", juce::dontSendNotification);
        analyzeButton.setEnabled(false);
        renderButton.setEnabled(false);
        setStatus("Sessão limpa.");
    }
}

void MainComponent::timerCallback()
{
    if (pendingStatus.isNotEmpty())
    {
        status.setText(pendingStatus, juce::dontSendNotification);
        pendingStatus.clear();
    }
}

void MainComponent::refreshTrackList()
{
    trackModel = std::make_unique<TrackListModel>(session.getTracks());
    trackList.setModel(trackModel.get());
    trackList.updateContent();
    trackList.repaint();
}

void MainComponent::setStatus(const juce::String& message)
{
    pendingStatus = message;
}
