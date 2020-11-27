#include "PluginEditor.h"
#include "PluginProcessor.h"

AmorphetudeAudioProcessorEditor::AmorphetudeAudioProcessorEditor(AmorphetudeAudioProcessor& parent)
    : GenericAudioProcessorEditor(parent), audioProcessor(parent)
{
    setSize(600, 800);
}

AmorphetudeAudioProcessorEditor::~AmorphetudeAudioProcessorEditor()
{
}

void AmorphetudeAudioProcessorEditor::resized()
{
    float parentHeight = (float) getChildren().getFirst()->getHeight();
    auto& audioProcessorEditors = static_cast<AmorphetudeAudioProcessor*>(&audioProcessor)->getAudioProcessorEditors();

    FlexBox fb;

    fb.flexDirection = FlexBox::Direction::column;

    fb.items.add(FlexItem(*getChildren().getFirst()).withFlex(0, 1, parentHeight));

    for (auto* audioProcessorEditor : audioProcessorEditors)
    {
        addAndMakeVisible(audioProcessorEditor);
        fb.items.add(FlexItem(*audioProcessorEditor).withFlex(0, 1, audioProcessorEditor->getHeight()));
    }

    fb.performLayout(getLocalBounds().toFloat());
}
