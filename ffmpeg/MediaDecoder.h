#pragma once

#include <memory>
#include "FFmpeg.h"

namespace media {

    class MediaDecoder {
    public:
        MediaDecoder(const MediaDecoder&) = delete;
        MediaDecoder& operator=(const MediaDecoder&) = delete;
        MediaDecoder(MediaDecoder&&) = delete;
        MediaDecoder& operator=(MediaDecoder&&) = delete;

        MediaDecoder();
        ~MediaDecoder();

        // Open video decoder (ctx, useHW, threads) >= 0
        int openVideoDecoder(AVFormatContext* ctx, bool useHW = false, unsigned int threads = 0);
        // Open audio decoder (ctx, threads) >= 0
        int openAudioDecoder(AVFormatContext* ctx, unsigned int threads = 0);

        // Flush video decoder
        void flushVideoDecoder();
        // Flush audio decoder
        void flushAudioDecoder();

        // Reset video decoder
        void resetVideoDecoder();
        // Reset audio decoder
        void resetAudioDecoder();

        AVCodecContext* videoDecoder() const { return videoDecoder_.get(); }
        AVCodecContext* audioDecoder() const { return audioDecoder_.get(); }

    private:
        AVPixelFormat findHWFormat(const AVCodec* codec, AVHWDeviceType type);

    private:
        const AVCodec* videoCodec_;
        const AVCodec* audioCodec_;
        std::shared_ptr<AVCodecContext> videoDecoder_;
        std::shared_ptr<AVCodecContext> audioDecoder_;
    };

} // namespace media