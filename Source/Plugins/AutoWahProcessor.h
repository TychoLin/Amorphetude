#include "ProcessorBase.h"

class AutoWahProcessor : public ProcessorBase, public AudioProcessorValueTreeState::Listener
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

    const String getName() const override { return PLUGIN_IDs::autowah.toString(); }

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
