#pragma once

#include <JuceHeader.h>

namespace PLUGIN_IDs
{
#define DECLARE_ID(name) const Identifier name(#name);

DECLARE_ID(PLUGIN_VALUE_TREE)

DECLARE_ID(amorphetude)
DECLARE_ID(overdrive)
DECLARE_ID(autowah)
DECLARE_ID(echo)
DECLARE_ID(bitCrushing)

#undef DECLARE_ID
} // namespace PLUGIN_IDs

namespace PARAMETER_IDs
{
#define DECLARE_ID(str) constexpr const char* str { #str };

DECLARE_ID(overdriveBypass)
DECLARE_ID(overdriveTone)
DECLARE_ID(overdriveGain)
DECLARE_ID(overdriveMixer)

DECLARE_ID(autowahBypass)
DECLARE_ID(autowahMode)
DECLARE_ID(autowahTempo)
DECLARE_ID(autowahRatio)
DECLARE_ID(autowahFrom)
DECLARE_ID(autowahTo)

DECLARE_ID(echoBypass)
DECLARE_ID(echoTempo)
DECLARE_ID(echoRatio)
DECLARE_ID(echoSmooth)
DECLARE_ID(echoFeedback)
DECLARE_ID(echoMix)

DECLARE_ID(bitCrushingBypass)
DECLARE_ID(bitCrushingDepth)
DECLARE_ID(bitCrushingDitherNoise)

#undef DECLARE_ID
} // namespace PARAMETER_IDs

template <typename Func, typename... Items>
constexpr void forEach(Func&& func, Items&&... items) noexcept(noexcept(std::initializer_list<int> { (func(std::forward<Items>(items)), 0)... }))
{
    (void) std::initializer_list<int> { ((void) func(std::forward<Items>(items)), 0)... };
}

template <typename... Processors>
void prepareAll(const dsp::ProcessSpec& spec, Processors&... processors)
{
    forEach([&](auto& proc) { proc.prepare(spec); }, processors...);
}

template <typename... Processors>
void resetAll(Processors&... processors)
{
    forEach([](auto& proc) { proc.reset(); }, processors...);
}

class ProcessorBase : public AudioProcessor
{
public:
    ProcessorBase() {}

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(AudioBuffer<float>&, MidiBuffer&) override {}

    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const String getName() const override { return {}; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }

    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    void getStateInformation(MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    virtual ValueTree getParametersValueTree() { return {}; }
    virtual void updateParameters(ValueTree&) {}
    virtual bool isParametersUpdated() { return parametersUpdated; }

protected:
    bool parametersUpdated = false;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessorBase)
};

class OverdriveProcessor : public ProcessorBase, AudioProcessorValueTreeState::Listener
{
public:
    OverdriveProcessor()
        : parameters(*this,
                     nullptr,
                     PLUGIN_IDs::overdrive,
                     { std::make_unique<AudioParameterFloat>(PARAMETER_IDs::overdriveTone, "Overdrive Tone", NormalisableRange<float>(-40.0f, 40.0f), 0.0f, "dB"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::overdriveGain, "Overdrive Gain", NormalisableRange<float>(-40.0f, 40.0f), 0.0f, "dB"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::overdriveMixer, "Overdrive Mix", NormalisableRange<float>(0.0f, 100.0f), 100.0f, "%") })
    {
        waveShaper.functionToUse = [](float x) {
            return std::sin(x);
        };

        parameters.addParameterListener(PARAMETER_IDs::overdriveTone, this);
        parameters.addParameterListener(PARAMETER_IDs::overdriveGain, this);
        parameters.addParameterListener(PARAMETER_IDs::overdriveMixer, this);

        parameterChanged(PARAMETER_IDs::overdriveTone, *parameters.getRawParameterValue(PARAMETER_IDs::overdriveTone));
        parameterChanged(PARAMETER_IDs::overdriveGain, *parameters.getRawParameterValue(PARAMETER_IDs::overdriveGain));
        parameterChanged(PARAMETER_IDs::overdriveMixer, *parameters.getRawParameterValue(PARAMETER_IDs::overdriveMixer));
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        dsp::ProcessSpec spec { sampleRate, static_cast<uint32>(samplesPerBlock), 2 };

        oversampling.initProcessing(spec.maximumBlockSize);

        prepareAll(spec, tone, gain, mixer);
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        dsp::AudioBlock<float> block(buffer);
        dsp::ProcessContextReplacing<float> context(block);

        const auto& inputBlock = context.getInputBlock();

        mixer.setWetLatency(oversampling.getLatencyInSamples());
        mixer.pushDrySamples(inputBlock);

        tone.process(context);

        auto oversamplingBlock = oversampling.processSamplesUp(inputBlock);

        dsp::ProcessContextReplacing<float> waveshaperContext(oversamplingBlock);

        waveShaper.process(waveshaperContext);

        auto& outputBlock = context.getOutputBlock();
        oversampling.processSamplesDown(outputBlock);

        gain.process(context);

        mixer.mixWetSamples(outputBlock);
    }

