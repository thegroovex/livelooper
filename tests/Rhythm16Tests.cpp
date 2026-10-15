// v1.6 rhythm (branch v1.6-rhythm): style kits, sample variation, BEATS step sequencer timing,
// quantised pattern switching, fill on demand, drums through the performance FX, sound packs.

#include "engine/LooperEngine.h"
#include "engine/RhythmGenerator.h"
#include "plugin/AppSettings.h"
#include "plugin/BeatsPage.h"
#include "plugin/PluginProcessor.h"
#include "plugin/SoundPacks.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <set>

using namespace livelooper;

namespace
{
    constexpr double kSr16 = 48000.0;
    constexpr double kPpq16 = 2.0 / kSr16;      // 120 BPM
    constexpr int kStep16 = 6000;               // samples per 16th

    /** Renders [from, to) samples of the timeline (block-wise) into out (which grows). */
    void renderRange (RhythmGenerator& r, std::vector<float>& out, int to)
    {
        std::vector<float> tmp (256), tmpR (256);
        int pos = (int) out.size();
        while (pos < to)
        {
            const int n = std::min (256, to - pos);
            std::fill (tmp.begin(), tmp.end(), 0.0f);
            r.process (tmp.data(), tmpR.data(), n, pos * kPpq16, kPpq16, 4, true);
            out.insert (out.end(), tmp.begin(), tmp.begin() + n);
            pos += n;
        }
    }

    int firstAbove (const std::vector<float>& v, int from, int to, float threshold = 1.0e-4f)
    {
        for (int i = std::max (0, from); i < std::min ((int) v.size(), to); ++i)
            if (std::abs (v[(size_t) i]) > threshold)
                return i;
        return -1;
    }

    void makeGen (RhythmGenerator& r, BeatPatternBank& bank)
    {
        r.prepare (kSr16);
        r.setLevel (1.0f);
        r.setHumanize (0.0f);
        r.setGroove (0);
        r.setSwing (0.5f);
        r.setVariation (0.0f);
        r.setUserPatterns (&bank);
    }

    void writeWav (const juce::File& f, float freq, int len)
    {
        juce::AudioBuffer<float> b (1, len);
        for (int i = 0; i < len; ++i)
            b.setSample (0, i, 0.5f * std::sin (6.2831853f * freq * (float) i / 44100.0f) * std::exp (-(float) i / 4000.0f));
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (16));
        if (w != nullptr)
            w->writeFromAudioSampleBuffer (b, 0, len);
    }

    const char* kPackJson = R"({
  "format": "livelooper-pack", "version": 1,
  "name": "Test Pack", "author": "livelooper tests", "license": "CC0 1.0",
  "kits": [ {
      "name": "Dusty", "room": 0.12, "fill": "snare roll",
      "voices": {
        "kick":  { "name": "Thump", "level": 0.9, "samples": [ { "file": "kick_soft.wav", "velocity": 0.4 }, { "file": "kick_hard.wav", "velocity": 1.0 } ] },
        "snare": { "samples": [ "snare.wav" ] },
        "hat_closed": { "samples": [ { "file": "hat.wav", "velocity": 0.8 } ] }
      } } ],
  "patterns": [
    { "name": "Dusty Groove", "steps_per_bar": 16, "bars": 1,
      "rows": { "kick": "X..x....X.x.....", "snare": "....X.......X..g", "hat_closed": [96, 0, 96, 0, 96, 0, 96, 0, 96, 0, 96, 0, 96, 0, 96, 50] } }
  ] })";

    juce::File makePackFolder (const juce::File& parent)
    {
        auto dir = parent.getChildFile ("Test Pack");
        dir.createDirectory();
        writeWav (dir.getChildFile ("kick_soft.wav"), 60.0f, 8000);
        writeWav (dir.getChildFile ("kick_hard.wav"), 70.0f, 9000);
        writeWav (dir.getChildFile ("snare.wav"), 200.0f, 6000);
        writeWav (dir.getChildFile ("hat.wav"), 7000.0f, 2000);
        dir.getChildFile (SoundPacks::kPackFileName).replaceWithText (kPackJson);
        return dir;
    }
}

class Rhythm16Tests : public juce::UnitTest
{
public:
    Rhythm16Tests() : juce::UnitTest ("Rhythm 1.6", "livelooper") {}

