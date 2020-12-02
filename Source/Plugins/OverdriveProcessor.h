#include "ProcessorBase.h"

class OverdriveProcessor : public ProcessorBase, public AudioProcessorValueTreeState::Listener
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

    const String getName() const override { return PLUGIN_IDs::overdrive.toString(); }

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
