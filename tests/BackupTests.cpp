// v1.7 auto backup and crash recovery: timing and change detection, atomic writes, rotation, crash
// marker, the processor's backups (same .livelooper format as a shared set, loops included), the
// standalone start (auto load / restore after a crash), the plug-in's restore offer and the notice.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Parameters.h"
#include "plugin/AppSettings.h"
#include "plugin/AutoBackup.h"

using namespace livelooper;
using namespace livelooper::params;

namespace
{
struct BackupFolder
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ll-backup-tests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
    BackupFolder()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~BackupFolder() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); }
};

/** A fake set for AutoBackup: a version number, a busy flag and a counter of written files. */
struct FakeSet
{
    juce::uint64 version = 1;
    bool busy = false;
    int prepared = 0;
    AutoBackup::Source source()
    {
        AutoBackup::Source s;
        s.signature = [this] { return version; };
        s.busy = [this] { return busy; };
        s.prepare = [this]() -> AutoBackup::Job {
            ++prepared;
            const auto v = version;
            return [v] (const juce::File& f) {
                return backup::writeAtomically (f, [v] (juce::OutputStream& o) { return o.writeText ("set " + juce::String ((juce::int64) v), false, false, nullptr); });
            };
        };
        return s;
    }
};

struct BackupRig
{
    LiveLooperProcessor proc { false, false };
    juce::AudioBuffer<float> buffer { 2, 480 };
    juce::MidiBuffer midi;
    BackupRig()
    {
        proc.enableBackupsForTests (true);
        proc.prepareToPlay (48000.0, 480);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    void run (int blocks, float in)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, 480);
            proc.processBlock (buffer, midi);
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                        r->setValueNotifyingHost (0.0f);
        }
    }
    /** Records a one-bar loop on a track (fixed loop length: it ends by itself). */
    void recordBar (int t, float level)
    {
        auto* p = proc.getState().getParameter (trackId (t, kMeasure));
        p->setValueNotifyingHost (p->convertTo0to1 (1.0f));
        proc.pressTrigger (trackId (t, kRecPlay));
        run (260, level);
    }
    LoopTrack::State state (int t) { return (LoopTrack::State) proc.getEngine().getTrackStatus (t).state.load(); }
};
} // namespace

class BackupTests : public juce::UnitTest
{
public:
    BackupTests() : juce::UnitTest ("Auto backup + crash recovery (1.7)", "livelooper") {}

