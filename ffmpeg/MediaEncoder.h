#pragma once

#include <string>
#include <memory>
#include "FFmpeg.h"

namespace media {

    class MediaEncoder {
    public:
        MediaEncoder(const MediaEncoder&) = delete;
        MediaEncoder& operator=(const MediaEncoder&) = delete;
        MediaEncoder(MediaEncoder&&) = delete;
        MediaEncoder& operator=(MediaEncoder&&) = delete;

        MediaEncoder();
        ~MediaEncoder();

        // Open video encoder (codecid, width, height, bitrate, timebase, framerate, pixfmt, useHW, threads, opt) >= 0
        int openVideoEncoder(AVCodecID codecid,
                             int width,
                             int height,
                             int64_t bitrate,
                             AVRational timebase,
                             AVRational framerate,
                             AVPixelFormat pixfmt,
                             bool useHW = false,
                             unsigned int threads = 0,
                             AVDictionary* opt = nullptr);

        // Open audio encoder (codecid, framesize, samplerate, bitrate, timebase, chlayout, samplefmt, threads, opt) >= 0
        int openAudioEncoder(AVCodecID codecid,
                             int framesize,
                             int samplerate,
                             int64_t bitrate,
                             AVRational timebase,
                             const AVChannelLayout& chlayout,
                             AVSampleFormat samplefmt,
                             unsigned int threads = 0,
                             AVDictionary* opt = nullptr);

        // Flush video encoder
        void flushVideoEncoder();
        // Flush audio encoder
        void flushAudioEncoder();

        // Reset video encoder
        void resetVideoEncoder();
        // Reset audio encoder
        void resetAudioEncoder();

        AVCodecContext* videoEncoder() const { return videoEncoder_.get(); }
        AVCodecContext* audioEncoder() const { return audioEncoder_.get(); }

    private:
        std::string getHWEncoderName(AVCodecID codecid, AVHWDeviceType type) const;

    private:
        const AVCodec* videoCodec_;
        const AVCodec* audioCodec_;
        std::shared_ptr<AVCodecContext> videoEncoder_;
        std::shared_ptr<AVCodecContext> audioEncoder_;
    };

} // namespace media