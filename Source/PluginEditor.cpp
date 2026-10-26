#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kPanel     { 0xff1b1c1f };
const Colour kPanelHi   { 0xff26282c };
const Colour kSection   { 0xff111214 };
const Colour kLegend    { 0xffd9d6cf };
const Colour kLegendDim { 0xff8a8780 };
const Colour kAccent    { 0xffe8a33c };     // EPN amber
const Colour kSeg       { 0xffff4a26 };     // display red
const Colour kSegOff    { 0x22ff4a26 };

Font mono (float h, bool bold = true)
{
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, bold ? Font::bold : Font::plain));
}
Font sans (float h, bool bold = true)
{
    return Font (FontOptions (Font::getDefaultSansSerifFontName(), h, bold ? Font::bold : Font::plain));
}
}

// ============================================================================
void Dr16AudioProcessorEditor::PanelKey::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    // shadow
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), 4.0f);
    if (down) r.translate (0, 1.0f);

    if (lightKey)
    {
        ColourGradient cg (Colour (0xfff1ede3), r.getX(), r.getY(), Colour (0xffc9c3b6), r.getX(), r.getBottom(), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, 3.5f);
        g.setColour (Colour (0xff6b665c)); g.drawRoundedRectangle (r, 3.5f, 1.0f);
        g.setColour (Colour (0xff1b1b1b));
        if (subLegend.isNotEmpty())
        {
            g.setFont (sans (r.getHeight() * 0.22f));
            g.drawText (subLegend, r.withHeight (r.getHeight() * 0.42f).withTrimmedTop (2), Justification::centred);
            g.setFont (sans (r.getHeight() * 0.42f));
            g.drawText (getButtonText(), r.withTrimmedTop (r.getHeight() * 0.36f), Justification::centred);
        }
        else
        {
            g.setFont (sans (r.getHeight() * 0.5f));
            g.drawText (getButtonText(), r, Justification::centred);
        }
    }
    else
    {
        ColourGradient cg (Colour (0xff3a3c40), r.getX(), r.getY(), Colour (0xff17181a), r.getX(), r.getBottom(), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (over ? Colour (0xff6a6d73) : Colour (0xff4a4c50));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (kLegend);
        g.setFont (sans (std::min (13.0f, r.getHeight() * 0.36f)));
        g.drawText (getButtonText(), r.withTrimmedTop (led ? 6.0f : 0.0f), Justification::centred);
        if (led)
        {
            auto l = Rectangle<float> (r.getX() + 5.0f, r.getY() + 4.0f, 5.0f, 5.0f);
            if (ledOn)
            {
                g.setColour (kSeg.withAlpha (0.35f)); g.fillEllipse (l.expanded (3.0f));
                g.setColour (kSeg); g.fillEllipse (l);
            }
            else { g.setColour (Colour (0xff3d1510)); g.fillEllipse (l); }
        }
    }
}

void Dr16AudioProcessorEditor::Encoder::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    auto c = r.getCentre();
    const float R = r.getWidth() * 0.5f;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (r.translated (0, 3.0f));
    // knurled skirt
    ColourGradient sk (Colour (0xffd8dadc), c.x - R, c.y - R, Colour (0xff6e7174), c.x + R, c.y + R, false);
    g.setGradientFill (sk); g.fillEllipse (r);
    g.setColour (Colour (0xff4b4e51));
    for (int i = 0; i < 60; ++i)
    {
        float a = angle + (float) i * MathConstants<float>::twoPi / 60.0f;
        g.drawLine (c.x + std::cos (a) * R * 0.86f, c.y + std::sin (a) * R * 0.86f, c.x + std::cos (a) * R, c.y + std::sin (a) * R, 1.0f);
    }
    // cap with spun finish
    auto cap = r.reduced (R * 0.2f);
    ColourGradient cg (Colour (0xfff2f3f4), cap.getX(), cap.getY(), Colour (0xff8d9093), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (cg); g.fillEllipse (cap);
    g.setColour (Colours::white.withAlpha (0.08f));
    for (float k = 0.15f; k < 1.0f; k += 0.12f) g.drawEllipse (cap.reduced (cap.getWidth() * 0.5f * k), 0.8f);
    g.setColour (Colour (0xff55585b)); g.drawEllipse (cap, 1.0f);
    // dimple
    float da = angle - MathConstants<float>::halfPi;
    float dr = cap.getWidth() * 0.32f;
    g.setColour (Colour (0xff3a3c3f));
    g.fillEllipse (c.x + std::cos (da) * dr - 4.0f, c.y + std::sin (da) * dr - 4.0f, 8.0f, 8.0f);
}
void Dr16AudioProcessorEditor::Encoder::mouseDown (const MouseEvent& e) { lastY = e.getPosition().y; acc = 0.0f; }
void Dr16AudioProcessorEditor::Encoder::mouseDrag (const MouseEvent& e)
{
    acc += (float) (lastY - e.getPosition().y);
    lastY = e.getPosition().y;
    const float px = e.mods.isShiftDown() ? 18.0f : 7.0f;
    while (acc >= px) { acc -= px; step (+1); }
    while (acc <= -px) { acc += px; step (-1); }
}
void Dr16AudioProcessorEditor::Encoder::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    step (w.deltaY > 0 ? 1 : -1);
}

