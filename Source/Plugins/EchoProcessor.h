#include "ProcessorBase.h"

class EchoProcessor : public ProcessorBase, public AudioProcessorValueTreeState::Listener
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

        parameterChanged(PARAMETER_IDs::echoRatio, *parameters.getRawParameterValue(PARAMETER_IDs::echoRatio));
        parameterChanged(PARAMETER_IDs::echoSmooth, *parameters.getRawParameterValue(PARAMETER_IDs::echoSmooth));
        parameterChanged(PARAMETER_IDs::echoFeedback, *parameters.getRawParameterValue(PARAMETER_IDs::echoFeedback));
        parameterChanged(PARAMETER_IDs::echoMix, *parameters.getRawParameterValue(PARAMETER_IDs::echoMix));

        smoothFilter.setType(dsp::FirstOrderTPTFilterType::lowpass);
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

    const String getName() const override { return PLUGIN_IDs::echo.toString(); }

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
