#pragma once

#include <map>

#include "Plugins/AutoWahProcessor.h"
#include "Plugins/BitCrushingProcessor.h"
#include "Plugins/CompressorProcessor.h"
#include "Plugins/EchoProcessor.h"
#include "Plugins/MorphingProcessor.h"
#include "Plugins/OverdriveProcessor.h"

class AmorphetudeAudioProcessor : public AudioProcessor, public AudioProcessorValueTreeState::Listener
{
public:
    using AudioGraphIOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;
    using Node = AudioProcessorGraph::Node;

    AmorphetudeAudioProcessor();
    ~AmorphetudeAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String& newName) override;

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == PARAMETER_IDs::compressorBypass)
            bypassParameters[0] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::morphingBypass)
            bypassParameters[1] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::overdriveBypass)
            bypassParameters[2] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::autowahBypass)
            bypassParameters[3] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::echoBypass)
            bypassParameters[4] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::bitCrushingBypass)
            bypassParameters[5] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::effectSelector)
        {
            selectedEffectIndex = (int) newValue;

            for (auto& item : audioProcessorEditorMap)
                item.second->setVisible(item.first == processorChoices[selectedEffectIndex]);
        }
    }

    std::map<String, AudioProcessorEditor*>& getAudioProcessorEditorMap()
    {
        for (auto slot : slots)
        {
            if (slot != nullptr)
            {
                auto* processor = slot->getProcessor();

                if (audioProcessorEditorMap.count(processor->getName()) == 0)
                    audioProcessorEditorMap[processor->getName()] = processor->createEditor();
            }
        }

        return audioProcessorEditorMap;
    }

    String getSelectedEffectName() { return processorChoices[selectedEffectIndex]; }