// ============================================================================
Dr16AudioProcessorEditor::Dr16AudioProcessorEditor (Dr16AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    auto mk = [this] (const String& t, bool led, bool light = false, const String& sub = {}) {
        auto* k = keys.add (new PanelKey (t, led, light, sub));
        addAndMakeVisible (k);
        return k;
    };
    kSave = mk ("SAVE", true); kRecall = mk ("RECALL", true);
    kProgram = mk ("PROGRAM", true); kDecay = mk ("DECAY", true);
    kPre = mk ("PRE", true); kLo = mk ("LO", true); kHi = mk ("HI", true);
    kIn = mk ("IN", true); kMix = mk ("MIX", true); kOut = mk ("OUT", true);
    kDown = mk ("DOWN", true); kUp = mk ("UP", true);
    kVintage = mk ("16-BIT", true);

    kSave->onClick    = [this] { selectFn (Fn::Save); };
    kRecall->onClick  = [this] { selectFn (Fn::Recall); };
    kProgram->onClick = [this] { selectFn (Fn::Program); };
    kDecay->onClick   = [this] { selectFn (Fn::Decay); };
    kPre->onClick     = [this] { selectFn (Fn::Pre); };
    kLo->onClick      = [this] { selectFn (Fn::Lo); };
    kHi->onClick      = [this] { selectFn (Fn::Hi); };
    kIn->onClick      = [this] { selectFn (Fn::In); };
    kMix->onClick     = [this] { selectFn (Fn::Mix); };
    kOut->onClick     = [this] { selectFn (Fn::Out); };
    kDown->onClick    = [this] { nudge (-1); };
    kUp->onClick      = [this] { nudge (+1); };
    kVintage->onClick = [this] { proc.set ("vintage", proc.get ("vintage") > 0.5f ? 0.0f : 1.0f); };

    const char* legends[16] = { "1", "2", "3", "A", "4", "5", "6", "B", "7", "8", "9", "C", "*", "0", "#", "D" };
    const char* subs[16]    = { "", "ABC", "DEF", "", "GHI", "JKL", "MNO", "", "PRS", "TUV", "WXY", "", "", "OPER", "", "" };
    for (int i = 0; i < 16; ++i)
    {
        pad[(size_t) i] = mk (legends[i], false, true, subs[i]);
        const String L (legends[i]);
        pad[(size_t) i]->onClick = [this, L] {
            if (L.containsOnly ("0123456789")) keyDigit (L.getIntValue());
            else if (L == "*") keyStar();
            else if (L == "#") keyEnter();
            else if (L == "A") keyAB (false);
            else if (L == "B") keyAB (true);
            // C and D have no function (manual)
        };
    }

    encoder.onStep = [this] (int d) { nudge (d); };
    addAndMakeVisible (encoder);

    setWantsKeyboardFocus (true);
    setSize (1000, 320);
    startTimerHz (30);
}

Dr16AudioProcessorEditor::~Dr16AudioProcessorEditor() { stopTimer(); }

