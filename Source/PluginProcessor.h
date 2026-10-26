#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Machine.h"
#include <array>

struct UserMemory
{
    bool used = false;
    int program = 1;
    float pre = 0, decay = 2, mix = 100, echoB = 150, echoBDecay = 2;
    int lo = 0, hi = 0;
};

class Dr16AudioProcessor : public juce::AudioProcessor
{
public:
    Dr16AudioProcessor();
    ~Dr16AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EPN DR-16"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- helpers used by the front panel
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void set (const char* id, float plainValue);
    void loadProgramDefaults (int program);   // manual: custom settings are lost on program change
    void saveMemory (int slot);
    bool recallMemory (int slot);
    bool memoryUsed (int slot) const { return slot >= 0 && slot < 100 && memories[(size_t) slot].used; }

    juce::AudioProcessorValueTreeState apvts;

    // meters (GUI reads)
    std::atomic<float> inputPeak { 0.0f };    // linear, ADC full scale = 1
    std::atomic<bool> overflow { false };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    epn::Dr16Core core;
    std::array<UserMemory, 100> memories {};
    std::vector<float> mono, wetL, wetR;
    juce::SmoothedValue<float> mixS, outS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dr16AudioProcessor)
};
