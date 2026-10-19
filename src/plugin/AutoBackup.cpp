#include "AutoBackup.h"
#include "AppSettings.h"

namespace livelooper
{
namespace backup
{
//============================================================================== settings
bool isEnabled()                 { return appsettings::getValue ("backup.enabled", "1") != "0"; }
void setEnabled (bool on)        { appsettings::setValue ("backup.enabled", on ? "1" : "0"); }
bool getAutoLoad()               { return appsettings::getValue ("backup.autoLoad", "1") != "0"; }
void setAutoLoad (bool on)       { appsettings::setValue ("backup.autoLoad", on ? "1" : "0"); }

int getIntervalMinutes()
{
    const int m = appsettings::getValue ("backup.intervalMinutes", juce::String (kDefaultIntervalMinutes)).getIntValue();
    for (int c : kIntervalChoices)
        if (c == m)
            return m;
    return kDefaultIntervalMinutes;
}

void setIntervalMinutes (int minutes)
{
    for (int c : kIntervalChoices)
        if (c == minutes)
            appsettings::setValue ("backup.intervalMinutes", juce::String (minutes));
}

//============================================================================== files
juce::File getRootFolder()                { return appsettings::getSettingsFile().getParentDirectory().getChildFile ("Backups"); }
juce::File getStandaloneFolder()          { return getRootFolder().getChildFile ("standalone"); }

juce::File getPluginFolder (const juce::String& instanceId)
{
    return getRootFolder().getChildFile ("plugin-" + juce::File::createLegalFileName (instanceId));
}

juce::String fileNameFor (juce::Time t)
{
    return kPrefix + t.formatted ("%Y%m%d-%H%M%S") + "-" + juce::String (t.getMilliseconds()).paddedLeft ('0', 3) + kExtension;
}

juce::Array<juce::File> list (const juce::File& folder)
{
    juce::Array<juce::File> files;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, juce::String (kPrefix) + "*" + kExtension))
        files.add (f);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) {
        return a.getFileName() > b.getFileName();          // the name sorts by time: newest first
    });
    return files;
}

juce::File newest (const juce::File& folder)
{
    const auto files = list (folder);
    return files.isEmpty() ? juce::File() : files.getFirst();
}

juce::Time timeOf (const juce::File& f)
{
    // backup-YYYYMMDD-HHMMSS-mmm
    const auto s = f.getFileNameWithoutExtension().fromFirstOccurrenceOf (kPrefix, false, false);
    if (s.length() >= 19)
    {
        const int y = s.substring (0, 4).getIntValue(), mo = s.substring (4, 6).getIntValue(), d = s.substring (6, 8).getIntValue();
        const int h = s.substring (9, 11).getIntValue(), mi = s.substring (11, 13).getIntValue(), se = s.substring (13, 15).getIntValue();
        const int ms = s.substring (16, 19).getIntValue();
        if (y > 2000 && mo >= 1 && mo <= 12 && d >= 1)
            return juce::Time (y, mo - 1, d, h, mi, se, ms, true);
    }
    return f.getLastModificationTime();
}

void rotate (const juce::File& folder, int keep)
{
    const auto files = list (folder);
    for (int i = juce::jmax (0, keep); i < files.size(); ++i)
        files[i].deleteFile();
    // temporary files of an interrupted write (crash / power loss)
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.tmp"))
        if (f.getFileName().startsWith (kPrefix) || f.getFileName().startsWith ("."))
            f.deleteFile();
}

bool writeAtomically (const juce::File& target, const std::function<bool (juce::OutputStream&)>& writer)
{
    if (! target.getParentDirectory().createDirectory())
        return false;
    const auto temp = target.getSiblingFile (target.getFileName() + "." + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()) + ".tmp");
    bool ok = false;
    {
        juce::FileOutputStream out (temp);
        ok = out.openedOk() && out.setPosition (0) && out.truncate().wasOk();
        if (ok)
            ok = writer (out);
        if (ok)
        {
            out.flush();
            ok = out.getStatus().wasOk();
        }
    }
    if (ok)
        ok = temp.moveFileTo (target);       // rename in the same folder: the target is either old or complete
    if (! ok)
        temp.deleteFile();
    return ok;
}

