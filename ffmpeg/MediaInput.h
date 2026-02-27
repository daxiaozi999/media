#pragma once

#include <string>
#include <memory>
#include "FFmpeg.h"

namespace media {

    struct VideoParams {
        // Input video stream index
        int index = -1;
        int width = 0;
        int height = 0;
        int64_t bitrate = 0;
        AVRational framerate = { 0, 0 };
        AVRational timebase = { 0, 0 };
        AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
    };

    struct AudioParams {
        // Input audio stream index
        int index = -1;
        int framesize = 0;
        int samplerate = 0;
        int64_t bitrate = 0;
        AVRational timebase = { 0, 0 };
        AVChannelLayout chlayout = {};
        AVSampleFormat samplefmt = AV_SAMPLE_FMT_NONE;

        AudioParams() {
            av_channel_layout_default(&chlayout, 2);
        }

        ~AudioParams() {
            av_channel_layout_uninit(&chlayout);
        }
    };

    class MediaInput {
    public:
        MediaInput(const MediaInput&) = delete;
        MediaInput& operator=(const MediaInput&) = delete;
        MediaInput(MediaInput&&) = delete;
        MediaInput& operator=(MediaInput&&) = delete;

        MediaInput();
        ~MediaInput();

        // Open file stream (filepath) >= 0
        int openFileStream(const std::string& url);
        // Open device stream (camera+/microphone) >= 0
        int openDeviceStream(const std::string& url);
        // Open desktop stream (desktop, opt) >= 0
        int openDesktopStream(const std::string& url = "", AVDictionary* opt = nullptr);
        // Open network stream (RTSP/RTMP/HTTP/..., opt) >= 0
        int openNetworkStream(const std::string& url, AVDictionary* opt = nullptr);

        // Reset current stream
        void reset();

        bool hasVideoStream() const { return videoParams_.index != -1; }
        bool hasAudioStream() const { return audioParams_.index != -1; }

        int64_t duration()               const { return duration_; }
        const VideoParams& videoParams() const { return videoParams_; }
        const AudioParams& audioParams() const { return audioParams_; }
        AVFormatContext* inputContext()  const { return inputCtx_.get(); }

    private:
        void extractParams();

    private:
        int64_t duration_;
        VideoParams videoParams_;
        AudioParams audioParams_;

        const AVInputFormat* inputFmt_;
        std::shared_ptr<AVFormatContext> inputCtx_;
    };

} // namespace media