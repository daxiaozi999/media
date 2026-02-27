#pragma once

#include <memory>
#include "FFmpeg.h"

namespace media {

    class MediaResampler {
    public:
        MediaResampler(const MediaResampler&) = delete;
        MediaResampler& operator=(const MediaResampler&) = delete;
        MediaResampler(MediaResampler&&) = delete;
        MediaResampler& operator=(MediaResampler&&) = delete;

        MediaResampler();
        ~MediaResampler();

        // Config sws context (srcW, srcH, srcFmt, dstW, dstH, dstFmt, flags) >= 0
        int configSwsContext(int srcW, int srcH, AVPixelFormat srcFmt,
                             int dstW, int dstH, AVPixelFormat dstFmt,
                             int flags = 0);

        // Config swr context (inSampleRate, inChLayout, inSampleFmt, outSampleRate, outChLayout, outSampleFmt) >= 0
        int configSwrContext(int inSampleRate,  const AVChannelLayout& inChLayout,  AVSampleFormat inSampleFmt,
                             int outSampleRate, const AVChannelLayout& outChLayout, AVSampleFormat outSampleFmt);

        // Reset sws context
        void resetSwsContext();
        // Reset swr context
        void resetSwrContext();

        SwsContext* swsContext() const { return swsCtx_.get(); }
        SwrContext* swrContext() const { return swrCtx_.get(); }

    private:
        int getBestFlags(int srcW, int srcH, int dstW, int dstH) const;

    private:
        std::shared_ptr<SwsContext> swsCtx_;
        std::shared_ptr<SwrContext> swrCtx_;
    };

} // namespace media