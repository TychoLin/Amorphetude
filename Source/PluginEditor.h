#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

class AmorphetudeAudioProcessorEditor : public GenericAudioProcessorEditor
{
public:
    AmorphetudeAudioProcessorEditor(AmorphetudeAudioProcessor&);
    ~AmorphetudeAudioProcessorEditor() override;

    void resized() override;

private:
    AmorphetudeAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmorphetudeAudioProcessorEditor)
};
