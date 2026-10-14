#include "Updates.h"

#include <juce_cryptography/juce_cryptography.h>

#if JUCE_WINDOWS
 #include <process.h>
#else
 #include <unistd.h>
#endif

namespace livelooper::updates
{
namespace
{
    struct SemVer
    {
        int major = 0, minor = 0, patch = 0;
        juce::String pre;   // empty = release
    };

    SemVer parseVersion (juce::String s)
    {
        s = s.trim();
        if (s.startsWithIgnoreCase ("v"))
            s = s.substring (1);
        SemVer v;
        const auto core = s.upToFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("+", false, false);
        if (s.containsChar ('-'))
            v.pre = s.fromFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("+", false, false);
        const auto parts = juce::StringArray::fromTokens (core, ".", "");
        v.major = parts[0].getIntValue();
        v.minor = parts[1].getIntValue();
        v.patch = parts[2].getIntValue();
        return v;
    }

    int comparePre (const juce::String& a, const juce::String& b)
    {
        if (a == b) return 0;
        if (a.isEmpty()) return 1;     // release > pre-release
        if (b.isEmpty()) return -1;
        const auto pa = juce::StringArray::fromTokens (a, ".", ""), pb = juce::StringArray::fromTokens (b, ".", "");
        for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
        {
            if (i >= pa.size()) return -1;
            if (i >= pb.size()) return 1;
            const bool na = pa[i].containsOnly ("0123456789"), nb = pb[i].containsOnly ("0123456789");
            if (na && nb)
            {
                const int x = pa[i].getIntValue(), y = pb[i].getIntValue();
                if (x != y) return x < y ? -1 : 1;
            }
            else if (na != nb)
                return na ? -1 : 1;
            else if (pa[i] != pb[i])
                return pa[i] < pb[i] ? -1 : 1;
        }
        return 0;
    }

    juce::URL::InputStreamOptions requestOptions (int* status)
    {
        return juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders ("User-Agent: livelooper-updater\r\nAccept: application/vnd.github+json")
            .withConnectionTimeoutMs (15000)
            .withNumRedirectsToFollow (8)
            .withStatusCode (status);
    }

    juce::File findInside (const juce::File& folder, const juce::String& name)
    {
        if (folder.getChildFile (name).exists())
            return folder.getChildFile (name);
        for (const auto& f : folder.findChildFiles (juce::File::findDirectories, true))
            if (f.getFileName() == name)
                return f;
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, name))
            return f;
        return {};
    }

    bool runAndWait (const juce::StringArray& args, juce::String& error)
    {
        juce::ChildProcess p;
        if (! p.start (args))
        {
            error = "Could not start " + args[0];
            return false;
        }
        const auto out = p.readAllProcessOutput();
        p.waitForProcessToFinish (120000);
        if (p.getExitCode() != 0)
        {
            error = out.trim().isNotEmpty() ? out.trim() : ("Failed: " + args[0]);
            return false;
        }
        return true;
    }

    bool copyBundle (const juce::File& src, const juce::File& dst, juce::String& error)
    {
       #if JUCE_MAC
        dst.deleteRecursively();
        dst.getParentDirectory().createDirectory();
        return runAndWait ({ "/usr/bin/ditto", src.getFullPathName(), dst.getFullPathName() }, error);
       #else
        dst.deleteRecursively();
        dst.getParentDirectory().createDirectory();
        const bool ok = src.isDirectory() ? src.copyDirectoryTo (dst) : src.copyFileTo (dst);
        if (! ok)
            error = "Could not copy to " + dst.getFullPathName();
        return ok;
       #endif
    }
}

//==============================================================================
int compareVersions (const juce::String& a, const juce::String& b)
{
    const auto x = parseVersion (a), y = parseVersion (b);
    if (x.major != y.major) return x.major < y.major ? -1 : 1;
    if (x.minor != y.minor) return x.minor < y.minor ? -1 : 1;
    if (x.patch != y.patch) return x.patch < y.patch ? -1 : 1;
    return comparePre (x.pre, y.pre);
}

juce::String platformName()
{
   #if JUCE_MAC
    return "macOS";
   #elif JUCE_WINDOWS
    return "Windows";
   #else
    return "Linux";
   #endif
}

const Asset* ReleaseInfo::platformAsset() const
{
    const auto p = platformName();
    for (auto& a : assets)
        if (a.name.containsIgnoreCase (p) && a.name.endsWithIgnoreCase (".zip"))
            return &a;
    return nullptr;
}