    void runTest() override
    {
        beginTest ("Timing: interval, skipped when unchanged, never while busy, soon after a recording ends");
        {
            BackupFolder tmp;
            FakeSet set;
            AutoBackup ab (set.source());
            ab.setFolder (tmp.dir.getChildFile ("t"));
            ab.setIntervalMs (60000);
            ab.setBaseline();                               // the loaded set is the reference
            juce::int64 now = 1000000;
            ab.tick (now);
            ab.tick (now + 59000);
            expectEquals (set.prepared, 0, "nothing before the interval");
            ab.tick (now + 61000);
            expectEquals (set.prepared, 0, "unchanged: skipped");
            expectEquals (ab.getNumSkipped(), 1);

            set.version = 2;                                // the set changed
            ab.tick (now + 100000);
            expectEquals (set.prepared, 0, "not due yet");
            ab.tick (now + 121500);
            ab.waitForWrite();
            ab.tick (now + 121600);
            expectEquals (set.prepared, 1, "written at the interval");
            expectEquals (ab.getNumWritten(), 1);
            expectEquals (backup::list (ab.getFolder()).size(), 1);

            // recording: nothing is written while busy, a backup follows a few seconds after it ends
            now += 200000;
            set.busy = true;
            set.version = 3;
            ab.tick (now);
            ab.tick (now + 70000);
            expectEquals (set.prepared, 1, "never while a track records / overdubs");
            set.busy = false;
            ab.tick (now + 71000);                          // the interval passed while busy: written right away
            ab.waitForWrite();
            ab.tick (now + 71100);
            expectEquals (set.prepared, 2, "overdue interval: written as soon as the recording ended");

            set.busy = true;                                // a short overdub well inside the interval
            set.version = 4;
            ab.tick (now + 72000);
            set.busy = false;
            ab.tick (now + 80000);                          // ended: event, due 3 s later
            expectEquals (set.prepared, 2);
            ab.tick (now + 80000 + AutoBackup::kEventDelayMs - 100);
            expectEquals (set.prepared, 2, "not before the event delay");
            ab.tick (now + 80000 + AutoBackup::kEventDelayMs + 10);
            ab.waitForWrite();
            ab.tick (now + 84000);
            expectEquals (set.prepared, 3, "written soon after the recording");
            expectEquals (ab.getNumWritten(), 3);
            expectEquals (backup::list (ab.getFolder()).size(), 3);

            // switched off: nothing at all
            ab.setEnabled (false);
            set.version = 5;
            ab.tick (now + 500000);
            expectEquals (set.prepared, 3, "auto backup off");
            ab.setEnabled (true);

            // backupNow: skips when unchanged, force writes
            expect (ab.backupNow(), "changed: written now");
            expect (! ab.backupNow(), "unchanged: skipped");
            expect (ab.backupNow (true), "forced");
        }

        beginTest ("Atomic write: a failed write leaves the old file and no temporary file");
        {
            BackupFolder tmp;
            const auto f = tmp.dir.getChildFile ("a").getChildFile ("backup-x.livelooper");
            expect (backup::writeAtomically (f, [] (juce::OutputStream& o) { return o.writeText ("first", false, false, nullptr); }));
            expectEquals (f.loadFileAsString(), juce::String ("first"));
            expect (! backup::writeAtomically (f, [] (juce::OutputStream& o) { o.writeText ("half", false, false, nullptr); return false; }));
            expectEquals (f.loadFileAsString(), juce::String ("first"), "old file untouched");
            expectEquals (f.getParentDirectory().findChildFiles (juce::File::findFiles, false, "*.tmp").size(), 0, "no temporary file left");
            expect (backup::writeAtomically (f, [] (juce::OutputStream& o) { return o.writeText ("second", false, false, nullptr); }));
            expectEquals (f.loadFileAsString(), juce::String ("second"), "replaced");
            expectEquals (f.getParentDirectory().getNumberOfChildFiles (juce::File::findFiles), 1);
        }

        beginTest ("Rotation: the newest 3 stay, names sort by time, stale temporary files go");
        {
            BackupFolder tmp;
            const auto dir = tmp.dir.getChildFile ("r");
            dir.createDirectory();
            const juce::Time base (2026, 9, 19, 12, 0, 0, 0, true);
            for (int i = 0; i < 6; ++i)
                dir.getChildFile (backup::fileNameFor (base + juce::RelativeTime::seconds (i * 61))).replaceWithText ("x");
            dir.getChildFile ("backup-20261019-120000-000.livelooper.1234.tmp").replaceWithText ("partial");
            dir.getChildFile ("other.txt").replaceWithText ("keep");
            expectEquals (backup::list (dir).size(), 6);
            backup::rotate (dir, backup::kKeep);
            const auto left = backup::list (dir);
            expectEquals (left.size(), 3);
            expect (backup::timeOf (left[0]) == base + juce::RelativeTime::seconds (5 * 61), "newest first, time from the name");
            expect (backup::timeOf (left[2]) == base + juce::RelativeTime::seconds (3 * 61));
            expect (dir.getChildFile ("other.txt").existsAsFile(), "other files untouched");
            expectEquals (dir.findChildFiles (juce::File::findFiles, false, "*.tmp").size(), 0);
            expect (backup::fileNameFor (base).startsWith ("backup-20261019-120000-000"), backup::fileNameFor (base));
        }

        beginTest ("Crash marker and settings (defaults: on, auto load on, 2 minutes)");
        {
            BackupFolder tmp;
            const auto dir = tmp.dir.getChildFile ("m");
            expect (! backup::hasMarker (dir));
            backup::writeMarker (dir);
            expect (backup::hasMarker (dir));
            backup::removeMarker (dir);
            expect (! backup::hasMarker (dir));
            expect (backup::isEnabled());
            expect (backup::getAutoLoad());
            expectEquals (backup::getIntervalMinutes(), 2);
            backup::setIntervalMinutes (5);
            expectEquals (backup::getIntervalMinutes(), 5);
            backup::setIntervalMinutes (7);                 // not a choice: ignored
            expectEquals (backup::getIntervalMinutes(), 5);
            expect (backup::getRootFolder().isAChildOf (tmp.dir), "in the app data folder");
        }

        beginTest ("Processor: backup = full set with loops (.livelooper), change detection, restore");
        {
            BackupFolder tmp;
            juce::File backupFile;
            int loopLength = 0;
            {
                BackupRig r;
                r.proc.beginPluginBackupSession();
                const auto folder = r.proc.getBackupFolder();
                expect (backup::hasMarker (folder), "marker while the instance runs");
                const auto sig0 = r.proc.computeBackupSignature();
                expect (sig0 == r.proc.computeBackupSignature(), "stable while nothing changes");
                r.recordBar (0, 0.25f);
                expect (r.state (0) == LoopTrack::State::Playing);
                loopLength = r.proc.getEngine().getTrack (0).getLength();
                expect (loopLength > 0);
                const auto sig1 = r.proc.computeBackupSignature();
                expect (sig1 != sig0, "a new loop changes the signature");
                r.set (trackId (0, kLevel), 0.3f);
                expect (r.proc.computeBackupSignature() != sig1, "a setting changes the signature");

                auto& ab = r.proc.getAutoBackup();
                expect (ab.backupNow(), "written");
                expect (! ab.backupNow(), "unchanged: skipped");
                backupFile = backup::newest (folder);
                expect (backupFile.existsAsFile());
                juce::ZipFile zip (backupFile);
                expect (zip.getIndexOfFileName ("set.xml") >= 0, "same container as a shared set");
                expect (zip.getIndexOfFileName ("audio/track1.flac") >= 0, "the loop as FLAC");
                // background write path (timer): event after a recording, written off the message thread
                r.recordBar (1, 0.5f);
                const auto t0 = juce::Time::currentTimeMillis();
                r.proc.backupTick (t0);
                r.proc.getAutoBackup().notifyEvent (t0);
                r.proc.backupTick (t0 + AutoBackup::kEventDelayMs + 10);
                ab.waitForWrite();
                r.proc.backupTick (t0 + AutoBackup::kEventDelayMs + 600);
                expectEquals (ab.getNumWritten(), 2, "background backup written");
                expectEquals (backup::list (folder).size(), 2);
                backupFile = backup::newest (folder);
            }
            {
                BackupRig r2;
                juce::String error;
                expect (r2.proc.restoreBackup (backupFile, error), error);
                r2.run (2, 0.0f);
                expectEquals (r2.proc.getEngine().getTrack (0).getLength(), loopLength, "loop restored");
                expect (! r2.proc.getEngine().getTrack (1).isEmpty(), "second loop restored");
                expectWithinAbsoluteError (r2.proc.getState().getParameter (trackId (0, kLevel))->getValue(), 0.3f, 0.01f);
            }
            expect (! backup::hasMarker (backupFile.getParentDirectory()), "clean end: marker removed");
        }

        beginTest ("Plug-in: a newer backup than the host state after a crash is offered, never applied silently");
        {
            BackupFolder tmp;
            juce::MemoryBlock hostState;
            juce::File folder;
            juce::String id;
            {
                BackupRig r;
                r.recordBar (0, 0.25f);
                r.proc.getStateInformation (hostState);     // the project was saved here
                id = r.proc.getInstanceId();
                r.proc.beginPluginBackupSession();
                folder = r.proc.getBackupFolder();
                juce::Thread::sleep (2100);                 // later work, backed up
                r.recordBar (1, 0.5f);
                expect (r.proc.getAutoBackup().backupNow());
            }
            backup::writeMarker (folder);                   // ... and the host crashed (no clean end)
            {
                BackupRig r;
                r.proc.setStateInformation (hostState.getData(), (int) hostState.getSize());
                expectEquals (r.proc.getInstanceId(), id, "the instance finds its id in the project");
                expect (r.proc.hasBackupNotice(), "restore offered");
                const auto& n = r.proc.peekBackupNotice();
                expect (n.kind == LiveLooperProcessor::BackupNotice::Kind::Offer);
                expect (n.file == backup::newest (folder));
                expect (r.proc.getEngine().getTrack (1).isEmpty(), "host state kept (not overridden)");

                // the notice in the editor: Restore backup loads it
                std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc.createEditor());
                auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
                e->updateBackupNotice();
                expect (e->getBackupToast() != nullptr && e->getBackupToast()->isVisible(), "notice shown");
                e->pressBackupToastButton (0);
                r.run (2, 0.0f);
                expect (! r.proc.getEngine().getTrack (1).isEmpty(), "backup restored on request");
            }
            {
                // no crash (no marker): nothing offered
                backup::removeMarker (folder);
                BackupRig r;
                r.proc.setStateInformation (hostState.getData(), (int) hostState.getSize());
                expect (! r.proc.hasBackupNotice());
            }
            {
                // crash, but the project was saved after the newest backup: nothing offered
                juce::Thread::sleep (2100);
                juce::MemoryBlock newer;
                {
                    BackupRig saver;
                    saver.proc.setStateInformation (hostState.getData(), (int) hostState.getSize());
                    saver.proc.takeBackupNotice();
                    saver.proc.getStateInformation (newer);
                }
                backup::writeMarker (folder);
                BackupRig r;
                r.proc.setStateInformation (newer.getData(), (int) newer.getSize());
                expectEquals (r.proc.getInstanceId(), id);
                expect (! r.proc.hasBackupNotice(), "project newer than the backup");
            }
        }