void Dr16AudioProcessorEditor::resized()
{
    rLeft    = { 14.0f, 14.0f, 196.0f, 292.0f };
    rBadge   = { 26.0f, 26.0f, 172.0f, 92.0f };
    rDisplay = { 224.0f, 22.0f, 384.0f, 170.0f };
    rFnBox   = { 224.0f, 202.0f, 384.0f, 104.0f };
    rMeter   = { 624.0f, 22.0f, 136.0f, 284.0f };
    rPad     = { 774.0f, 14.0f, 212.0f, 292.0f };

    kSave->setBounds (30, 150, 76, 40);
    kRecall->setBounds (118, 150, 76, 40);
    kVintage->setBounds (30, 206, 76, 34);

    // function keys: two rows of four + nudge
    const int fx = 236, fy = 212, fw = 82, fh = 38, gx = 10;
    kProgram->setBounds (fx, fy, fw, fh);
    kPre->setBounds (fx + (fw + gx), fy, fw, fh);
    kLo->setBounds (fx + 2 * (fw + gx), fy, fw, fh);
    kHi->setBounds (fx + 3 * (fw + gx), fy, fw, fh);
    kDecay->setBounds (fx, fy + fh + 10, fw, fh);
    kIn->setBounds (fx + (fw + gx), fy + fh + 10, fw, fh);
    kMix->setBounds (fx + 2 * (fw + gx), fy + fh + 10, fw, fh);
    kOut->setBounds (fx + 3 * (fw + gx), fy + fh + 10, fw, fh);

    encoder.setBounds (654, 74, 100, 100);
    kDown->setBounds (640, 250, 56, 40);
    kUp->setBounds (704, 250, 56, 40);

    const int px = 790, py = 30, kw = 42, kh = 58, g = 8;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            pad[(size_t) (r * 4 + c)]->setBounds (px + c * (kw + g), py + r * (kh + 10), kw, kh);
}

// ============================================================================
void Dr16AudioProcessorEditor::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    ColourGradient bg (kPanelHi, 0, 0, kPanel, 0, b.getBottom(), false);
    g.setGradientFill (bg); g.fillAll();
    // brushed texture
    Random rnd (1982);
    for (int y = 0; y < getHeight(); y += 2)
    {
        g.setColour (Colours::white.withAlpha (rnd.nextFloat() * 0.018f));
        g.fillRect (0, y, getWidth(), 1);
    }
    // rack screws
    for (auto pt : { Point<float> (6, 6), Point<float> (6, b.getBottom() - 6), Point<float> (b.getRight() - 6, 6), Point<float> (b.getRight() - 6, b.getBottom() - 6) })
    {
        g.setColour (Colour (0xff0b0b0c)); g.fillEllipse (pt.x - 3.5f, pt.y - 3.5f, 7, 7);
    }

    auto section = [&g] (Rectangle<float> r) {
        g.setColour (kSection.withAlpha (0.65f)); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 6.0f, 1.2f);
        g.setColour (Colours::white.withAlpha (0.05f)); g.drawRoundedRectangle (r.translated (0, 1), 6.0f, 1.0f);
    };
    section (rLeft); section (rFnBox); section (rMeter); section (rPad);

    drawBadge (g, rBadge);
    g.setColour (kLegendDim); g.setFont (sans (10.5f, false));
    g.drawText ("USER MEMORY 00-99", Rectangle<float> (30, 132, 164, 14), Justification::centredLeft);
    g.drawText ("I/O", Rectangle<float> (114, 206, 80, 34), Justification::centredLeft);

    drawDisplay (g, rDisplay);

    // keypad texture
    auto padIn = rPad.reduced (8.0f);
    g.setColour (Colour (0xff0d0d0e)); g.fillRoundedRectangle (padIn, 10.0f);
    Random r2 (16);
    for (int i = 0; i < 1400; ++i)
    {
        g.setColour (Colours::white.withAlpha (r2.nextFloat() * 0.05f));
        g.fillRect (padIn.getX() + r2.nextFloat() * padIn.getWidth(), padIn.getY() + r2.nextFloat() * padIn.getHeight(), 1.5f, 1.5f);
    }

    // level LEDs: green -18, yellow -12, red -6, o/flo
    struct L { const char* t; float th; Colour c; };
    const L leds[4] = { { "o/flo", 999.0f, Colour (0xffff2a1a) }, { "-6", -6.0f, Colour (0xffff2a1a) },
                        { "-12", -12.0f, Colour (0xffffc21a) }, { "-18", -18.0f, Colour (0xff38e05a) } };
    for (int i = 0; i < 4; ++i)
    {
        auto c = Rectangle<float> (634.0f, 70.0f + i * 28.0f, 10.0f, 10.0f);
        bool on = (i == 0) ? ovfHold > 0 : meterDb >= leds[i].th;
        if (on) { g.setColour (leds[i].c.withAlpha (0.3f)); g.fillEllipse (c.expanded (4)); }
        g.setColour (on ? leds[i].c : leds[i].c.darker (2.6f)); g.fillEllipse (c);
        g.setColour (kLegendDim); g.setFont (sans (9.5f, false));
        g.drawText (leds[i].t, Rectangle<float> (610, c.getY() + 11, 58, 11), Justification::centred);
    }
    g.setColour (kLegendDim); g.setFont (sans (10.0f, false));
    g.drawText ("NUDGE", Rectangle<float> (640, 232, 120, 14), Justification::centred);
    g.drawText ("DATA", Rectangle<float> (654, 180, 100, 14), Justification::centred);

    // model line
    g.setColour (kLegendDim); g.setFont (sans (11.0f, false));
    g.drawText ("model DR-16  :  digital reverberation system", Rectangle<float> (224, 300, 384, 16), Justification::centred);
}

