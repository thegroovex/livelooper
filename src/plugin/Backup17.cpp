// v1.7 auto backup and crash recovery of the processor (see AutoBackup.h), kept in its own file.

#include "PluginProcessor.h"

#include "AppSettings.h"
#include "Localisation.h"

#include <set>

namespace livelooper
{
using namespace params;

namespace
{
    /** Plug-in instance ids in use in this process (a duplicated track brings a copy of the id). */
    struct ActiveIds
    {
        std::mutex lock;
        std::set<juce::String> ids;
        bool add (const juce::String& id)    { std::lock_guard<std::mutex> l (lock); return ids.insert (id).second; }
        void remove (const juce::String& id) { std::lock_guard<std::mutex> l (lock); ids.erase (id); }
        bool contains (const juce::String& id) { std::lock_guard<std::mutex> l (lock); return ids.count (id) > 0; }
    };
    ActiveIds& activeIds() { static ActiveIds a; return a; }

    juce::String newInstanceId() { return juce::Uuid().toString().substring (0, 12); }

    juce::uint64 fnv (juce::uint64 h, const void* data, size_t size)
    {
        auto* p = static_cast<const juce::uint8*> (data);
        for (size_t i = 0; i < size; ++i)
            h = (h ^ p[i]) * 1099511628211ull;
        return h;
    }
    template <typename T> juce::uint64 fnvValue (juce::uint64 h, T v) { return fnv (h, &v, sizeof (v)); }

    /** Everything in a set XML that is not the set itself (the host / app keeps it). */
    void stripNonSetAttributes (juce::XmlElement& xml)
    {
        for (auto* a : { "instanceId", "savedAt", "cuePair", "clickRoute", "trackOutputs", "push" })
            xml.removeAttribute (a);
        if (auto* mm = xml.getChildByName ("MIDIMAP")) xml.removeChildElement (mm, true);
        if (auto* km = xml.getChildByName ("KEYMAP"))  xml.removeChildElement (km, true);
    }
}

//============================================================================== setup
void LiveLooperProcessor::initBackup()
{
    createdMs = juce::Time::currentTimeMillis();
    instanceId = newInstanceId();
    activeIds().add (instanceId);

    AutoBackup::Source src;
    src.signature = [this] { return computeBackupSignature(); };
    src.busy = [this] {
        for (int t = 0; t < kNumTracks; ++t)
        {
            const auto st = engine.getTrack (t).getState();
            if (st == LoopTrack::State::Recording || st == LoopTrack::State::Overdubbing)
                return true;
        }
        return false;
    };
    src.prepare = [this]() -> AutoBackup::Job {
        auto loops = std::make_shared<std::vector<StoredLoop>>();
        std::shared_ptr<juce::XmlElement> xml (createSetXmlWithoutAudio (*loops).release());
        if (xml == nullptr)
            return {};
        const double bpm = (double) tempo->load();
        return [xml, loops, bpm] (const juce::File& target) { return writeSetContainer (*xml, *loops, bpm, target); };
    };
    autoBackup = std::make_unique<AutoBackup> (std::move (src));
    applyBackupSettings();

    if (wrapperType != wrapperType_Undefined)
    {
        static std::once_flag pruned;
        std::call_once (pruned, [] { backup::pruneStaleInstanceFolders(); });
    }
}

void LiveLooperProcessor::applyBackupSettings()
{
    autoBackup->setEnabled (backup::isEnabled());
    autoBackup->setIntervalMs ((juce::int64) backup::getIntervalMinutes() * 60 * 1000);
}

juce::File LiveLooperProcessor::getBackupFolder() const
{
    if (autoBackup != nullptr && autoBackup->getFolder() != juce::File())
        return autoBackup->getFolder();
    return standaloneSession ? backup::getStandaloneFolder() : backup::getPluginFolder (instanceId);
}

LiveLooperProcessor::BackupNotice LiveLooperProcessor::takeBackupNotice()
{
    auto n = backupNotice;
    backupNotice = {};
    return n;
}

//============================================================================== snapshot
juce::uint64 LiveLooperProcessor::computeBackupSignature()
{
    juce::uint64 h = 1469598103934665603ull;
    {
        const bool keep = saveLoops;
        saveLoops = false;                   // settings only - the loops are fingerprinted below
        juce::MemoryBlock block;
        getStateInformation (block);
        saveLoops = keep;
        if (auto xml = getXmlFromBinary (block.getData(), (int) block.getSize()))
        {
            stripNonSetAttributes (*xml);
            const auto text = xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
            h = fnv (h, text.toRawUTF8(), text.getNumBytesAsUTF8());
        }
    }
    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto& tr = engine.getTrack (t);
        h = fnvValue (h, (int) tr.getState());
        h = fnvValue (h, tr.getLength());
        if (! tr.isEmpty())
        {
            h = fnvValue (h, tr.getSamplesPerPpq());
            h = fnvValue (h, tr.sparseChecksum());
        }
    }
    return h;
}

std::unique_ptr<juce::XmlElement> LiveLooperProcessor::createSetXmlWithoutAudio (std::vector<StoredLoop>& loops)
{
    const bool keep = saveLoops;
    saveLoops = false;
    juce::MemoryBlock block;
    getStateInformation (block);
    saveLoops = keep;
    auto xml = getXmlFromBinary (block.getData(), (int) block.getSize());
    if (xml == nullptr)
        return nullptr;
    xml->setTagName ("LIVELOOPER_SET");
    xml->setAttribute ("saveLoops", keep);
    stripNonSetAttributes (*xml);

    // the loops: copied here (fast, the audio thread keeps playing), encoded on the backup thread
    for (int t = 0; t < kNumTracks; ++t)
    {
        StoredLoop loop;
        if (snapshotTrack (t, loop.audio, &loop.samplesPerPpq))
        {
            loop.track = t;
            loop.sampleRate = engine.getSampleRate();
            loops.push_back (std::move (loop));
        }
    }
    return xml;
}

