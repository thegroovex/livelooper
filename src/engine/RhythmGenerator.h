#pragma once

#include "Types.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace livelooper
{
/** v1.5: the recorded drum samples of the kits (filled by the plug-in from its embedded CC0 /
    public-domain recordings, see resources/drums/SOURCES.md, and since v1.6 from
    sound packs). Without samples for a kit the rhythm falls back to the synthesised voices. */
struct DrumBank
{
    static constexpr int kNumVoices = 8;   // voices: kick, snare, clap, closed hat, open hat, rim, shaker, tom
    struct Sample
    {
        std::vector<float> data;    // mono
        double sampleRate = 44100.0;
        float layer = 1.0f;         // velocity layer 0 .. 1
    };
    /** v1.6: one kit (built-in or from a sound pack). */
    struct Kit
    {
        std::string name;                                    // display name (packs; built-ins use getKitName)
        std::array<std::vector<Sample>, kNumVoices> voices;
        std::array<float, kNumVoices> trim { 1.0f, 0.8f, 0.6f, 0.45f, 0.4f, 0.55f, 0.4f, 0.65f };   // level per voice
        float room = 0.08f;                                  // amount of the small room
        std::array<std::string, kNumVoices> voiceNames;      // empty = the standard voice name
        int fillStyle = -1;                                  // RhythmGenerator::FillStyle, -1 = default
        bool empty() const noexcept { return voices[0].empty() && voices[1].empty() && voices[3].empty(); }
    };
    /** Index = value of the kit parameter (built-in kits first, then the sound-pack slots). */
    std::vector<std::shared_ptr<const Kit>> kits;
    /** v1.6 BEATS: the user's own sample for a row (empty = the kit's samples). Replaces the
        voice in every kit until the row is reset. */
    std::array<std::vector<Sample>, kNumVoices> rowSamples;

    /** The samples a voice of a kit plays (the row's own sample, if there is one). */
    const std::vector<Sample>& samplesFor (const Kit& kit, int voice) const noexcept
    {
        const auto v = (size_t) std::clamp (voice, 0, kNumVoices - 1);
        return rowSamples[v].empty() ? kit.voices[v] : rowSamples[v];
    }

    const Kit* getKit (int index) const noexcept
    {
        return index >= 0 && index < (int) kits.size() && kits[(size_t) index] != nullptr && ! kits[(size_t) index]->empty()
                 ? kits[(size_t) index].get() : nullptr;
    }
    int numSamples() const
    {
        int n = 0;
        for (auto& k : kits)
            if (k != nullptr)
                for (auto& v : k->voices) n += (int) v.size();
        return n;
    }
};

/** v1.6 BEATS: one step-sequencer pattern (8 rows = the kit voices). Plain data for the message
    thread (editing, saving); the audio thread reads the BeatPatternBank. */
struct BeatPattern
{
    static constexpr int kRows = 8, kMaxBars = 4, kMaxStepsPerBar = 32, kMaxSteps = kMaxBars * kMaxStepsPerBar;
    std::string name;
    int stepsPerBar = 16;          // 16 (16th notes) or 32 (32nd notes)
    int bars = 1;                  // 1 .. 4
    /** velocity 0 = off, 1 .. 127 */
    std::array<std::array<uint8_t, kMaxSteps>, kRows> cells {};

    int numSteps() const noexcept { return stepsPerBar * bars; }
    bool isEmpty() const noexcept
    {
        for (auto& r : cells) for (auto c : r) if (c != 0) return false;
        return true;
    }
    uint8_t get (int row, int step) const noexcept
    {
        return row >= 0 && row < kRows && step >= 0 && step < kMaxSteps ? cells[(size_t) row][(size_t) step] : (uint8_t) 0;
    }
    void set (int row, int step, int velocity) noexcept
    {
        if (row >= 0 && row < kRows && step >= 0 && step < kMaxSteps)
            cells[(size_t) row][(size_t) step] = (uint8_t) std::clamp (velocity, 0, 127);
    }
    /** Changes the resolution / length; existing steps are kept where they still fit. */
    void resize (int newStepsPerBar, int newBars) noexcept;
    bool operator== (const BeatPattern& o) const noexcept { return stepsPerBar == o.stepsPerBar && bars == o.bars && cells == o.cells; }
};

/** v1.6: the user pattern slots, shared lock-free between the editor (message thread) and the
    rhythm (audio thread): every cell is an atomic, so editing while playing never blocks. */
class BeatPatternBank
{
public:
    static constexpr int kNumSlots = 16;
    BeatPatternBank();
    void store (int slot, const BeatPattern& p) noexcept;   // name is kept by the caller
    BeatPattern load (int slot) const noexcept;
    void setCell (int slot, int row, int step, int velocity) noexcept;
    int  getCell (int slot, int row, int step) const noexcept;
    int  getStepsPerBar (int slot) const noexcept;
    int  getBars (int slot) const noexcept;
    bool isEmpty (int slot) const noexcept;
    /** Increases with every change (UI repaint). */
    int  getVersion() const noexcept { return version.load (std::memory_order_relaxed); }

private:
    struct Slot
    {
        std::array<std::array<std::atomic<uint8_t>, BeatPattern::kMaxSteps>, BeatPattern::kRows> cells;
        std::atomic<int> stepsPerBar { 16 }, bars { 1 };
    };
    std::array<Slot, kNumSlots> slots;
    std::atomic<int> version { 0 };
};

/**
    Built-in drum machine. All sounds are synthesised (no samples), patterns are 16 steps
    of 16th notes that follow the timeline, so the rhythm is always locked to the host
    tempo or the internal clock.

    Since 1.3:
    - four kits (Studio, 808, 909, Lo-Fi) - each voice is shaped per kit
    - voices: kick, snare, hand clap, closed / open hi-hat (with choke), rimshot, shaker, tom
    - every step has a velocity (accent, normal, ghost note) and hi-hat rolls
    - swing (MPC style, 50 % = straight, 66 % = triplet feel) and humanize (timing and
      velocity variation), a short room on snare / clap / rim and a bus saturator
    - metronome sounds: beep, woodblock, click, rimshot
*/
class RhythmGenerator
{
public:
    static int getNumPatterns() noexcept;
    static const char* getPatternName (int index) noexcept;
    static int getNumKits() noexcept;
    static const char* getKitName (int index) noexcept;
    /** v1.5 groove templates (per-instrument micro-timing + accent curve) and fills. */
    static int getNumGrooves() noexcept;
    static const char* getGrooveName (int index) noexcept;
    static int getNumFillModes() noexcept { return 3; }   // off, every 4 bars, every 8 bars
    /** Micro-timing of a voice on a 16th step for a groove, in milliseconds (public for tests). */
    static double grooveOffsetMs (int groove, int voice, int step) noexcept;
    /** Accent factor (velocity) of a 16th step for a groove (public for tests). */
    static float grooveAccent (int groove, int step) noexcept;
    static constexpr int kNumClickSounds = 4;   // 0 = beep, 1 = woodblock, 2 = click, 3 = rimshot

    // ---- v1.6 rhythm: kits, BEATS step sequencer, fill on demand
    static constexpr int kNumPackKitSlots = 8;
    /** Kit parameter choices: the built-in kits, then the sound-pack slots "Pack Kit 1 .. 8". */
    static int getNumKitChoices() noexcept { return getNumKits() + kNumPackKitSlots; }
    /** Pattern parameter choices: the presets, then the user slots "User 1 .. 16". */
    static int getNumPatternChoices() noexcept { return getNumPatterns() + BeatPatternBank::kNumSlots; }
    static bool isUserPattern (int index) noexcept { return index >= getNumPatterns(); }
    static int  userSlotOf (int index) noexcept    { return index - getNumPatterns(); }
    /** Level trims / room of the built-in sample kits (used by the sample loader). */
    static float builtInTrim (int kit, int voice) noexcept;
    static float builtInRoom (int kit) noexcept;
    /** English voice names: "Kick", "Snare", "Clap", "Closed Hat", "Open Hat", "Rim", "Shaker", "Tom". */
    static const char* getVoiceName (int voice) noexcept;
    /** English name of a voice in a built-in kit ("Conga" for the tom of Afro Latin). */
    static const char* getKitVoiceName (int kit, int voice) noexcept;
    enum FillStyle : int { FillToms = 0, FillSnareRoll, FillKickRoll, FillHatRoll, FillBreaks, FillConga, FillClapRoll, FillDisco, kNumFillStyles };
    static int kitFillStyle (int kit) noexcept;
    /** Fill style of a preset pattern, -1 = the kit decides. */
    static int patternFillStyle (int pattern) noexcept;
    /** A preset pattern as a 16-step BEATS pattern (rolls become single hits). */
    static BeatPattern presetToPattern (int pattern);
    /** Kick accent shape: velocity factor of a kick on a 16th step for a variation amount (tests). */
    static float kickAccent (int step, long long bar, float variation) noexcept;

    /** 0 = machine-like, 1 = lots of variation (round robin across layers, pitch / decay, blend). */
    void setVariation (float v) noexcept  { variation = std::clamp (v, 0.0f, 1.0f); }
    void setUserPatterns (const BeatPatternBank* b) noexcept { userBank = b; }
    /** Pattern changes while playing: 0 = immediately, 1 = on the next beat, 2 = on the next bar. */
    void setSwitchQuantize (int q) noexcept { switchQuantize = std::clamp (q, 0, 2); }
    int  getActivePattern() const noexcept { return activePattern.load (std::memory_order_relaxed); }
    int  getRequestedPattern() const noexcept { return requestedPattern; }
    /** Fill on demand: fills while held, back to the groove on the next downbeat after release. */
    void setFillHeld (bool held) noexcept { if (held && ! fillHeld) fillPressed = true; fillHeld = held; }
    bool isFillPlaying() const noexcept   { return fillPlaying.load (std::memory_order_relaxed); }
    /** Row mixer of the voices (all patterns): level 0 .. 2, pitch in semitones, muted (solo resolved by the caller). */
    void setVoiceMix (int voice, float levelGain, float pitchSemitones, bool muted) noexcept
    {
        if (voice < 0 || voice >= DrumBank::kNumVoices) return;
        voiceGain[(size_t) voice] = levelGain;
        voicePitch[(size_t) voice] = pitchSemitones;
        voiceMuted[(size_t) voice] = muted;
    }
    /** The 16th step of the timeline that sounds now (-1 = stopped), for the step display. */
    long long getCurrentStep() const noexcept { return currentStep.load (std::memory_order_relaxed); }
    /** Tests: velocity of the last fired hit per voice. */
    float getLastVelocity (int voice) const noexcept { return lastVelocity[(size_t) std::clamp (voice, 0, 7)]; }
    /** Tests: number of hits fired per voice since prepare(). */
    int getHitCount (int voice) const noexcept { return hitCount[(size_t) std::clamp (voice, 0, 7)]; }

    void prepare (double sampleRate);
    void reset() noexcept;

    void setPattern (int index) noexcept  { requestedPattern = index; }
    void setLevel (float l) noexcept      { level = l; }
    void setClickSound (int s) noexcept   { clickSound = s; }
    /** v1.5: metronome accent on the first beat of the bar (default on). */
    void setClickAccent (bool a) noexcept { clickAccent = a; }
    void setKit (int k) noexcept          { kit = k; }
    /** 0.5 = straight ... 0.75 = hard swing (the odd 16th notes are delayed). */
    void setSwing (float s) noexcept      { swing = s; }
    /** 0 = machine-exact, 1 = loose (timing up to 8 ms late, velocity variation). */
    void setHumanize (float h) noexcept   { humanize = h; }
    void setGroove (int g) noexcept       { groove = g; }
    /** 0 = no fills, 1 = a fill in the last beat of every 4th bar, 2 = every 8th bar. */
    void setFills (int f) noexcept        { fills = f; }
    /** Samples for the kits (message thread, before playing). */
    void setDrumBank (std::shared_ptr<const DrumBank> b) noexcept { bank = std::move (b); bankPtr.store (bank.get()); }
    /** v1.6: replaces the bank while playing (audio-thread safe; the caller keeps the bank alive). */
    void setDrumBankPointer (const DrumBank* b) noexcept { bankPtr.store (b); }
    bool isUsingSamples() const noexcept;
    /** Tests: the sample chosen last for a voice (index in the kit's list), -1 = none. */
    int getLastSampleIndex (int voice) const noexcept { return lastSampleIndex[(size_t) std::clamp (voice, 0, 7)]; }

    /** Adds the rhythm to the output. timelineRunning = false produces silence (voices decay). */
    void process (float* outL, float* outR, int numSamples, double ppqStart, double ppqPerSample,
                  int beatsPerMeasure, bool timelineRunning, float* kickOut = nullptr) noexcept;

    /** Simple TPT state-variable filter (public for tests). */
    struct Svf
    {
        float g = 0.0f, k = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
        void set (double sampleRate, double freq, double q) noexcept;
        void reset() noexcept { ic1 = ic2 = 0.0f; }
        /** returns band-pass, writes low / high pass */
        float process (float x, float& lp, float& hp) noexcept;
    };

    enum Voice : uint8_t { Kick, Snare, Clap, HatClosed, HatOpen, Rim, Shaker, Tom, Click };

    /** Step offset (in 16th notes) of a step for a given swing (public for tests). */
    static double swingOffset (int step, float swing) noexcept;

private:
    void scheduleStep (long long step, int beatsPerMeasure, double baseDelay, bool allowEarly) noexcept;
    void scheduleUserStep (int slot, long long step, double base, bool allowEarly) noexcept;
    void scheduleFill (int style, int f, double base, int s, bool allowEarly) noexcept;
    const DrumBank::Kit* currentKit() const noexcept;
    void addEvent (double delaySamples, Voice v, float velocity, uint8_t variant = 0, int step = 0, bool allowEarly = false) noexcept;
    bool fireSample (Voice v, float vel) noexcept;
    struct Player;
    Player* startPlayer (const DrumBank::Sample& sample, Voice v, float gain, double inc, double fadeAt, uint32_t age) noexcept;
    void fire (Voice v, float velocity, uint8_t variant) noexcept;
    float decayFor (double seconds) const noexcept;
    float noise() noexcept;
    float random01() noexcept;

    double sampleRate = 44100.0;
    double samplesPerStep = 5512.5;
    int    pattern = 0;
    int    clickSound = 0;
    bool   clickAccent = true;
    int    kit = 0;
    float  swing = 0.5f, humanize = 0.25f;
    float  level = 0.7f;
    long long lastStep = -1;

    struct Event { double delay; Voice voice; float velocity; uint8_t variant; };
    // v1.6
    float variation = 0.5f;
    const BeatPatternBank* userBank = nullptr;
    int switchQuantize = 2, requestedPattern = 0;
    std::atomic<int> activePattern { 0 };
    bool fillHeld = false, fillPressed = false, fillActive = false;
    int fillSteps = 0;
    std::atomic<bool> fillPlaying { false };
    std::atomic<long long> currentStep { -1 };
    std::atomic<const DrumBank*> bankPtr { nullptr };
    std::array<float, 8> voiceGain { 1, 1, 1, 1, 1, 1, 1, 1 }, voicePitch {}, lastVelocity {};
    std::array<bool, 8> voiceMuted {};
    std::array<int, 8> hitCount {}, lastRrIndex { -1, -1, -1, -1, -1, -1, -1, -1 };
    long long scheduleBar = 0;
    std::array<Event, 96> events {};
    // v1.5
    int groove = 1, fills = 0;
    std::shared_ptr<const DrumBank> bank;
    struct Player
    {
        const DrumBank::Sample* sample = nullptr;
        double pos = 0.0, inc = 1.0;
        float gain = 0.0f, fade = 1.0f, fadeStep = 0.0f, room = 0.0f;
        double fadeAt = 1.0e18, fadeLen = 0.0;   // v1.6: humanised decay (tail fades from fadeAt)
        Voice voice = Kick;
        bool active = false;
        uint32_t age = 0;
    };
    std::array<Player, 32> players {};
    std::array<int, 8> roundRobin {}, lastSampleIndex { -1, -1, -1, -1, -1, -1, -1, -1 };
    uint32_t playerAge = 0;
    float kickFollow = 0.0f;
    int numEvents = 0;

    // kick
    float kickEnv = 0.0f, kickDecay = 0.999f, kickPhase = 0.0f, kickPitch = 0.0f, kickPitchDecay = 0.99f;
    float kickClick = 0.0f, kickClickDecay = 0.9f, kickVel = 1.0f;
    Svf   kickClickFilter;
    // snare
    float snBodyEnv = 0.0f, snBodyDecay = 0.999f, snPh1 = 0.0f, snPh2 = 0.0f, snPitch = 0.0f, snPitchDecay = 0.99f;
    float snNoiseEnv = 0.0f, snNoiseDecay = 0.999f, snSnap = 0.0f, snSnapDecay = 0.9f, snVel = 1.0f;
    Svf   snBand, snHigh;
    // clap
    float clapEnv = 0.0f, clapDecay = 0.999f, clapVel = 1.0f;
    int   clapBurst = 0, clapBurstLeft = 0;
    Svf   clapFilter, clapHigh;
    // hats
    float hatEnv = 0.0f, hatDecay = 0.99f, hatVel = 1.0f;
    float hatPhase[6] {};
    Svf   hatBand, hatHighPass;
    // rimshot
    float rimEnv = 0.0f, rimDecay = 0.99f, rimPh1 = 0.0f, rimPh2 = 0.0f, rimVel = 1.0f;
    Svf   rimFilter;
    // shaker
    float shEnv = 0.0f, shDecay = 0.99f, shAttack = 0.0f, shAttackCoeff = 0.1f, shVel = 1.0f;
    Svf   shBand;
    // tom
    float tomEnv = 0.0f, tomDecay = 0.999f, tomPhase = 0.0f, tomPitch = 0.0f, tomPitchDecay = 0.99f, tomVel = 1.0f;
    // bus
    Svf   busLowPass;
    // small room (4 combs + 2 all-passes) for snare / clap / rim
    std::vector<float> combBuf[4], apBuf[2];
    int   combPos[4] {}, apPos[2] {};
    float combStore[4] {};
    // metronome
    float clickEnv = 0.0f, clickDecay = 0.99f, clickPh1 = 0.0f, clickPh2 = 0.0f, clickF1 = 0.0f, clickF2 = 0.0f, clickAmp = 1.0f;
    float clickGlide = 0.0f, clickGlideDecay = 0.9f, clickKnock = 0.0f, clickKnockDecay = 0.9f;
    int   activeClickSound = 0;
    Svf   clickFilter;

    uint32_t noiseSeed = 0x12345678u, randSeed = 0x9e3779b9u;
};
} // namespace livelooper
