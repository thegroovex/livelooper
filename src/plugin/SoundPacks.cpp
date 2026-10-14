#include "SoundPacks.h"
#include "AppSettings.h"
#include "DrumSampleLoader.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace livelooper
{
namespace
{
    constexpr const char* kVoiceKeys[DrumBank::kNumVoices] = { "kick", "snare", "clap", "hat_closed", "hat_open", "rim", "shaker", "tom" };
    constexpr const char* kFillKeys[RhythmGenerator::kNumFillStyles] = { "toms", "snare roll", "kick roll", "hat roll",
                                                                         "breaks", "conga", "clap roll", "disco" };
    constexpr const char* kSlotsKey = "packKitSlots";

    juce::File musicFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("livelooper");
    }

    int velocityFromChar (juce::juce_wchar c)
    {
        if (c == 'X') return 127;
        if (c == 'x') return 96;
        if (c == 'g' || c == 'G') return 40;
        if (c >= '1' && c <= '9') return (int) (c - '0') * 14;
        return 0;
    }

    juce::String safeName (const juce::String& s)
    {
        auto r = juce::File::createLegalFileName (s.trim());
        return r.isEmpty() ? juce::String ("Pattern") : r;
    }
}

SoundPacks& SoundPacks::get()
{
    static SoundPacks instance;
    return instance;
}

SoundPacks::SoundPacks() = default;

juce::File SoundPacks::getDefaultPackFolder()    { return musicFolder().getChildFile ("Sound Packs"); }
juce::File SoundPacks::getDefaultPatternFolder() { return musicFolder().getChildFile ("Patterns"); }

void SoundPacks::setFoldersForTests (const juce::File& p, const juce::File& pat)
{
    std::lock_guard<std::recursive_mutex> l (lock);
    packFolderOverride = p;
    patternFolderOverride = pat;
    scanned = false;
}

juce::File SoundPacks::getPackFolder() const
{
    std::lock_guard<std::recursive_mutex> l (lock);
    return packFolderOverride != juce::File() ? packFolderOverride : getDefaultPackFolder();
}

juce::File SoundPacks::getPatternFolder() const
{
    std::lock_guard<std::recursive_mutex> l (lock);
    return patternFolderOverride != juce::File() ? patternFolderOverride : getDefaultPatternFolder();
}

const char* SoundPacks::voiceKey (int v) noexcept { return kVoiceKeys[juce::jlimit (0, DrumBank::kNumVoices - 1, v)]; }

int SoundPacks::voiceFromKey (const juce::String& key) noexcept
{
    const auto k = key.trim().toLowerCase().replaceCharacter (' ', '_');
    for (int v = 0; v < DrumBank::kNumVoices; ++v)
        if (k == kVoiceKeys[v])
            return v;
    if (k == "hat" || k == "hihat" || k == "closed_hat" || k == "hatc") return 3;
    if (k == "open_hat" || k == "hato") return 4;
    if (k == "bd") return 0;
    if (k == "sd") return 1;
    return -1;
}

juce::String SoundPacks::rowToText (const BeatPattern& p, int row)
{
    juce::String s;
    for (int st = 0; st < p.numSteps(); ++st)
    {
        const int v = p.get (row, st);
        s << (v == 0 ? "." : v == 127 ? "X" : v == 96 ? "x" : v == 40 ? "g"
                         : juce::String::charToString ((juce::juce_wchar) ('0' + juce::jlimit (1, 9, juce::roundToInt (v / 14.0f)))));
    }
    return s;
}

bool SoundPacks::patternFromVar (const juce::var& v, BeatPattern& out, juce::String& name)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return false;
    out = BeatPattern();
    name = v.getProperty ("name", "Pattern").toString();
    out.stepsPerBar = (int) v.getProperty ("steps_per_bar", 16) >= 32 ? 32 : 16;
    out.bars = juce::jlimit (1, BeatPattern::kMaxBars, (int) v.getProperty ("bars", 1));
    const auto rows = v.getProperty ("rows", {});
    auto* rowObj = rows.getDynamicObject();
    if (rowObj == nullptr)
        return false;
    for (const auto& prop : rowObj->getProperties())
    {
        const int row = voiceFromKey (prop.name.toString());
        if (row < 0)
            continue;
        if (prop.value.isArray())
        {
            const auto* arr = prop.value.getArray();
            for (int st = 0; st < arr->size() && st < out.numSteps(); ++st)
                out.set (row, st, (int) (*arr)[st]);
        }
        else
        {
            const auto text = prop.value.toString().removeCharacters (" |");
            for (int st = 0; st < text.length() && st < out.numSteps(); ++st)
                out.set (row, st, velocityFromChar (text[st]));
        }
    }
    out.name = name.toStdString();
    return true;
}