ReleaseInfo parseRelease (const juce::String& json)
{
    ReleaseInfo r;
    const auto v = juce::JSON::parse (json);
    if (! v.isObject() || ! v.hasProperty ("tag_name"))
    {
        r.error = v.hasProperty ("message") ? v["message"].toString() : juce::String ("Unexpected answer from GitHub");
        return r;
    }
    r.tag = v["tag_name"].toString();
    r.version = r.tag.startsWithIgnoreCase ("v") ? r.tag.substring (1) : r.tag;
    r.name = v["name"].toString();
    r.notes = v["body"].toString();
    r.pageUrl = v["html_url"].toString();
    if (auto* assets = v["assets"].getArray())
        for (const auto& a : *assets)
        {
            Asset as;
            as.name = a["name"].toString();
            as.url = a["browser_download_url"].toString();
            as.digest = a["digest"].toString();
            as.size = (juce::int64) a["size"];
            r.assets.add (as);
        }
    r.ok = r.version.isNotEmpty();
    return r;
}

//============================================================================== Checker
Checker::Checker() : juce::Thread ("livelooper update check") {}

Checker::~Checker()
{
    alive->store (false);
    stopThread (16000);
}

void Checker::check (Callback cb)
{
    if (isThreadRunning())
        return;
    callback = std::move (cb);
    startThread();
}

void Checker::run()
{
    ReleaseInfo info;
    int status = 0;
    if (auto stream = juce::URL (kLatestReleaseUrl).createInputStream (requestOptions (&status)))
    {
        const auto body = stream->readEntireStreamAsString();
        info = parseRelease (body);
        if (status == 404)
            info.error = "No release published yet";
    }
    else
        info.error = "No connection to GitHub (offline, or not available in this build)";

    auto cb = callback;
    auto flag = alive;
    juce::MessageManager::callAsync ([cb, flag, info] {
        if (flag->load() && cb)
            cb (info);
    });
}

//============================================================================== Downloader
Downloader::Downloader() : juce::Thread ("livelooper update download") {}

Downloader::~Downloader()
{
    alive->store (false);
    stopThread (20000);
}

juce::File Downloader::targetFolder (const juce::String& v)
{
    auto base = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
    if (! base.isDirectory())
        base = juce::File::getSpecialLocation (juce::File::tempDirectory);
    return base.getChildFile ("livelooper-" + v);
}

void Downloader::start (const Asset& a, const juce::String& v, Callback done, Progress progress)
{
    if (isThreadRunning())
        return;
    asset = a;
    version = v;
    callback = std::move (done);
    progressCallback = std::move (progress);
    startThread();
}

bool verifyDownload (const juce::File& file, const juce::String& digest, juce::String& error)
{
    if (! file.existsAsFile() || file.getSize() < 1024)
    {
        error = "The download is incomplete";
        return false;
    }
    if (digest.startsWithIgnoreCase ("sha256:"))
    {
        juce::FileInputStream in (file);
        const auto hex = juce::SHA256 (in).toHexString();
        if (! hex.equalsIgnoreCase (digest.fromFirstOccurrenceOf (":", false, false).trim()))
        {
            error = "Checksum mismatch - the download was damaged or changed";
            return false;
        }
    }
    juce::ZipFile zip (file);
    if (zip.getNumEntries() == 0)
    {
        error = "The download is not a valid ZIP file";
        return false;
    }
    return true;
}

void Downloader::run()
{
    Result r;
    const auto folder = targetFolder (version);
    const auto zipFile = folder.getSiblingFile ("livelooper-" + version + "-" + platformName() + ".zip");
    zipFile.deleteFile();

    int status = 0;
    bool ok = false;
    if (auto in = juce::URL (asset.url).createInputStream (requestOptions (&status)))
    {
        juce::FileOutputStream out (zipFile);
        if (out.openedOk())
        {
            const auto total = juce::jmax<juce::int64> (1, asset.size > 0 ? asset.size : in->getTotalLength());
            juce::int64 done = 0;
            juce::HeapBlock<char> buffer (65536);
            float lastReported = -1.0f;
            while (! in->isExhausted() && ! threadShouldExit())
            {
                const auto n = in->read (buffer, 65536);
                if (n <= 0)
                    break;
                out.write (buffer, (size_t) n);
                done += n;
                const float p = (float) juce::jlimit (0.0, 1.0, (double) done / (double) total);
                if (progressCallback && p - lastReported >= 0.02f)
                {
                    lastReported = p;
                    auto cb = progressCallback;
                    auto flag = alive;
                    juce::MessageManager::callAsync ([cb, flag, p] { if (flag->load()) cb (p); });
                }
            }
            out.flush();
            ok = ! threadShouldExit() && out.getStatus().wasOk();
        }
    }
    if (! ok)
        r.error = "Download failed";
    else if (verifyDownload (zipFile, asset.digest, r.error))
    {
        folder.deleteRecursively();
        folder.createDirectory();
       #if JUCE_MAC
        ok = runAndWait ({ "/usr/bin/ditto", "-x", "-k", zipFile.getFullPathName(), folder.getFullPathName() }, r.error);
       #else
        juce::ZipFile zip (zipFile);
        ok = zip.uncompressTo (folder, true).wasOk();
        if (! ok)
            r.error = "Could not unpack the download";
       #endif
        if (ok)
        {
            r.ok = true;
            r.folder = folder;
           #if JUCE_MAC
            r.app = findInside (folder, "livelooper.app");
            r.au = findInside (folder, "livelooper.component");
           #else
            r.app = findInside (folder, "livelooper.exe");
           #endif
            r.vst3 = findInside (folder, "livelooper.vst3");
        }
    }

    auto cb = callback;
    auto flag = alive;
    juce::MessageManager::callAsync ([cb, flag, r] { if (flag->load() && cb) cb (r); });
}

