#include "PluginProcessor.h"
#include "PluginEditor.h"

AmorphetudeAudioProcessor::AmorphetudeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
                         .withInput("Input", AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)),
#endif
      mainProcessor(new AudioProcessorGraph()),
      parameters(*this,
                 nullptr,
                 PLUGIN_IDs::amorphetude,
                 { std::make_unique<AudioParameterBool>(PARAMETER_IDs::overdriveBypass, "Overdrive Bypass", false),
                   std::make_unique<AudioParameterBool>(PARAMETER_IDs::autowahBypass, "Auto-Wah Bypass", false),
                   std::make_unique<AudioParameterBool>(PARAMETER_IDs::echoBypass, "Echo Bypass", false),
                   std::make_unique<AudioParameterBool>(PARAMETER_IDs::bitCrushingBypass, "Bit Crushing Bypass", true) })
{
    parameters.addParameterListener(PARAMETER_IDs::overdriveBypass, this);
    parameters.addParameterListener(PARAMETER_IDs::autowahBypass, this);
    parameters.addParameterListener(PARAMETER_IDs::echoBypass, this);
    parameters.addParameterListener(PARAMETER_IDs::bitCrushingBypass, this);

    parameterChanged(PARAMETER_IDs::overdriveBypass, *parameters.getRawParameterValue(PARAMETER_IDs::overdriveBypass));
    parameterChanged(PARAMETER_IDs::autowahBypass, *parameters.getRawParameterValue(PARAMETER_IDs::autowahBypass));
    parameterChanged(PARAMETER_IDs::echoBypass, *parameters.getRawParameterValue(PARAMETER_IDs::echoBypass));
    parameterChanged(PARAMETER_IDs::bitCrushingBypass, *parameters.getRawParameterValue(PARAMETER_IDs::bitCrushingBypass));
}

AmorphetudeAudioProcessor::~AmorphetudeAudioProcessor()
{
}

const String AmorphetudeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AmorphetudeAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AmorphetudeAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AmorphetudeAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AmorphetudeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AmorphetudeAudioProcessor::getNumPrograms()
{
    return 1;
}

int AmorphetudeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AmorphetudeAudioProcessor::setCurrentProgram(int index)
{
}

const String AmorphetudeAudioProcessor::getProgramName(int index)
{
    return {};
}

void AmorphetudeAudioProcessor::changeProgramName(int index, const String& newName)
{
}

void AmorphetudeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mainProcessor->setPlayConfigDetails(getMainBusNumInputChannels(),
                                        getMainBusNumOutputChannels(),
                                        sampleRate,
                                        samplesPerBlock);

    mainProcessor->prepareToPlay(sampleRate, samplesPerBlock);

    initialiseGraph();
}

void AmorphetudeAudioProcessor::releaseResources()
{
    mainProcessor->releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AmorphetudeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

#if !JucePlugin_IsSynth
    // This checks if the input layout matches the output layout
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void AmorphetudeAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    updateGraph();

    mainProcessor->processBlock(buffer, midiMessages);
}

bool AmorphetudeAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* AmorphetudeAudioProcessor::createEditor()
{
    return new AmorphetudeAudioProcessorEditor(*this);
}

void AmorphetudeAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    std::unique_ptr<XmlElement> xml(getPluginValueTree().createXml());
    copyXmlToBinary(*xml, destData);
}

void AmorphetudeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        pluginValueTree = ValueTree::fromXml(*xmlState);

    ValueTree childVT = pluginValueTree.getChildWithName(PLUGIN_IDs::amorphetude);

    if (childVT.isValid())
        parameters.replaceState(childVT);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmorphetudeAudioProcessor();
}
