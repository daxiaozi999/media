#include "MediaResampler.h"

namespace media {

    MediaResampler::MediaResampler()
        : swsCtx_(nullptr)
        , swrCtx_(nullptr) {
    }

    MediaResampler::~MediaResampler() {
        resetSwsContext();
        resetSwrContext();
    }

    int MediaResampler::configSwsContext(int srcW, int srcH, AVPixelFormat srcFmt,
                                         int dstW, int dstH, AVPixelFormat dstFmt,
                                         int flags) {
        if (srcW <= 0 || srcH <= 0 || srcFmt == AV_PIX_FMT_NONE ||
            dstW <= 0 || dstH <= 0 || dstFmt == AV_PIX_FMT_NONE) {
            return AVERROR(EINVAL);
        }

        resetSwsContext();

        if (flags == 0) {
            flags = getBestFlags(srcW, srcH, dstW, dstH);
        }

        SwsContext* ctx = sws_getContext(srcW, srcH, srcFmt,
                                         dstW, dstH, dstFmt,
                                         flags, nullptr, nullptr, nullptr);
        if (!ctx) {
            return AVERROR(EINVAL);
        }

        swsCtx_ = std::shared_ptr<SwsContext>(ctx, [](SwsContext* p) {
            if (p) {
                sws_freeContext(p);
            }
            });

        return 0;
    }

    int MediaResampler::configSwrContext(int inSampleRate, const AVChannelLayout& inChLayout, AVSampleFormat inSampleFmt,
                                         int outSampleRate, const AVChannelLayout& outChLayout, AVSampleFormat outSampleFmt) {
        if (inSampleRate  <= 0 || inSampleFmt  == AV_SAMPLE_FMT_NONE ||
            outSampleRate <= 0 || outSampleFmt == AV_SAMPLE_FMT_NONE) {
            return AVERROR(EINVAL);
        }

        resetSwrContext();

        SwrContext* ctx = swr_alloc();
        if (!ctx) {
            return AVERROR(ENOMEM);
        }

        int ret = 0;
        ret |= av_opt_set_int(ctx, "in_sample_rate", inSampleRate, 0);
        ret |= av_opt_set_chlayout(ctx, "in_chlayout", &inChLayout, 0);
        ret |= av_opt_set_sample_fmt(ctx, "in_sample_fmt", inSampleFmt, 0);
        ret |= av_opt_set_int(ctx, "out_sample_rate", outSampleRate, 0);
        ret |= av_opt_set_chlayout(ctx, "out_chlayout", &outChLayout, 0);
        ret |= av_opt_set_sample_fmt(ctx, "out_sample_fmt", outSampleFmt, 0);
        
        if (ret < 0) {
            swr_free(&ctx);
            return ret;
        }

        ret = swr_init(ctx);
        if (ret < 0) {
            swr_free(&ctx);
            return ret;
        }

        swrCtx_ = std::shared_ptr<SwrContext>(ctx, [](SwrContext* p) {
            if (p) {
                swr_free(&p);
            }
            });

        return 0;
    }

    void MediaResampler::resetSwsContext() {
        swsCtx_.reset();
    }

    void MediaResampler::resetSwrContext() {
        swrCtx_.reset();
    }

    int MediaResampler::getBestFlags(int srcW, int srcH, int dstW, int dstH) const {
        if (dstW > srcW || dstH > srcH) {
            return SWS_LANCZOS;
        }
        else if (dstW * 2 < srcW || dstH * 2 < srcH) {
            return SWS_BICUBIC;
        }
        else {
            return SWS_BILINEAR;
        }
    }

} // namespace media