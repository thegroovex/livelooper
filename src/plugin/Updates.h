#pragma once

#include <juce_events/juce_events.h>

/**
    v1.5 update check against the GitHub releases of livelooper.

    Everything that touches the network runs on a background thread; results are delivered
    on the message thread. What "install" can honestly do:

    - standalone app: the downloaded release is verified, unpacked and staged; it replaces the
      app when livelooper quits (a small helper script waits until the app has exited, swaps the
      app, and - when asked for - starts the new version). Nothing is replaced while it runs.
    - plug-ins: a plug-in is loaded by your DAW, so it is never replaced while it runs. The
      release is downloaded, verified and unpacked into a folder that is opened for you; the
      plug-ins can be copied into the user plug-in folders with one click once every DAW that
      uses livelooper is closed.
*/
namespace livelooper::updates
{
inline constexpr const char* kLatestReleaseUrl = "https://api.github.com/repos/thegroovex/livelooper/releases/latest";
inline constexpr const char* kReleasesPage = "https://github.com/thegroovex/livelooper/releases";

/** Semantic version compare ("v1.5.0" vs "1.4.2", "1.5.0-beta.1" < "1.5.0"): -1, 0 or 1. */
int compareVersions (const juce::String& a, const juce::String& b);

struct Asset
{
    juce::String name, url, digest;   // digest: "sha256:<hex>" when GitHub provides it
    juce::int64 size = 0;
};

struct ReleaseInfo
{
    bool ok = false;
    juce::String error;               // English, for tr()
    juce::String version, tag, name, notes, pageUrl;
    juce::Array<Asset> assets;

    /** The ZIP for this platform (livelooper-macOS.zip / livelooper-Windows.zip), or nullptr. */
    const Asset* platformAsset() const;
};

/** Parses the JSON of the GitHub "latest release" API (public for tests). */
ReleaseInfo parseRelease (const juce::String& json);

/** Name part of the release ZIP for this platform ("macOS", "Windows", "Linux"). */
juce::String platformName();

/** Checks for the latest release in the background; the callback runs on the message thread. */
class Checker : private juce::Thread
{
public:
    using Callback = std::function<void (const ReleaseInfo&)>;
    Checker();
    ~Checker() override;
    void check (Callback cb);
    bool isChecking() const { return isThreadRunning(); }

private:
    void run() override;
    Callback callback;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

/** Downloads, verifies and unpacks a release asset in the background. */
class Downloader : private juce::Thread
{
public:
    struct Result
    {
        bool ok = false;
        juce::String error;           // English
        juce::File folder;            // unpacked release
        juce::File app;               // livelooper.app / livelooper.exe inside it (may not exist)
        juce::File vst3, au;          // plug-in bundles inside it
    };
    using Callback = std::function<void (const Result&)>;
    using Progress = std::function<void (float)>;

    Downloader();
    ~Downloader() override;
    void start (const Asset& asset, const juce::String& version, Callback done, Progress progress = {});
    bool isBusy() const { return isThreadRunning(); }

    /** Where releases are downloaded to (Downloads/livelooper-<version>). */
    static juce::File targetFolder (const juce::String& version);

private:
    void run() override;
    Asset asset;
    juce::String version;
    Callback callback;
    Progress progressCallback;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

/** Verifies a downloaded file against a "sha256:<hex>" digest (true when no digest is given
    and the file is a readable ZIP). */
bool verifyDownload (const juce::File& file, const juce::String& digest, juce::String& error);

/** Standalone app: replaces the running app with newApp once this process has exited
    (helper script), optionally restarting it. Returns false if this is not possible here. */
bool scheduleAppReplacement (const juce::File& newApp, bool restart, juce::String& error);
/** The bundle / exe of the running standalone app. */
juce::File currentAppLocation();

/** Copies the plug-ins of an unpacked release to the user plug-in folders (call only when no
    DAW has livelooper loaded). Returns a list of the installed paths, error on failure. */
juce::StringArray installPlugins (const Downloader::Result& r, juce::String& error);
} // namespace livelooper::updates
