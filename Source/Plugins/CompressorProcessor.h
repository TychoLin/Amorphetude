#include "ProcessorBase.h"

class CompressorProcessor : public ProcessorBase, public AudioProcessorValueTreeState::Listener
{
public:
    CompressorProcessor()
        : parameters(*this,
                     nullptr,
                     PLUGIN_IDs::compressor,
                     { std::make_unique<AudioParameterFloat>(PARAMETER_IDs::compressorThreshold, "Compressor Threshold", NormalisableRange<float>(-100.0f, 0.0f), 0.0f, "dB"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::compressorRatio, "Compressor Ratio", NormalisableRange<float>(1.0f, 100.0f, 0.0f, 0.25f), 1.0f, ":1"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::compressorAttack, "Compressor Attack", NormalisableRange<float>(0.01f, 1000.0f, 0.0f, 0.25f), 1.0f, "ms"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::compressorRelease, "Compressor Release", NormalisableRange<float>(10.0f, 10000.0f, 0.0f, 0.25f), 100.0f, "ms") })
    {
        parameters.addParameterListener(PARAMETER_IDs::compressorThreshold, this);
        parameters.addParameterListener(PARAMETER_IDs::compressorRatio, this);
        parameters.addParameterListener(PARAMETER_IDs::compressorAttack, this);
        parameters.addParameterListener(PARAMETER_IDs::compressorRelease, this);

        parameterChanged(PARAMETER_IDs::compressorThreshold, *parameters.getRawParameterValue(PARAMETER_IDs::compressorThreshold));
        parameterChanged(PARAMETER_IDs::compressorRatio, *parameters.getRawParameterValue(PARAMETER_IDs::compressorRatio));
        parameterChanged(PARAMETER_IDs::compressorAttack, *parameters.getRawParameterValue(PARAMETER_IDs::compressorAttack));
        parameterChanged(PARAMETER_IDs::compressorRelease, *parameters.getRawParameterValue(PARAMETER_IDs::compressorRelease));
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        dsp::ProcessSpec spec { sampleRate, static_cast<uint32>(samplesPerBlock), 2 };

        compressor.prepare(spec);
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        dsp::AudioBlock<float> block(buffer);
        dsp::ProcessContextReplacing<float> context(block);

        compressor.process(context);
    }

    void reset() override
    {
        compressor.reset();
    }

    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const String getName() const override { return PLUGIN_IDs::compressor.toString(); }

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == PARAMETER_IDs::compressorThreshold)
            compressor.setThreshold(newValue);
        else if (parameterID == PARAMETER_IDs::compressorRatio)
            compressor.setRatio(newValue);
        else if (parameterID == PARAMETER_IDs::compressorAttack)
            compressor.setAttack(newValue);
        else if (parameterID == PARAMETER_IDs::compressorRelease)
            compressor.setRelease(newValue);
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

    dsp::Compressor<float> compressor;
};
