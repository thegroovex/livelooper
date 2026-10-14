// macOS microphone permission for the standalone app (v1.5).
// macOS (TCC) remembers the answer per app identity; livelooper asks only while the status is
// "not determined". If macOS still asks on every start, the app's code signature changed (see
// docs/en/INSTALLATION.md, "Microphone permission").
#import <AVFoundation/AVFoundation.h>
#include <functional>

namespace livelooper
{
int macMicrophonePermissionStatus()
{
    if (@available (macOS 10.14, *))
    {
        switch ([AVCaptureDevice authorizationStatusForMediaType: AVMediaTypeAudio])
        {
            case AVAuthorizationStatusNotDetermined: return 0;
            case AVAuthorizationStatusAuthorized:    return 1;
            default:                                 return 2;
        }
    }
    return 1;
}

void macRequestMicrophonePermission (std::function<void (bool)> done)
{
    if (@available (macOS 10.14, *))
    {
        auto callback = std::move (done);
        [AVCaptureDevice requestAccessForMediaType: AVMediaTypeAudio
                                 completionHandler: ^(BOOL granted) { if (callback) callback (granted == YES); }];
        return;
    }
    if (done)
        done (true);
}
}