    void runTest() override
    {
        beginTest ("Style kits are appended, every new groove has a matching fill");
        {
            expectEquals (RhythmGenerator::getNumKits(), 12);
            expectEquals (juce::String (RhythmGenerator::getKitName (0)), juce::String ("Studio"));
            expectEquals (juce::String (RhythmGenerator::getKitName (2)), juce::String ("909 Style"));
            const juce::StringArray styles { "House", "Techno", "Boom Bap", "Trap", "Breaks", "Rock", "Disco", "Afro Latin" };
            for (int k = 4; k < 12; ++k)
                expectEquals (juce::String (RhythmGenerator::getKitName (k)), styles[k - 4]);
            expectEquals (RhythmGenerator::getNumKitChoices(), 20, "8 sound-pack slots");
            expectEquals (juce::String (RhythmGenerator::getKitVoiceName (11, RhythmGenerator::Tom)), juce::String ("Conga"));
            expectEquals (juce::String (RhythmGenerator::getPatternName (20)), juce::String ("Afro House"), "old indices kept");
            expectGreaterOrEqual (RhythmGenerator::getNumPatterns(), 39);
            for (int p = 21; p < RhythmGenerator::getNumPatterns(); ++p)
            {
                const int f = RhythmGenerator::patternFillStyle (p);
                expect (f >= 0 && f < RhythmGenerator::kNumFillStyles, RhythmGenerator::getPatternName (p));
            }
            expectEquals (RhythmGenerator::patternFillStyle (11), -1, "old grooves: the kit decides");
            expectEquals (RhythmGenerator::kitFillStyle (0), (int) RhythmGenerator::FillToms);
            expectEquals (RhythmGenerator::getNumPatternChoices(), RhythmGenerator::getNumPatterns() + 16);
            const auto rock = RhythmGenerator::presetToPattern (1);
            expectEquals ((int) rock.get (RhythmGenerator::Kick, 0), 127);
            expectEquals ((int) rock.get (RhythmGenerator::Snare, 4), 127);
            expectEquals ((int) rock.get (RhythmGenerator::Snare, 11), 40, "ghost note");
        }

        beginTest ("Every style kit plays every new groove within range (synthesised fallback)");
        {
            for (int k = 4; k < 12; ++k)
                for (int p = 21; p < RhythmGenerator::getNumPatterns(); p += 3)
                {
                    RhythmGenerator r;
                    r.prepare (kSr16);
                    r.setKit (k);
                    r.setPattern (p);
                    r.setLevel (1.0f);
                    std::vector<float> out;
                    renderRange (r, out, kStep16 * 16);
                    float pk = 0.0f;
                    for (auto x : out) pk = std::max (pk, std::abs (x));
                    const auto what = juce::String (RhythmGenerator::getKitName (k)) + " / " + RhythmGenerator::getPatternName (p);
                    expectGreaterThan (pk, 0.05f, what);
                    expectLessThan (pk, 1.0f, what);
                }
        }

        beginTest ("Kick accents: downbeat strongest, phrase start, no change at variation 0");
        {
            expectEquals (RhythmGenerator::kickAccent (4, 1, 0.0f), 1.0f);
            expectGreaterThan (RhythmGenerator::kickAccent (0, 0, 1.0f), RhythmGenerator::kickAccent (8, 0, 1.0f));
            expectGreaterThan (RhythmGenerator::kickAccent (8, 0, 1.0f), RhythmGenerator::kickAccent (4, 0, 1.0f));
            expectGreaterThan (RhythmGenerator::kickAccent (4, 0, 1.0f), RhythmGenerator::kickAccent (3, 0, 1.0f));
            expectGreaterThan (RhythmGenerator::kickAccent (0, 4, 1.0f), RhythmGenerator::kickAccent (0, 5, 1.0f), "first bar of 4");
        }

        beginTest ("Kick variation: round robin across samples and layers, never the same sample twice in a row");
        {
            auto bank = std::make_shared<DrumBank>();
            auto kit = std::make_shared<DrumBank::Kit>();
            for (int layer = 0; layer < 3; ++layer)
                for (int take = 0; take < 3; ++take)
                {
                    DrumBank::Sample s;
                    s.layer = 0.4f + 0.3f * (float) layer;
                    s.data.resize (2000);
                    for (int i = 0; i < 2000; ++i)
                        s.data[(size_t) i] = 0.5f * std::sin (0.01f * (float) (i * (layer * 3 + take + 1))) * std::exp (-(float) i / 500.0f);
                    kit->voices[0].push_back (s);
                }
            kit->voices[1] = kit->voices[0];
            kit->voices[3] = kit->voices[0];
            bank->kits.push_back (kit);
            for (float var : { 0.0f, 1.0f })
            {
                BeatPatternBank patterns;
                BeatPattern p;
                for (int s = 0; s < 16; ++s)
                    p.set (RhythmGenerator::Kick, s, 127);
                patterns.store (0, p);
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setDrumBank (bank);
                r.setVariation (var);
                r.setPattern (RhythmGenerator::getNumPatterns());
                std::vector<float> out;
                std::set<int> used;
                int repeats = 0, last = -1, otherLayer = 0;
                for (int step = 0; step < 48; ++step)
                {
                    renderRange (r, out, (step + 1) * kStep16 - 10);
                    const int idx = r.getLastSampleIndex (RhythmGenerator::Kick);
                    used.insert (idx);
                    repeats += idx == last ? 1 : 0;
                    otherLayer += idx < 6 ? 1 : 0;
                    last = idx;
                }
                expect (r.isUsingSamples());
                expectEquals (repeats, 0, "no sample twice in a row, variation " + juce::String (var));
                if (var == 0.0f)
                    expectEquals (otherLayer, 0, "variation 0: only the accent layer");
                else
                {
                    expectGreaterThan (otherLayer, 0, "variation: neighbouring layer sometimes");
                    expectGreaterOrEqual ((int) used.size(), 4, "many different takes");
                }
            }
        }

        beginTest ("BEATS sequencer timing: 16ths, 32nds, 2 bars, swing - sample accurate");
        {
            BeatPatternBank patterns;
            const int user = RhythmGenerator::getNumPatterns();
            {
                BeatPattern p;
                p.set (RhythmGenerator::Kick, 4, 127);
                patterns.store (0, p);
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setPattern (user);
                std::vector<float> out;
                renderRange (r, out, kStep16 * 8);
                const int on = firstAbove (out, 0, (int) out.size());
                expect (on >= 4 * kStep16 && on <= 4 * kStep16 + 2, "step 5 on the grid: " + juce::String (on));
            }
            {
                BeatPattern p;
                p.resize (32, 1);
                p.set (RhythmGenerator::Kick, 9, 127);      // 32nd 10 = 16th 4.5
                patterns.store (1, p);
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setPattern (user + 1);
                std::vector<float> out;
                renderRange (r, out, kStep16 * 8);
                const int on = firstAbove (out, 0, (int) out.size());
                expect (std::abs (on - (int) (4.5 * kStep16)) <= 2, "32nd note: " + juce::String (on));
            }
            {
                BeatPattern p;
                p.resize (16, 2);
                p.set (RhythmGenerator::Kick, 16, 127);     // first step of bar 2
                patterns.store (2, p);
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setPattern (user + 2);
                std::vector<float> out;
                renderRange (r, out, kStep16 * 50);
                expectEquals (firstAbove (out, 0, 16 * kStep16 - 10), -1, "bar 1 silent");
                expect (std::abs (firstAbove (out, 16 * kStep16 - 10, 20 * kStep16) - 16 * kStep16) <= 2, "bar 2");
                expect (std::abs (firstAbove (out, 47 * kStep16, 50 * kStep16) - 48 * kStep16) <= 2, "loops after 2 bars");
            }
            {
                BeatPattern p;
                p.set (RhythmGenerator::Kick, 1, 127);
                patterns.store (3, p);
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setSwing (0.75f);
                r.setPattern (user + 3);
                std::vector<float> out;
                renderRange (r, out, kStep16 * 4);
                const int on = firstAbove (out, 0, (int) out.size());
                expect (std::abs (on - (int) (1.5 * kStep16)) <= 2, "swing 75 %: half a 16th late: " + juce::String (on));
            }
            {
                // empty user slot: silence, no crash
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setPattern (user + 9);
                std::vector<float> out;
                renderRange (r, out, kStep16 * 16);
                expectEquals (firstAbove (out, 0, (int) out.size()), -1);
            }
        }

        beginTest ("Quantised pattern switching: immediately, next beat, next bar");
        {
            BeatPatternBank patterns;
            BeatPattern kicks, snares;
            for (int s = 0; s < 16; ++s)
            {
                kicks.set (RhythmGenerator::Kick, s, 100);
                snares.set (RhythmGenerator::Snare, s, 100);
            }
            patterns.store (0, kicks);
            patterns.store (1, snares);
            const int user = RhythmGenerator::getNumPatterns();
            for (int q = 0; q < 3; ++q)
            {
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setSwitchQuantize (q);
                r.setPattern (user);
                std::vector<float> out;
                renderRange (r, out, 5 * kStep16 + 100);           // inside step 6
                r.setPattern (user + 1);
                int firstSnareStep = -1;
                for (int step = 6; step < 20 && firstSnareStep < 0; ++step)
                {
                    renderRange (r, out, step * kStep16 + 100);
                    if (r.getHitCount (RhythmGenerator::Snare) > 0)
                        firstSnareStep = step;
                }
                const int expected = q == 0 ? 7 : q == 1 ? 8 : 16;   // step 6 was already scheduled ahead
                expectEquals (firstSnareStep, expected, "switch mode " + juce::String (q));
                expectEquals (r.getActivePattern(), user + 1);
            }
        }

        beginTest ("Fill on demand: plays while held, lands on the next downbeat");
        {
            BeatPatternBank patterns;
            BeatPattern p;
            for (int s = 0; s < 16; ++s)
                p.set (RhythmGenerator::HatClosed, s, 90);
            for (int s : { 0, 4, 8, 12 })
                p.set (RhythmGenerator::Kick, s, 110);
            patterns.store (0, p);
            RhythmGenerator r;
                makeGen (r, patterns);
            r.setPattern (RhythmGenerator::getNumPatterns());
            std::vector<float> out;
            renderRange (r, out, 5 * kStep16 + 100);
            expect (! r.isFillPlaying());
            r.setFillHeld (true);
            renderRange (r, out, 7 * kStep16 + 100);
            expect (r.isFillPlaying(), "fill started");
            const int hatsAtFill = r.getHitCount (RhythmGenerator::HatClosed);
            const int tomsAtFill = r.getHitCount (RhythmGenerator::Tom);
            renderRange (r, out, 9 * kStep16 + 100);
            r.setFillHeld (false);
            renderRange (r, out, 14 * kStep16 + 100);
            expect (r.isFillPlaying(), "still filling until the downbeat");
            expectGreaterThan (r.getHitCount (RhythmGenerator::Tom), tomsAtFill, "tom fill (Studio kit)");
            expectEquals (r.getHitCount (RhythmGenerator::HatClosed), hatsAtFill, "groove paused during the fill");
            const int openBefore = r.getHitCount (RhythmGenerator::HatOpen);
            renderRange (r, out, 16 * kStep16 + 100);
            expect (! r.isFillPlaying(), "landed on the downbeat");
            expectEquals (r.getHitCount (RhythmGenerator::HatOpen), openBefore + 1, "accented landing");
            expectWithinAbsoluteError (r.getLastVelocity (RhythmGenerator::Kick), 1.0f, 1.0e-4f);
            renderRange (r, out, 18 * kStep16 + 100);
            expectGreaterThan (r.getHitCount (RhythmGenerator::HatClosed), hatsAtFill, "groove back");

            // a short tap: the fill still runs to the end of the bar
            renderRange (r, out, 21 * kStep16 + 100);
            r.setFillHeld (true);
            r.setFillHeld (false);
            renderRange (r, out, 24 * kStep16 + 100);
            expect (r.isFillPlaying(), "tap: fill until the bar ends");
            renderRange (r, out, 32 * kStep16 + 100);
            expect (! r.isFillPlaying());
        }

        beginTest ("Row mixer: mute silences a voice in every pattern, level scales it");
        {
            BeatPatternBank patterns;
            RhythmGenerator r;
                makeGen (r, patterns);
            r.setPattern (1);         // Rock preset
            r.setVoiceMix (RhythmGenerator::Kick, 1.0f, 0.0f, true);
            std::vector<float> out;
            renderRange (r, out, kStep16 * 16);
            expectEquals (r.getHitCount (RhythmGenerator::Kick), 0, "kick muted");
            expectGreaterThan (r.getHitCount (RhythmGenerator::Snare), 0);
        }

        beginTest ("Drums through beat FX / repeat: off = dry (as before), on = processed");
        {
            auto runEngine = [] (bool bfxOn, bool routeBfx, bool repeatOn, bool routeRep) {
                LooperEngine e;
                e.prepare (kSr16, 512, 4.0);
                GlobalSettings g;
                g.syncToHost = false;
                g.monitorLevel = 0.0f;
                e.setGlobalSettings (g);
                e.setRhythm (true, 1, 1.0f);
                BeatFxSettings b;
                b.on = bfxOn;
                b.type = BeatFxType::Echo;
                b.depth = 1.0f;
                e.setPerformanceFx (b, (int) FxTarget::Input, repeatOn, 3, false);
                e.setDrumFxRouting (routeBfx, routeRep);
                std::vector<float> in (512, 0.0f), l (512), r (512), all;
                TimeInfo t;
                for (int blk = 0; blk < 400; ++blk)
                {
                    e.process (in.data(), in.data(), l.data(), r.data(), 512, t);
                    all.insert (all.end(), l.begin(), l.end());
                }
                return all;
            };
            auto diff = [] (const std::vector<float>& a, const std::vector<float>& b) {
                float d = 0.0f;
                for (size_t i = 0; i < a.size(); ++i) d = std::max (d, std::abs (a[i] - b[i]));
                return d;
            };
            const auto dry = runEngine (false, false, false, false);
            expectLessThan (diff (dry, runEngine (true, false, false, false)), 1.0e-6f, "beat FX on the input: drums stay dry");
            expectLessThan (diff (dry, runEngine (false, false, true, false)), 1.0e-6f, "repeat on the input: drums stay dry");
            expectGreaterThan (diff (dry, runEngine (true, true, false, false)), 0.01f, "drums through the beat FX");
            expectGreaterThan (diff (dry, runEngine (false, false, true, true)), 0.01f, "drums through beat repeat");
            expectLessThan (diff (dry, runEngine (false, true, false, true)), 1.0e-3f, "routing on but effects off: (almost) dry");
        }

        beginTest ("BeatPattern resize keeps the steps");
        {
            BeatPattern p;
            p.set (0, 0, 127);
            p.set (0, 5, 96);
            p.resize (32, 1);
            expectEquals ((int) p.get (0, 0), 127);
            expectEquals ((int) p.get (0, 10), 96);
            p.resize (32, 2);
            expectEquals ((int) p.get (0, 42), 96, "new bar repeats the first");
            p.resize (16, 1);
            expectEquals ((int) p.get (0, 5), 96);
            expectEquals ((int) p.get (0, 21), 0, "cut off");
        }

        beginTest ("Sound packs: pack format, patterns (text and numbers), zip import, kit slots");
        {
            auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("livelooper-packtest-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            tmp.createDirectory();
            const auto src = makePackFolder (tmp.getChildFile ("src"));
            SoundPacks::LoadedPack pack;
            juce::String error;
            expect (SoundPacks::loadPack (src, pack, error), error);
            expectEquals (pack.info.name, juce::String ("Test Pack"));
            expectEquals ((int) pack.kits.size(), 1);
            expectEquals ((int) pack.kits[0]->voices[0].size(), 2);
            expectWithinAbsoluteError (pack.kits[0]->voices[0][0].layer, 0.4f, 1.0e-4f);
            expectEquals (juce::String (pack.kits[0]->voiceNames[0]), juce::String ("Thump"));
            expectEquals (pack.kits[0]->fillStyle, (int) RhythmGenerator::FillSnareRoll);
            expectEquals ((int) pack.kits[0]->voices[3].size(), 1, "hat_closed");
            expectEquals ((int) pack.patterns.size(), 1);
            const auto& pt = pack.patterns[0].pattern;
            expectEquals ((int) pt.get (0, 0), 127);
            expectEquals ((int) pt.get (0, 3), 96);
            expectEquals ((int) pt.get (1, 15), 40);
            expectEquals ((int) pt.get (3, 15), 50, "number arrays");

            // round trip of the pattern format
            BeatPattern back;
            juce::String name;
            expect (SoundPacks::patternFromVar (juce::JSON::parse (juce::JSON::toString (SoundPacks::patternToVar (pt, "X"))), back, name));
            expect (back == pt, "pattern json round trip");

            // broken pack
            auto bad = tmp.getChildFile ("bad");
            bad.createDirectory();
            bad.getChildFile (SoundPacks::kPackFileName).replaceWithText ("{\"format\": \"something else\"}");
            SoundPacks::LoadedPack dummy;
            expect (! SoundPacks::loadPack (bad, dummy, error), "wrong format refused");

            // zip import into the packs folder, slots
            const auto zipFile = tmp.getChildFile ("pack.zip");
            {
                juce::ZipFile::Builder zb;
                for (const auto& f : src.findChildFiles (juce::File::findFiles, false))
                    zb.addFile (f, 6, "Test Pack/" + f.getFileName());
                juce::FileOutputStream os (zipFile);
                expect (zb.writeToStream (os, nullptr));
            }
            appsettings::setStorageFolderForTests (tmp.getChildFile ("settings"));
            auto& packs = SoundPacks::get();
            packs.setFoldersForTests (tmp.getChildFile ("packs"), tmp.getChildFile ("patterns"));
            packs.rescan();
            expectEquals ((int) packs.getPacks().size(), 0);
            const int v0 = packs.getVersion();
            expect (packs.importPack (zipFile, error), error);
            expectGreaterThan (packs.getVersion(), v0);
            expect (tmp.getChildFile ("packs/Test Pack/" + juce::String (SoundPacks::kPackFileName)).existsAsFile(), "extracted");
            expectEquals ((int) packs.getPacks().size(), 1);
            expectEquals (packs.getSlotName (0), juce::String ("Test Pack / Dusty"));
            const auto bank = packs.getBank();
            expect (bank->getKit (RhythmGenerator::getNumKits()) != nullptr, "pack kit in slot 1");
            expect (bank->getKit (RhythmGenerator::getNumKits() + 1) == nullptr, "slot 2 free");
            expect (bank->getKit (0) != nullptr || bank->numSamples() >= 0);
            expectEquals ((int) packs.getPackPatterns().size(), 1);

            // user pattern library
            const auto f = packs.saveUserPattern (pt, "My Groove");
            expect (f.existsAsFile());
            const auto lib = packs.getUserPatterns();
            expectEquals ((int) lib.size(), 1);
            expectEquals (lib[0].name, juce::String ("My Groove"));
            expect (lib[0].pattern == pt);

            // a processor finds the pack kit
            {
                LiveLooperProcessor proc (false, false);
                expect (proc.getKitDisplayName (RhythmGenerator::getNumKits()).contains ("Dusty"));
                expect (proc.getKitDisplayName (RhythmGenerator::getNumKits() + 3).contains ("4"));
            }

            packs.setFoldersForTests ({}, {});
            packs.rescan();
            appsettings::setStorageFolderForTests ({});
            tmp.deleteRecursively();
        }

        beginTest ("Processor: BEATS patterns are saved with the set, editing a groove copies it first");
        {
            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (48000.0, 512);
            auto* pattern = proc.getState().getParameter (params::gRhythmPattern);
            pattern->setValueNotifyingHost (pattern->convertTo0to1 (3.0f));     // Funk
            const int slot = proc.prepareEditSlot();
            expectEquals (slot, 0);
            expectEquals (proc.getCurrentPattern(), RhythmGenerator::getNumPatterns(), "the copy plays");
            expect (proc.getUserPatterns().load (0) == RhythmGenerator::presetToPattern (3), "copy of Funk");
            expect (proc.getPatternDisplayName (RhythmGenerator::getNumPatterns()).contains ("Funk"));
            BeatPattern p;
            p.resize (32, 3);
            p.set (2, 77, 55);
            proc.storeUserPattern (5, p, "Broken Beat");
            juce::MemoryBlock state;
            proc.getStateInformation (state);

            LiveLooperProcessor other (false, false);
            other.setStateInformation (state.getData(), (int) state.getSize());
            expect (other.getUserPatterns().load (5) == p, "pattern restored");
            expectEquals (other.getUserPatternName (5), juce::String ("Broken Beat"));
            expect (other.getUserPatterns().load (0) == RhythmGenerator::presetToPattern (3));
            expect (other.getUserPatterns().isEmpty (6));
            for (const char* id : { "rhythm_variation", "rhythm_fill", "beat_switch", "drums_bfx", "drums_repeat",
                                    "beat_mute1", "beat_solo8", "beat_vol4", "beat_pitch8" })
                expect (proc.getState().getParameter (id) != nullptr, id);
            expect (params::isMomentary (params::gRhythmFill), "FILL is momentary (note on / off via MIDI)");
        }

        beginTest ("Beat library: SAVE / LOAD keep name, steps, velocities, length, swing and row mixer");
        {
            const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ll_beatlib_test");
            tmp.deleteRecursively();
            appsettings::setStorageFolderForTests (tmp.getChildFile ("settings"));
            auto& packs = SoundPacks::get();
            packs.setFoldersForTests (tmp.getChildFile ("packs"), tmp.getChildFile ("patterns"));
            packs.rescan();

            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (48000.0, 512);
            auto& st = proc.getState();
            auto setPlain = [&st] (const juce::String& id, float v) { auto* p = st.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            auto plain = [&st] (const juce::String& id) { auto* p = st.getParameter (id); return p->convertFrom0to1 (p->getValue()); };

            BeatPattern p;
            p.resize (32, 2);
            p.set (0, 0, 127);
            p.set (1, 9, 40);
            p.set (3, 63, 96);
            proc.storeUserPattern (2, p, "Night Drive");
            proc.setBeatsEditSlot (2);
            setPlain (params::gRhythmSwing, 61.0f);
            setPlain (params::beatRowId (params::kBeatVol, 1), 0.6f);
            setPlain (params::beatRowId (params::kBeatPitch, 3), -5.0f);
            setPlain (params::beatRowId (params::kBeatMute, 6), 1.0f);
            setPlain (params::beatRowId (params::kBeatSolo, 0), 1.0f);

            const auto f = proc.saveBeat (2, "Night Drive");
            expect (f.existsAsFile(), "saved into the library");
            expectEquals (f.getParentDirectory().getFileName(), juce::String ("patterns"));
            expectEquals (f.getFileExtension(), juce::String (SoundPacks::kPatternExtension));
            const auto json = juce::JSON::parse (f.loadFileAsString());
            expectEquals ((int) json.getProperty ("format_version", 0), 2);
            const auto anywhere = tmp.getChildFile ("elsewhere/my beat");
            const auto f2 = proc.saveBeat (2, "Night Drive", anywhere);
            expect (f2.existsAsFile() && f2.hasFileExtension ("llpattern"), "Save as: any folder, extension added");

            // change everything, then load it back
            setPlain (params::gRhythmSwing, 50.0f);
            for (int r = 0; r < BeatPattern::kRows; ++r)
            {
                setPlain (params::beatRowId (params::kBeatVol, r), 1.0f);
                setPlain (params::beatRowId (params::kBeatPitch, r), 0.0f);
                setPlain (params::beatRowId (params::kBeatMute, r), 0.0f);
                setPlain (params::beatRowId (params::kBeatSolo, r), 0.0f);
            }
            const auto lib = packs.getUserPatterns();
            expectEquals ((int) lib.size(), 1);
            expectEquals (lib[0].name, juce::String ("Night Drive"));
            expect (lib[0].pattern == p, "steps, velocities, resolution, length");
            expect (lib[0].settings.hasSwing && lib[0].settings.hasMix);
            expect (proc.slotForLoading (false) == 0, "edit slot 2 is used: the first free slot");
            proc.loadBeat (lib[0], proc.slotForLoading (false));
            expectEquals (proc.getBeatsEditSlot(), 0);
            expect (proc.getUserPatterns().load (0) == p);
            expectEquals (proc.getUserPatternName (0), juce::String ("Night Drive"));
            expectWithinAbsoluteError (plain (params::gRhythmSwing), 61.0f, 0.06f);
            expectWithinAbsoluteError (plain (params::beatRowId (params::kBeatVol, 1)), 0.6f, 0.01f);
            expectWithinAbsoluteError (plain (params::beatRowId (params::kBeatPitch, 3)), -5.0f, 0.01f);
            expect (plain (params::beatRowId (params::kBeatMute, 6)) >= 0.5f, "row mute");
            expect (plain (params::beatRowId (params::kBeatSolo, 0)) >= 0.5f, "row solo");
            expectEquals (proc.getCurrentPattern(), RhythmGenerator::getNumPatterns() + 0, "the loaded beat plays");
            expect (proc.slotForLoading (true) == 0, "replace: the edit slot");

            // Open file... (a file anywhere) into the edit slot when it is empty
            proc.setBeatsEditSlot (9);
            juce::String error;
            expect (proc.loadBeatFile (f2, proc.slotForLoading (false), error), error);
            expect (proc.getUserPatterns().load (9) == p);
            expect (! proc.loadBeatFile (tmp.getChildFile ("nothing.llpattern"), 9, error), "missing file refused");
            expect (error.isNotEmpty());

            // old library files without swing / mixer still load; the settings stay as they are
            packs.saveUserPattern (RhythmGenerator::presetToPattern (2), "Old Beat");
            const auto lib2 = packs.getUserPatterns();
            expectEquals ((int) lib2.size(), 2);
            expectEquals (lib2[1].name, juce::String ("Old Beat"));
            expect (! lib2[1].settings.hasSwing && ! lib2[1].settings.hasMix);
            setPlain (params::gRhythmSwing, 55.0f);
            proc.loadBeat (lib2[1], 9);
            expectWithinAbsoluteError (plain (params::gRhythmSwing), 55.0f, 0.06f);

            // next / previous library beat (host parameters): one slot, wraps around
            for (int s = 0; s < BeatPatternBank::kNumSlots; ++s)
                proc.storeUserPattern (s, {}, {});
            proc.setBeatsEditSlot (4);
            expectEquals (proc.browseLibraryBeat (1), 0, "first: Night Drive");
            expectEquals (proc.getBeatsEditSlot(), 4);
            expectEquals (proc.getUserPatternName (4), juce::String ("Night Drive"));
            expectEquals (proc.browseLibraryBeat (1), 1, "next: Old Beat");
            expectEquals (proc.getBeatsEditSlot(), 4, "browsing reuses its slot");
            expectEquals (proc.browseLibraryBeat (1), 0, "wraps around");
            expectEquals (proc.browseLibraryBeat (-1), 1, "previous wraps too");
            proc.getUserPatterns().setCell (4, 0, 1, 90);          // edited: the next step keeps the edit
            proc.browseLibraryBeat (1);
            expect (proc.getBeatsEditSlot() != 4, "an edited slot is not overwritten");
            expectEquals (proc.getUserPatterns().getCell (4, 0, 1), 90);

            // the host parameters reach the message thread
            expect (params::isTrigger (params::gBeatLibNext) && params::isTrigger (params::gBeatLibPrev));
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            st.getParameter (params::gBeatLibNext)->setValueNotifyingHost (1.0f);
            buf.clear();
            proc.processBlock (buf, midi);
            st.getParameter (params::gBeatLibPrev)->setValueNotifyingHost (1.0f);
            buf.clear();
            proc.processBlock (buf, midi);
            st.getParameter (params::gBeatLibPrev)->setValueNotifyingHost (0.0f);
            st.getParameter (params::gBeatLibNext)->setValueNotifyingHost (0.0f);
            buf.clear();
            proc.processBlock (buf, midi);
            st.getParameter (params::gBeatLibPrev)->setValueNotifyingHost (1.0f);
            buf.clear();
            proc.processBlock (buf, midi);
            expectEquals (proc.takeBeatLibraryRequest(), 1 - 1 - 1, "next + 2 x previous");
            expectEquals (proc.takeBeatLibraryRequest(), 0);

            // the BEATS page: SAVE / LOAD buttons do the same
            {
                BeatsPage page (proc);
                page.setSize (1100, 700);
                page.setEditSlot (3);
                proc.storeUserPattern (3, p, "Page Beat");
                const auto pf = page.saveBeat ("Page Beat");
                expect (pf.existsAsFile(), "SAVE writes into the library");
                expectEquals ((int) packs.getUserPatterns().size(), 3);
                page.setReplaceOnLoad (true);
                expectEquals (page.getLoadTargetSlot(), 3, "replace: the edit slot");
                page.setReplaceOnLoad (false);
                const int target = page.getLoadTargetSlot();
                expect (target != 3 && proc.getUserPatterns().isEmpty (target), "otherwise a free slot");
                expectEquals (page.loadLibraryBeat (2), target);
                expectEquals (proc.getUserPatternName (target), juce::String ("Page Beat"));
                expectEquals (page.getEditSlot(), target, "the page shows the loaded beat");
                expect (page.openBeatFile (f2));
                expect (! page.openBeatFile (tmp.getChildFile ("nope.llpattern")));
            }

            packs.setFoldersForTests ({}, {});
            packs.rescan();
            appsettings::setStorageFolderForTests ({});
            tmp.deleteRecursively();
        }

        beginTest ("Own sample per BEATS row: plays instead of the kit voice, mute / level / pitch still apply");
        {
            auto bank = std::make_shared<DrumBank>();
            auto kit = std::make_shared<DrumBank::Kit>();
            DrumBank::Sample quiet;
            quiet.data.assign (2000, 0.001f);
            for (int v = 0; v < DrumBank::kNumVoices; ++v)
                kit->voices[(size_t) v].push_back (quiet);
            kit->room = 0.0f;
            bank->kits.push_back (kit);
            DrumBank::Sample loud;
            loud.data.assign (3000, 0.5f);
            bank->rowSamples[(size_t) RhythmGenerator::Snare].push_back (loud);
            expect (bank->samplesFor (*kit, RhythmGenerator::Snare).size() == 1 && bank->samplesFor (*kit, RhythmGenerator::Snare)[0].data[0] == 0.5f);
            expect (bank->samplesFor (*kit, RhythmGenerator::Kick)[0].data[0] == 0.001f, "other rows keep the kit");

            auto render = [&] (bool mute, float level, float pitch) {
                BeatPatternBank patterns;
                BeatPattern p;
                p.set (RhythmGenerator::Snare, 0, 127);
                patterns.store (0, p);
                RhythmGenerator r;
                makeGen (r, patterns);
                r.setDrumBank (bank);
                r.setPattern (RhythmGenerator::getNumPatterns());
                r.setVoiceMix (RhythmGenerator::Snare, level, pitch, mute);
                std::vector<float> out;
                renderRange (r, out, kStep16 * 2);
                float peak = 0.0f;
                int len = 0;
                for (size_t i = 0; i < out.size(); ++i)
                {
                    peak = std::max (peak, std::abs (out[i]));
                    len += std::abs (out[i]) > 0.01f ? 1 : 0;
                }
                return std::make_pair (peak, len);
            };
            const auto normal = render (false, 1.0f, 0.0f);
            expectGreaterThan (normal.first, 0.05f, "the row's own sample plays");
            expectLessThan (render (true, 1.0f, 0.0f).first, 0.01f, "mute");
            expectLessThan (render (false, 0.5f, 0.0f).first, normal.first * 0.7f, "level");
            expectLessThan (render (false, 1.0f, 12.0f).second, (int) (normal.second * 0.7f), "pitch up = shorter");
        }

        beginTest ("Own sample per BEATS row: load, state, set container, beat file, reset, BEATS page");
        {
            const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ll_rowsample_test");
            tmp.deleteRecursively();
            tmp.createDirectory();
            const auto wav = tmp.getChildFile ("My Snare.wav");
            writeWav (wav, 300.0f, 6000);
            tmp.getChildFile ("notes.txt").replaceWithText ("x");

            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (48000.0, 512);
            juce::String error;
            expect (! proc.loadRowSample (1, tmp.getChildFile ("notes.txt"), error));
            expect (error.isNotEmpty());
            expect (! proc.hasRowSample (1));
            expect (proc.loadRowSample (1, wav, error), error);
            expect (proc.hasRowSample (1) && ! proc.hasRowSample (0));
            expectEquals (proc.getRowSampleName (1), juce::String ("My Snare"));
            expect (proc.getRowSampleFile (1) == wav);
            expect (proc.getCurrentDrumBank() != nullptr && proc.getCurrentDrumBank()->rowSamples[1].size() == 1, "in the playing bank");
            expectEquals ((int) proc.getCurrentDrumBank()->rowSamples[1][0].data.size(), 6001);
            expect (LiveLooperProcessor::isRowSampleFile (wav) && ! LiveLooperProcessor::isRowSampleFile (tmp.getChildFile ("notes.txt")));
            for (auto* ext : { "x.aiff", "x.flac", "x.WAV" })
                expect (LiveLooperProcessor::isRowSampleFile (tmp.getChildFile (ext)), ext);

            // plug-in state: audio embedded, path kept - works after the original is gone
            juce::MemoryBlock state;
            proc.getStateInformation (state);
            wav.moveFileTo (tmp.getChildFile ("moved.wav"));
            {
                LiveLooperProcessor other (false, false);
                other.setStateInformation (state.getData(), (int) state.getSize());
                expect (other.hasRowSample (1), "restored from the embedded audio");
                expectEquals (other.getRowSampleName (1), juce::String ("My Snare"));
                expect (other.getRowSampleFile (1) == wav, "original path kept");
                expect (! other.hasRowSample (2));
            }
            tmp.getChildFile ("moved.wav").moveFileTo (wav);

            // .livelooper container: the sample is a FLAC file in audio/
            const auto set = tmp.getChildFile ("shared.livelooper");
            expect (proc.saveSetContainer (set));
            {
                juce::ZipFile zip (set);
                expect (zip.getIndexOfFileName ("audio/row2.flac") >= 0, "row sample in the container");
                LiveLooperProcessor other (false, false);
                expect (other.loadSet (set, error), error);
                expect (other.hasRowSample (1));
                expectEquals (other.getRowSampleName (1), juce::String ("My Snare"));
            }

            // beat files remember which rows play own samples (by path)
            const auto beat = proc.saveBeat (0, "With Sample", tmp.getChildFile ("with-sample.llpattern"));
            expect (beat.existsAsFile());
            expect (beat.loadFileAsString().contains ("My Snare"));
            proc.clearRowSample (1);
            expect (! proc.hasRowSample (1), "reset to the kit sample");
            expect (proc.getCurrentDrumBank()->rowSamples[1].empty());
            expect (proc.loadBeatFile (beat, 2, error), error);
            expect (proc.hasRowSample (1), "the beat brings its sample back");
            {
                // an old beat without the information leaves the rows alone
                SoundPacks::BeatSettings st;
                st.hasMix = true;
                proc.applyBeatSettings (st);
                expect (proc.hasRowSample (1));
                st.hasSamples = true;          // saved without own samples: back to the kit
                proc.applyBeatSettings (st);
                expect (! proc.hasRowSample (1));
            }

            // the BEATS page: load / drop / reset, name on the row
            {
                BeatsPage page (proc);
                page.setSize (1100, 700);
                auto* btn = page.getRowSampleButton (3);
                expect (btn != nullptr && btn->isVisible() && btn->getWidth() > 60, "row name button");
                expect (page.loadRowSample (3, wav));
                expect (proc.hasRowSample (3));
                expect (btn->getTooltip().contains ("My Snare"), "tooltip names the sample");
                page.resetRowSample (3);
                expect (! proc.hasRowSample (3));
                juce::StringArray files { wav.getFullPathName() };
                expect (page.isInterestedInFileDrag (files));
                expect (! page.isInterestedInFileDrag ({ tmp.getChildFile ("notes.txt").getFullPathName() }));
                auto* row5 = page.getRowSampleButton (5);
                expectEquals (page.rowAtY (row5->getBounds().getCentreY()), 5);
                expectEquals (page.rowAtY (2), -1, "above the rows");
                page.filesDropped (files, 500, row5->getBounds().getCentreY());
                expect (proc.hasRowSample (5), "dropped on row 6");
                expect (! proc.hasRowSample (4) && ! proc.hasRowSample (6));
                // the name button sits left of M / S / Lvl / Pitch without overlapping them
                for (int r = 0; r < BeatPattern::kRows; ++r)
                    for (auto* c : page.getChildren())
                        if (c != page.getRowSampleButton (r) && c->isVisible())
                            expect (! c->getBounds().intersects (page.getRowSampleButton (r)->getBounds()), "row " + juce::String (r + 1));
            }
            tmp.deleteRecursively();
        }
    }
};

static Rhythm16Tests rhythm16Tests;