//============================================================================== timer
void LiveLooperProcessor::beginPluginBackupSession()
{
    if (backupSessionStarted)
        return;
    backupSessionStarted = true;
    const auto folder = backup::getPluginFolder (instanceId);
    backup::writeMarker (folder);
    autoBackup->setFolder (folder);
    autoBackup->setBaseline();          // what the host restored is the reference
}

void LiveLooperProcessor::backupTick (juce::int64 nowMs)
{
    if (autoBackup == nullptr || (wrapperType == wrapperType_Undefined && ! backupsForTests))
        return;
    if (! backupSessionStarted)
    {
        // plug-in: the host has restored the project state by now; the app starts its session itself
        if (wrapperType == wrapperType_Standalone || standaloneSession || nowMs - createdMs < 1500)
            return;
        beginPluginBackupSession();
    }
    autoBackup->tick (nowMs);
}

//============================================================================== plug-in: id in the host state
void LiveLooperProcessor::adoptInstanceId (const juce::String& id, juce::int64 hostSavedMs)
{
    if (id.isEmpty() || id == instanceId || standaloneSession || wrapperType == wrapperType_Standalone)
        return;
    if (activeIds().contains (id))
        return;                          // a copy of a running instance (duplicated track): keep our own id

    const auto oldFolder = backup::getPluginFolder (instanceId);
    activeIds().remove (instanceId);
    instanceId = id;
    activeIds().add (instanceId);

    const auto folder = backup::getPluginFolder (instanceId);
    // crash check: the marker of this instance's last session is still there
    if (backup::hasMarker (folder))
    {
        const auto newest = backup::newest (folder);
        if (newest.existsAsFile() && backup::timeOf (newest).toMilliseconds() > hostSavedMs + 2000)
            backupNotice = { BackupNotice::Kind::Offer, newest, backup::timeOf (newest) };
    }
    if (backupSessionStarted)
    {
        backup::removeMarker (oldFolder);
        if (backup::list (oldFolder).isEmpty())
            oldFolder.deleteRecursively();
        backup::writeMarker (folder);
        autoBackup->setFolder (folder);
        autoBackup->setBaseline();
    }
}

//============================================================================== standalone
void LiveLooperProcessor::startStandaloneSession()
{
    if (standaloneSession)
        return;
    standaloneSession = true;
    activeIds().remove (instanceId);

    // only the first running app owns the standalone folder (crash check, auto load); a second app
    // started at the same time backs up into its own folder
    standaloneLock = std::make_unique<juce::InterProcessLock> ("livelooper-standalone-session");
    const bool primary = standaloneLock->enter (0);
    const auto folder = primary ? backup::getStandaloneFolder()
                                : backup::getRootFolder().getChildFile ("standalone-" + instanceId);

    const bool crashed = primary && backup::hasMarker (folder);
    const auto newest = primary ? backup::newest (folder) : juce::File();
    baselineIsNewest = false;
    if (newest.existsAsFile() && (crashed || backup::getAutoLoad()))
    {
        juce::String error;
        if (loadSetContainer (newest, error))
        {
            baselineIsNewest = true;
            if (crashed)
                backupNotice = { BackupNotice::Kind::Restored, newest, backup::timeOf (newest) };
        }
    }
    backup::writeMarker (folder);
    autoBackup->setFolder (folder);
    autoBackup->setBaseline();
    backupSessionStarted = true;
}

void LiveLooperProcessor::shutdownBackup()
{
    endBackupSession();
    activeIds().remove (instanceId);
    autoBackup.reset();
}

void LiveLooperProcessor::simulateCrashForTests()
{
    autoBackup->waitForWrite();
    backupSessionStarted = false;
    if (standaloneLock != nullptr)
        standaloneLock->exit();
}

void LiveLooperProcessor::startEmptyAfterRestore()
{
    loadEmptySet();
    baselineIsNewest = false;            // quitting writes the empty set, so the next start is empty too
}

void LiveLooperProcessor::endBackupSession()
{
    if (! backupSessionStarted || autoBackup == nullptr)
        return;
    backupSessionStarted = false;
    const auto folder = autoBackup->getFolder();
    if (standaloneSession && (backup::getAutoLoad() || backup::isEnabled()))
        autoBackup->backupNow (backup::getAutoLoad() && ! baselineIsNewest);   // the last session, for "auto load"
    else
        autoBackup->waitForWrite();
    backup::removeMarker (folder);
    if (! standaloneSession && backup::list (folder).isEmpty())
        folder.deleteRecursively();      // a plug-in instance that never needed a backup leaves nothing behind
    if (standaloneLock != nullptr)
        standaloneLock->exit();
}

bool LiveLooperProcessor::restoreBackup (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = i18n::tr ("The backup no longer exists.");
        return false;
    }
    // copy first: the backup of the current set below may rotate the chosen file away
    juce::TemporaryFile copy { juce::String (backup::kExtension) };   // in the temp folder, not among the backups
    if (! file.copyFileTo (copy.getFile()))
    {
        error = i18n::tr ("The backup could not be read.");
        return false;
    }
    const auto source = copy.getFile();
    autoBackup->backupNow();             // keep what is there now (only when it changed)
    if (! loadSetContainer (source, error))
        return false;
    autoBackup->setBaseline();
    baselineIsNewest = false;
    return true;
}
} // namespace livelooper