juce::var SoundPacks::patternToVar (const BeatPattern& p, const juce::String& name)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);
    obj->setProperty ("steps_per_bar", p.stepsPerBar);
    obj->setProperty ("bars", p.bars);
    auto* rows = new juce::DynamicObject();
    for (int r = 0; r < BeatPattern::kRows; ++r)
    {
        bool any = false;
        for (int st = 0; st < p.numSteps(); ++st)
            any = any || p.get (r, st) != 0;
        if (! any)
            continue;
        bool simple = true;
        for (int st = 0; st < p.numSteps(); ++st)
        {
            const int v = p.get (r, st);
            simple = simple && (v == 0 || v == 127 || v == 96 || v == 40);
        }
        if (simple)
            rows->setProperty (voiceKey (r), rowToText (p, r));
        else
        {
            juce::Array<juce::var> arr;
            for (int st = 0; st < p.numSteps(); ++st)
                arr.add (p.get (r, st));
            rows->setProperty (voiceKey (r), arr);
        }
    }
    obj->setProperty ("rows", juce::var (rows));
    return juce::var (obj);
}

bool SoundPacks::loadPack (const juce::File& folder, LoadedPack& out, juce::String& error)
{
    const auto jsonFile = folder.getChildFile (kPackFileName);
    if (! jsonFile.existsAsFile())
    {
        error = "no " + juce::String (kPackFileName) + " in " + folder.getFileName();
        return false;
    }
    const auto json = juce::JSON::parse (jsonFile.loadFileAsString());
    if (json.getDynamicObject() == nullptr || json.getProperty ("format", "").toString() != "livelooper-pack")
    {
        error = folder.getFileName() + ": not a livelooper pack (\"format\": \"livelooper-pack\" missing)";
        return false;
    }
    out = LoadedPack();
    out.info.folder = folder;
    out.info.name = json.getProperty ("name", folder.getFileName()).toString();
    out.info.author = json.getProperty ("author", "").toString();
    out.info.license = json.getProperty ("license", "").toString();
    out.info.description = json.getProperty ("description", "").toString();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    if (const auto* kits = json.getProperty ("kits", {}).getArray())
    {
        for (const auto& k : *kits)
        {
            auto kit = std::make_shared<DrumBank::Kit>();
            kit->name = k.getProperty ("name", "Kit").toString().toStdString();
            kit->room = juce::jlimit (0.0f, 0.5f, (float) (double) k.getProperty ("room", 0.08));
            const auto fill = k.getProperty ("fill", "").toString().toLowerCase();
            for (int f = 0; f < RhythmGenerator::kNumFillStyles; ++f)
                if (fill == kFillKeys[f])
                    kit->fillStyle = f;
            const auto voices = k.getProperty ("voices", {});
            if (auto* vo = voices.getDynamicObject())
            {
                for (const auto& prop : vo->getProperties())
                {
                    const int v = voiceFromKey (prop.name.toString());
                    if (v < 0)
                        continue;
                    kit->voiceNames[(size_t) v] = prop.value.getProperty ("name", RhythmGenerator::getVoiceName (v)).toString().toStdString();
                    kit->trim[(size_t) v] = juce::jlimit (0.0f, 2.0f, (float) (double) prop.value.getProperty ("level", 0.8));
                    const auto* samples = prop.value.getProperty ("samples", {}).getArray();
                    if (samples == nullptr)
                        continue;
                    for (const auto& sv : *samples)
                    {
                        if ((int) kit->voices[(size_t) v].size() >= kMaxSamplesPerVoice)
                            break;
                        const auto file = folder.getChildFile (sv.isString() ? sv.toString() : sv.getProperty ("file", "").toString());
                        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
                        if (reader == nullptr || reader->lengthInSamples <= 0)
                        {
                            error = out.info.name + ": cannot read " + file.getFileName();
                            continue;
                        }
                        const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 4.0));
                        juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
                        reader->read (&buf, 0, len, 0, true, true);
                        DrumBank::Sample s;
                        s.sampleRate = reader->sampleRate;
                        s.layer = juce::jlimit (0.05f, 1.0f, sv.isString() ? 0.8f : (float) (double) sv.getProperty ("velocity", 0.8));
                        s.data.resize ((size_t) len + 1, 0.0f);
                        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                            for (int i = 0; i < len; ++i)
                                s.data[(size_t) i] += buf.getSample (ch, i) / (float) buf.getNumChannels();
                        kit->voices[(size_t) v].push_back (std::move (s));
                    }
                }
            }
            if (! kit->empty())
            {
                out.info.kitNames.add (juce::String (kit->name));
                out.kits.push_back (kit);
            }
        }
    }
    if (const auto* pats = json.getProperty ("patterns", {}).getArray())
        for (const auto& pv : *pats)
        {
            NamedPattern np;
            if (patternFromVar (pv, np.pattern, np.name))
            {
                np.source = out.info.name;
                out.patterns.push_back (std::move (np));
            }
        }
    out.info.numPatterns = (int) out.patterns.size();
    if (out.kits.empty() && out.patterns.empty())
    {
        if (error.isEmpty())
            error = out.info.name + ": no kits or patterns";
        return false;
    }
    return true;
}