void pruneStaleInstanceFolders (int days)
{
    const auto limit = juce::Time::getCurrentTime() - juce::RelativeTime::days (days);
    for (const auto& dir : getRootFolder().findChildFiles (juce::File::findDirectories, false, "plugin-*"))
    {
        if (hasMarker (dir))
            continue;
        juce::Time last = dir.getLastModificationTime();
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false))
            last = juce::jmax (last, f.getLastModificationTime());
        if (last < limit)
            dir.deleteRecursively();
    }
}

void revealFolder (const juce::File& folder)
{
    folder.createDirectory();
    folder.startAsProcess();
}

//============================================================================== crash marker
bool hasMarker (const juce::File& folder)       { return folder.getChildFile (kMarkerName).existsAsFile(); }
void removeMarker (const juce::File& folder)    { folder.getChildFile (kMarkerName).deleteFile(); }

void writeMarker (const juce::File& folder)
{
    folder.createDirectory();
    folder.getChildFile (kMarkerName).replaceWithText ("livelooper session started " + juce::Time::getCurrentTime().toISO8601 (true) + "\n");
}
} // namespace backup

//==============================================================================
AutoBackup::AutoBackup (Source s) : source (std::move (s)) {}

AutoBackup::~AutoBackup()
{
    waitForWrite();
    pool.removeAllJobs (true, 20000);
}

void AutoBackup::setFolder (const juce::File& f)
{
    waitForWrite();
    collectFinished();
    folder = f;
}

void AutoBackup::setBaseline()
{
    if (source.signature)
    {
        lastSig = source.signature();
        haveBaseline = true;
    }
}

void AutoBackup::notifyEvent (juce::int64 nowMs)
{
    eventPending = true;
    eventDueMs = nowMs + kEventDelayMs;      // several events in a row: one backup after the last
}

void AutoBackup::collectFinished()
{
    const int r = result.exchange (-1);
    if (r < 0)
        return;
    if (r == 1)
    {
        lastSig = pendingSig;
        haveBaseline = true;
        lastBackupMs = juce::Time::currentTimeMillis();
        ++numWritten;
        backup::rotate (folder);
    }
}

void AutoBackup::tick (juce::int64 nowMs)
{
    collectFinished();
    if (nextDueMs < 0)
        nextDueMs = nowMs + intervalMs;

    const bool busy = source.busy && source.busy();
    if (wasBusy && ! busy)
        notifyEvent (nowMs);                 // a recording / overdub just ended
    wasBusy = busy;

    if (! enabled || folder == juce::File() || writing.load() || busy)
        return;

    const bool eventDue = eventPending && nowMs >= eventDueMs;
    if (! eventDue && nowMs < nextDueMs)
        return;

    eventPending = false;
    nextDueMs = nowMs + intervalMs;
    const auto sig = source.signature ? source.signature() : (juce::uint64) 0;
    if (! haveBaseline)
    {
        // first look at this session's set: it is the reference (what was just loaded / restored)
        lastSig = sig;
        haveBaseline = true;
        return;
    }
    if (sig == lastSig)
    {
        ++numSkipped;
        return;
    }
    start (sig, nowMs);
}

void AutoBackup::start (juce::uint64 sig, juce::int64)
{
    auto job = source.prepare ? source.prepare() : Job();
    if (! job)
        return;
    pendingSig = sig;
    pendingTarget = folder.getChildFile (backup::fileNameFor (juce::Time::getCurrentTime()));
    writing = true;
    const auto target = pendingTarget;
    pool.addJob ([this, job, target] {
        const bool ok = job (target);
        result = ok ? 1 : 0;
        writing = false;
    });
}

void AutoBackup::waitForWrite (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (writing.load() && juce::Time::getMillisecondCounter() < end)
        juce::Thread::sleep (5);
}

bool AutoBackup::backupNow (bool force)
{
    waitForWrite();
    collectFinished();
    if (folder == juce::File() || ! source.signature || ! source.prepare)
        return false;
    const auto sig = source.signature();
    if (! force && haveBaseline && sig == lastSig)
        return false;
    auto job = source.prepare();
    if (! job)
        return false;
    const auto target = folder.getChildFile (backup::fileNameFor (juce::Time::getCurrentTime()));
    if (! job (target))
        return false;
    lastSig = sig;
    haveBaseline = true;
    lastBackupMs = juce::Time::currentTimeMillis();
    ++numWritten;
    backup::rotate (folder);
    return true;
}
} // namespace livelooper