void Dr16AudioProcessorEditor::drawBadge (Graphics& g, Rectangle<float> r)
{
    g.setColour (kAccent); g.drawRoundedRectangle (r, 8.0f, 2.0f);
    g.setColour (kAccent.withAlpha (0.08f)); g.fillRoundedRectangle (r, 8.0f);
    g.setColour (kAccent);
    g.setFont (sans (44.0f));
    g.drawText ("EPN", r.withTrimmedBottom (26), Justification::centred);
    g.setFont (sans (11.0f));
    g.drawText ("A U D I O", r.withTrimmedTop (56).withTrimmedBottom (16), Justification::centred);
    g.setColour (kLegend); g.setFont (sans (12.0f));
    g.drawText ("DR-16", r.withTrimmedTop (72), Justification::centred);
}

String Dr16AudioProcessorEditor::fieldText (Fn f) const
{
    const auto& s = epn::program ((int) proc.get ("program"));
    const bool flashing = entering && active == f;
    if (active == f && error) return "E";
    if (flashing) return entry.isEmpty() ? "_" : entry;
    switch (f)
    {
        case Fn::Pre:
            if (s.algo == epn::Algo::Freeze) return "--";
            return String (roundToInt (isEcho() && echoB ? proc.get ("echoB") : std::min (proc.get ("pre"), (float) s.preMaxMs)));
        case Fn::Decay:
            return String (isEcho() && echoB ? proc.get ("echoBDecay") : jlimit (s.decMin, s.decMax, proc.get ("decay")), 1);
        case Fn::Lo: { if (! s.lo) return "--"; int v = (int) proc.get ("lo"); return (v > 0 ? "+" : (v == 0 ? "+" : "")) + String (v); }
        case Fn::Hi: { if (! s.hi) return "--"; int v = (int) proc.get ("hi"); return (v == 0 ? "+" : "") + String (v); }
        case Fn::Mix: return String (proc.get ("mix"), 1);
        case Fn::In:  { float v = proc.get ("in");  return (v >= 0 ? "+" : "") + String (v, 1); }
        case Fn::Out: { float v = proc.get ("out"); return (v >= 0 ? "+" : "") + String (v, 1); }
        default: return {};
    }
}

