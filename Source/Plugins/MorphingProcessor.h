#include <algorithm>
#include <numeric>

#include "ProcessorBase.h"

class MorphingProcessor : public ProcessorBase, public AudioProcessorValueTreeState::Listener
{
public:
    MorphingProcessor()
        : parameters(*this,
                     nullptr,
                     PLUGIN_IDs::morphing,
                     { std::make_unique<AudioParameterFloat>(PARAMETER_IDs::morphingSemitone, "Morphing Semitone", NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::morphingAnalysisThreshold, "Morphing Analysis Threshold", NormalisableRange<float>(-100.0f, 0.0f), -80.0f, "dB"),
                       std::make_unique<AudioParameterFloat>(PARAMETER_IDs::morphingRatio, "Morphing Ratio", NormalisableRange<float>(0.0f, 1.0f), 0.5f) }),
          channel0Analysis(windowSize, hopSize)
    {
        parameters.addParameterListener(PARAMETER_IDs::morphingSemitone, this);
        parameters.addParameterListener(PARAMETER_IDs::morphingAnalysisThreshold, this);
        parameters.addParameterListener(PARAMETER_IDs::morphingRatio, this);

        parameterChanged(PARAMETER_IDs::morphingSemitone, *parameters.getRawParameterValue(PARAMETER_IDs::morphingSemitone));
        parameterChanged(PARAMETER_IDs::morphingAnalysisThreshold, *parameters.getRawParameterValue(PARAMETER_IDs::morphingAnalysisThreshold));
        parameterChanged(PARAMETER_IDs::morphingRatio, *parameters.getRawParameterValue(PARAMETER_IDs::morphingRatio));
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        dsp::ProcessSpec spec { sampleRate, static_cast<uint32>(samplesPerBlock), 2 };

        reset();
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        dsp::AudioBlock<float> block(buffer);
        dsp::ProcessContextReplacing<float> context(block);

        const auto& inputBlock = context.getInputBlock();
        const auto& outputBlock = context.getOutputBlock();
        const auto numSamples = inputBlock.getNumSamples();
        const auto numChannels = inputBlock.getNumChannels();

        for (size_t channel = 0; channel < 1; ++channel)
        {
            auto* inputSamples = inputBlock.getChannelPointer(channel);
            auto* outputSamples = outputBlock.getChannelPointer(channel);

            for (size_t i = 0; i < numSamples; ++i)
            {
                float in = inputSamples[i];

                channel0Analysis.pushSample(in);

                float out = wrappedOutputBuffer[outputBufferReadPointer];

                wrappedOutputBuffer[outputBufferReadPointer] = 0;

                out *= (float) hopSize / (float) channel0Analysis.getWindowSize();

                outputBufferReadPointer++;

                if (outputBufferReadPointer >= wrappedOutputBuffer.size())
                    outputBufferReadPointer = 0;

                if (++hopCounter >= hopSize)
                {
                    hopCounter = 0;

                    processFFT();
                }

                outputSamples[i] = out;
            }
        }
    }

