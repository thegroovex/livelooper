// v1.6 features of the processor (branch v1.6-wip), kept in their own file to ease merges:
//  - set sharing: one .livelooper file (zip container) with the set and all its audio
//  - demo set: running groove + three pre-recorded loops (self-made, CC0, embedded)
//  - empty set

#include "PluginProcessor.h"

#include "AppSettings.h"
#include "BinaryData.h"
#include "Localisation.h"

namespace livelooper
{
using namespace params;

namespace
{
    constexpr const char* kSetEntry = "set.xml";
    constexpr const char* kInfoEntry = "info.json";
    constexpr const char* kReadmeEntry = "README.txt";

    juce::String containerReadme()
    {
        return "This is a livelooper set (https://github.com/thegroovex/livelooper).\n"
               "Open it in livelooper with SETUP > Open set... or drop it on the livelooper window.\n"
               "set.xml holds the settings, audio/ the loops as FLAC files.\n\n"
               "Dies ist ein livelooper-Set. In livelooper mit SETUP > Set laden... oeffnen\n"
               "oder auf das livelooper-Fenster ziehen. set.xml enthaelt die Einstellungen, audio/ die Loops (FLAC).\n";
    }
}

//============================================================================== set sharing (.livelooper)
bool LiveLooperProcessor::saveSetContainer (const juce::File& file)
{
    auto xml = createSetXml();
    if (xml == nullptr)
        return false;

    juce::ZipFile::Builder zip;
    const auto now = juce::Time::getCurrentTime();
    juce::var tracks;
    if (auto* loops = xml->getChildByName ("LOOPS"))
        for (auto* e : loops->getChildWithTagNameIterator ("LOOP"))
        {
            juce::MemoryBlock flac;
            if (! flac.fromBase64Encoding (e->getStringAttribute ("data")))
                continue;
            const auto path = "audio/track" + juce::String (e->getIntAttribute ("track") + 1) + ".flac";
            e->removeAttribute ("data");
            e->setAttribute ("file", path);
            zip.addEntry (new juce::MemoryInputStream (flac, true), 0, path, now);   // FLAC is compressed already
            tracks.append (e->getIntAttribute ("track") + 1);
        }

    juce::DynamicObject::Ptr info = new juce::DynamicObject();
    info->setProperty ("format", "livelooper-set");
    info->setProperty ("formatVersion", 1);
    info->setProperty ("app", juce::String ("livelooper ") + LIVELOOPER_VERSION);
    info->setProperty ("tempo", (double) tempo->load());
    info->setProperty ("tracksWithLoops", tracks);
    info->setProperty ("created", now.toISO8601 (true));

    zip.addEntry (new juce::MemoryInputStream (juce::MemoryBlock (xml->toString().toRawUTF8(), xml->toString().getNumBytesAsUTF8()), true),
                  9, kSetEntry, now);
    const auto json = juce::JSON::toString (juce::var (info.get()));
    zip.addEntry (new juce::MemoryInputStream (juce::MemoryBlock (json.toRawUTF8(), json.getNumBytesAsUTF8()), true), 9, kInfoEntry, now);
    const auto readme = containerReadme();
    zip.addEntry (new juce::MemoryInputStream (juce::MemoryBlock (readme.toRawUTF8(), readme.getNumBytesAsUTF8()), true), 9, kReadmeEntry, now);

    auto target = file.hasFileExtension (kContainerExtension) ? file : file.withFileExtension (kContainerExtension);
    juce::TemporaryFile temp (target);
    {
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk() || ! zip.writeToStream (out, nullptr))
            return false;
    }
    return temp.overwriteTargetFileWithTemporary();
}

