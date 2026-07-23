#if REFCAP_E2E

#include "E2ENativeSnapshot.h"

#import <WebKit/WebKit.h>

static WKWebView* findWKWebView(NSView* view)
{
    if ([view isKindOfClass:[WKWebView class]])
        return (WKWebView*) view;

    for (NSView* sub in view.subviews)
        if (auto* found = findWKWebView(sub))
            return found;

    return nil;
}

void e2eTakeNativeSnapshot(juce::Component& componentInTargetWindow,
                           std::function<void(juce::String, juce::String)> done)
{
    auto* peer = componentInTargetWindow.getPeer();
    if (peer == nullptr)
    {
        done({}, "component has no peer (window not created yet)");
        return;
    }

    auto* rootView = (NSView*) peer->getNativeHandle();
    WKWebView* webView = findWKWebView(rootView);
    if (webView == nil)
    {
        done({}, "no WKWebView found in window view hierarchy");
        return;
    }

    auto doneCopy = std::make_shared<std::function<void(juce::String, juce::String)>>(std::move(done));

    WKSnapshotConfiguration* config = [[[WKSnapshotConfiguration alloc] init] autorelease];
    [webView takeSnapshotWithConfiguration:config
                         completionHandler:^(NSImage* image, NSError* error) {
                             if (image == nil)
                             {
                                 (*doneCopy)({}, error != nil
                                                     ? juce::String(error.localizedDescription.UTF8String)
                                                     : juce::String("snapshot returned no image"));
                                 return;
                             }

                             NSData* tiff = image.TIFFRepresentation;
                             if (tiff == nil)
                             {
                                 (*doneCopy)({}, "failed to get TIFF representation");
                                 return;
                             }

                             NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:tiff];
                             NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG
                                                             properties:@{}];
                             if (png == nil)
                             {
                                 (*doneCopy)({}, "failed to encode PNG");
                                 return;
                             }

                             (*doneCopy)(juce::Base64::toBase64(png.bytes, png.length), {});
                         }];
}

#endif
