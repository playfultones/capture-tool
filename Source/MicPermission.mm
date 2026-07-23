#include "MicPermission.h"

#if defined(__APPLE__)
 #import <AVFoundation/AVFoundation.h>
#endif

namespace macpermissions
{

const char* toString(MicStatus status)
{
    switch (status)
    {
        case MicStatus::notDetermined: return "notDetermined";
        case MicStatus::restricted:    return "restricted";
        case MicStatus::denied:        return "denied";
        case MicStatus::granted:       return "granted";
        case MicStatus::unsupported:   return "unsupported";
    }
    return "unknown";
}

#if defined(__APPLE__)

MicStatus getMicrophoneStatus()
{
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
    {
        case AVAuthorizationStatusNotDetermined: return MicStatus::notDetermined;
        case AVAuthorizationStatusRestricted:    return MicStatus::restricted;
        case AVAuthorizationStatusDenied:        return MicStatus::denied;
        case AVAuthorizationStatusAuthorized:    return MicStatus::granted;
    }
    return MicStatus::unsupported;
}

void requestMicrophoneAccess(std::function<void(bool)> callback)
{
    const MicStatus before = getMicrophoneStatus();
    NSLog(@"[MicPermission] microphone status before request: %s", toString(before));

    // requestAccessForMediaType shows the system prompt only when the status is
    // notDetermined; otherwise it invokes the handler immediately with the
    // existing decision. Safe to call unconditionally.
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL granted) {
        NSLog(@"[MicPermission] microphone access %s", granted ? "GRANTED" : "DENIED");
        if (callback)
            callback(granted == YES);
    }];
}

#else // non-Apple

MicStatus getMicrophoneStatus() { return MicStatus::unsupported; }

void requestMicrophoneAccess(std::function<void(bool)> callback)
{
    if (callback)
        callback(true);
}

#endif

} // namespace macpermissions
