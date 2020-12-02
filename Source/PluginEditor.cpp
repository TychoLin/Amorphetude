#include "PluginEditor.h"
#include "PluginProcessor.h"

AmorphetudeAudioProcessorEditor::AmorphetudeAudioProcessorEditor(AmorphetudeAudioProcessor& parent)
    : GenericAudioProcessorEditor(parent), audioProcessor(parent)
{
    setSize(600, 400);
}

AmorphetudeAudioProcessorEditor::~AmorphetudeAudioProcessorEditor()
{
}

void AmorphetudeAudioProcessorEditor::resized()
{
    float parentHeight = (float) getChildren().getFirst()->getHeight();
    auto& audioProcessorEditorMap = static_cast<AmorphetudeAudioProcessor*>(&audioProcessor)->getAudioProcessorEditorMap();

    auto bounds = getLocalBounds();
    bounds.removeFromTop(parentHeight);

    for (auto& item : audioProcessorEditorMap)
    {
        if (findChildWithID(item.first) == nullptr)
            addChildAndSetID(item.second, item.first);

        item.second->setBounds(bounds);
    }
}