void SoundPacks::ensureScanned()
{
    if (! scanned)
        rescan();
}

void SoundPacks::rescan()
{
    std::lock_guard<std::recursive_mutex> l (lock);
    scanned = true;
    packs.clear();
    const auto folder = getPackFolder();
    if (folder.isDirectory())
    {
        auto dirs = folder.findChildFiles (juce::File::findDirectories, false);
        dirs.sort();
        for (const auto& d : dirs)
        {
            LoadedPack p;
            juce::String error;
            if (loadPack (d, p, error))
                packs.push_back (std::move (p));
        }
    }
    rebuildBank();
}

void SoundPacks::rebuildBank()
{
    // every pack kit gets a slot; the slot of a kit is remembered on this computer
    juce::StringArray found;
    std::vector<std::shared_ptr<const DrumBank::Kit>> foundKits;
    for (const auto& p : packs)
        for (size_t k = 0; k < p.kits.size(); ++k)
        {
            found.add (p.info.name + " / " + juce::String (p.kits[k]->name));
            foundKits.push_back (p.kits[k]);
        }
    auto saved = juce::StringArray::fromLines (appsettings::getValue (kSlotsKey));
    juce::StringArray slots;
    for (int i = 0; i < RhythmGenerator::kNumPackKitSlots; ++i)
        slots.add (i < saved.size() && found.contains (saved[i]) ? saved[i] : juce::String());
    for (const auto& id : found)
        if (! slots.contains (id))
            for (int i = 0; i < slots.size(); ++i)
                if (slots[i].isEmpty()) { slots.set (i, id); break; }
    const auto text = slots.joinIntoString ("\n");
    if (text != appsettings::getValue (kSlotsKey) && ! found.isEmpty())
        appsettings::setValue (kSlotsKey, text);

    auto newBank = std::make_shared<DrumBank>();
    const auto embedded = getEmbeddedDrumBank();
    newBank->kits = embedded->kits;
    newBank->kits.resize ((size_t) RhythmGenerator::getNumKits());
    for (int i = 0; i < RhythmGenerator::kNumPackKitSlots; ++i)
    {
        const int idx = found.indexOf (slots[i]);
        slotKits[(size_t) i] = idx >= 0 ? foundKits[(size_t) idx] : nullptr;
        slotNames[(size_t) i] = idx >= 0 ? slots[i] : juce::String();
        newBank->kits.push_back (slotKits[(size_t) i]);
    }
    if (bank != nullptr)
        retired.push_back (bank);
    bank = newBank;
    ++version;
}

std::shared_ptr<const DrumBank> SoundPacks::getBank()
{
    std::lock_guard<std::recursive_mutex> l (lock);
    ensureScanned();
    return bank;
}