    void reset() override
    {
        resetAll(tone, gain, mixer, oversampling);
    }

    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == PARAMETER_IDs::overdriveTone)
            tone.setGainDecibels(newValue);
        else if (parameterID == PARAMETER_IDs::overdriveGain)
            gain.setGainDecibels(newValue);
        else if (parameterID == PARAMETER_IDs::overdriveMixer)
            mixer.setWetMixProportion(newValue / 100.0f);
    }

    ValueTree getParametersValueTree() override
    {
        return parameters.copyState();
    }

    void updateParameters(ValueTree& valueTree) override
    {
        parameters.replaceState(valueTree);
        parametersUpdated = true;
    }

private:
    AudioProcessorValueTreeState parameters;

    dsp::Gain<float> tone, gain;
    dsp::DryWetMixer<float> mixer { 10 };
    dsp::Oversampling<float> oversampling { 2, 2, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    dsp::WaveShaper<float> waveShaper;
};

class AutoWahProcessor : public ProcessorBase, AudioProcessorValueTreeState::Listener
{
public:
    AutoWahProcessor()
        : parameters(*this,
                     nullptr,
                     PLUGIN_IDs::autowah,
                     { std::make_unique<AudioParameterChoice>(PARAMETER_IDs::autowahMode, "Auto-Wah Mode", StringArray { "LP12", "LP24", "BP12", "BP24", "HP12", "HP24" }, 2),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::autowahTempo, "Auto-Wah Tempo", NormalisableRange<float>(20.0f, 400.0f), 100.0f, "BPM"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::autowahRatio, "Auto-Wah Ratio", NormalisableRange<float>(0.01f, 1.0f), 0.25f),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::autowahFrom, "Auto-Wah From", NormalisableRange<float>(20.0f, 22000.0f, 0.0f, 0.25f), 500.0f, "Hz"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::autowahTo, "Auto-Wah To", NormalisableRange<float>(20.0f, 22000.0f, 0.0f, 0.25f), 3000.0f, "Hz") })
    {
        parameters.addParameterListener(PARAMETER_IDs::autowahMode, this);
        parameters.addParameterListener(PARAMETER_IDs::autowahTempo, this);
        parameters.addParameterListener(PARAMETER_IDs::autowahRatio, this);
        parameters.addParameterListener(PARAMETER_IDs::autowahFrom, this);
        parameters.addParameterListener(PARAMETER_IDs::autowahTo, this);

        parameterChanged(PARAMETER_IDs::autowahMode, *parameters.getRawParameterValue(PARAMETER_IDs::autowahMode));
        parameterChanged(PARAMETER_IDs::autowahTempo, *parameters.getRawParameterValue(PARAMETER_IDs::autowahTempo));
        parameterChanged(PARAMETER_IDs::autowahRatio, *parameters.getRawParameterValue(PARAMETER_IDs::autowahRatio));
        parameterChanged(PARAMETER_IDs::autowahFrom, *parameters.getRawParameterValue(PARAMETER_IDs::autowahFrom));
        parameterChanged(PARAMETER_IDs::autowahTo, *parameters.getRawParameterValue(PARAMETER_IDs::autowahTo));

        ladder.setCutoffFrequencyHz(autowahFrom);
        ladder.setResonance(0.7f);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        dsp::ProcessSpec spec { sampleRate, static_cast<uint32>(samplesPerBlock), 2 };

        prepareAll(spec, ladder);

        reset();
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        dsp::AudioBlock<float> block(buffer);
        dsp::ProcessContextReplacing<float> context(block);

        const auto& inputBlock = context.getInputBlock();
        const auto& outputBlock = context.getOutputBlock();
        const auto numSamples = inputBlock.getNumSamples();

        float wahTime = 60.0f / autowahTempo * autowahRatio;
        float alpha = std::exp(-std::log(9) / (getSampleRate() * wahTime));

        for (size_t i = 0; i < numSamples; ++i)
        {
            // assume the attack of audio channel 0 decide auto-wah effect
            absInput = std::abs(inputBlock.getSample(0, (int) i));

            wahEnv = (1.0f - alpha) * absInput + alpha * lastWahEnv;

            smoothCutoffFreqHz = jmin(autowahFrom + wahEnv * autowahTo, autowahTo);

            lastWahEnv = wahEnv;

            ladder.setCutoffFrequencyHz(smoothCutoffFreqHz);

            auto block = outputBlock.getSubBlock(i, 1);
            dsp::ProcessContextReplacing<float> autowahContext(block);
            ladder.process(autowahContext);
        }
    }

    void reset() override
    {
        resetAll(ladder);

        absInput = 0.0f;
        wahEnv = 0.0f;
        lastWahEnv = 0.0f;
    }

    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == PARAMETER_IDs::autowahMode)
            ladder.setMode([&] {
                switch ((int) newValue)
                {
                    case 0:
                        return dsp::LadderFilterMode::LPF12;
                    case 1:
                        return dsp::LadderFilterMode::LPF24;
                    case 2:
                        return dsp::LadderFilterMode::BPF12;
                    case 3:
                        return dsp::LadderFilterMode::BPF24;
                    case 4:
                        return dsp::LadderFilterMode::HPF12;
                    case 5:
                        return dsp::LadderFilterMode::HPF24;
                    default:
                        break;
                }

                return dsp::LadderFilterMode::BPF12;
            }());
        else if (parameterID == PARAMETER_IDs::autowahTempo)
            autowahTempo = newValue;
        else if (parameterID == PARAMETER_IDs::autowahRatio)
            autowahRatio = newValue;
        else if (parameterID == PARAMETER_IDs::autowahFrom)
            autowahFrom = newValue;
        else if (parameterID == PARAMETER_IDs::autowahTo)
            autowahTo = newValue;
    }

    ValueTree getParametersValueTree() override
    {
        return parameters.copyState();
    }

    void updateParameters(ValueTree& valueTree) override
    {
        parameters.replaceState(valueTree);
        parametersUpdated = true;
    }

