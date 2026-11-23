#include "PluginEditor.h"

// ------------------------------------------------------------------ colours / fonts
const ui::Finish& ui::finish (int i)
{
    static const Finish f[3] = {
        { juce::Colour (0xffd6cfbf), juce::Colour (0xff8e877b), juce::Colour (0xff26231f), juce::Colour (0xff9f9686), juce::Colour (0x8cffffff), juce::Colour (0xff3a3631) },
        { juce::Colour (0xff505b55), juce::Colour (0xff323a35), juce::Colour (0xffefe6d2), juce::Colour (0xff2c332f), juce::Colour (0x1fffffff), juce::Colour (0xff141816) },
        { juce::Colour (0xff23211e), juce::Colour (0xff141311), juce::Colour (0xffe9dec7), juce::Colour (0xff0d0c0b), juce::Colour (0x14ffffff), juce::Colour (0xff050505) } };
    return f[juce::jlimit (0, 2, i)];
}

juce::Font ui::cond (float size, bool bold, bool italic)
{
    int style = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
    juce::Font f (juce::FontOptions (size, style));
    f.setHorizontalScale (0.86f);
    return f;
}

static void spacedText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float spacing, juce::Justification j)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (g.getCurrentFont(), t, 0, 0);
    float w = 0; for (int i = 0; i < ga.getNumGlyphs(); ++i) w += ga.getGlyph (i).getBounds().getWidth() + spacing;
    w -= spacing;
    float x = r.getX();
    if (j.testFlags (juce::Justification::horizontallyCentred)) x = r.getCentreX() - w / 2;
    else if (j.testFlags (juce::Justification::right)) x = r.getRight() - w;
    const float y = r.getCentreY() + g.getCurrentFont().getAscent() * 0.5f - 1.f;
    float cx = x;
    for (int i = 0; i < ga.getNumGlyphs(); ++i)
    {
        auto gl = ga.getGlyph (i);
        g.drawSingleLineText (juce::String::charToString (gl.getCharacter()), (int) std::round (cx), (int) std::round (y));
        cx += gl.getBounds().getWidth() + spacing;
    }
}

// ------------------------------------------------------------------ knob
void FaceLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    const float d = (float) juce::jmin (w, h), cx = x + w * 0.5f, cy = y + h * 0.5f, r = d * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    // shadow
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - r, cy - r + 5.f, d, d);
    // knurled skirt
    g.setColour (juce::Colour (0xff0e0d0c)); g.fillEllipse (cx - r, cy - r, d, d);
    g.setColour (juce::Colour (0xff2b2825));
    for (int k = 0; k < 36; ++k)
    {
        const float a = ang + k * juce::MathConstants<float>::twoPi / 36.f;
        juce::Path wedge; wedge.addPieSegment (cx - r, cy - r, d, d, a, a + juce::MathConstants<float>::twoPi / 72.f, 0.f);
        g.fillPath (wedge);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (cx - r + 1, cy - r + 1, d - 2, d - 2, 2.f);
    // cap
    const float cr = r * 0.74f;
    juce::ColourGradient cg (juce::Colour (0xff4d4843), cx - cr * 0.24f, cy - cr * 0.36f, juce::Colour (0xff1a1816), cx + cr, cy + cr, true);
    g.setGradientFill (cg); g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.f);
    // pointer
    juce::Path ptr; ptr.addRoundedRectangle (-2.f, -r * 0.90f, 4.f, r * 0.72f, 2.f);
    g.setColour (juce::Colour (0xffefe6d2));
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

FaceKnob::FaceKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::StringArray n, int ticks, bool st)
    : nums (n), nTicks (ticks), stepped (st)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.f), juce::degreesToRadians (135.f), true);
    slider.setMouseDragSensitivity (stepped ? 120 : 220);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }
    setInterceptsMouseClicks (false, true);
}

void FaceKnob::resized()
{
    const auto c = getLocalBounds().getCentre();
    slider.setBounds (juce::Rectangle<int> (diameter, diameter).withCentre (c));
}