void Dr16AudioProcessorEditor::drawDisplay (Graphics& g, Rectangle<float> r)
{
    // bezel + red filter glass
    g.setColour (Colour (0xff050505)); g.fillRoundedRectangle (r, 6.0f);
    auto glass = r.reduced (10.0f);
    ColourGradient gg (Colour (0xff1f0603), glass.getX(), glass.getY(), Colour (0xff0e0302), glass.getX(), glass.getBottom(), false);
    g.setGradientFill (gg); g.fillRoundedRectangle (glass, 3.0f);

    const int prog = (int) proc.get ("program");
    const auto& s = epn::program (prog);
    auto line = [&] (Rectangle<float> row, const String& label, const String& value, bool big, const String& ghost) {
        g.setFont (mono (big ? 15.0f : 15.0f, false));
        g.setColour (kSeg.withAlpha (0.9f));
        g.drawText (label, row.withTrimmedLeft (8), Justification::centredLeft);
        g.setFont (mono (big ? 30.0f : 26.0f));
        if (ghost.isNotEmpty()) { g.setColour (kSegOff); g.drawText (ghost, row.withTrimmedRight (8), Justification::centredRight); }
        g.setColour (kSeg.withAlpha (0.25f));
        g.drawText (value, row.withTrimmedRight (8).translated (0.0f, 0.6f), Justification::centredRight);
        g.setColour (kSeg);
        g.drawText (value, row.withTrimmedRight (8), Justification::centredRight);
    };

    // status strip (mix / in / out / converter mode) at the bottom of the glass
    auto inner = glass.reduced (2.0f, 4.0f);
    auto info = inner.removeFromBottom (18.0f).reduced (8.0f, 0.0f);
    g.setColour (kSeg.withAlpha (0.35f)); g.fillRect (glass.getX(), info.getY() - 3.0f, glass.getWidth(), 1.0f);
    g.setFont (mono (12.0f, false)); g.setColour (kSeg.withAlpha (0.75f));
    String inf = "mix " + fieldText (Fn::Mix) + "%   in " + fieldText (Fn::In) + "dB   out " + fieldText (Fn::Out)
                 + "dB" + (proc.get ("vintage") > 0.5f ? "   16-bit" : "   clean");
    g.drawText (inf, info, Justification::centredLeft);
    inner.removeFromBottom (4.0f);

    auto rows = inner;
    const float h = (rows.getHeight() - 8.0f) / 4.2f;
    // row 1: program
    String progTxt;
    if (active == Fn::Program && error) progTxt = "E";
    else if (active == Fn::Program && entering) progTxt = entry.isEmpty() ? "_" : entry;
    else if (active == Fn::Save || active == Fn::Recall)
        progTxt = (active == Fn::Save ? "SAVE " : "RCL ") + (error ? String ("E") : (entering ? (entry.isEmpty() ? "_" : entry) : (memMsg > 0 ? memText : String ("__"))));
    else progTxt = String (s.algo == epn::Algo::Echo ? (echoB ? "ECHO B" : "ECHO A") : s.name);
    line (rows.removeFromTop (h * 1.2f), String (prog).paddedLeft ('0', 2) + "  program", progTxt, true, {});
    g.setColour (kSeg.withAlpha (0.8f)); g.fillRect (glass.getX(), rows.getY() + 1, glass.getWidth(), 2.0f);
    rows.removeFromTop (4);
    line (rows.removeFromTop (h), s.algo == epn::Algo::Echo || s.algo == epn::Algo::Delay ? "delay" : "predelay", fieldText (Fn::Pre), false, "8888");
    line (rows.removeFromTop (h), s.algo == epn::Algo::Echo || s.algo == epn::Algo::Delay ? "regen" : (s.algo == epn::Algo::Chorus ? "depth" : "decay time"), fieldText (Fn::Decay), false, "8.8");
    g.setColour (kSeg.withAlpha (0.8f)); g.fillRect (glass.getX(), rows.getY() + 1, glass.getWidth(), 2.0f);
    rows.removeFromTop (4);
    auto fr = rows.removeFromTop (h);
    g.setFont (mono (15.0f, false)); g.setColour (kSeg.withAlpha (0.9f));
    g.drawText ("filter", fr.withTrimmedLeft (8), Justification::centredLeft);
    g.drawText ("low", fr.withX (fr.getX() + 104).withWidth (40), Justification::centredLeft);
    g.drawText ("high", fr.withX (fr.getX() + 236).withWidth (48), Justification::centredLeft);
    g.setFont (mono (24.0f)); g.setColour (kSeg);
    g.drawText (fieldText (Fn::Lo), fr.withX (fr.getX() + 142).withWidth (70), Justification::centredLeft);
    g.drawText (fieldText (Fn::Hi), fr.withTrimmedRight (8), Justification::centredRight);
}

// ============================================================================
bool Dr16AudioProcessorEditor::isEcho() const { return epn::program ((int) proc.get ("program")).algo == epn::Algo::Echo; }