private:
    AudioProcessorValueTreeState parameters;

    dsp::LadderFilter<float> ladder;

    float smoothCutoffFreqHz;

    float autowahTempo;
    float autowahRatio;
    float autowahFrom;
    float autowahTo;

    float absInput;
    float wahEnv;
    float lastWahEnv;
};

class EchoProcessor : public ProcessorBase, AudioProcessorValueTreeState::Listener
{
public:
    EchoProcessor()
        : parameters(*this,
                     nullptr,
                     PLUGIN_IDs::echo,
                     { std::make_unique<AudioParameterFloat>(PARAMETER_IDs::echoTempo, "Echo Tempo", NormalisableRange<float>(20.0f, 400.0f), 100.0f, "BPM"),
                       std::make_unique<AudioParameterChoice>(PARAMETER_IDs::echoRatio, "Echo Ratio", StringArray { "1", "1/2", "1/3", "1/4" }, 0),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::echoSmooth, "Echo Smooth", NormalisableRange<float>(20.0f, 10000.0f, 0.0f, 0.25f), 600.0f, "ms"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::echoFeedback, "Echo Feedback", NormalisableRange<float>(-100.0f, 0.0f), -100.0f, "dB"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::echoMix, "Echo Mix", NormalisableRange<float>(0.0f, 100.0f), 50.0f, "%") })
    {
        parameters.addParameterListener(PARAMETER_IDs::echoRatio, this);
        parameters.addParameterListener(PARAMETER_IDs::echoSmooth, this);
        parameters.addParameterListener(PARAMETER_IDs::echoFeedback, this);
        parameters.addParameterListener(PARAMETER_IDs::echoMix, this);

        smoothFilter.setType(dsp::FirstOrderTPTFilterType::lowpass);

        parameterChanged(PARAMETER_IDs::echoRatio, *parameters.getRawParameterValue(PARAMETER_IDs::echoRatio));
        parameterChanged(PARAMETER_IDs::echoSmooth, *parameters.getRawParameterValue(PARAMETER_IDs::echoSmooth));
        parameterChanged(PARAMETER_IDs::echoFeedback, *parameters.getRawParameterValue(PARAMETER_IDs::echoFeedback));
        parameterChanged(PARAMETER_IDs::echoMix, *parameters.getRawParameterValue(PARAMETER_IDs::echoMix));
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        dsp::ProcessSpec spec { sampleRate, static_cast<uint32>(samplesPerBlock), 2 };

        prepareAll(spec, lagrange, smoothFilter, mixer);

        feedback.reset(spec.sampleRate, 0.05);
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        dsp::AudioBlock<float> block(buffer);
        dsp::ProcessContextReplacing<float> context(block);

        const auto& inputBlock = context.getInputBlock();
        const auto& outputBlock = context.getOutputBlock();
        const auto numSamples = inputBlock.getNumSamples();
        const auto numChannels = inputBlock.getNumChannels();

        mixer.pushDrySamples(inputBlock);

        delayLineValue = 60.0 / *parameters.getRawParameterValue(PARAMETER_IDs::echoTempo) * getSampleRate() * echoRatio;
        float smoothDelayLineValue = smoothFilter.processSample(0, delayLineValue);

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* inputSamples = inputBlock.getChannelPointer(channel);
            auto* outputSamples = outputBlock.getChannelPointer(channel);

            for (size_t i = 0; i < numSamples; ++i)
            {
                auto output = inputSamples[i] - feedback.getNextValue() * lastOutput[channel];

                outputSamples[i] = output;

                lagrange.pushSample((int) channel, output);
                lagrange.setDelay(smoothDelayLineValue);
                lastOutput[channel] = lagrange.popSample((int) channel);
            }
        }

        mixer.mixWetSamples(outputBlock);
    }

    void reset() override
    {
        resetAll(lagrange, smoothFilter, mixer);

        std::fill(lastOutput.begin(), lastOutput.end(), 0.0f);
    }

    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == PARAMETER_IDs::echoRatio)
            echoRatio = echoRatios[(int) newValue];
        else if (parameterID == PARAMETER_IDs::echoSmooth)
            smoothFilter.setCutoffFrequency(1000.0 / newValue);
        else if (parameterID == PARAMETER_IDs::echoFeedback)
            feedback.setTargetValue(Decibels::decibelsToGain(newValue, -100.0f));
        else if (parameterID == PARAMETER_IDs::echoMix)
            mixer.setWetMixProportion(newValue / 100.0f);
    }

    ValueTree getParametersValueTree() override
    {
        return parameters.copyState();
    }

    void updateParameters(ValueTree& valueTree) override
    {
        parameters.replaceState(valueTree);
        parametersUpdated = true;
    }

