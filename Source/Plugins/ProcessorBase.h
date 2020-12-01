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