void FaceKnob::paint (juce::Graphics& g)
{
    const float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f, d = (float) diameter;
    const int n = nTicks > 0 ? nTicks : juce::jmax (2, nums.size() * 2 - 1);
    const float R = d / 2 + (d >= 80 ? 7.f : 5.f), Rn = d / 2 + (d >= 80 ? 20.f : 14.f);
    g.setColour (ink);
    for (int i = 0; i < n; ++i)
    {
        const float a = juce::degreesToRadians (-135.f + 270.f * i / (n - 1));
        const bool major = stepped ? true : (nTicks > 0 ? (i == 0 || i == n - 1 || i * 2 == n - 1) : (i % 2 == 0));
        const float h = major ? 7.f : 4.f, r = R + h / 2;
        juce::Path t; t.addRoundedRectangle (-1.f, -h / 2, 2.f, h, 1.f);
        g.fillPath (t, juce::AffineTransform::rotation (a).translated (cx + r * std::sin (a), cy - r * std::cos (a)));
    }
    g.setFont (ui::cond (12.f, true));
    for (int i = 0; i < nums.size(); ++i)
    {
        const float a = juce::degreesToRadians (-135.f + 270.f * i / juce::jmax (1, nums.size() - 1));
        const float x = cx + Rn * std::sin (a), y = cy - Rn * std::cos (a);
        g.drawText (nums[i], juce::Rectangle<float> (x - 20, y - 7, 40, 14), juce::Justification::centred);
    }
}