bool Dr16AudioProcessorEditor::fnAvailable (Fn f) const
{
    const auto& s = epn::program ((int) proc.get ("program"));
    if (f == Fn::Lo) return s.lo;
    if (f == Fn::Hi) return s.hi;
    if (f == Fn::Pre) return s.algo != epn::Algo::Freeze;
    return true;
}

void Dr16AudioProcessorEditor::selectFn (Fn f)
{
    // re-pressing the key clears an error; selecting a key aborts any half-typed entry
    active = f; entering = false; error = false; entry.clear();
    repaint();
}

void Dr16AudioProcessorEditor::keyDigit (int d)
{
    if (error) return;
    if (entry.length() >= 6) return;
    entering = true; entry << d;
    repaint();
}

void Dr16AudioProcessorEditor::keyStar()
{
    if (error) return;
    // '*' is the decimal point for times/levels and the minus sign for filters (manual)
    const bool signFirst = active == Fn::Lo || active == Fn::Hi || active == Fn::In || active == Fn::Out;
    if (signFirst && entry.isEmpty()) { entering = true; entry = "-"; }
    else if (active == Fn::Decay || active == Fn::Mix || active == Fn::In || active == Fn::Out)
    {
        if (! entry.containsChar ('.')) { entering = true; entry << (entry.isEmpty() || entry == "-" ? "0." : "."); }
    }
    repaint();
}

void Dr16AudioProcessorEditor::keyEnter()
{
    if (! entering || entry.isEmpty() || entry == "-") return;
    const auto& s = epn::program ((int) proc.get ("program"));
    const double v = entry.getDoubleValue();
    bool ok = true;

    switch (active)
    {
        case Fn::Program:
            ok = v >= 1 && v <= epn::kNumPrograms && ! entry.containsChar ('.');
            if (ok) { proc.loadProgramDefaults ((int) v); echoB = false; }
            break;
        case Fn::Pre:
        {
            const int mx = isEcho() ? (echoB ? epn::kEchoBMaxMs : epn::kEchoAMaxMs) : s.preMaxMs;
            ok = fnAvailable (Fn::Pre) && v >= 0 && v <= mx;
            if (ok) proc.set (isEcho() && echoB ? "echoB" : "pre", (float) v);
            break;
        }
        case Fn::Decay:
        {
            const float lo = isEcho() && echoB ? 0.0f : s.decMin, hi = isEcho() && echoB ? 9.9f : s.decMax;
            ok = v >= lo - 1e-6 && v <= hi + 1e-6;
            if (ok) proc.set (isEcho() && echoB ? "echoBDecay" : "decay", (float) v);
            break;
        }
        case Fn::Lo: ok = s.lo && v >= -9 && v <= 9; if (ok) proc.set ("lo", (float) v); break;
        case Fn::Hi: ok = s.hi && v >= -9 && v <= 0; if (ok) proc.set ("hi", (float) v); break;
        case Fn::Mix: ok = v >= 0 && v <= 100; if (ok) proc.set ("mix", (float) v); break;
        case Fn::In:  ok = v >= -24 && v <= 12; if (ok) proc.set ("in", (float) v); break;
        case Fn::Out: ok = v >= -24 && v <= 12; if (ok) proc.set ("out", (float) v); break;
        case Fn::Save:
            ok = v >= 0 && v <= 99;
            if (ok) { proc.saveMemory ((int) v); memText = String ((int) v).paddedLeft ('0', 2) + " OK"; memMsg = 45; }
            break;
        case Fn::Recall:
            ok = v >= 0 && v <= 99 && proc.recallMemory ((int) v);
            if (ok) { memText = String ((int) v).paddedLeft ('0', 2) + " OK"; memMsg = 45; echoB = false; }
            break;
    }
    if (ok) { entering = false; entry.clear(); }
    else error = true;   // 'E' shown, LED keeps flashing until the key is pressed again
    repaint();
}

void Dr16AudioProcessorEditor::keyAB (bool b)
{
    if (! isEcho()) return;   // A/B select the ECHO output being programmed
    echoB = b; entering = false; entry.clear(); error = false;
    repaint();
}