    void reset() override
    {
        channel0Analysis.reset();
        wrappedOutputBuffer.fill(0);
    }

    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const String getName() const override { return PLUGIN_IDs::morphing.toString(); }

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == PARAMETER_IDs::morphingSemitone)
            semitone = newValue;
        else if (parameterID == PARAMETER_IDs::morphingAnalysisThreshold)
            channel0Analysis.setThreshold(newValue);
        else if (parameterID == PARAMETER_IDs::morphingRatio)
            morphingRatio = newValue;
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
    class SignalAnalysis
    {
    public:
        SignalAnalysis(size_t windowSize, int hopSize)
            : windowSize(windowSize),
              hopSize(hopSize),
              window(windowSize, dsp::WindowingFunction<float>::blackman),
              fft(fftOrder),
              unwrappedWindowBuffer(windowSize),
              unwrappedGrainBuffer(fftSize),
              unwrappedSineBuffer(fftSize),
              analysisDecibels(fftSize / 2 + 1),
              analysisBins(fftSize / 2 + 1),
              lastInputPhases(fftSize / 2 + 1),
              lastOutputPhases(fftSize / 2 + 1)
        {
        }

        void pushSample(float sample)
        {
            wrappedInputBuffer[inputBufferPointer++] = sample;

            if (inputBufferPointer >= wrappedInputBuffer.size())
                inputBufferPointer = 0;
        }

        void forwardProcess()
        {
            zeromem(fftData, sizeof(fftData));

            for (int i = 0; i < windowSize; ++i)
            {
                auto circularBufferIndex = (inputBufferPointer + i - windowSize + wrappedInputBuffer.size()) % wrappedInputBuffer.size();
                unwrappedWindowBuffer[i] = wrappedInputBuffer[circularBufferIndex];
            }

            // windowing
            window.multiplyWithWindowingTable(unwrappedWindowBuffer.data(), windowSize);

            // zero phase
            int mid = std::floor(windowSize / 2);

            for (int n = mid, i = 0; n < windowSize; ++n, ++i)
                fftData[i] = unwrappedWindowBuffer[n];

            for (int n = mid - 1, i = fftSize - 1; n >= 0; --n, --i)
                fftData[i] = unwrappedWindowBuffer[n];

            // FFT
            fft.performRealOnlyForwardTransform(fftData, true);
            fftComplexData = reinterpret_cast<dsp::Complex<float>*>(fftData);

            // analysis
            for (int i = 0; i <= fftSize / 2; ++i)
            {
                float phase = std::arg(fftComplexData[i]);
                float phaseDiff = phase - lastInputPhases[i];
                float binCentreFreq = MathConstants<float>::twoPi * i / fftSize;
                phaseDiff = wrapPhase(phaseDiff - binCentreFreq * hopSize);
                float binDeviation = (phaseDiff / hopSize) * fftSize / MathConstants<float>::twoPi;
                analysisBins[i] = i + binDeviation;

                analysisDecibels[i] = Decibels::gainToDecibels(std::abs(fftComplexData[i]));

                lastInputPhases[i] = phase;

                // if (analysisBins[i] < 0)
                //     DBG("i: " << i << " binDeviation: " << binDeviation << " dB: " << analysisDecibels[i]);
            }

            localMaximaCounter = 0;
            localMaximaIndexes.clear();

            for (int i = 1; i < fftSize / 2 && localMaximaCounter < sinusoidalNum; ++i)
            {
                if (analysisDecibels[i - 1] < analysisDecibels[i] && analysisDecibels[i] > analysisDecibels[i + 1])
                {
                    if (analysisDecibels[i] - Decibels::gainToDecibels((float) fftSize / 2) > threshold)
                    {
                        localMaximaIndexes.push_back(i);
                        localMaximaCounter++;
                    }
                }
            }

            partials.clear();
            auto baseFreqIndexIterator = std::find_if(
                localMaximaIndexes.begin(),
                localMaximaIndexes.end(),
                [&](auto binIndex) { return analysisBins[binIndex] > 0; });
            auto localMaximaIndexesIterator = baseFreqIndexIterator;

            while (localMaximaIndexesIterator != localMaximaIndexes.end())
            {
                float partial = analysisBins[*localMaximaIndexesIterator] / analysisBins[*baseFreqIndexIterator];

                jassert(partial > 0);

                if (partial > 0)
                    partials.push_back(partial);

                localMaximaIndexesIterator++;
            }

            // clear previous complex spectrum
            zeromem(synthComplexInput, sizeof(synthComplexInput));
        }

        void generateComplexSpectrum(float freqScaleFactor)
        {
            for (int i = 0; i <= fftSize / 2; ++i)
                lastOutputPhases[i] = wrapPhase(lastOutputPhases[i] + MathConstants<float>::twoPi * freqScaleFactor * analysisBins[i] / fftSize * hopSize);

            // folding frequency filter
            std::vector<int> filteredLocalMaximaIndexs;

            for (int i = 0; i < localMaximaIndexes.size(); ++i)
            {
                if (freqScaleFactor * analysisBins[localMaximaIndexes[i]] < fftSize / 2)
                    filteredLocalMaximaIndexs.push_back(localMaximaIndexes[i]);
            }

            // generate complex spectrum of sinusoidals & scale frequency
            for (int i = 0; i < filteredLocalMaximaIndexs.size(); ++i)
            {
                float localMaximaBin = freqScaleFactor * analysisBins[filteredLocalMaximaIndexs[i]];
                float roundLocalMaximaBin = std::round(localMaximaBin);
                float binDiff = roundLocalMaximaBin - localMaximaBin;
                float mainLobeRange[9] { -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0 };

                for (auto& m : mainLobeRange)
                    m += binDiff;

                dsp::Matrix<float> mainLobeBins(1, 9, mainLobeRange);
                dsp::Matrix<float> mainLobe { generateMainLobe(mainLobeBins) };
                mainLobe *= Decibels::decibelsToGain(analysisDecibels[filteredLocalMaximaIndexs[i]]);
                float phase = lastOutputPhases[filteredLocalMaximaIndexs[i]];

                std::vector<int> localMaximaBins(9);
                std::iota(localMaximaBins.begin(), localMaximaBins.end(), static_cast<int>(roundLocalMaximaBin) - 4);

                for (int ii = 0; ii < 9; ++ii)
                {
                    dsp::Complex<float> z { mainLobe(0, ii) * std::cos(phase), mainLobe(0, ii) * std::sin(phase) };

                    if (localMaximaBins[ii] == 0 || localMaximaBins[ii] == fftSize / 2)
                        synthComplexInput[localMaximaBins[ii]] += dsp::Complex<float> { mainLobe(0, ii) * std::cos(phase), 0 };
                    else if (localMaximaBins[ii] > 0 && localMaximaBins[ii] < fftSize / 2)
                        synthComplexInput[localMaximaBins[ii]] += z;
                    else if (localMaximaBins[ii] < 0)
                        synthComplexInput[-localMaximaBins[ii]] += std::conj(z);
                    else if (localMaximaBins[ii] > fftSize / 2)
                        synthComplexInput[fftSize - localMaximaBins[ii]] += std::conj(z);
                }
            }

            // generate negative frequency
            for (int i = 1; i < fftSize / 2; ++i)
                synthComplexInput[fftSize - i] = std::conj(synthComplexInput[i]);
        }

        void inverseProcess()
        {
            fft.performRealOnlyInverseTransform(fftData);

            // undo zero phase
            // int mid = std::floor(windowSize / 2);
            // int n = 0;

            // for (int i = fftSize - (windowSize - mid); i < fftSize; ++n, ++i)
            //     unwrappedGrainBuffer[n] = fftData[i];

            // for (int i = 0; i < mid; ++n, ++i)
            //     unwrappedGrainBuffer[n] = fftData[i];

            int mid = std::floor(fftSize / 2);
            int n = 0;

            for (int i = fftSize - mid; i < fftSize; ++n, ++i)
                unwrappedGrainBuffer[n] = fftData[i];

            for (int i = 0; i < mid; ++n, ++i)
                unwrappedGrainBuffer[n] = fftData[i];

            // reconstruct sinusoidals
            zeromem(synthComplexOutput, sizeof(synthComplexOutput));
            fft.perform(synthComplexInput, synthComplexOutput, true);

            mid = std::floor(fftSize / 2);
            n = 0;

            for (int i = fftSize - mid; i < fftSize; ++n, ++i)
                unwrappedSineBuffer[n] = synthComplexOutput[i].real();

            for (int i = 0; i < mid; ++n, ++i)
                unwrappedSineBuffer[n] = synthComplexOutput[i].real();
        }

        void reset()
        {
            zeromem(fftData, sizeof(fftData));
            zeromem(synthComplexInput, sizeof(synthComplexInput));
            zeromem(synthComplexOutput, sizeof(synthComplexOutput));
            std::fill(analysisBins.begin(), analysisBins.end(), 0);
            std::fill(analysisDecibels.begin(), analysisDecibels.end(), 0);
            std::fill(unwrappedGrainBuffer.begin(), unwrappedGrainBuffer.end(), 0);
            std::fill(unwrappedSineBuffer.begin(), unwrappedSineBuffer.end(), 0);
            std::fill(unwrappedWindowBuffer.begin(), unwrappedWindowBuffer.end(), 0);
            std::fill(lastInputPhases.begin(), lastInputPhases.end(), 0);
            std::fill(lastOutputPhases.begin(), lastOutputPhases.end(), 0);
            wrappedInputBuffer.fill(0);
        }

        size_t getWindowSize()
        {
            return windowSize;
        }

        void setThreshold(float threshold)
        {
            this->threshold = threshold;
        }

        dsp::Matrix<float> sinc(dsp::Matrix<float> fractionalBins, int N)
        {
            dsp::Matrix<float> sincMainLobe(1, fractionalBins.getNumColumns());

            for (int i = 0; i < fractionalBins.getNumColumns(); ++i)
            {
                float magnitude = std::sin(MathConstants<float>::pi * fractionalBins(0, i)) / std::sin(MathConstants<float>::pi * fractionalBins(0, i) / N);

                if (std::isnan(magnitude))
                    magnitude = N;

                sincMainLobe(0, i) = magnitude;
            }

            return sincMainLobe;
        }

        dsp::Matrix<float> generateMainLobe(dsp::Matrix<float> mainLobeBins)
        {
            int N = 512;
            dsp::Matrix<float> ones(1, mainLobeBins.getNumColumns());
            std::fill(ones.begin(), ones.end(), 1);
            dsp::Matrix<float> mainLobe(1, mainLobeBins.getNumColumns());
            float windowConstants[] { 0.35875, 0.48829, 0.14128, 0.01168 };

            mainLobe += sinc(mainLobeBins, N) * windowConstants[0];
            mainLobe += (sinc(mainLobeBins - ones, N) + sinc(mainLobeBins + ones, N)) * windowConstants[1] * 0.5;
            mainLobe += (sinc(mainLobeBins - ones * 2, N) + sinc(mainLobeBins + ones * 2, N)) * windowConstants[2] * 0.5;
            mainLobe += (sinc(mainLobeBins - ones * 3, N) + sinc(mainLobeBins + ones * 3, N)) * windowConstants[3] * 0.5;

            return mainLobe * (1 / (static_cast<float>(N) * windowConstants[0]));
        }

        enum
        {
            fftOrder = 10,
            fftSize = 1 << fftOrder
        };

        float fftData[2 * fftSize];
        dsp::Complex<float>* fftComplexData;
        std::vector<float> analysisBins;
        std::vector<float> analysisDecibels;
        const int sinusoidalNum = 30;
        std::vector<int> localMaximaIndexes;
        int localMaximaCounter = 0;
        std::vector<float> partials;
        dsp::Complex<float> synthComplexInput[fftSize];
        dsp::Complex<float> synthComplexOutput[fftSize];

        std::vector<float> unwrappedGrainBuffer;
        std::vector<float> unwrappedSineBuffer;

    private:
        float wrapPhase(float phaseIn)
        {
            if (phaseIn >= 0)
                return std::fmod(phaseIn + MathConstants<float>::pi, MathConstants<float>::twoPi) - MathConstants<float>::pi;
            else
                return std::fmod(phaseIn - MathConstants<float>::pi, -MathConstants<float>::twoPi) + MathConstants<float>::pi;
        }

        dsp::FFT fft;
        dsp::WindowingFunction<float> window;

        size_t windowSize;
        int hopSize;

        float threshold = -80.0;

        std::vector<float> unwrappedWindowBuffer;
        std::vector<float> lastInputPhases;
        std::vector<float> lastOutputPhases;

        std::array<float, (1 << 14)> wrappedInputBuffer;
        int inputBufferPointer = 0;
    };

    void processFFT()
    {
        channel0Analysis.forwardProcess();
        channel0Analysis.generateComplexSpectrum(std::pow(2.0, semitone / 12.0));
        channel0Analysis.inverseProcess();

        // overlap-add
        for (int i = 0; i < channel0Analysis.unwrappedSineBuffer.size(); ++i)
        {
            int circularBufferIndex = (outputBufferWritePointer + i) % wrappedOutputBuffer.size();
            wrappedOutputBuffer[circularBufferIndex] += (1.0 - morphingRatio) * channel0Analysis.unwrappedGrainBuffer[i] + morphingRatio * channel0Analysis.unwrappedSineBuffer[i];
        }

        outputBufferWritePointer = (outputBufferWritePointer + hopSize) % wrappedOutputBuffer.size();
    }

    AudioProcessorValueTreeState parameters;

    int windowSize = 1005;
    int hopSize = 201;
    int hopCounter = 0;

    SignalAnalysis channel0Analysis;

    std::array<float, (1 << 14)> wrappedOutputBuffer;
    int outputBufferWritePointer = 1 * hopSize;
    int outputBufferReadPointer = 0;

    float semitone;
    float morphingRatio;
};