// ------------------------------------------------------------------ bat switch
BatSwitch::BatSwitch (juce::RangedAudioParameter& p)
    : param (p), att (p, [this] (float) { repaint(); }, nullptr)
{
    att.sendInitialUpdate();
    setRepaintsOnMouseActivity (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void BatSwitch::paint (juce::Graphics& g)
{
    const bool up = param.getValue() > 0.5f;
    const float cx = getWidth() * 0.5f;
    juce::ColourGradient base (juce::Colour (0xfff6f3ec), cx - 3, 26, juce::Colour (0xff9c968b), cx + 13, 45, true);
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (cx - 13, 21, 26, 26);
    g.setGradientFill (base); g.fillEllipse (cx - 13, 19, 26, 26);
    const float top = up ? 4.f : 30.f;
    juce::ColourGradient lev (juce::Colour (0xff8f8a80), cx - 5, 0, juce::Colour (0xffa29d93), cx + 5, 0, false);
    lev.addColour (0.45, juce::Colour (0xfff2efe8));
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (cx - 5, top + 2, 10, 30, 5);
    g.setGradientFill (lev); g.fillRoundedRectangle (cx - 5, top, 10, 30, 5);
}

void BatSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    att.setValueAsCompleteGesture (param.getValue() > 0.5f ? 0.f : 1.f);
}

// ------------------------------------------------------------------ segmented buttons
Segmented::Segmented (juce::RangedAudioParameter& p, juce::StringArray l, juce::Array<int> v, bool dk)
    : param (p), labels (l), values (v), dark (dk)
{
    if (values.isEmpty()) for (int i = 0; i < labels.size(); ++i) values.add (i);
    startTimerHz (10);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int Segmented::current() const { return juce::roundToInt (param.convertFrom0to1 (param.getValue())); }

void Segmented::paint (juce::Graphics& g)
{
    const int n = labels.size();
    auto r = getLocalBounds().toFloat();
    if (dark) { g.setColour (ui::barWell); g.fillRoundedRectangle (r, 6.f); r.reduce (2, 2); }
    const float w = r.getWidth() / n;
    g.setFont (dark ? ui::cond (14.f, true) : ui::cond (13.f, true));
    for (int i = 0; i < n; ++i)
    {
        auto b = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (dark ? 1.f : 2.f, 0);
        const bool on = current() == values[i];
        const bool avail = ! isAvailable || isAvailable (values[i]);
        if (dark)
        {
            g.setColour (on ? ui::amber : ui::barBtn); g.fillRoundedRectangle (b, 4.f);
            g.setColour ((on ? ui::bar : ui::barText).withAlpha (avail ? 1.f : 0.35f));
        }
        else
        {
            g.setColour (inkC); if (on) g.fillRoundedRectangle (b, 4.f); else g.drawRoundedRectangle (b.reduced (0.5f), 4.f, 1.f);
            g.setColour ((on ? plateC : inkC).withAlpha (avail ? 1.f : 0.35f));
        }
        g.drawText (labels[i], b, juce::Justification::centred);
    }
}

void Segmented::mouseUp (const juce::MouseEvent& e)
{
    const int n = labels.size();
    const int i = juce::jlimit (0, n - 1, (int) (e.position.x / (getWidth() / (float) n)));
    if (isAvailable && ! isAvailable (values[i])) return;
    param.beginChangeGesture();
    param.setValueNotifyingHost (param.convertTo0to1 ((float) values[i]));
    param.endChangeGesture();
    repaint();
}

// ------------------------------------------------------------------ VU meter
void VuMeter::paint (juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff141311)); g.fillRoundedRectangle (outer, 10.f);
    auto face = outer.reduced (6.f);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (face, 6.f); g.reduceClipRegion (clip);
    const juce::Colour f0 = power ? juce::Colour (0xfff2c66e) : juce::Colour (0xff7a6a4c);
    juce::ColourGradient fg (f0.brighter (power ? 0.25f : 0.f), face.getCentreX(), face.getY() + face.getHeight() * 0.4f, f0.darker (0.25f), face.getX(), face.getBottom(), true);
    g.setGradientFill (fg); g.fillRect (face);
    g.setOrigin (face.getTopLeft().toInt());
    // scale (geometry from the design: pivot at (140, 214), radius 150)
    const float px = 140, py = 214, R = 150;
    auto angFor = [] (float vuDb) { const float fr = (std::pow (10.f, vuDb / 20.f) - 0.1f) / (std::pow (10.f, 3.f / 20.f) - 0.1f); return juce::degreesToRadians (-48.f + 96.f * fr); };
    auto pt = [&] (float a, float rr) { return juce::Point<float> (px + rr * std::sin (a), py - rr * std::cos (a)); };
    juce::Path arc1, arc2;
    arc1.addCentredArc (px, py, R, R, 0, angFor (-20), angFor (0), true);
    arc2.addCentredArc (px, py, R, R, 0, angFor (0), angFor (3), true);
    g.setColour (ui::meterInk); g.strokePath (arc1, juce::PathStrokeType (1.5f));
    g.setColour (ui::red); g.strokePath (arc2, juce::PathStrokeType (5.f));
    const float majors[] = { -20, -10, -7, -5, -3, -1, 0, 1, 2, 3 };
    const char* lab[] = { "20", "10", "7", "5", "3", "1", "0", "1", "2", "3" };
    g.setFont (ui::cond (14.f, true));
    for (int i = 0; i < 10; ++i)
    {
        const float a = angFor (majors[i]);
        g.setColour (majors[i] > 0 ? ui::red : ui::meterInk);
        g.drawLine (juce::Line<float> (pt (a, R), pt (a, R - 13)), 2.f);
        const auto tp = pt (a, R + 12);
        g.drawText (lab[i], juce::Rectangle<float> (tp.x - 15, tp.y - 8, 30, 16), juce::Justification::centred);
    }
    const float minors[] = { -15, -8, -6, -4, -2, -0.5f, 0.5f, 1.5f, 2.5f };
    g.setColour (ui::meterInk);
    for (float m : minors) { const float a = angFor (m); g.drawLine (juce::Line<float> (pt (a, R), pt (a, R - 7)), 1.2f); }
    g.setFont (ui::cond (18.f, true)); g.setColour (ui::meterInk);
    g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92"), juce::Rectangle<float> (40, 108, 22, 20), juce::Justification::centred);
    g.setColour (ui::red); g.drawText ("+", juce::Rectangle<float> (218, 108, 22, 20), juce::Justification::centred);
    g.setColour (ui::meterInk); g.setFont (ui::cond (22.f, true));
    g.drawText ("VU", juce::Rectangle<float> (100, 110, 80, 24), juce::Justification::centred);
    g.setFont (ui::cond (12.f, true));
    spacedText (g, out ? "OUTPUT" : "GAIN REDUCTION", juce::Rectangle<float> (40, 132, 200, 16), 1.8f, juce::Justification::centred);
    // needle
    const float a = angFor (juce::jlimit (-20.f, 3.f, vu));
    g.setColour (juce::Colour (0xff1a1612));
    g.drawLine (juce::Line<float> (pt (a, 0), pt (a, 162)), 2.f);
    // bottom shade
    juce::Path sh; sh.startNewSubPath (0, 184); sh.lineTo (0, 166); sh.quadraticTo (140, 146, 280, 166); sh.lineTo (280, 184); sh.closeSubPath();
    g.setColour (juce::Colour (0xff141311)); g.fillPath (sh);
    g.restoreState();
}

