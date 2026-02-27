#include "MediaOutput.h"

namespace media {

    MediaOutput::MediaOutput()
        : videoIndex_(-1)
        , audioIndex_(-1)
        , videoStream_(nullptr)
        , audioStream_(nullptr)
        , outputCtx_(nullptr) {
    }

    MediaOutput::~MediaOutput() {
        reset();
    }

    int MediaOutput::writeFile(const std::string& url,
                               const std::string& format,
                               AVCodecContext* videoEncoder,
                               AVCodecContext* audioEncoder) {
        if (url.empty()) {
            return AVERROR(EINVAL);
        }

        if (!videoEncoder && !audioEncoder) {
            return AVERROR(EINVAL);
        }

        reset();

        AVFormatContext* ctx = nullptr;
        int ret = avformat_alloc_output_context2(&ctx, nullptr, format.c_str(), url.c_str());
        if (ret < 0) {
            return ret;
        }

        if (videoEncoder) {
            videoStream_ = avformat_new_stream(ctx, nullptr);
            if (!videoStream_) {
                avformat_free_context(ctx);
                return AVERROR(ENOMEM);
            }

            ret = avcodec_parameters_from_context(videoStream_->codecpar, videoEncoder);
            if (ret < 0) {
                avformat_free_context(ctx);
                return ret;
            }

            videoStream_->time_base = videoEncoder->time_base;
            videoIndex_ = videoStream_->index;
        }

        if (audioEncoder) {
            audioStream_ = avformat_new_stream(ctx, nullptr);
            if (!audioStream_) {
                avformat_free_context(ctx);
                return AVERROR(ENOMEM);
            }

            ret = avcodec_parameters_from_context(audioStream_->codecpar, audioEncoder);
            if (ret < 0) {
                avformat_free_context(ctx);
                return ret;
            }

            audioStream_->time_base = audioEncoder->time_base;
            audioIndex_ = audioStream_->index;
        }

        if (!(ctx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&ctx->pb, url.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                avformat_free_context(ctx);
                return ret;
            }
        }

        ret = avformat_write_header(ctx, nullptr);
        if (ret < 0) {
            if (ctx->pb) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
            return ret;
        }

        outputCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* p) {
            if (p) {
                av_write_trailer(p);
                if (p->pb) {
                    avio_closep(&p->pb);
                }
                avformat_free_context(p);
            }
            });

        return 0;
    }

    int MediaOutput::writeNetwork(const std::string& url,
                                  const std::string& format,
                                  AVCodecContext* videoEncoder,
                                  AVCodecContext* audioEncoder,
                                  AVDictionary* opt) {
        if (url.empty() || format.empty()) {
            return AVERROR(EINVAL);
        }

        if (format != "flv" && format != "hls") {
            return AVERROR(EINVAL);
        }

        if (!videoEncoder && !audioEncoder) {
            return AVERROR(EINVAL);
        }

        reset();

        AVFormatContext* ctx = nullptr;
        int ret = avformat_alloc_output_context2(&ctx, nullptr, format.c_str(), url.c_str());
        if (ret < 0) {
            return ret;
        }

        if (videoEncoder) {
            videoStream_ = avformat_new_stream(ctx, nullptr);
            if (!videoStream_) {
                avformat_free_context(ctx);
                return AVERROR(ENOMEM);
            }

            ret = avcodec_parameters_from_context(videoStream_->codecpar, videoEncoder);
            if (ret < 0) {
                avformat_free_context(ctx);
                return ret;
            }

            videoStream_->time_base = videoEncoder->time_base;
            videoIndex_ = videoStream_->index;
        }

        if (audioEncoder) {
            audioStream_ = avformat_new_stream(ctx, nullptr);
            if (!audioStream_) {
                avformat_free_context(ctx);
                return AVERROR(ENOMEM);
            }

            ret = avcodec_parameters_from_context(audioStream_->codecpar, audioEncoder);
            if (ret < 0) {
                avformat_free_context(ctx);
                return ret;
            }

            audioStream_->time_base = audioEncoder->time_base;
            audioIndex_ = audioStream_->index;
        }

        if (!(ctx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&ctx->pb, url.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                avformat_free_context(ctx);
                return ret;
            }
        }

        ret = avformat_write_header(ctx, &opt);
        if (ret < 0) {
            if (ctx->pb) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
            return ret;
        }

        outputCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* p) {
            if (p) {
                av_write_trailer(p);
                if (p->pb) {
                    avio_closep(&p->pb);
                }
                avformat_free_context(p);
            }
            });

        return 0;
    }

    void MediaOutput::reset() {
        videoIndex_ = -1;
        audioIndex_ = -1;
        videoStream_ = nullptr;
        audioStream_ = nullptr;
        outputCtx_.reset();
    }

} // namespace media