void Dr16AudioProcessorEditor::nudge (int dir)
{
    entering = false; error = false; entry.clear();
    const auto& s = epn::program ((int) proc.get ("program"));
    auto bump = [this] (const char* id, float step, float lo, float hi, int dir2) {
        float v = proc.get (id);
        v = std::round ((v + step * (float) dir2) / step) * step;
        proc.set (id, jlimit (lo, hi, v));
    };
    switch (active)
    {
        case Fn::Program:
        {
            int p = jlimit (1, epn::kNumPrograms, (int) proc.get ("program") + dir);
            if (p != (int) proc.get ("program")) { proc.loadProgramDefaults (p); echoB = false; }
            break;
        }
        case Fn::Pre:
            if (! fnAvailable (Fn::Pre)) break;
            if (isEcho() && echoB) bump ("echoB", 10.0f, 1.0f, (float) epn::kEchoBMaxMs, dir);
            else bump ("pre", (float) s.preStepMs, 0.0f, (float) (isEcho() ? epn::kEchoAMaxMs : s.preMaxMs), dir);
            break;
        case Fn::Decay:
            if (isEcho() && echoB) bump ("echoBDecay", 0.1f, 0.0f, 9.9f, dir);
            else bump ("decay", 0.1f, s.decMin, s.decMax, dir);
            break;
        case Fn::Lo: if (s.lo) bump ("lo", 1.0f, -9.0f, 9.0f, dir); break;
        case Fn::Hi: if (s.hi) bump ("hi", 1.0f, -9.0f, 0.0f, dir); break;
        case Fn::Mix: bump ("mix", 1.0f, 0.0f, 100.0f, dir); break;
        case Fn::In:  bump ("in", 0.5f, -24.0f, 12.0f, dir); break;
        case Fn::Out: bump ("out", 0.5f, -24.0f, 12.0f, dir); break;
        default: break;
    }
    (dir > 0 ? kUp : kDown)->ledOn = true;
    blink = std::max (blink, 0);
    repaint();
}

bool Dr16AudioProcessorEditor::keyPressed (const KeyPress& k)
{
    const auto c = k.getTextCharacter();
    if (c >= '0' && c <= '9') { keyDigit (c - '0'); return true; }
    if (c == '.' || c == '*' || c == '-' || c == ',') { keyStar(); return true; }
    if (k == KeyPress::returnKey || c == '#') { keyEnter(); return true; }
    if (k == KeyPress::upKey || k == KeyPress::rightKey) { nudge (+1); return true; }
    if (k == KeyPress::downKey || k == KeyPress::leftKey) { nudge (-1); return true; }
    if (c == 'a' || c == 'A') { keyAB (false); return true; }
    if (c == 'b' || c == 'B') { keyAB (true); return true; }
    if (k == KeyPress::escapeKey) { selectFn (active); return true; }
    return false;
}

void Dr16AudioProcessorEditor::timerCallback()
{
    ++blink;
    const bool flashOn = (blink / 5) % 2 == 0;

    float pk = proc.inputPeak.exchange (0.0f);
    float db = Decibels::gainToDecibels (pk, -90.0f);
    meterDb = std::max (db, meterDb - 1.5f);
    if (proc.overflow.exchange (false)) ovfHold = 15;
    else if (ovfHold > 0) --ovfHold;
    if (memMsg > 0 && --memMsg == 0) memText.clear();

    auto setLed = [] (PanelKey* k, bool on) { if (k->ledOn != on) { k->ledOn = on; k->repaint(); } };
    auto fnLed = [&] (PanelKey* k, Fn f) {
        bool sel = active == f;
        bool on = sel && ((entering || error) ? flashOn : true);
        setLed (k, on);
    };
    fnLed (kProgram, Fn::Program); fnLed (kPre, Fn::Pre); fnLed (kDecay, Fn::Decay);
    fnLed (kLo, Fn::Lo); fnLed (kHi, Fn::Hi); fnLed (kMix, Fn::Mix);
    fnLed (kIn, Fn::In); fnLed (kOut, Fn::Out); fnLed (kSave, Fn::Save); fnLed (kRecall, Fn::Recall);
    setLed (kVintage, proc.get ("vintage") > 0.5f);
    // nudge LEDs light while pressed / briefly after
    setLed (kUp, kUp->isDown()); setLed (kDown, kDown->isDown());

    repaint (rDisplay.toNearestInt());
    repaint (rMeter.toNearestInt());
}