bool LiveLooperProcessor::loadSetContainer (const juce::File& file, juce::String& error)
{
    juce::ZipFile zip (file);
    const int setIndex = zip.getIndexOfFileName (kSetEntry);
    if (setIndex < 0)
    {
        error = i18n::tr ("This is not a livelooper set.");
        return false;
    }
    auto readEntry = [&zip] (int index, juce::MemoryBlock& dest)
    {
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (index));
        if (in == nullptr)
            return false;
        in->readIntoMemoryBlock (dest);
        return true;
    };

    juce::MemoryBlock xmlData;
    if (! readEntry (setIndex, xmlData))
    {
        error = i18n::tr ("This is not a livelooper set.");
        return false;
    }
    auto xml = juce::parseXML (xmlData.toString());
    if (xml == nullptr || ! xml->hasTagName ("LIVELOOPER_SET"))
    {
        error = i18n::tr ("This is not a livelooper set.");
        return false;
    }
    if (auto* loops = xml->getChildByName ("LOOPS"))
        for (auto* e : loops->getChildWithTagNameIterator ("LOOP"))
        {
            const auto path = e->getStringAttribute ("file");
            if (path.isEmpty())
                continue;
            juce::MemoryBlock audio;
            const int idx = zip.getIndexOfFileName (path);
            if (idx < 0 || ! readEntry (idx, audio))
            {
                error = i18n::tr ("The set is incomplete: %1 is missing.", path);
                return false;
            }
            e->setAttribute ("data", audio.toBase64Encoding());
            e->removeAttribute ("file");
        }
    return loadSetXml (std::move (xml), error);
}

//============================================================================== demo set / empty set
void LiveLooperProcessor::loadEmptySet()
{
    for (int t = 0; t < kNumTracks; ++t)
    {
        engine.lockTrack (t);
        engine.getTrack (t).clear();
        engine.unlockTrack (t);
    }
    pendingLoops.clear();
    applyDefaultSettings();
    for (int i = 0; i < kNumScenes; ++i)
        clearScene (i);
    lastScene.store (-1);
    sendChangeMessage();
}

bool LiveLooperProcessor::loadDemoSet()
{
    struct DemoLoop { const char* data; int size; int track; float level; float pan; };
    const DemoLoop loops[] = {
        { BinaryData::demo_bass_flac, BinaryData::demo_bass_flacSize, 0, 0.75f, 0.0f },
        { BinaryData::demo_keys_flac, BinaryData::demo_keys_flacSize, 1, 0.80f, -0.1f },
        { BinaryData::demo_arp_flac,  BinaryData::demo_arp_flacSize,  2, 1.30f, 0.1f },
    };
    constexpr double kDemoBpm = 100.0;

    loadEmptySet();

    auto set = [this] (const juce::String& id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
            p->endChangeGesture();
        }
    };
    setInternalTempo (kDemoBpm);
    set (gQuantize, 2.0f);           // measure
    set (gRhythmOn, 1.0f);
    set (gRhythmLevel, 0.55f);
    set (gRhythmPattern, 1.0f);
    set (gRhythmKit, 0.0f);
    set (gRhythmGroove, 1.0f);
    set (gRhythmSwing, 54.0f);

    std::vector<StoredLoop> demo;
    juce::FlacAudioFormat flac;
    for (const auto& l : loops)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (
            flac.createReaderFor (new juce::MemoryInputStream (l.data, (size_t) l.size, false), true));
        if (reader == nullptr)
            return false;
        StoredLoop s;
        s.track = l.track;
        s.sampleRate = reader->sampleRate;
        s.samplesPerPpq = reader->sampleRate * 60.0 / kDemoBpm;
        const int len = (int) reader->lengthInSamples;
        s.audio.setSize (2, len);
        reader->read (&s.audio, 0, len, 0, true, true);
        demo.push_back (std::move (s));

        set (trackId (l.track, kLevel), l.level);
        set (trackId (l.track, kPan), l.pan);
        set (trackId (l.track, kStretch), 1.0f);     // follows a host tempo other than 100 BPM
    }
    // scene 1: bass + keys, scene 2: everything
    sceneMasks[0].store (0x3);
    sceneMasks[1].store (0x7);

    pendingLoops = std::move (demo);
    if (prepared)
        applyPendingLoops();
    sendChangeMessage();
    return true;
}
} // namespace livelooper
