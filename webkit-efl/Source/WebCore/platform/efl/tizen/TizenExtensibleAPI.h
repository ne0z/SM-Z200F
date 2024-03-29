/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef TizenExtensibleAPI_h
#define TizenExtensibleAPI_h

#if ENABLE(TIZEN_EXTENSIBLE_API)

namespace WebCore {

enum _ExtensibleAPI {
    BackgroundMusic,
    CSP,
    EncryptionDatabase,
    FullScreen,
    MediaStreamRecord,
    MediaVolumeControl,
    PrerenderingForRotation,
    RotateCameraView,
    RotationLock,
    SoundMode,
    SupportFullScreen,
    VisibilitySuspend,
    XwindowForFullScreenVideo,
    SupportMultimedia,
    BackgroundVibration,
    BlockMultimediaOnCall,
    IgnoreUltraPowerSaving,
    EnableManualVideoRotation
};
typedef enum _ExtensibleAPI ExtensibleAPI;

class TizenExtensibleAPI {
public:
    static void initializeTizenExtensibleAPI();
    static TizenExtensibleAPI& extensibleAPI();

    bool backgroundMusic() const { return m_backgroundMusic; }
    bool backgroundVibration() const { return m_backgroundVibration; }
    bool blockMultimediaOnCall() const { return m_blockMultimediaOnCall; }
    bool csp() const { return m_csp; }
    bool enableManualVideoRotation() const { return m_enableManualVideoRotation; }
    bool encryptDatabase() const { return m_encryptDatabase; }
    bool fullScreen() const { return m_fullScreen; }
    bool hwVideoOverlayInFullscreen() const;// It consider not only setting value but also other situations.
    bool ignoreUltraPowerSaving() const { return m_ignoreUltraPowerSaving; }
    bool mediaStreamRecord() const { return m_mediaStreamRecord; }
    bool mediaVolumeControl() const { return m_mediaVolumeControl; }
#if ENABLE(TIZEN_PRERENDERING_FOR_ROTATION)
    bool prerenderingForRotation() const { return m_prerenderingForRotation; }
#endif
    bool rotateCameraView() const { return m_rotateCameraView; }
    bool rotationLock() const { return m_rotationLock; }
    bool soundMode() const { return m_soundMode; }
    bool supportFullScreen() const { return m_supportFullScreen; }
    bool visibilitySuspend() const { return m_visibilitySuspend; }
    bool supportMultimedia() const { return m_supportMultimedia; }

    void setTizenExtensibleAPI(ExtensibleAPI, bool enable);
    bool getTizenExtensibleAPI(ExtensibleAPI);

private:
    TizenExtensibleAPI();

    bool m_backgroundMusic;
    bool m_backgroundVibration;
    bool m_blockMultimediaOnCall;
    bool m_csp;
    bool m_encryptDatabase;
    bool m_enableManualVideoRotation;
    bool m_fullScreen;
    bool m_ignoreUltraPowerSaving;
    bool m_mediaStreamRecord;
    bool m_mediaVolumeControl;
    bool m_prerenderingForRotation;
    bool m_rotateCameraView;
    bool m_rotationLock;
    bool m_soundMode;
    bool m_supportFullScreen;
    bool m_visibilitySuspend;
    bool m_xwindowForFullScreenVideo;
    bool m_supportMultimedia;
};

} // namespace WebCore

#endif // ENABLE(TIZEN_EXTENSIBLE_API)

#endif // TizenExtensibleAPI_h
