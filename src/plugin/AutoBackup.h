#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

/**
    v1.7 auto backup and crash recovery.

    The complete set (the same .livelooper container as "Share set": settings, FX, rhythm, loops and the
    own BEATS samples as FLAC) is written in the background into the app data folder:

        <settings folder>/Backups/standalone/          the app
        <settings folder>/Backups/plugin-<instance>/   one folder per plug-in instance (its id travels in
                                                       the host's project state, so a reopened project
                                                       finds its own backups)

    - when: every few minutes (SETUP > Sets > Auto backup, default 2 min) and a few seconds after a
      recording / overdub ends; never while a track records or overdubs, never when nothing changed
      since the last backup (cheap signature: settings XML + per-track length / state / sparse audio sum)
    - how: the message thread only copies the loops (the audio thread keeps playing, it is never locked);
      FLAC encoding, zipping and writing run on a low-priority background thread. Every file is written
      next to its target as a temporary file and then renamed (atomic: a crash never leaves half a
      backup), the newest kKeep backups are kept.
    - crash marker: "session.running" is written when a session starts and removed when it ends
      cleanly. If it is still there on the next start, the previous run crashed.

    The class is independent of the processor: the processor gives it a Source (signature, busy, and
    a snapshot that returns the writer job) - tests drive it with fakes and an explicit clock (tick).
*/
namespace livelooper
{
namespace backup
{
    inline constexpr int kKeep = 3;
    inline constexpr int kIntervalChoices[] = { 1, 2, 5, 10 };      // minutes
    inline constexpr int kDefaultIntervalMinutes = 2;
    inline constexpr const char* kPrefix = "backup-";
    inline constexpr const char* kExtension = ".livelooper";
    inline constexpr const char* kMarkerName = "session.running";

    //-------------------------------------------------------------------------- settings (app level, this computer)
    bool isEnabled();                      // default on
    void setEnabled (bool on);
    bool getAutoLoad();                    // standalone: reload the last session on start, default on
    void setAutoLoad (bool on);
    int  getIntervalMinutes();             // one of kIntervalChoices, default 2
    void setIntervalMinutes (int minutes);

    //-------------------------------------------------------------------------- files
    /** <settings folder>/Backups */
    juce::File getRootFolder();
    juce::File getStandaloneFolder();
    juce::File getPluginFolder (const juce::String& instanceId);
    /** "backup-20261019-024312-123.livelooper" (sorts by time). */
    juce::String fileNameFor (juce::Time t);
    /** The backups in a folder, newest first. */
    juce::Array<juce::File> list (const juce::File& folder);
    juce::File newest (const juce::File& folder);
    /** When a backup was written (from its name, the file time as fallback). */
    juce::Time timeOf (const juce::File& backup);
    /** Keeps the newest `keep` backups, deletes older ones and stale temporary files. */
    void rotate (const juce::File& folder, int keep = kKeep);
    /** Writes through a temporary file in the same folder and renames it onto the target. A failed
        or interrupted write leaves the previous target untouched and no partial file behind. */
    bool writeAtomically (const juce::File& target, const std::function<bool (juce::OutputStream&)>& writer);
    /** Removes plug-in instance folders not used for `days` days (no running marker in them). */
    void pruneStaleInstanceFolders (int days = 30);
    /** Opens the folder in Finder / Explorer (creates it first). */
    void revealFolder (const juce::File& folder);

    //-------------------------------------------------------------------------- crash marker
    bool hasMarker (const juce::File& folder);
    void writeMarker (const juce::File& folder);
    void removeMarker (const juce::File& folder);
} // namespace backup

//==============================================================================
class AutoBackup
{
public:
    /** Runs on the backup thread: writes the snapshot to the given file (atomically), true = written. */
    using Job = std::function<bool (const juce::File& target)>;

    struct Source
    {
        /** Message thread, cheap: changes whenever the set changes. */
        std::function<juce::uint64()> signature;
        /** True while a track records or overdubs (the backup waits until it ends). */
        std::function<bool()> busy;
        /** Message thread: copies what is needed and returns the writer (null = nothing to write). */
        std::function<Job()> prepare;
    };

    explicit AutoBackup (Source source);
    ~AutoBackup();

    void setFolder (const juce::File& f);
    juce::File getFolder() const { return folder; }
    void setEnabled (bool on) noexcept       { enabled = on; }
    bool isEnabled() const noexcept          { return enabled; }
    void setIntervalMs (juce::int64 ms) noexcept { intervalMs = juce::jmax ((juce::int64) 1000, ms); }
    juce::int64 getIntervalMs() const noexcept { return intervalMs; }

    /** The current set is the reference: nothing is written until it changes. */
    void setBaseline();
    /** Forget the reference (the next due check writes a backup). */
    void clearBaseline() noexcept            { haveBaseline = false; }
    bool hasBaseline() const noexcept        { return haveBaseline; }

    /** Message thread, called regularly (the processor's timer, about twice a second). */
    void tick (juce::int64 nowMs);
    /** Something important happened (a recording ended, a set was loaded): back up soon. */
    void notifyEvent (juce::int64 nowMs);
    /** Writes now on the calling thread if the set changed (or force); waits for a running background
        write first. For quitting and before restoring a backup. Returns true if a file was written. */
    bool backupNow (bool force = false);
    /** Waits for a running background write (tests, shutdown). */
    void waitForWrite (int timeoutMs = 20000);

    bool isWriting() const noexcept          { return writing.load(); }
    juce::int64 getLastBackupMs() const noexcept { return lastBackupMs; }
    int getNumWritten() const noexcept       { return numWritten; }
    int getNumSkipped() const noexcept       { return numSkipped; }
    juce::int64 getNextDueMs() const noexcept { return nextDueMs; }

    static constexpr juce::int64 kEventDelayMs = 3000;

private:
    void collectFinished();
    void start (juce::uint64 sig, juce::int64 nowMs);

    Source source;
    juce::File folder;
    bool enabled = true;
    juce::int64 intervalMs = backup::kDefaultIntervalMinutes * 60 * 1000;
    juce::uint64 lastSig = 0, pendingSig = 0;
    bool haveBaseline = false, wasBusy = false, eventPending = false;
    juce::int64 nextDueMs = -1, eventDueMs = 0, lastBackupMs = 0;
    int numWritten = 0, numSkipped = 0;
    std::atomic<bool> writing { false };
    std::atomic<int> result { -1 };      // -1 none, 0 failed, 1 written
    juce::File pendingTarget;
    juce::ThreadPool pool { juce::ThreadPoolOptions().withThreadName ("livelooper backup").withNumberOfThreads (1)
                                                     .withDesiredThreadPriority (juce::Thread::Priority::low) };
};
} // namespace livelooper