private:
    AudioProcessorValueTreeState parameters;

    static constexpr auto delaySamples = 192000;
    dsp::DelayLine<float, dsp::DelayLineInterpolationTypes::Lagrange3rd> lagrange { delaySamples };

    double delayLineValue;
    dsp::FirstOrderTPTFilter<double> smoothFilter;

    static constexpr double echoRatios[4] { 1.0,
                                            1.0 / 2.0,
                                            1.0 / 3.0,
                                            1.0 / 4.0 };
    double echoRatio;

    LinearSmoothedValue<float> feedback;
    dsp::DryWetMixer<float> mixer;

    std::array<float, 2> lastOutput;
};

class BitCrushingProcessor : public ProcessorBase, AudioProcessorValueTreeState::Listener
{
public:
    BitCrushingProcessor()
        : parameters(*this,
                     nullptr,
                     PLUGIN_IDs::bitCrushing,
                     { std::make_unique<AudioParameterChoice>(PARAMETER_IDs::bitCrushingDepth, "Bit Crushing Depth", StringArray { "8", "10", "12" }, 1),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::bitCrushingDitherNoise, "Bit Crushing Dither Noise", NormalisableRange<float>(-100.0f, 0.0f), -60.0f, "dB") })
    {
        parameters.addParameterListener(PARAMETER_IDs::bitCrushingDepth, this);
        parameters.addParameterListener(PARAMETER_IDs::bitCrushingDitherNoise, this);

        parameterChanged(PARAMETER_IDs::bitCrushingDitherNoise, *parameters.getRawParameterValue(PARAMETER_IDs::bitCrushingDitherNoise));
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        coefficients = FilterCoefs::makePeakFilter(sampleRate, 3750.0f, 10.0f, 0.1f)->coefficients;

        reset();

        ditherNoise.reset(sampleRate, 0.05);
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        dsp::AudioBlock<float> block(buffer);
        dsp::ProcessContextReplacing<float> context(block);

        const auto& inputBlock = context.getInputBlock();
        const auto& outputBlock = context.getOutputBlock();
        const auto numSamples = inputBlock.getNumSamples();
        const auto numChannels = inputBlock.getNumChannels();

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* inputSamples = inputBlock.getChannelPointer(channel);
            auto* outputSamples = outputBlock.getChannelPointer(channel);

            float lastErrorIn = 0.0f;

            for (size_t i = 0; i < numSamples; ++i)
            {
                outputSamples[i] = inputSamples[i] + lastErrorOut[channel] + ditherNoise.getNextValue() * random.nextFloat();

                lastErrorIn = bitReduction(outputSamples[i]) - outputSamples[i];
                lastErrorOut[channel] = coefficients[0] * lastErrorIn + errorDelay1[channel];
                errorDelay1[channel] = coefficients[1] * lastErrorIn - coefficients[3] * lastErrorOut[channel] + errorDelay2[channel];
                errorDelay2[channel] = coefficients[2] * lastErrorIn - coefficients[4] * lastErrorOut[channel];
            }
        }
    }

    void reset() override
    {
        std::fill(lastErrorOut.begin(), lastErrorOut.end(), 0.0f);
        std::fill(errorDelay1.begin(), errorDelay1.end(), 0.0f);
        std::fill(errorDelay2.begin(), errorDelay2.end(), 0.0f);
    }

    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == PARAMETER_IDs::bitCrushingDepth)
            nBitsSize = 1 << nBits[(int) newValue];
        else if (parameterID == PARAMETER_IDs::bitCrushingDitherNoise)
            ditherNoise.setTargetValue(Decibels::decibelsToGain(newValue, -100.0f));
    }

    ValueTree getParametersValueTree() override
    {
        return parameters.copyState();
    }

    void updateParameters(ValueTree& valueTree) override
    {
        parameters.replaceState(valueTree);
        parametersUpdated = true;
    }