// ------------------------------------------------------------------ editor
FreeLA2Editor::FreeLA2Editor (FreeLA2Processor& p)
    : AudioProcessorEditor (&p), proc (p),
      preDrive (p.apvts, ID::drive, { "-30", "-15", "0", "+15", "+30" }),
      preOut (p.apvts, ID::level, { "-18", "-9", "0", "+9", "+18" }),
      reduction (p.apvts, ID::reduction, { "0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100" }),
      gain (p.apvts, ID::gain, { "0", "5", "10", "15", "20", "25" }),
      ahead (p.apvts, ID::ahead, { "0", "1.5", "3", "4.5", "6" }, 5, true),
      emphasis (p.apvts, ID::emphasis, {}, 11),
      mix (p.apvts, ID::mix, { "0", "50", "100" }),
      clipDrive (p.apvts, ID::clipdrive, { "0", "10", "20", "30", "40" }),
      clipOut (p.apvts, ID::clipout, { "0", "2", "4", "6", "8", "10" }),
      pushedSw (P (ID::pushed)), limitSw (P (ID::limit)),
      filterSeg (P (ID::filter), { "OFF", "CLEAN", "AIR" }),
      osSeg (P (ID::oslive), { "Off", "2x", "4x", "8x" }),
      renderOsSeg (P (ID::osrender), { "2x", "4x", "8x" }, { 1, 2, 3 }),
      meterSeg (P (ID::meter), { "GR", "OUT" }, {}, false),
      finishSeg (P (ID::finish), { "ENAMEL", "HAMMER", "MIDNIGHT" }, {}, false)
{
    setLookAndFeel (&look);
    struct K { FaceKnob* k; int d; };
    for (auto k : { K { &preDrive, 84 }, K { &preOut, 84 }, K { &reduction, 116 }, K { &gain, 116 }, K { &ahead, 46 },
                    K { &emphasis, 54 }, K { &mix, 54 }, K { &clipDrive, 84 }, K { &clipOut, 84 } })
    {
        k.k->diameter = k.d; k.k->slider.setLookAndFeel (&look); addAndMakeVisible (*k.k);
    }
    for (auto* c : { (juce::Component*) &pushedSw, (juce::Component*) &limitSw, (juce::Component*) &filterSeg, (juce::Component*) &osSeg,
                     (juce::Component*) &renderOsSeg, (juce::Component*) &meterSeg, (juce::Component*) &finishSeg, (juce::Component*) &vu })
        addAndMakeVisible (*c);
    const int mf = p.maxFactorForRate();
    auto avail = [mf] (int idx) { const int f[4] = { 1, 2, 4, 8 }; return f[juce::jlimit (0, 3, idx)] <= mf; };
    osSeg.isAvailable = avail; renderOsSeg.isAvailable = avail;
    filterSeg.setTooltip ("Ultrasonic band-limiting (needs 88.2 kHz or more inside):\nOFF = none (null tests)\n"
                          "CLEAN = a complete 25 kHz filter after each stage\nAIR = D > Pre > C > Comp > B > Clipper > A, then a complete filter");
    preOut.slider.setTooltip ("Pre Output = Leveler Input (one gain: drives the Leveler and the Clipper)");
    mix.slider.setTooltip ("Leveler Mix: blends the Leveler only. 0 % = the Preamp goes straight into the Clipper");
    renderBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff24221f));
    renderBtn.setColour (juce::TextButton::textColourOffId, ui::barText);
    renderBtn.onClick = [this]
    {
        auto& rp = P (ID::rendermode);
        const int next = (choice (ID::rendermode) + 1) % 3;
        rp.beginChangeGesture(); rp.setValueNotifyingHost (rp.convertTo0to1 ((float) next)); rp.endChangeGesture();
    };
    renderBtn.setTooltip ("Use the render oversampling: Auto = when rendering, Always, or Never");
    addAndMakeVisible (renderBtn);
    phaseBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff24221f));
    phaseBtn.setColour (juce::TextButton::textColourOffId, ui::barText);
    phaseBtn.onClick = [this]
    {
        auto& pp = P (ID::phase);
        const int next = 1 - choice (ID::phase);
        pp.beginChangeGesture(); pp.setValueNotifyingHost (pp.convertTo0to1 ((float) next)); pp.endChangeGesture();
    };
    phaseBtn.setTooltip ("Filter phase:\nANALOG = analog-style filters, lowest latency\n"
                         "LINEAR = linear phase: no phase shift in the top end (+1 ms latency)");
    addAndMakeVisible (phaseBtn);
    setSize (1120, 620);
    startTimerHz (30);
}