        beginTest ("Standalone: auto load of the last session, restore + notice after a crash, start empty");
        {
            BackupFolder tmp;
            {
                BackupRig r;
                r.proc.startStandaloneSession();
                expect (backup::hasMarker (backup::getStandaloneFolder()));
                expect (! r.proc.hasBackupNotice(), "first start: nothing to restore");
                r.recordBar (0, 0.25f);
            }   // clean quit: the last session is written, the marker removed
            expect (! backup::hasMarker (backup::getStandaloneFolder()), "clean quit");
            expectEquals (backup::list (backup::getStandaloneFolder()).size(), 1, "last session written on quit");
            {
                BackupRig r;
                r.proc.startStandaloneSession();
                r.run (2, 0.0f);
                expect (! r.proc.getEngine().getTrack (0).isEmpty(), "auto load: the loop is back");
                expect (! r.proc.hasBackupNotice(), "normal start: no notice");
                r.recordBar (1, 0.5f);
                expect (r.proc.getAutoBackup().backupNow());
                r.proc.simulateCrashForTests();             // no clean end: the marker stays
            }
            expect (backup::hasMarker (backup::getStandaloneFolder()), "crashed: marker left");
            {
                BackupRig r;
                r.proc.startStandaloneSession();
                r.run (2, 0.0f);
                expect (! r.proc.getEngine().getTrack (1).isEmpty(), "after a crash the newest backup is restored");
                expect (r.proc.hasBackupNotice());
                std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc.createEditor());
                auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
                e->updateBackupNotice();
                expect (e->getBackupToast() != nullptr, "notice shown");
                expect (! r.proc.hasBackupNotice(), "shown once");
                e->pressBackupToastButton (1);              // Start empty
                r.run (2, 0.0f);
                expect (r.proc.getEngine().getTrack (0).isEmpty() && r.proc.getEngine().getTrack (1).isEmpty(), "start empty");
            }
            {
                BackupRig r;
                r.proc.startStandaloneSession();
                r.run (2, 0.0f);
                expect (r.proc.getEngine().getTrack (1).isEmpty(), "empty was kept as the last session");
            }
            {
                backup::setAutoLoad (false);
                BackupRig r;
                r.proc.startStandaloneSession();
                r.run (2, 0.0f);
                expect (r.proc.getEngine().getTrack (2).isEmpty(), "auto load off: the last session is not loaded");
                backup::setAutoLoad (true);
            }
        }

        beginTest ("SETUP > Sets > Auto backup: switches, interval, restore, folder");
        {
            BackupFolder tmp;
            BackupRig r;
            std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            juce::Array<int> toggles;
            const auto menu = e->buildSetupMenu (toggles);
            expect (toggles.contains (2100), "auto backup switch");
            expect (! toggles.contains (2101), "auto load only in the app");
            bool found = false;
            for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
                found = found || it.getItem().text == "Auto backup";
            expect (found, "group in SETUP");
            e->handleSetupResult (2100);
            expect (! backup::isEnabled());
            expect (! r.proc.getAutoBackup().isEnabled(), "applied at once");
            e->handleSetupResult (2100);
            expect (backup::isEnabled());
            e->handleSetupResult (2112);
            expectEquals (backup::getIntervalMinutes(), 5);
            expect (r.proc.getAutoBackup().getIntervalMs() == 5 * 60 * 1000);
            auto panel = e->createSettingsPanel ("Sets");
            expect (panel != nullptr);
        }
    }
};

static BackupTests backupTests;
