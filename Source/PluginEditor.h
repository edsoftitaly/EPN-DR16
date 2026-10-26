#pragma once
#include "PluginProcessor.h"

// Front panel: calculator keypad entry, nudge keys, endless encoder and an
// alphanumeric display, operated the way the RMX16 manual describes.
class Dr16AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Dr16AudioProcessorEditor (Dr16AudioProcessor&);
    ~Dr16AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    enum class Fn { Program, Pre, Decay, Lo, Hi, Mix, In, Out, Save, Recall };

private:
    // ---- widgets ------------------------------------------------------------
    struct PanelKey : public juce::Button
    {
        PanelKey (const juce::String& legend, bool hasLed, bool light = false, const juce::String& sub = {})
            : juce::Button (legend), led (hasLed), lightKey (light), subLegend (sub) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
        bool led, lightKey; juce::String subLegend; bool ledOn = false;
    };

    struct Encoder : public juce::Component
    {
        std::function<void (int)> onStep;
        float angle = 0.0f; float acc = 0.0f; int lastY = 0;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void step (int d) { angle += 0.26f * (float) d; repaint(); if (onStep) onStep (d); }
    };

    // ---- panel logic --------------------------------------------------------
    void selectFn (Fn f);
    void keyDigit (int d);
    void keyStar();
    void keyEnter();
    void keyAB (bool b);
    void nudge (int dir);
    bool isEcho() const;
    bool fnAvailable (Fn f) const;
    juce::String fieldText (Fn f) const;
    void timerCallback() override;

    void drawDisplay (juce::Graphics&, juce::Rectangle<float>);
    void drawBadge (juce::Graphics&, juce::Rectangle<float>);

    Dr16AudioProcessor& proc;

    juce::OwnedArray<PanelKey> keys;
    PanelKey *kSave, *kRecall, *kProgram, *kDecay, *kPre, *kLo, *kHi, *kIn, *kMix, *kOut, *kDown, *kUp, *kVintage;
    std::array<PanelKey*, 16> pad {};
    Encoder encoder;

    Fn active = Fn::Program;
    juce::String entry;
    bool entering = false, error = false, echoB = false;
    int blink = 0, ovfHold = 0, memMsg = 0; juce::String memText;
    float meterDb = -90.0f;

    juce::Rectangle<float> rBadge, rDisplay, rMeter, rPad, rFnBox, rLeft;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dr16AudioProcessorEditor)
};
