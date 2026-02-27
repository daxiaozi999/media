#include "MediaEncoder.h"

namespace media {

    MediaEncoder::MediaEncoder()
        : videoCodec_(nullptr)
        , audioCodec_(nullptr)
        , videoEncoder_(nullptr)
        , audioEncoder_(nullptr) {
    }

    MediaEncoder::~MediaEncoder() {
        resetVideoEncoder();
        resetAudioEncoder();
    }

    int MediaEncoder::openVideoEncoder(AVCodecID codecid,
                                       int width,
                                       int height,
                                       int64_t bitrate,
                                       AVRational timebase,
                                       AVRational framerate,
                                       AVPixelFormat pixfmt,
                                       bool useHW,
                                       unsigned int threads,
                                       AVDictionary* opt) {
        if (codecid == AV_CODEC_ID_NONE) {
            return AVERROR(EINVAL);
        }

        resetVideoEncoder();

        if (useHW) {
            for (int i = 0; i < (sizeof(HW_Types) / sizeof(HW_Types[0])); ++i) {
                std::string encoder_name = getHWEncoderName(codecid, HW_Types[i]);
                if (encoder_name.empty()) {
                    continue;
                }

                videoCodec_ = avcodec_find_encoder_by_name(encoder_name.c_str());
                if (videoCodec_) {
                    break;
                }
            }
        }

        if (!videoCodec_) {
            videoCodec_ = avcodec_find_encoder(codecid);
        }

        if (!videoCodec_) {
            return AVERROR_ENCODER_NOT_FOUND;
        }

        AVCodecContext* encoder = avcodec_alloc_context3(videoCodec_);
        if (!encoder) {
            return AVERROR(ENOMEM);
        }

        encoder->codec_id = codecid;
        encoder->codec_type = AVMEDIA_TYPE_VIDEO;
        encoder->width = width;
        encoder->height = height;
        encoder->time_base = timebase;
        encoder->framerate = framerate;
        encoder->bit_rate = bitrate;
        encoder->rc_max_rate = bitrate;
        encoder->rc_buffer_size = bitrate / 2;
        encoder->gop_size = static_cast<int>(av_q2d(framerate));
        encoder->max_b_frames = 0;
        encoder->pix_fmt = pixfmt;
        encoder->thread_count = threads > 4u ? 4 : static_cast<int>(threads);
        encoder->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        encoder->flags |= AV_CODEC_FLAG_LOW_DELAY;
        encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        int ret = avcodec_open2(encoder, videoCodec_, &opt);
        if (ret < 0) {
            avcodec_free_context(&encoder);
            return ret;
        }

        videoEncoder_ = std::shared_ptr<AVCodecContext>(encoder, [](AVCodecContext* p) {
            if (p) {
                avcodec_free_context(&p);
            }
            });

        return 0;
    }

    int MediaEncoder::openAudioEncoder(AVCodecID codecid,
                                       int framesize,
                                       int samplerate,
                                       int64_t bitrate,
                                       AVRational timebase,
                                       const AVChannelLayout& chlayout,
                                       AVSampleFormat samplefmt,
                                       unsigned int threads,
                                       AVDictionary* opt) {
        if (codecid == AV_CODEC_ID_NONE) {
            return AVERROR(EINVAL);
        }

        resetAudioEncoder();

        audioCodec_ = avcodec_find_encoder(codecid);
        if (!audioCodec_) {
            return AVERROR_ENCODER_NOT_FOUND;
        }

        AVCodecContext* encoder = avcodec_alloc_context3(audioCodec_);
        if (!encoder) {
            return AVERROR(ENOMEM);
        }

        encoder->codec_id = codecid;
        encoder->codec_type = AVMEDIA_TYPE_AUDIO;
        encoder->sample_rate = samplerate;
        encoder->bit_rate = bitrate;
        encoder->time_base = timebase;
        encoder->sample_fmt = samplefmt;
        encoder->frame_size = framesize;
        encoder->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        encoder->thread_count = threads > 4u ? 4 : static_cast<int>(threads);
        encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (av_channel_layout_copy(&encoder->ch_layout, &chlayout) < 0) {
            av_channel_layout_default(&encoder->ch_layout, 2);
        }

        int ret = avcodec_open2(encoder, audioCodec_, &opt);
        if (ret < 0) {
            avcodec_free_context(&encoder);
            return ret;
        }

        audioEncoder_ = std::shared_ptr<AVCodecContext>(encoder, [](AVCodecContext* p) {
            if (p) {
                avcodec_free_context(&p);
            }
            });

        return 0;
    }

    void MediaEncoder::flushVideoEncoder() {
        if (videoEncoder_) {
            avcodec_flush_buffers(videoEncoder_.get());
        }
    }

    void MediaEncoder::flushAudioEncoder() {
        if (audioEncoder_) {
            avcodec_flush_buffers(audioEncoder_.get());
        }
    }

    void MediaEncoder::resetVideoEncoder() {
        videoCodec_ = nullptr;
        videoEncoder_.reset();
    }

    void MediaEncoder::resetAudioEncoder() {
        audioCodec_ = nullptr;
        audioEncoder_.reset();
    }

    std::string MediaEncoder::getHWEncoderName(AVCodecID codecid, AVHWDeviceType type) const {
        std::string prefix;

        switch (codecid) {
        case AV_CODEC_ID_H264:
            prefix = "h264";
            break;
        case AV_CODEC_ID_HEVC:
            prefix = "hevc";
            break;
        case AV_CODEC_ID_VP9:
            prefix = "vp9";
            break;
        case AV_CODEC_ID_AV1:
            prefix = "av1";
            break;
        default:
            return "";
        }

        switch (type) {
        case AV_HWDEVICE_TYPE_CUDA:
            return prefix + "_nvenc";
#if defined(_WIN32)
        case AV_HWDEVICE_TYPE_D3D11VA:
            return prefix + "_qsv";
#elif defined(__linux__)
        case AV_HWDEVICE_TYPE_VAAPI:
            return prefix + "_vaapi";
#elif defined(__APPLE__)
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
            return prefix + "_videotoolbox";
#endif
        default:
            return "";
        }
    }

} // namespace media