//============================================================================== install
juce::File currentAppLocation()
{
    auto exe = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
   #if JUCE_MAC
    // .../livelooper.app (currentApplicationFile is the bundle on macOS)
    if (! exe.hasFileExtension ("app"))
        for (auto f = exe; f.getFullPathName().length() > 1; f = f.getParentDirectory())
            if (f.hasFileExtension ("app"))
                return f;
   #endif
    return exe;
}

bool scheduleAppReplacement (const juce::File& newApp, bool restart, juce::String& error)
{
   #if JUCE_MAC || JUCE_WINDOWS
    const auto current = currentAppLocation();
    if (! newApp.exists() || ! current.exists())
    {
        error = "The new app was not found in the download";
        return false;
    }
    if (! current.getParentDirectory().hasWriteAccess())
    {
        error = "No permission to replace the app in " + current.getParentDirectory().getFullPathName();
        return false;
    }
   #if JUCE_WINDOWS
    const auto pid = juce::String ((juce::int64) _getpid());
   #else
    const auto pid = juce::String ((juce::int64) getpid());
   #endif
    const auto scriptDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
   #if JUCE_MAC
    const auto script = scriptDir.getChildFile ("livelooper-update.sh");
    juce::String s;
    s << "#!/bin/sh\n"
      << "while kill -0 " << pid << " 2>/dev/null; do sleep 0.5; done\n"
      << "OLD=" << current.getFullPathName().quoted ('\'') << "\n"
      << "NEW=" << newApp.getFullPathName().quoted ('\'') << "\n"
      << "rm -rf \"$OLD.previous\"\n"
      << "mv \"$OLD\" \"$OLD.previous\" && /usr/bin/ditto \"$NEW\" \"$OLD\" && rm -rf \"$OLD.previous\" || mv \"$OLD.previous\" \"$OLD\"\n"
      << "/usr/bin/xattr -dr com.apple.quarantine \"$OLD\" 2>/dev/null\n"
      << (restart ? "open \"$OLD\"\n" : "");
    if (! script.replaceWithText (s) || ! script.setExecutePermission (true))
    {
        error = "Could not write the update helper";
        return false;
    }
    const auto cmd = "nohup /bin/sh " + script.getFullPathName().quoted() + " >/dev/null 2>&1 &";
    if (std::system (cmd.toRawUTF8()) != 0)
   #else
    const auto script = scriptDir.getChildFile ("livelooper-update.cmd");
    juce::String s;
    s << "@echo off\r\n"
      << ":wait\r\n"
      << "tasklist /FI \"PID eq " << pid << "\" | find \"" << pid << "\" >nul && (timeout /t 1 /nobreak >nul & goto wait)\r\n"
      << "copy /Y " << newApp.getFullPathName().quoted() << " " << current.getFullPathName().quoted() << " >nul\r\n"
      << (restart ? "start \"\" " + current.getFullPathName().quoted() + "\r\n" : juce::String())
      << "del \"%~f0\"\r\n";
    if (! script.replaceWithText (s))
    {
        error = "Could not write the update helper";
        return false;
    }
    const auto cmd = "start \"\" /min cmd /c " + script.getFullPathName().quoted();
    if (std::system (cmd.toRawUTF8()) != 0)
   #endif
    {
        error = "Could not start the update helper";
        return false;
    }
    return true;
   #else
    juce::ignoreUnused (newApp, restart);
    error = "Automatic installation is not available on this platform";
    return false;
   #endif
}

juce::StringArray installPlugins (const Downloader::Result& r, juce::String& error)
{
    juce::StringArray installed;
   #if JUCE_MAC
    const auto base = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Plug-Ins");
    const auto vst3Dir = base.getChildFile ("VST3"), auDir = base.getChildFile ("Components");
   #elif JUCE_WINDOWS
    // the per-user VST3 folder (no administrator rights needed)
    const auto vst3Dir = juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("Programs\\Common\\VST3");
    const juce::File auDir;
   #else
    const auto vst3Dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3");
    const juce::File auDir;
   #endif
    if (r.vst3.exists())
    {
        const auto dst = vst3Dir.getChildFile ("livelooper.vst3");
        if (! copyBundle (r.vst3, dst, error))
            return {};
        installed.add (dst.getFullPathName());
    }
    if (r.au.exists() && auDir != juce::File())
    {
        const auto dst = auDir.getChildFile ("livelooper.component");
        if (! copyBundle (r.au, dst, error))
            return {};
        installed.add (dst.getFullPathName());
    }
    if (installed.isEmpty())
        error = "No plug-ins found in the download";
    return installed;
}
} // namespace livelooper::updates