private:
    void initialiseGraph()
    {
        mainProcessor->clear();

        audioInputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioInputNode));
        audioOutputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioOutputNode));
        midiInputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::midiInputNode));
        midiOutputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::midiOutputNode));

        connectAudioNodes();
        connectMidiNodes();

        slots.add(slot1Node);
        slots.add(slot2Node);
        slots.add(slot3Node);
        slots.add(slot4Node);
        slots.add(slot5Node);
        slots.add(slot6Node);
    }

    void updateGraph()
    {
        bool hasChanged = false;

        hasChanged = createAndUpdateSlot<CompressorProcessor>(0, PLUGIN_IDs::compressor);
        hasChanged = createAndUpdateSlot<MorphingProcessor>(1, PLUGIN_IDs::morphing);
        hasChanged = createAndUpdateSlot<OverdriveProcessor>(2, PLUGIN_IDs::overdrive);
        hasChanged = createAndUpdateSlot<AutoWahProcessor>(3, PLUGIN_IDs::autowah);
        hasChanged = createAndUpdateSlot<EchoProcessor>(4, PLUGIN_IDs::echo);
        hasChanged = createAndUpdateSlot<BitCrushingProcessor>(5, PLUGIN_IDs::bitCrushing);

        if (hasChanged)
        {
            for (auto connection : mainProcessor->getConnections())
                mainProcessor->removeConnection(connection);

            ReferenceCountedArray<Node> activeSlots;

            for (auto slot : slots)
            {
                if (slot != nullptr)
                {
                    activeSlots.add(slot);

                    slot->getProcessor()->setPlayConfigDetails(getMainBusNumInputChannels(),
                                                               getMainBusNumOutputChannels(),
                                                               getSampleRate(),
                                                               getBlockSize());
                }
            }

            if (activeSlots.isEmpty())
            {
                connectAudioNodes();
            }
            else
            {
                for (int i = 0; i < activeSlots.size() - 1; ++i)
                {
                    for (int channel = 0; channel < 2; ++channel)
                        mainProcessor->addConnection({ { activeSlots.getUnchecked(i)->nodeID, channel },
                                                       { activeSlots.getUnchecked(i + 1)->nodeID, channel } });
                }

                for (int channel = 0; channel < 2; ++channel)
                {
                    mainProcessor->addConnection({ { audioInputNode->nodeID, channel },
                                                   { activeSlots.getFirst()->nodeID, channel } });
                    mainProcessor->addConnection({ { activeSlots.getLast()->nodeID, channel },
                                                   { audioOutputNode->nodeID, channel } });
                }
            }

            connectMidiNodes();

            for (auto node : mainProcessor->getNodes())
                node->getProcessor()->enableAllBuses();
        }

        slot1Node = slots.getUnchecked(0);
        slot2Node = slots.getUnchecked(1);
        slot3Node = slots.getUnchecked(2);
        slot4Node = slots.getUnchecked(3);
        slot5Node = slots.getUnchecked(4);
        slot6Node = slots.getUnchecked(5);

        // bypass setting
        Node::Ptr slot;

        for (int i = 0; i < slots.size(); ++i)
        {
            slot = slots.getUnchecked(i);

            if (slot != nullptr)
                slot->setBypassed(bypassParameters[i]);
        }
    }

    void connectAudioNodes()
    {
        for (int channel = 0; channel < 2; ++channel)
            mainProcessor->addConnection({ { audioInputNode->nodeID, channel },
                                           { audioOutputNode->nodeID, channel } });
    }

    void connectMidiNodes()
    {
        mainProcessor->addConnection({ { midiInputNode->nodeID, AudioProcessorGraph::midiChannelIndex },
                                       { midiOutputNode->nodeID, AudioProcessorGraph::midiChannelIndex } });
    }

    ValueTree getPluginValueTree()
    {
        ValueTree pluginVT { PLUGIN_IDs::PLUGIN_VALUE_TREE, {}, {} };

        for (auto slot : slots)
        {
            if (slot != nullptr)
                pluginVT.appendChild(static_cast<ProcessorBase*>(slot->getProcessor())->getParametersValueTree(), nullptr);
        }

        pluginVT.appendChild(parameters.copyState(), nullptr);

        return pluginVT;
    }

    template <typename Processor>
    bool createAndUpdateSlot(int index, const Identifier& id)
    {
        bool hasChanged = false;
        Node::Ptr slot;
        ValueTree childVT;

        slot = slots.getUnchecked(index);

        if (slot == nullptr)
        {
            slots.set(index, mainProcessor->addNode(std::make_unique<Processor>()));
            hasChanged = true;
        }

        auto* processor = static_cast<ProcessorBase*>(slots.getUnchecked(index)->getProcessor());
        childVT = pluginValueTree.getChildWithName(id);

        if (childVT.isValid() && processor->isParametersUpdated() == false)
            processor->updateParameters(childVT);

        return hasChanged;
    }

    std::array<bool, 6> bypassParameters;
    int selectedEffectIndex = 0;
    StringArray processorChoices { PLUGIN_IDs::compressor.toString(),
                                   PLUGIN_IDs::morphing.toString(),
                                   PLUGIN_IDs::overdrive.toString(),
                                   PLUGIN_IDs::autowah.toString(),
                                   PLUGIN_IDs::echo.toString(),
                                   PLUGIN_IDs::bitCrushing.toString() };

    ValueTree pluginValueTree;
    AudioProcessorValueTreeState parameters;

    Node::Ptr audioInputNode;
    Node::Ptr audioOutputNode;
    Node::Ptr midiInputNode;
    Node::Ptr midiOutputNode;

    ReferenceCountedArray<Node> slots;

    Node::Ptr slot1Node;
    Node::Ptr slot2Node;
    Node::Ptr slot3Node;
    Node::Ptr slot4Node;
    Node::Ptr slot5Node;
    Node::Ptr slot6Node;

    std::map<String, AudioProcessorEditor*> audioProcessorEditorMap;

    std::unique_ptr<AudioProcessorGraph> mainProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmorphetudeAudioProcessor)
};
