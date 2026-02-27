#include "MediaDecoder.h"

namespace media {

    static AVPixelFormat get_hw_format(AVCodecContext* ctx, const AVPixelFormat* fmt) {
        AVPixelFormat* target_hw_format = static_cast<AVPixelFormat*>(ctx->opaque);
        if (!target_hw_format) {
            return AV_PIX_FMT_NONE;
        }

        const AVPixelFormat* p = fmt;
        for (; *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == *target_hw_format) {
                return *p;
            }
        }

        return AV_PIX_FMT_NONE;
    }

    MediaDecoder::MediaDecoder()
        : videoCodec_(nullptr)
        , audioCodec_(nullptr)
        , videoDecoder_(nullptr)
        , audioDecoder_(nullptr) {
    }

    MediaDecoder::~MediaDecoder() {
        resetVideoDecoder();
        resetAudioDecoder();
    }

    int MediaDecoder::openVideoDecoder(AVFormatContext* ctx, bool useHW, unsigned int threads) {
        if (!ctx || !ctx->streams) {
            return AVERROR(EINVAL);
        }

        int index = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (index < 0) {
            return AVERROR(EINVAL);
        }

        AVStream* s = ctx->streams[index];
        if (!s || !s->codecpar) {
            return AVERROR(EINVAL);
        }

        AVCodecParameters* p = s->codecpar;

        resetVideoDecoder();

        videoCodec_ = avcodec_find_decoder(p->codec_id);
        if (!videoCodec_) {
            return AVERROR_DECODER_NOT_FOUND;
        }

        int ret = 0;
        AVPixelFormat hw_format = AV_PIX_FMT_NONE;
        AVBufferRef* hw_device_ctx = nullptr;

        if (useHW) {
            for (int i = 0; i < (sizeof(HW_Types) / sizeof(HW_Types[0])); ++i) {
                AVPixelFormat format = findHWFormat(videoCodec_, HW_Types[i]);
                if (format == AV_PIX_FMT_NONE) {
                    continue;
                }

                ret = av_hwdevice_ctx_create(&hw_device_ctx, HW_Types[i], nullptr, nullptr, 0);
                if (ret >= 0) {
                    hw_format = format;
                    break;
                }
            }
        }

        AVCodecContext* decoder = avcodec_alloc_context3(videoCodec_);
        if (!decoder) {
            if (hw_device_ctx) {
                av_buffer_unref(&hw_device_ctx);
            }
            return AVERROR(ENOMEM);
        }

        ret = avcodec_parameters_to_context(decoder, p);
        if (ret < 0) {
            if (hw_device_ctx) {
                av_buffer_unref(&hw_device_ctx);
            }
            avcodec_free_context(&decoder);
            return ret;
        }

        if (hw_device_ctx) {
            decoder->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            decoder->opaque = new AVPixelFormat(hw_format);
            decoder->get_format = get_hw_format;
            av_buffer_unref(&hw_device_ctx);
        }

        decoder->time_base = s->time_base;
        decoder->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        decoder->thread_count = threads > 4u ? 4 : static_cast<int>(threads);

        ret = avcodec_open2(decoder, videoCodec_, nullptr);
        if (ret < 0) {
            if (decoder->opaque) {
                delete static_cast<AVPixelFormat*>(decoder->opaque);
                decoder->opaque = nullptr;
            }
            avcodec_free_context(&decoder);
            return ret;
        }

        videoDecoder_ = std::shared_ptr<AVCodecContext>(decoder, [](AVCodecContext* p) {
            if (p) {
                if (p->opaque) {
                    delete static_cast<AVPixelFormat*>(p->opaque);
                    p->opaque = nullptr;
                }
                avcodec_free_context(&p);
            }
            });

        return 0;
    }

    int MediaDecoder::openAudioDecoder(AVFormatContext* ctx, unsigned int threads) {
        if (!ctx || !ctx->streams) {
            return AVERROR(EINVAL);
        }

        int index = av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (index < 0) {
            return AVERROR(EINVAL);
        }

        AVStream* s = ctx->streams[index];
        if (!s || !s->codecpar) {
            return AVERROR(EINVAL);
        }

        AVCodecParameters* p = s->codecpar;

        resetAudioDecoder();

        audioCodec_ = avcodec_find_decoder(p->codec_id);
        if (!audioCodec_) {
            return AVERROR_DECODER_NOT_FOUND;
        }

        AVCodecContext* decoder = avcodec_alloc_context3(audioCodec_);
        if (!decoder) {
            return AVERROR(ENOMEM);
        }

        int ret = avcodec_parameters_to_context(decoder, p);
        if (ret < 0) {
            avcodec_free_context(&decoder);
            return ret;
        }

        decoder->time_base = s->time_base;
        decoder->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        decoder->thread_count = threads > 4u ? 4 : static_cast<int>(threads);

        ret = avcodec_open2(decoder, audioCodec_, nullptr);
        if (ret < 0) {
            avcodec_free_context(&decoder);
            return ret;
        }

        audioDecoder_ = std::shared_ptr<AVCodecContext>(decoder, [](AVCodecContext* p) {
            if (p) {
                avcodec_free_context(&p);
            }
            });

        return 0;
    }

    void MediaDecoder::flushVideoDecoder() {
        if (videoDecoder_) {
            avcodec_flush_buffers(videoDecoder_.get());
        }
    }

    void MediaDecoder::flushAudioDecoder() {
        if (audioDecoder_) {
            avcodec_flush_buffers(audioDecoder_.get());
        }
    }

    void MediaDecoder::resetVideoDecoder() {
        videoCodec_ = nullptr;
        videoDecoder_.reset();
    }

    void MediaDecoder::resetAudioDecoder() {
        audioCodec_ = nullptr;
        audioDecoder_.reset();
    }

    AVPixelFormat MediaDecoder::findHWFormat(const AVCodec* codec, AVHWDeviceType type) {
        for (int i = 0;; ++i) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
            if (!config) {
                break;
            }

            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
                if (config->device_type == type) {
                    return config->pix_fmt;
                }
            }
        }

        return AV_PIX_FMT_NONE;
    }

} // namespace media