FreeLA2Editor::~FreeLA2Editor()
{
    for (auto* k : { &preDrive, &preOut, &reduction, &gain, &ahead, &emphasis, &mix, &clipDrive, &clipOut })
        k->slider.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void FreeLA2Editor::resized()
{
    auto place = [] (FaceKnob& k, int cx, int cy) { const int s = k.diameter + 60; k.setBounds (cx - s / 2, cy - s / 2, s, s); };
    place (preDrive, 160, 300); place (preOut, 160, 480);
    place (reduction, 712, 204); place (gain, 712, 430);
    place (ahead, 480, 396);
    place (emphasis, 400, 526); place (mix, 560, 526);
    place (clipDrive, 960, 300); place (clipOut, 960, 480);
    pushedSw.setBounds (136, 112, 48, 64);
    limitSw.setBounds (316, 364, 48, 64);
    vu.setBounds (310, 90, 292, 196);
    meterSeg.setBounds (372, 299, 104, 26);
    finishSeg.setBounds (850, 590, 214, 22);
    // top bar, right to left
    powerArea = { 1068, 6, 36, 36 };
    renderOsSeg.setBounds (966, 9, 90, 30);
    renderBtn.setBounds (898, 9, 64, 30);
    osSeg.setBounds (690, 9, 142, 30);
    phaseBtn.setBounds (512, 9, 72, 30);
    filterSeg.setBounds (372, 9, 136, 30);
}

void FreeLA2Editor::mouseUp (const juce::MouseEvent& e)
{
    if (powerArea.contains (e.getPosition()))
    {
        auto& pw = P (ID::power);
        pw.beginChangeGesture(); pw.setValueNotifyingHost (pw.getValue() > 0.5f ? 0.f : 1.f); pw.endChangeGesture();
        repaint();
    }
}

void FreeLA2Editor::timerCallback()
{
    const bool power = proc.apvts.getRawParameterValue (ID::power)->load() > 0.5f;
    const bool outMode = choice (ID::meter) == 1;
    // VU ballistics (about 300 ms)
    float target;
    if (! power) target = -20.f;
    else if (outMode)
    {
        const float rms = proc.outRms.load();
        target = 20.f * std::log10 (rms + 1e-9f) + 18.f;           // 0 VU = -18 dBFS RMS
    }
    else target = -proc.grMeter.load();
    vuSmooth += (juce::jlimit (-22.f, 4.f, target) - vuSmooth) * 0.105f;
    vu.setState (vuSmooth, power, outMode);
    if (proc.clipLevel.load() >= 1.f) clipHold = 6; else if (clipHold > 0) --clipHold;
    const int fin = choice (ID::finish);
    if (fin != lastFinish)
    {
        lastFinish = fin;
        const auto& F = ui::finish (fin);
        for (auto* k : { &preDrive, &preOut, &reduction, &gain, &ahead, &emphasis, &mix, &clipDrive, &clipOut }) k->setInk (F.ink);
        meterSeg.setColours (F.ink, F.plate); finishSeg.setColours (F.ink, F.plate);
        repaint();                                       // the whole faceplate, not just the knob areas
    }
    renderBtn.setButtonText (juce::StringArray { "AUTO", "ALWAYS", "NEVER" }[choice (ID::rendermode)]);
    phaseBtn.setButtonText (choice (ID::phase) == 1 ? "LINEAR" : "ANALOG");
    repaint (0, 0, getWidth(), 48);
    repaint (540, 290, 80, 40);
    repaint (930, 110, 60, 50);
}

void FreeLA2Editor::paint (juce::Graphics& g)
{
    const auto& F = ui::finish (choice (ID::finish));
    // plate with a brushed texture
    g.fillAll (F.plate);
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    for (int x = 0; x < getWidth(); x += 3) g.fillRect (x, 48, 1, getHeight() - 48);
    // ears and rack slots
    for (int ex : { 0, 1080 })
    {
        g.setColour (F.ear); g.fillRect (ex, 48, 40, 572);
        g.setColour (F.slot); g.fillRoundedRectangle ((float) ex + 8, 88, 24, 12, 6); g.fillRoundedRectangle ((float) ex + 8, 564, 24, 12, 6);
    }
    // rules
    for (int rx : { 280, 840 }) { g.setColour (F.rule); g.fillRect (rx, 64, 1, 524); g.setColour (F.hi); g.fillRect (rx + 1, 64, 1, 524); }
    g.setColour (F.ink);
    auto label = [&] (const juce::String& t, int x, int y, int w, float size, float sp, bool bold = true)
    {
        g.setFont (ui::cond (size, bold)); spacedText (g, t, juce::Rectangle<float> ((float) x, (float) y, (float) w, size + 4), sp, juce::Justification::centred);
    };
    label ("PREAMP", 56, 62, 208, 15.f, 3.4f);
    label ("LEVELING AMPLIFIER", 296, 62, 528, 15.f, 3.4f);
    label ("CLIPPER", 856, 62, 208, 15.f, 3.4f);
    label ("PUSHED", 110, 92, 100, 13.f, 1.8f); label ("NORMAL", 110, 180, 100, 13.f, 1.8f);
    label ("LIMIT", 300, 344, 80, 13.f, 1.8f); label ("COMP", 300, 432, 80, 13.f, 1.8f);
    label ("DRIVE", 100, 352, 120, 13.f, 1.8f); label ("OUTPUT", 100, 532, 120, 13.f, 1.8f);
    label ("PEAK REDUCTION", 632, 288, 160, 14.f, 2.2f); label ("GAIN", 632, 514, 160, 14.f, 2.2f);
    label ("AHEAD ms", 440, 432, 80, 13.f, 1.4f);
    label ("EMPHASIS", 350, 566, 100, 13.f, 1.8f); label ("MIX", 510, 566, 100, 13.f, 1.8f);
    label ("DRIVE", 900, 352, 120, 13.f, 1.8f); label ("OUTPUT", 900, 532, 120, 13.f, 1.8f);
    label ("CLIP", 910, 158, 100, 13.f, 1.8f);
    label ("METER", 314, 303, 56, 13.f, 1.8f); label ("ON", 578, 305, 30, 13.f, 1.8f);
    g.setFont (ui::cond (12.f, true));
    g.drawFittedText ("FAST\nATTACK", 300, 532, 58, 28, juce::Justification::centredRight, 2);
    g.drawFittedText ("SLOW\nBASS", 442, 532, 58, 28, juce::Justification::centredLeft, 2);
    g.setFont (ui::cond (12.f, true));
    g.setColour (F.ink.withAlpha (0.85f));
    spacedText (g, juce::String::fromUTF8 ("PREAMP  \xe2\x86\x92  LEVELER  \xe2\x86\x92  CLIPPER"), juce::Rectangle<float> (40, 592, 1040, 18), 3.6f, juce::Justification::centred);

    const bool power = proc.apvts.getRawParameterValue (ID::power)->load() > 0.5f;
    // ON lamp
    {
        const juce::Rectangle<float> r (552, 302, 22, 22);
        juce::ColourGradient lg = power ? juce::ColourGradient (juce::Colour (0xfffff1c9), r.getX() + 8, r.getY() + 7, juce::Colour (0xff8a5208), r.getRight(), r.getBottom(), true)
                                        : juce::ColourGradient (juce::Colour (0xff8a7a5a), r.getX() + 8, r.getY() + 7, juce::Colour (0xff241a08), r.getRight(), r.getBottom(), true);
        if (power) { g.setColour (juce::Colour (0x8cf0a83a)); g.fillEllipse (r.expanded (5)); }
        g.setGradientFill (lg); g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawEllipse (r, 2.f);
    }
    // CLIP lamp
    {
        const bool lit = power && clipHold > 0;
        const juce::Rectangle<float> r (944, 118, 32, 32);
        juce::ColourGradient lg = lit ? juce::ColourGradient (juce::Colour (0xffffb3a6), r.getX() + 12, r.getY() + 11, juce::Colour (0xff7a120a), r.getRight(), r.getBottom(), true)
                                      : juce::ColourGradient (juce::Colour (0xff8a4a42), r.getX() + 12, r.getY() + 11, juce::Colour (0xff2a0a06), r.getRight(), r.getBottom(), true);
        if (lit) { g.setColour (juce::Colour (0x8cf03c28)); g.fillEllipse (r.expanded (7)); }
        g.setGradientFill (lg); g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawEllipse (r, 2.f);
    }

    // ---- top bar
    g.setColour (ui::bar); g.fillRect (0, 0, getWidth(), 48);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRect (0, 48, getWidth(), 3);
    g.setColour (ui::barText); g.setFont (ui::cond (28.f, true, true));
    g.drawText ("FreeLA2", 20, 6, 110, 36, juce::Justification::centredLeft);
    g.setColour (ui::barSub); g.setFont (ui::cond (12.f, true));
    spacedText (g, "OPTICAL LEVELING AMPLIFIER", juce::Rectangle<float> (128, 8, 220, 16), 1.6f, juce::Justification::left);
    g.setColour (ui::amber.withAlpha (0.9f)); g.setFont (ui::cond (12.f, true));
    spacedText (g, FreeLA2Processor::versionString().toUpperCase(), juce::Rectangle<float> (128, 25, 220, 16), 1.2f, juce::Justification::left);
    g.setColour (ui::barSub); g.setFont (ui::cond (12.f, true));
    spacedText (g, "FILTER", juce::Rectangle<float> (310, 16, 58, 16), 1.6f, juce::Justification::right);
    spacedText (g, "OVERSAMPLE", juce::Rectangle<float> (588, 16, 96, 16), 1.6f, juce::Justification::right);
    spacedText (g, "RENDER", juce::Rectangle<float> (836, 16, 58, 16), 1.6f, juce::Justification::right);
    if (choice (ID::filter) > 0 && ! proc.filtersOn.load())
    {
        g.setColour (ui::barSub.withAlpha (0.8f)); g.setFont (ui::cond (10.f, true));
        g.drawText ("inactive below 88.2k", 372, 38, 136, 10, juce::Justification::centred);
    }
    // power button
    const auto pc = power ? ui::amber : juce::Colour (0xff6b665e);
    const auto pr = powerArea.toFloat();
    g.setColour (juce::Colour (0xff24221f)); g.fillEllipse (pr);
    g.setColour (pc); g.drawEllipse (pr.reduced (1), 2.f);
    juce::Path sym; const float cx = pr.getCentreX(), cy = pr.getCentreY();
    sym.addCentredArc (cx, cy, 8, 8, 0, juce::degreesToRadians (35.f), juce::degreesToRadians (325.f), true);
    sym.startNewSubPath (cx, cy - 10); sym.lineTo (cx, cy - 1);
    g.strokePath (sym, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
