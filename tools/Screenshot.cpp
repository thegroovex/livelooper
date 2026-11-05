// Renders the livelooper editor into PNG files for the documentation.
// usage: livelooper_screenshot <output directory>
// Writes every picture in English (livelooper*.png) and German (livelooper*-de.png).

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Localisation.h"
#include "plugin/KeyMapEditor.h"
#include "plugin/KeyMap.h"
#include "plugin/LiveView.h"
#include "plugin/Widgets.h"
#include "plugin/InfoWindows.h"
#include "plugin/Parameters.h"
#include "plugin/PerformanceRecorder.h"
#include "plugin/SettingsPanel.h"
#include "plugin/WelcomeScreen.h"
#include "plugin/ControllerView.h"
#include "plugin/controllers/Ko2.h"
#include "plugin/AppSettings.h"
#include "plugin/push/PushDevice.h"
#include "plugin/push/PushSurface.h"

using namespace livelooper;
using namespace livelooper::params;

namespace
{
void runBlocks (LiveLooperProcessor& proc, int blocks, float input)
{
    // a pulsing tone, so the waveform overview shows something like a played phrase
    static long long pos = 0;
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i, ++pos)
        {
            const double t = (double) pos / 48000.0;
            const double beat = std::fmod (t * 2.0, 1.0);                     // 120 BPM
            const double env = std::exp (-beat * 5.0) * (0.55 + 0.45 * std::sin (t * 1.7));
            const float v = input * (float) (env * std::sin (t * 2.0 * 3.14159265 * 220.0));
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }
        proc.processBlock (buffer, midi);
    }
}

void press (LiveLooperProcessor& proc, const juce::String& id)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // let earlier triggers reset
    runBlocks (proc, 1, 0.0f);
    proc.pressTrigger (id);
}

void save (juce::Component& c, const juce::File& file, float scale = 1.0f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
    std::cout << "wrote " << file.getFullPathName() << std::endl;
}

//============================================================================== v1.7.1 keyboard map picture
/** A key cap: the key without its modifiers (arrows drawn), a Shift arrow in front, a red edge = hold 1.5 s. */
struct KeyCap
{
    juce::KeyPress key;
    bool hold = false;

    static float textW (const juce::String& t, float h) { return juce::GlyphArrangement::getStringWidth (LooperLookAndFeel::font (h * 0.56f, true), t); }
    juce::String label() const
    {
        const int code = key.getKeyCode();
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
            return {};
        return KeyMap::describeKey (juce::KeyPress (code));
    }
    bool shift() const { return key.getModifiers().isShiftDown(); }
    float width (float h) const
    {
        const auto t = label();
        float w = t.isEmpty() ? h * 0.55f : textW (t, h);
        if (shift())
            w += h * 0.62f;
        return juce::jmax (h, w + h * 0.62f);
    }
    void draw (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const float h = r.getHeight(), corner = h * 0.24f;
        // shadow, light key cap, brand gradient rim (red rim + bar for a held key)
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0.0f, h * 0.10f).expanded (1.0f), corner);
        juce::ColourGradient body (juce::Colour (0xffffffff), r.getX(), r.getY(), juce::Colour (0xffdfe2e9), r.getX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, corner);
        if (hold)
            g.setColour (colours::red);
        else
            g.setGradientFill (LooperLookAndFeel::brandGradient (r));
        g.drawRoundedRectangle (r.reduced (0.75f), corner, 1.6f);
        if (hold)
            g.fillRoundedRectangle (r.withTrimmedTop (h * 0.80f).reduced (h * 0.22f, 0.0f).withHeight (h * 0.09f), h * 0.04f);
        auto inner = r.reduced (h * 0.31f, 0.0f);
        const auto ink = juce::Colour (0xff17181d);
        g.setColour (ink);
        if (shift())
        {
            // Shift: an outlined up arrow
            const auto a = inner.removeFromLeft (h * 0.5f).withSizeKeepingCentre (h * 0.42f, h * 0.48f);
            juce::Path p;
            p.startNewSubPath (a.getCentreX(), a.getY());
            p.lineTo (a.getRight(), a.getY() + a.getHeight() * 0.5f);
            p.lineTo (a.getX() + a.getWidth() * 0.72f, a.getY() + a.getHeight() * 0.5f);
            p.lineTo (a.getX() + a.getWidth() * 0.72f, a.getBottom());
            p.lineTo (a.getX() + a.getWidth() * 0.28f, a.getBottom());
            p.lineTo (a.getX() + a.getWidth() * 0.28f, a.getY() + a.getHeight() * 0.5f);
            p.lineTo (a.getX(), a.getY() + a.getHeight() * 0.5f);
            p.closeSubPath();
            g.strokePath (p, juce::PathStrokeType (juce::jmax (1.2f, h * 0.06f), juce::PathStrokeType::mitered));
            inner.removeFromLeft (h * 0.12f);
        }
        const int code = key.getKeyCode();
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
        {
            const auto a = inner.withSizeKeepingCentre (h * 0.40f, h * 0.40f);
            juce::Path p;
            if (code == juce::KeyPress::upKey)
                p.addTriangle (a.getCentreX(), a.getY(), a.getRight(), a.getBottom(), a.getX(), a.getBottom());
            else
                p.addTriangle (a.getX(), a.getY(), a.getRight(), a.getY(), a.getCentreX(), a.getBottom());
            g.fillPath (p);
            return;
        }
        g.setFont (LooperLookAndFeel::font (h * 0.56f, true));
        g.drawText (label(), inner, juce::Justification::centred, false);
    }
};