private:
    using FilterCoefs = dsp::IIR::Coefficients<float>;

    float bitReduction(float in)
    {
        in = 0.5f * in + 0.5f;
        in = nBitsSize * in;
        in = std::round(in);

        return 2.0f * in / nBitsSize - 1.0f;
    }

    AudioProcessorValueTreeState parameters;

    LinearSmoothedValue<float> ditherNoise;

    static constexpr int nBits[3] { 8, 10, 12 };
    int nBitsSize = 1 << nBits[1];

    Array<float> coefficients;

    std::array<float, 2> lastErrorOut;
    std::array<float, 2> errorDelay1;
    std::array<float, 2> errorDelay2;

    Random random;
};

class AmorphetudeAudioProcessor : public AudioProcessor, AudioProcessorValueTreeState::Listener
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
        if (parameterID == PARAMETER_IDs::overdriveBypass)
            bypassParameters[0] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::autowahBypass)
            bypassParameters[1] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::echoBypass)
            bypassParameters[2] = newValue > 0.5f ? true : false;
        else if (parameterID == PARAMETER_IDs::bitCrushingBypass)
            bypassParameters[3] = newValue > 0.5f ? true : false;
    }

    std::vector<AudioProcessorEditor*>& getAudioProcessorEditors()
    {
        audioProcessorEditors.clear();

        for (auto* slotNode : slots)
        {
            if (slotNode != nullptr)
                audioProcessorEditors.push_back(slotNode->getProcessor()->createEditor());
        }

        return audioProcessorEditors;
    }

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
    }

    void updateGraph()
    {
        bool hasChanged = false;

        hasChanged = createAndUpdateSlot<OverdriveProcessor>(0, PLUGIN_IDs::overdrive);
        hasChanged = createAndUpdateSlot<AutoWahProcessor>(1, PLUGIN_IDs::autowah);
        hasChanged = createAndUpdateSlot<EchoProcessor>(2, PLUGIN_IDs::echo);
        hasChanged = createAndUpdateSlot<BitCrushingProcessor>(3, PLUGIN_IDs::bitCrushing);

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

        for (auto* slotNode : slots)
        {
            if (slotNode != nullptr)
                pluginVT.appendChild(static_cast<ProcessorBase*>(slotNode->getProcessor())->getParametersValueTree(), nullptr);
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

    std::unique_ptr<AudioProcessorGraph> mainProcessor;

    ValueTree pluginValueTree;
    AudioProcessorValueTreeState parameters;

    std::array<bool, 4> bypassParameters;

    Node::Ptr audioInputNode;
    Node::Ptr audioOutputNode;
    Node::Ptr midiInputNode;
    Node::Ptr midiOutputNode;

    ReferenceCountedArray<Node> slots;

    Node::Ptr slot1Node;
    Node::Ptr slot2Node;
    Node::Ptr slot3Node;
    Node::Ptr slot4Node;

    std::vector<AudioProcessorEditor*> audioProcessorEditors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmorphetudeAudioProcessor)
};
