#include "ProcessorBase.h"

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