/** The surface with a key cap at every button the default computer keyboard mapping plays, and a legend with all
    default keys - generated from KeyMap::setDefaults() and the components' parameter ids, so it follows both. */
void saveKeyMapPicture (LiveLooperEditor& ed, const juce::File& file)
{
    using i18n::tr;
    const KeyMap km;                                    // the defaults
    const float S = 1.5f;                               // output scale
    const float m = 34.0f, headH = 92.0f, capH = 24.0f;
    const auto edArea = juce::Rectangle<float> (m, headH, (float) ed.getWidth(), (float) ed.getHeight());

    // the legend rows: per track function one row with the 8 keys, then every other default key
    struct Row { std::vector<KeyCap> caps; juce::String text; };
    std::vector<Row> trackRows, globalRows;
    juce::StringArray listed;
    const std::pair<const char*, const char*> trackFns[] = {
        { kRecPlay, "REC / PLAY / DUB - tracks 1 - 8" }, { kStop, "STOP - tracks 1 - 8" },
        { kClear, "CLEAR the track - hold 1.5 s" }, { kUndo, "UNDO - hold 1.5 s when it empties the track" }, { kRedo, "REDO" } };
    for (const auto& [fn, text] : trackFns)
    {
        Row row { {}, tr (text) };
        for (int t = 0; t < kNumTracks; ++t)
        {
            const auto k = km.getKeyFor (trackId (t, fn));
            listed.add (trackId (t, fn));
            if (k.isValid())
                row.caps.push_back ({ k, juce::String (fn) == kClear });
        }
        if (! row.caps.empty())
            trackRows.push_back (row);
    }
    juce::StringArray globals { gAllStartStop, gClearAll, gTap, KeyMap::actEnterTempo, KeyMap::actTempoUp, KeyMap::actTempoDown,
                                KeyMap::actTempoUpFine, KeyMap::actTempoDownFine };
    for (const auto& a : KeyMap::getAvailableActions())
        if (! listed.contains (a) && ! globals.contains (a))
            globals.add (a);                            // any other default (rhythm, quantize ...) in the mapping's order
    for (const auto& a : globals)
    {
        const auto k = km.getKeyFor (a);
        if (! k.isValid() || listed.contains (a))
            continue;
        listed.add (a);
        const bool hold = a == gClearAll;
        auto text = KeyMap::describeAction (a);
        if (hold)
            text << " - " << tr ("hold 1.5 s");
        globalRows.push_back ({ { { k, hold } }, text });
    }
    // the legend sits on the right of the surface (landscape - fits the in-app help page too)
    const float legendW = 560.0f;
    const float W = edArea.getRight() + 24.0f + legendW + m, H = edArea.getBottom() + m;

    juce::Image img (juce::Image::ARGB, juce::roundToInt (W * S), juce::roundToInt (H * S), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (S));

    // brand background: dark with soft gradient glows
    g.fillAll (juce::Colour (0xff0b0a10));
    const std::tuple<float, float, juce::Colour, float> glows[] = {
        { 0.12f, 0.08f, colours::brand0, 0.30f }, { 0.55f, 0.98f, colours::brand1, 0.20f }, { 0.95f, 0.18f, colours::brand2, 0.28f } };
    for (const auto& [gx, gy, col, op] : glows)
    {
        const juce::Point<float> c (W * gx, H * gy);
        juce::ColourGradient rg (col.withAlpha (op), c, col.withAlpha (0.0f), c.translated (W * 0.45f, 0.0f), true);
        g.setGradientFill (rg);
        g.fillRect (0.0f, 0.0f, W, H);
    }

    // header: logo with badge, title and hints
    const float badgeRight = LiveView::drawLogo (g, { m, 18.0f }, i18n::upper (tr ("Keys")), 30.0f);
    g.setColour (juce::Colours::white);
    g.setFont (LooperLookAndFeel::font (21.0f, true));
    const float tx = badgeRight + 30.0f;
    g.drawText (tr ("Computer keyboard - default keys (standalone app)"), juce::Rectangle<float> (tx, 20.0f, W - tx - m, 30.0f), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffb9bcc8));
    g.setFont (LooperLookAndFeel::font (13.5f, false));
    g.drawText (tr ("Shift = second function  .  red key = hold 1.5 s  .  change it in SETUP > Keyboard mapping"),
                juce::Rectangle<float> (tx, 50.0f, W - tx - m, 24.0f), juce::Justification::centredLeft);

    // the surface, slightly dimmed so the caps stand out
    {
        const auto shot = ed.createComponentSnapshot (ed.getLocalBounds(), true, S);
        juce::Path clip;
        clip.addRoundedRectangle (edArea, 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (edArea.translated (0.0f, 8.0f).expanded (6.0f), 18.0f);
        g.saveState();
        g.reduceClipRegion (clip);
        g.drawImage (shot, edArea);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRect (edArea);
        g.restoreState();
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.strokePath (clip, juce::PathStrokeType (1.0f));
    }

    // a cap at every visible button of a mapped function (STOP also carries its hold-to-clear key)
    juce::StringArray done;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& parent) {
        for (auto* c : parent.getChildren())
        {
            if (! c->isVisible() || c->getWidth() <= 0)
                continue;
            const auto pid = getParamId (*c);
            if (pid.isNotEmpty() && ! done.contains (pid))
            {
                std::vector<KeyCap> caps;
                if (const auto k = km.getKeyFor (pid); k.isValid())
                    caps.push_back ({ k, pid == gClearAll });
                for (int t = 0; t < kNumTracks; ++t)
                    if (pid == trackId (t, kStop))
                        if (const auto k = km.getKeyFor (trackId (t, kClear)); k.isValid())
                            caps.push_back ({ k, true });
                if (! caps.empty())
                {
                    done.add (pid);
                    const auto r = ed.getLocalArea (c, c->getLocalBounds()).toFloat() + edArea.getPosition();
                    float x = r.getRight() + 4.0f;
                    float total = 0.0f;
                    for (const auto& cap : caps)
                        total += cap.width (capH) + 4.0f;
                    x = juce::jmin (r.getRight() - total * 0.5f, edArea.getRight() - total + 8.0f);
                    const float y = r.getY() - capH * 0.45f;
                    for (const auto& cap : caps)
                    {
                        const float w = cap.width (capH);
                        cap.draw (g, { x, y, w, capH });
                        x += w + 4.0f;
                    }
                }
            }
            walk (*c);
        }
    };
    walk (ed);

    // legend (glass panel, right): each track function with its 8 keys, then the global keys
    const auto panel = juce::Rectangle<float> (edArea.getRight() + 24.0f, edArea.getY(), legendW, edArea.getHeight());
    LooperLookAndFeel::drawGlassPanel (g, panel, 16.0f, 1, colours::brand1);
    auto inner = panel.reduced (22.0f, 16.0f);
    auto heading = [&g] (juce::Rectangle<float> r, const juce::String& text) {
        g.setColour (juce::Colour (0xffe6e8ee));
        g.setFont (LooperLookAndFeel::font (12.5f, true));
        g.drawText (i18n::upper (text), r, juce::Justification::centredLeft);
        g.setGradientFill (LooperLookAndFeel::brandGradient (r));
        g.fillRect (r.getX(), r.getBottom() - 1.0f, 46.0f, 2.0f);
    };
    auto drawCaps = [&g] (const std::vector<KeyCap>& caps, float x, float cy, float h) {
        for (const auto& cap : caps)
        {
            const float w = cap.width (h);
            cap.draw (g, { x, cy - h * 0.5f, w, h });
            x += w + 5.0f;
        }
        return x;
    };
    heading (inner.removeFromTop (24.0f), tr ("Tracks 1 - 8 (one keyboard column per track)"));
    inner.removeFromTop (8.0f);
    for (const auto& row : trackRows)
    {
        auto r = inner.removeFromTop (58.0f);
        g.setColour (juce::Colour (0xffd4d7e0));
        g.setFont (LooperLookAndFeel::font (13.5f, false));
        g.drawFittedText (row.text, r.removeFromTop (22.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
        drawCaps (row.caps, r.getX(), r.getY() + 15.0f, capH * 0.95f);
    }
    inner.removeFromTop (8.0f);
    heading (inner.removeFromTop (24.0f), tr ("Global"));
    inner.removeFromTop (6.0f);
    const float rh = juce::jmin (36.0f, inner.getHeight() / (float) juce::jmax ((size_t) 1, globalRows.size()));
    for (const auto& row : globalRows)
    {
        auto r = inner.removeFromTop (rh);
        const float x = drawCaps (row.caps, r.getX(), r.getCentreY(), capH * 0.95f);
        g.setColour (juce::Colour (0xffd4d7e0));
        g.setFont (LooperLookAndFeel::font (13.5f, false));
        const float tx = juce::jmax (x + 10.0f, r.getX() + 150.0f);
        g.drawFittedText (row.text, juce::Rectangle<float> (tx, r.getY(), r.getRight() - tx, r.getHeight()).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    }

    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
    std::cout << "wrote " << file.getFullPathName() << std::endl;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    i18n::setSetting (i18n::Language::English, false);
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    LiveLooperProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.markStandaloneForTests();      // v1.7: SETUP shows the app rows (auto load); no backups are written here
    auto set = [&proc] (const juce::String& id, float plain) {
        auto* p = proc.getState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };
    set ("quantize", 0.0f);
    set ("recaction", 1.0f);

    // Track 1: playing loop, track 2: overdubbing, track 3: stopped, track 4: recording
    press (proc, "t1_recplay"); runBlocks (proc, 300, 0.4f);
    press (proc, "t1_recplay"); runBlocks (proc, 60, 0.0f);
    press (proc, "t2_recplay"); runBlocks (proc, 200, 0.3f);
    press (proc, "t2_recplay"); runBlocks (proc, 20, 0.0f);
    press (proc, "t2_recplay"); runBlocks (proc, 70, 0.2f);
    press (proc, "t3_recplay"); runBlocks (proc, 150, 0.3f);
    press (proc, "t3_recplay"); runBlocks (proc, 2, 0.0f);
    press (proc, "t3_stop");    runBlocks (proc, 2, 0.0f);
    press (proc, "t4_recplay"); runBlocks (proc, 90, 0.5f);

    set ("t1_fx", 1.0f);
    set ("t2_reverse", 1.0f);
    set ("inA_on", 1.0f);
    set ("trA_on", 1.0f);
    set ("rhythm_on", 1.0f);
    set ("t3_level", 0.8f);
    set ("t2_pan", -0.3f);
    set ("t5_measure", 3.0f);   // v1.7 loop length: track 5 (empty) records 4 bars
    press (proc, "scene1");     // store a scene: shows its LED in the footer
    runBlocks (proc, 1, 0.0f);
    proc.loadDefaultMidiMap();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    runBlocks (proc, 1, 0.0f);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    // feed the meters: a few frames with audio running
    for (int f = 0; f < 4; ++f)
    {
        runBlocks (proc, 3, 0.5f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }
    editor->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
    auto* ed = dynamic_cast<LiveLooperEditor*> (editor.get());

    for (const auto& [language, suffix] : { std::pair<i18n::Language, const char*> { i18n::Language::English, "" },
                                            std::pair<i18n::Language, const char*> { i18n::Language::German, "-de" } })
    {
        i18n::setSetting (language, false);
        const juce::String sfx (suffix);
        ed->showStatus ({});
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        save (*editor, outDir.getChildFile ("livelooper" + sfx + ".png"));
        // v1.7.1: the default computer keyboard keys on the surface (docs + in-app help)
        saveKeyMapPicture (*ed, outDir.getChildFile ("livelooper-keymap" + sfx + ".png"));
        {
            // v1.7 loop length: the top of the track strips (title row with the loop length pills, displays), 2x
            auto img = editor->createComponentSnapshot ({ 12, 326, 904, 118 }, true, 2.0f);
            const auto f = outDir.getChildFile ("livelooper-loop-length" + sfx + ".png");
            f.deleteFile();
            juce::FileOutputStream out (f);
            juce::PNGImageFormat().writeImageToStream (img, out);
            std::cout << "wrote " << f.getFullPathName() << std::endl;
        }

        // v1.6: the welcome screen (demo set / empty set)
        {
            ed->showWelcome (false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            save (*editor, outDir.getChildFile ("livelooper-welcome" + sfx + ".png"));
            if (auto* w = ed->getWelcome())
                w->setVisible (false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        }
        // v1.6: the SETUP window (System Settings style), two pages
        {
            for (const auto& [page, name] : { std::pair<const char*, const char*> { "Looper", "setup" }, std::pair<const char*, const char*> { "App", "setup-app" },
                                                     std::pair<const char*, const char*> { "Audio", "setup-audio" } /* v1.7: latency */,
                                                     std::pair<const char*, const char*> { "Sets", "setup-sets" } /* v1.7: auto backup */ })
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr (page));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile (juce::String ("livelooper-") + name + sfx + ".png"));
                }
            }
        }

        // v1.6 rhythm: the BEATS page with the Boom Bap kit and a user pattern
        {
            set (gRhythmKit, 6.0f);
            set (gRhythmPattern, (float) (RhythmGenerator::getNumPatterns() - 14));    // Boom Bap
            ed->showBeatsPage (true);
            if (auto* page = ed->getBeatsPage())
            {
                page->loadPreset (RhythmGenerator::getNumPatterns() - 14);
                proc.setUserPatternName (page->getEditSlot(), "Boom Bap Flip");
                page->setResolution (16, 2);
                auto& bank = proc.getUserPatterns();
                bank.setCell (page->getEditSlot(), RhythmGenerator::Snare, 7, 40);
                bank.setCell (page->getEditSlot(), RhythmGenerator::Rim, 14, 96);
                bank.setCell (page->getEditSlot(), RhythmGenerator::HatOpen, 15, 110);
            }
            set (beatRowId (kBeatMute, 6), 1.0f);
            set (gDrumsBfx, 1.0f);
            {
                // an own sample on the snare row (shows its name under the voice)
                juce::AudioBuffer<float> hit (1, 8000);
                for (int i = 0; i < hit.getNumSamples(); ++i)
                    hit.setSample (0, i, 0.5f * std::sin ((float) i * 0.05f) * std::exp (-(float) i / 1500.0f));
                proc.setRowSampleAudio (1, hit, 44100.0, "Vinyl Snare 03", {});
            }
            for (int f = 0; f < 6; ++f)
            {
                runBlocks (proc, 3, 0.0f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
            save (*editor, outDir.getChildFile ("livelooper-beats" + sfx + ".png"));
            proc.clearRowSample (1);
            ed->showBeatsPage (false);
            set (beatRowId (kBeatMute, 6), 0.0f);
            set (gDrumsBfx, 0.0f);
            set (gRhythmKit, 0.0f);
            set (gRhythmPattern, 1.0f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        }

        // v1.5 live performance: beat FX, colour FX, beat repeat latch, metronome and Link on
        set (gBfxOn, 1.0f);
        ed->setFxTab (LiveLooperEditor::tabBeat);       // v1.6: the FX panel shows the beat FX
        set (gColorType, 1.0f);
        set (gRepeatLatch, 1.0f);
        set (gMetroOn, 1.0f);
        set (gLinkOn, 1.0f);
        set ("sync", 0.0f);    // Link drives the tempo like in the standalone app
        for (int f = 0; f < 4; ++f)
        {
            runBlocks (proc, 3, 0.6f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }
        save (*editor, outDir.getChildFile ("livelooper-performance" + sfx + ".png"));

        // v1.6 performance recorder: the round record button while it records (light on the beat)
        {
            const auto recDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("livelooper-screenshot-rec");
            PerformanceRecorder::setFolder (recDir);
            proc.startPerformanceRecording();
            for (int f = 0; f < 8; ++f)
            {
                runBlocks (proc, 40, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
            auto& rec = ed->getPerfRecButton();
            rec.setBeatPhase (0.1);          // light on for the picture
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            save (*editor, outDir.getChildFile ("livelooper-recording" + sfx + ".png"));
            proc.stopPerformanceRecording();
            PerformanceRecorder::setFolder ({});
            recDir.deleteRecursively();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }
        set (gBfxOn, 0.0f);
        ed->setFxTab (LiveLooperEditor::tabInput);
        set (gColorType, 0.0f);
        set (gRepeatLatch, 0.0f);
        set (gMetroOn, 0.0f);
        set (gLinkOn, 0.0f);
        set ("sync", 1.0f);
        runBlocks (proc, 2, 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        // v1.6 punch FX tab (Tape Stop held, send FX reverb)
        {
            ed->setFxTab (LiveLooperEditor::tabPunch);
            set (gSfxType, 2.0f);
            set (gSfxLevel, 0.35f);
            set (punchHoldId (4), 1.0f);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.4f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            }
            save (*editor, outDir.getChildFile ("livelooper-punch-fx" + sfx + ".png"));
            set (punchHoldId (4), 0.0f);
            set (gSfxType, 0.0f);
            ed->setFxTab (LiveLooperEditor::tabInput);
            runBlocks (proc, 4, 0.0f);
        }
        // v1.6 K.O. II (simulated): header chip + the one-time question, SETUP > Controllers, Controller GUI
        {
            proc.simulateControllers (true, true);
            ed->updateKo2Ui();
            ed->showKo2Toast (true);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            }
            save (*editor, outDir.getChildFile ("livelooper-ko2" + sfx + ".png"));
            if (auto* t = ed->getKo2Toast())
                t->setVisible (false);
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr ("Controllers"));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-controllers" + sfx + ".png"));
                    // the whole page (K.O. II audio output, hold to intensify, notes)
                    const auto size = sp->getBounds();
                    sp->setSize (size.getWidth(), 1900);   // 1.7: velocity curves, Push 1 display, MIDI feedback below K.O. II
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-controllers-full" + sfx + ".png"));
                    sp->setBounds (size);
                }
            }
            appsettings::setValue ("gui.layout", "controller");
            appsettings::setValue ("gui.controller", "ko2");
            ed->updateKo2Ui();
            if (auto* cv = ed->getControllerView())
                cv->setKo2Page (0);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            }
            save (*editor, outDir.getChildFile ("livelooper-controller-ko2" + sfx + ".png"));
            // hold to intensify: a punch pad (group C) held for about a beat - live amount on the display
            // and in the PUNCH FX title row of the standard surface
            if (auto* dev = proc.getKo2Device())
            {
                const int note = ko2::noteFor (2, ko2::padNumberOffset (2));
                dev->injectMessage (juce::MidiMessage::noteOn (1, note, (juce::uint8) 50));
                if (auto* cv = ed->getControllerView())
                    cv->setKo2Page (2);
                for (int f = 0; f < 10; ++f)
                {
                    runBlocks (proc, 2, 0.3f);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                }
                ed->updateKo2Ui();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                save (*editor, outDir.getChildFile ("livelooper-ko2-hold" + sfx + ".png"));
                appsettings::setValue ("gui.layout", "standard");
                ed->updateKo2Ui();
                for (int f = 0; f < 3; ++f)
                {
                    runBlocks (proc, 2, 0.3f);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                }
                ed->updateKo2Ui();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                save (*editor, outDir.getChildFile ("livelooper-punch-hold" + sfx + ".png"));
                dev->injectMessage (juce::MidiMessage::noteOff (1, note));
                appsettings::setValue ("gui.layout", "controller");
                if (auto* cv = ed->getControllerView())
                    cv->setKo2Page (0);
                ed->updateKo2Ui();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            }
            appsettings::setValue ("gui.controller", "push");
            ed->updateKo2Ui();
            // v1.7: two loops playing (progress bars on display line 3), a held punch FX (amount bar on line 4)
            const bool p1 = proc.getEngine().getTrack (0).isRunning(), p2 = proc.getEngine().getTrack (1).isRunning();
            if (! p1) press (proc, "t1_recplay");
            if (! p2) press (proc, "t2_recplay");
            runBlocks (proc, 40, 0.0f);
            set ("pfx_amount", 0.6f);
            proc.getState().getParameter (punchHoldId (4))->setValueNotifyingHost (1.0f);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            }
            save (*editor, outDir.getChildFile ("livelooper-controller-push" + sfx + ".png"));
            proc.getState().getParameter (punchHoldId (4))->setValueNotifyingHost (0.0f);
            if (auto* pd = proc.getPushDevice())
            {
                // v1.7 loop length: FIXED LENGTH held (pads / display show the eight lengths)
                auto& surf = pd->getSurface();
                surf.handleMidi (juce::MidiMessage::controllerEvent (1, push::cc::fixedLength, 127));
                std::vector<juce::MidiMessage> discard;
                surf.render (discard, false);
                editor->repaint();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                save (*editor, outDir.getChildFile ("livelooper-controller-push-length" + sfx + ".png"));
                surf.handleMidi (juce::MidiMessage::controllerEvent (1, push::cc::fixedLength, 0));
                surf.render (discard, false);
            }
            if (! p1) press (proc, "t1_stop");
            if (! p2) press (proc, "t2_stop");
            runBlocks (proc, 2, 0.0f);
            appsettings::setValue ("gui.layout", "standard");
            appsettings::setValue ("gui.controller", "ko2");
            if (auto* cv = ed->getControllerView())
                cv->setKo2Page (-1);
            proc.simulateControllers (false, false);
            ed->updateKo2Ui();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }

        // in-app help (searchable user guide) and the about window
        {
            HelpComponent help;
            help.setLookAndFeel (&editor->getLookAndFeel());
            // v1.6: the illustrated guide - Push 1 layout and the Loops page
            const auto& secs = help.getSections();
            auto find = [&secs] (const juce::String& title) { for (int i = 0; i < secs.size(); ++i) if (secs[i].title == title) return i; return 0; };
            help.showSectionForTests (find (i18n::tr ("Push 1")));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help" + sfx + ".png"));
            help.showSectionForTests (find (i18n::tr ("Loops")));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help-loops" + sfx + ".png"));
            help.showSectionForTests (find (i18n::tr ("K.O. II")));          // v1.6
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help-ko2" + sfx + ".png"));
            help.setQuery ("Link");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help-search" + sfx + ".png"));
            help.setLookAndFeel (nullptr);
        }
        {
            AboutComponent about (false, false);
            about.setLookAndFeel (&editor->getLookAndFeel());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (about, outDir.getChildFile ("livelooper-about" + sfx + ".png"));
            about.setLookAndFeel (nullptr);
        }

        // v1.7 auto backup: the notice after a crash (standalone: the newest backup was restored)
        {
            LiveLooperProcessor::BackupNotice n;
            n.kind = LiveLooperProcessor::BackupNotice::Kind::Restored;
            n.time = juce::Time::getCurrentTime() - juce::RelativeTime::minutes (3);
            proc.postBackupNoticeForTests (n);
            ed->updateBackupNotice();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            if (auto* t = ed->getBackupToast())
            {
                const auto area = t->getBounds().expanded (24);
                auto full = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                full.getClippedImage (area).createCopy();
                juce::PNGImageFormat png;
                const auto f = outDir.getChildFile ("livelooper-backup-notice" + sfx + ".png");
                f.deleteFile();
                juce::FileOutputStream out (f);
                png.writeImageToStream (full.getClippedImage (area), out);
            }
            ed->pressBackupToastButton (-1);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        }

        ed->setLearnMode (true);
        proc.startMidiLearn ("t5_recplay");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        save (*editor, outDir.getChildFile ("livelooper-midi-learn" + sfx + ".png"));
        proc.cancelMidiLearn();
        ed->setLearnMode (false);

        // typed-in tempo on the display
        ed->showTempoEntry();
        ed->getTempoEntry().setText ("128");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        save (*editor, outDir.getChildFile ("livelooper-tempo-entry" + sfx + ".png"));
        ed->getTempoEntry().onEscapeKey();

        // keyboard mapping window (standalone app)
        {
            KeyMapEditor keys (proc);
            keys.startCapture (keys.getActions().indexOf ("t2_reverse"));
            save (keys, outDir.getChildFile ("livelooper-keyboard" + sfx + ".png"), 2.0f);
        }

        // track setup panel on its own
        TrackSettingsPanel panel (proc, *ed, 0);
        panel.setLookAndFeel (&editor->getLookAndFeel());
        save (panel, outDir.getChildFile ("livelooper-track-setup" + sfx + ".png"), 2.0f);
        panel.setLookAndFeel (nullptr);
    }

    // v1.7 display: light theme, stage mode, waveforms, Live view, SETUP > Display (EN + DE)
    {
        auto setParam = [&proc] (const juce::String& id, float normalised) {
            proc.getState().getParameter (id)->setValueNotifyingHost (normalised);
        };
        auto frames = [&proc] (int n) {
            for (int f = 0; f < n; ++f)
            {
                runBlocks (proc, 1, 0.4f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
        };
        // the same set as the main picture (earlier pictures changed the track states):
        // track 1 plays, track 2 overdubs (its overdub layer shows), track 3 stopped, track 4 records, track 5 empty
        press (proc, gClearAll); runBlocks (proc, 40, 0.0f);
        press (proc, "t1_recplay"); runBlocks (proc, 300, 0.4f);
        press (proc, "t1_recplay"); runBlocks (proc, 60, 0.0f);
        press (proc, "t2_recplay"); runBlocks (proc, 200, 0.3f);
        press (proc, "t2_recplay"); runBlocks (proc, 20, 0.0f);
        press (proc, "t2_recplay"); runBlocks (proc, 70, 0.2f);
        press (proc, "t3_recplay"); runBlocks (proc, 150, 0.3f);
        press (proc, "t3_recplay"); runBlocks (proc, 2, 0.0f);
        press (proc, "t3_stop");    runBlocks (proc, 2, 0.0f);
        press (proc, "t4_recplay"); runBlocks (proc, 40, 0.5f);
        for (const auto& [language, suffix] : { std::pair<i18n::Language, const char*> { i18n::Language::English, "" },
                                                std::pair<i18n::Language, const char*> { i18n::Language::German, "-de" } })
        {
            i18n::setSetting (language, false);
            const juce::String sfx (suffix);
            ed->showStatus ({});
            setParam (gUiTheme, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-light" + sfx + ".png"));
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr ("Display"));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-display-light" + sfx + ".png"));
                }
            }
            setParam (gUiTheme, 0.0f);
            setParam (gStageMode, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-stage" + sfx + ".png"));
            setParam (gStageMode, 0.0f);
            setParam (gWaveforms, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-waveforms" + sfx + ".png"));
            save (*editor, outDir.getChildFile ("livelooper-waveforms-2x" + sfx + ".png"), 2.0f);
            setParam (gWaveforms, 0.0f);
            setParam (gPfxAmount, 0.62f);
            setParam (gLiveView, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-live" + sfx + ".png"));
            {
                // v1.7.1: the cards of the fullscreen window (smaller display, REVERSE / ONE SHOT / PITCH), and LIVE without the FX bar
                ed->getLiveView()->setFullscreenMode (true);
                frames (4);
                save (*editor, outDir.getChildFile ("livelooper-live-fullscreen" + sfx + ".png"));
                ed->getLiveView()->setFullscreenMode (false);
                setParam (gLiveFx, 0.0f);
                frames (4);
                save (*editor, outDir.getChildFile ("livelooper-live-nofx" + sfx + ".png"));
                setParam (gLiveFx, 1.0f);
                frames (4);
            }
            if (juce::SystemStats::getEnvironmentVariable ("LL_SHOT_CHECKS", {}).isNotEmpty())
            {
                // review only (not a docs image): MIDI learn frames over the Live view
                ed->setLearnMode (true);
                frames (4);
                save (*editor, outDir.getChildFile ("check-live-learn" + sfx + ".png"));
                ed->setLearnMode (false);
            }
            setParam (gStageMode, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-live-stage" + sfx + ".png"));
            setParam (gStageMode, 0.0f);
            setParam (gUiTheme, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-live-light" + sfx + ".png"));
            setParam (gUiTheme, 0.0f);
            {
                // v1.7 Performance view (4th view): beat FX on, a punch FX held in the XY field
                ed->setPerformView (true);
                setParam (gBfxOn, 1.0f);
                setParam (gPfxX, 0.72f);
                setParam (gPfxY, 0.66f);
                setParam (gPerfFilter, 0.2f);
                setParam (punchHoldId (4), 1.0f);
                frames (6);
                save (*editor, outDir.getChildFile ("livelooper-perform" + sfx + ".png"));
                ed->getPerformView()->setFullscreenMode (true);
                frames (4);
                save (*editor, outDir.getChildFile ("livelooper-perform-fullscreen" + sfx + ".png"));
                ed->getPerformView()->setFullscreenMode (false);
                frames (2);
                if (juce::SystemStats::getEnvironmentVariable ("LL_SHOT_CHECKS", {}).isNotEmpty())
                {
                    // review only (not docs images): learn frames, light theme, stage mode, waveforms, 75 % / 200 %
                    ed->setLearnMode (true);
                    frames (4);
                    save (*editor, outDir.getChildFile ("check-perform-learn" + sfx + ".png"));
                    ed->setLearnMode (false);
                    setParam (gUiTheme, 1.0f);
                    frames (6);
                    save (*editor, outDir.getChildFile ("check-perform-light" + sfx + ".png"));
                    setParam (gUiTheme, 0.0f);
                    setParam (gStageMode, 1.0f);
                    frames (6);
                    save (*editor, outDir.getChildFile ("check-perform-stage" + sfx + ".png"));
                    setParam (gStageMode, 0.0f);
                    setParam (gWaveforms, 1.0f);
                    frames (6);
                    save (*editor, outDir.getChildFile ("check-perform-waves" + sfx + ".png"));
                    setParam (gWaveforms, 0.0f);
                    ed->setGuiScale (75);
                    frames (4);
                    save (*editor, outDir.getChildFile ("check-perform-75" + sfx + ".png"));
                    ed->setGuiScale (200);
                    frames (4);
                    save (*editor, outDir.getChildFile ("check-perform-200" + sfx + ".png"));
                    ed->setGuiScale (100);
                    frames (3);
                }
                setParam (punchHoldId (4), 0.0f);
                setParam (gBfxOn, 0.0f);
                setParam (gPerfFilter, 0.5f);
                setParam (gView, 0.0f);
                ed->setLiveView (false);
                frames (3);
            }
            setParam (gLiveView, 0.0f);
            frames (4);
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr ("Display"));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-display" + sfx + ".png"));
                }
            }
        }
        // interface size 75 % (rendered at its real size)
        ed->setGuiScale (75);
        frames (3);
        ed->showStatus ({});
        frames (2);
        save (*editor, outDir.getChildFile ("livelooper-size-75.png"));
        ed->setGuiScale (100);
        ed->showStatus ({});
        frames (2);
        i18n::setSetting (i18n::Language::English, false);
    }

    // optional 8 tracks
    editor.reset();
    proc.setVisibleTracks (8);
    press (proc, "t6_recplay"); runBlocks (proc, 120, 0.35f);
    press (proc, "t6_recplay"); runBlocks (proc, 2, 0.0f);
    press (proc, "t8_recplay"); runBlocks (proc, 60, 0.45f);
    editor.reset (proc.createEditor());
    for (int f = 0; f < 4; ++f)
    {
        runBlocks (proc, 3, 0.5f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }
    for (const auto& [language, suffix] : { std::pair<i18n::Language, const char*> { i18n::Language::English, "" },
                                            std::pair<i18n::Language, const char*> { i18n::Language::German, "-de" } })
    {
        i18n::setSetting (language, false);
        dynamic_cast<LiveLooperEditor*> (editor.get())->showStatus ({});
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        save (*editor, outDir.getChildFile ("livelooper-8-tracks" + juce::String (suffix) + ".png"));
    }
    i18n::setSetting (i18n::Language::English, false);
    return 0;
}