juce::String SoundPacks::getSlotName (int slot) const
{
    std::lock_guard<std::recursive_mutex> l (lock);
    return juce::isPositiveAndBelow (slot, RhythmGenerator::kNumPackKitSlots) ? slotNames[(size_t) slot] : juce::String();
}

std::vector<SoundPacks::PackInfo> SoundPacks::getPacks() const
{
    std::lock_guard<std::recursive_mutex> l (lock);
    std::vector<PackInfo> r;
    for (const auto& p : packs)
        r.push_back (p.info);
    return r;
}

std::vector<SoundPacks::NamedPattern> SoundPacks::getPackPatterns() const
{
    std::lock_guard<std::recursive_mutex> l (lock);
    std::vector<NamedPattern> r;
    for (const auto& p : packs)
        for (const auto& np : p.patterns)
            r.push_back (np);
    return r;
}

bool SoundPacks::importPack (const juce::File& source, juce::String& error)
{
    const auto dest = getPackFolder();
    if (! dest.createDirectory())
    {
        error = "cannot create " + dest.getFullPathName();
        return false;
    }
    juce::File packDir;
    if (source.isDirectory())
    {
        packDir = dest.getChildFile (source.getFileName());
        if (packDir == source)
        {
            rescan();
            return true;
        }
        packDir.deleteRecursively();
        if (! source.copyDirectoryTo (packDir))
        {
            error = "cannot copy " + source.getFileName();
            return false;
        }
    }
    else if (source.hasFileExtension ("zip"))
    {
        juce::ZipFile zip (source);
        if (zip.getNumEntries() == 0)
        {
            error = source.getFileName() + ": empty or not a zip file";
            return false;
        }
        // the json may sit at the top of the zip or inside one folder
        juce::String prefix;
        bool foundJson = false;
        for (int i = 0; i < zip.getNumEntries() && ! foundJson; ++i)
        {
            const auto name = zip.getEntry (i)->filename.replaceCharacter ('\\', '/');
            if (name == kPackFileName || name.endsWith (juce::String ("/") + kPackFileName))
            {
                const auto pre = name.dropLastCharacters ((int) strlen (kPackFileName));
                if (pre.length() - pre.removeCharacters ("/").length() <= 1)
                {
                    prefix = pre;
                    foundJson = true;
                }
            }
        }
        if (! foundJson)
        {
            error = source.getFileName() + ": no " + juce::String (kPackFileName) + " inside";
            return false;
        }
        packDir = dest.getChildFile (prefix.isNotEmpty() ? prefix.trimCharactersAtEnd ("/") : source.getFileNameWithoutExtension());
        packDir.deleteRecursively();
        const auto target = prefix.isNotEmpty() ? dest : packDir;
        if (! zip.uncompressTo (target, true).wasOk())
        {
            error = source.getFileName() + ": cannot extract";
            return false;
        }
    }
    else
    {
        error = source.getFileName() + ": choose a pack folder or a .zip file";
        return false;
    }
    LoadedPack test;
    if (! loadPack (packDir, test, error))
    {
        packDir.deleteRecursively();
        return false;
    }
    rescan();
    return true;
}

std::vector<SoundPacks::NamedPattern> SoundPacks::getUserPatterns() const
{
    std::vector<NamedPattern> r;
    const auto folder = getPatternFolder();
    if (! folder.isDirectory())
        return r;
    auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kPatternExtension);
    files.sort();
    for (const auto& f : files)
    {
        NamedPattern np;
        if (patternFromVar (juce::JSON::parse (f.loadFileAsString()), np.pattern, np.name))
        {
            np.file = f;
            r.push_back (std::move (np));
        }
    }
    return r;
}

juce::File SoundPacks::saveUserPattern (const BeatPattern& p, const juce::String& name)
{
    const auto folder = getPatternFolder();
    if (! folder.createDirectory())
        return {};
    auto v = patternToVar (p, name);
    if (auto* o = v.getDynamicObject())
        o->setProperty ("format", "livelooper-pattern");
    const auto file = folder.getChildFile (safeName (name) + kPatternExtension);
    return file.replaceWithText (juce::JSON::toString (v)) ? file : juce::File();
}
} // namespace livelooper
