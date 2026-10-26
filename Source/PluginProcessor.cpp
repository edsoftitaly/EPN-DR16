#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout Dr16AudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto fl = [] (float a, float b, float s) { return NormalisableRange<float> (a, b, s); };

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "program", 1 }, "Program", 1, epn::kNumPrograms, 1,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " " + epn::program (v).name; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pre", 1 }, "Pre-Delay", fl (0.0f, 1200.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 }, "Decay", fl (0.0f, 9.9f, 0.1f), 2.0f,
        AudioParameterFloatAttributes().withLabel ("s")));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "lo", 1 }, "Lo Decay", -9, 9, 0));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "hi", 1 }, "Hi Decay", -9, 0, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Dry/Wet", fl (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "in", 1 }, "Input Level", fl (-24.0f, 12.0f, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output Level", fl (-24.0f, 12.0f, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "echoB", 1 }, "Echo B Delay", fl (1.0f, (float) epn::kEchoBMaxMs, 1.0f), 150.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "echoBDecay", 1 }, "Echo B Regen", fl (0.0f, 9.9f, 0.1f), 2.0f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "vintage", 1 }, "16-bit I/O", true));
    return { p.begin(), p.end() };
}

Dr16AudioProcessor::Dr16AudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DR16", createLayout())
{
}

bool Dr16AudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    const auto in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void Dr16AudioProcessor::prepareToPlay (double sr, int block)
{
    core.prepare (sr);
    mono.assign ((size_t) block, 0.0f); wetL.assign ((size_t) block, 0.0f); wetR.assign ((size_t) block, 0.0f);
    mixS.reset (sr, 0.03); outS.reset (sr, 0.03);
    mixS.setCurrentAndTargetValue (get ("mix") * 0.01f);
    outS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (get ("out")));
}

void Dr16AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nin = getTotalNumInputChannels();
    if ((int) mono.size() < n) { mono.resize ((size_t) n); wetL.resize ((size_t) n); wetR.resize ((size_t) n); }

    epn::MachineParams mp;
    mp.program = (int) get ("program");
    mp.preMs = get ("pre");
    mp.decay = get ("decay");
    mp.lo = (int) get ("lo");
    mp.hi = (int) get ("hi");
    mp.echoBMs = get ("echoB");
    mp.echoBDecay = get ("echoBDecay");
    mp.vintage = get ("vintage") > 0.5f;
    core.setParams (mp);

    // mono input (the original is mono in / stereo out)
    const float inG = juce::Decibels::decibelsToGain (get ("in"));
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float s = buffer.getSample (0, i);
        if (nin > 1) s = 0.5f * (s + buffer.getSample (1, i));
        s *= inG;
        pk = std::max (pk, std::abs (s));
        mono[(size_t) i] = s;
    }
    if (pk > inputPeak.load()) inputPeak.store (pk);   // GUI consumes with exchange(0)

    core.process (mono.data(), wetL.data(), wetR.data(), n);
    if (core.takeOverflow()) overflow.store (true);

    if (nin == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    mixS.setTargetValue (get ("mix") * 0.01f);
    outS.setTargetValue (juce::Decibels::decibelsToGain (get ("out")));
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float m = mixS.getNextValue(), og = outS.getNextValue();
        L[i] = L[i] * (1.0f - m) + wetL[(size_t) i] * og * m;
        R[i] = R[i] * (1.0f - m) + wetR[(size_t) i] * og * m;
    }
}

void Dr16AudioProcessor::set (const char* id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void Dr16AudioProcessor::loadProgramDefaults (int prog)
{
    const auto& s = epn::program (prog);
    set ("program", (float) prog);
    set ("pre", (float) s.preDef);
    set ("decay", s.decDef);
    set ("lo", (float) s.loDef);
    set ("hi", (float) s.hiDef);
    if (s.algo == epn::Algo::Echo) { set ("echoB", 150.0f); set ("echoBDecay", 2.0f); }
}

void Dr16AudioProcessor::saveMemory (int slot)
{
    if (slot < 0 || slot >= 100) return;
    auto& m = memories[(size_t) slot];
    m.used = true;
    m.program = (int) get ("program"); m.pre = get ("pre"); m.decay = get ("decay");
    m.lo = (int) get ("lo"); m.hi = (int) get ("hi"); m.mix = get ("mix");
    m.echoB = get ("echoB"); m.echoBDecay = get ("echoBDecay");
}

bool Dr16AudioProcessor::recallMemory (int slot)
{
    if (! memoryUsed (slot)) return false;
    const auto& m = memories[(size_t) slot];
    set ("program", (float) m.program); set ("pre", m.pre); set ("decay", m.decay);
    set ("lo", (float) m.lo); set ("hi", (float) m.hi); set ("mix", m.mix);
    set ("echoB", m.echoB); set ("echoBDecay", m.echoBDecay);
    return true;
}

void Dr16AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* mem = xml->createNewChildElement ("MEMORIES");
    for (int i = 0; i < 100; ++i)
    {
        const auto& m = memories[(size_t) i];
        if (! m.used) continue;
        auto* e = mem->createNewChildElement ("M");
        e->setAttribute ("slot", i); e->setAttribute ("program", m.program);
        e->setAttribute ("pre", m.pre); e->setAttribute ("decay", m.decay);
        e->setAttribute ("lo", m.lo); e->setAttribute ("hi", m.hi); e->setAttribute ("mix", m.mix);
        e->setAttribute ("echoB", m.echoB); e->setAttribute ("echoBDecay", m.echoBDecay);
    }
    copyXmlToBinary (*xml, dest);
}

void Dr16AudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr) return;
    for (auto& m : memories) m = UserMemory {};
    if (auto* mem = xml->getChildByName ("MEMORIES"))
    {
        for (auto* e : mem->getChildIterator())
        {
            int s = e->getIntAttribute ("slot", -1);
            if (s < 0 || s >= 100) continue;
            auto& m = memories[(size_t) s];
            m.used = true;
            m.program = e->getIntAttribute ("program", 1);
            m.pre = (float) e->getDoubleAttribute ("pre"); m.decay = (float) e->getDoubleAttribute ("decay", 2.0);
            m.lo = e->getIntAttribute ("lo"); m.hi = e->getIntAttribute ("hi");
            m.mix = (float) e->getDoubleAttribute ("mix", 100.0);
            m.echoB = (float) e->getDoubleAttribute ("echoB", 150.0); m.echoBDecay = (float) e->getDoubleAttribute ("echoBDecay", 2.0);
        }
        xml->removeChildElement (mem, true);
    }
    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Dr16AudioProcessor::createEditor() { return new Dr16AudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Dr16AudioProcessor(); }
