#include "MediaInput.h"

namespace media {

    MediaInput::MediaInput()
        : duration_(0)
        , inputFmt_(nullptr)
        , inputCtx_(nullptr) {

        avdevice_register_all();
        avformat_network_init();
    }

    MediaInput::~MediaInput() {
        reset();
    }

    int MediaInput::openFileStream(const std::string& url) {
        if (url.empty()) {
            return AVERROR(EINVAL);
        }

        reset();

        AVFormatContext* ctx = nullptr;
        int ret = avformat_open_input(&ctx, url.c_str(), nullptr, nullptr);
        if (ret < 0) {
            return ret;
        }

        ret = avformat_find_stream_info(ctx, nullptr);
        if (ret < 0) {
            avformat_close_input(&ctx);
            return ret;
        }

        inputCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* p) {
            if (p) {
                avformat_close_input(&p);
            }
            });

        extractParams();
        return 0;
    }

    int MediaInput::openDeviceStream(const std::string& url) {
        if (url.empty()) {
            return AVERROR(EINVAL);
        }

        reset();

        std::string dshow = "dshow";
#if defined(_WIN32)
        inputFmt_ = av_find_input_format(dshow.c_str());
#elif defined(__linux__)
        inputFmt_ = av_find_input_format("v4l2");
#elif defined(__APPLE__)
        inputFmt_ = av_find_input_format("avfoundation");
#endif

        if (!inputFmt_) {
            return AVERROR(EINVAL);
        }

        AVDictionary* opt = nullptr;
        av_dict_set(&opt, "video_size", "1280x720", 0);
        av_dict_set(&opt, "framerate", "30", 0);

#if defined(_WIN32)
        av_dict_set(&opt, "pixel_format", "yuyv422", 0);
#elif defined(__linux__)
        av_dict_set(&opt, "input_format", "mjpeg", 0);
#elif defined(__APPLE__)
        av_dict_set(&opt, "pixel_format", "uyvy422", 0);
#endif

        AVFormatContext* ctx = nullptr;
        int ret = avformat_open_input(&ctx, url.c_str(), inputFmt_, &opt);
        av_dict_free(&opt);

        if (ret < 0) {
            return ret;
        }

        ret = avformat_find_stream_info(ctx, nullptr);
        if (ret < 0) {
            avformat_close_input(&ctx);
            return ret;
        }

        inputCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* p) {
            if (p) {
                avformat_close_input(&p);
            }
            });

        extractParams();
        return 0;
    }

    int MediaInput::openDesktopStream(const std::string& url, AVDictionary* opt) {
        reset();

        std::string desktop_url;

#if defined(_WIN32)
        inputFmt_ = av_find_input_format("gdigrab");
        desktop_url = url.empty() ? "desktop" : url;
#elif defined(__linux__)
        inputFmt_ = av_find_input_format("x11grab");
        desktop_url = url.empty() ? ":0.0" : url;
#elif defined(__APPLE__)
        inputFmt_ = av_find_input_format("avfoundation");
        desktop_url = url.empty() ? "1" : url;
#endif

        if (!inputFmt_) {
            return AVERROR(EINVAL);
        }

        AVDictionary* desktop_opt = nullptr;

        if (!opt) {
            av_dict_set(&desktop_opt, "framerate", "30", 0);
#if defined(_WIN32)
            av_dict_set(&desktop_opt, "draw_mouse", "1", 0);
#elif defined(__linux__)
            av_dict_set(&desktop_opt, "draw_mouse", "1", 0);
#elif defined(__APPLE__)
            av_dict_set(&desktop_opt, "capture_cursor", "1", 0);
            av_dict_set(&desktop_opt, "capture_mouse_clicks", "1", 0);
#endif
        }
        else {
            desktop_opt = opt;
        }

        AVFormatContext* ctx = nullptr;
        int ret = avformat_open_input(&ctx, desktop_url.c_str(), inputFmt_, &desktop_opt);

        if (!opt) {
            av_dict_free(&desktop_opt);
        }

        if (ret < 0) {
            return ret;
        }

        ret = avformat_find_stream_info(ctx, nullptr);
        if (ret < 0) {
            avformat_close_input(&ctx);
            return ret;
        }

        inputCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* p) {
            if (p) {
                avformat_close_input(&p);
            }
            });

        extractParams();
        return 0;
    }

    int MediaInput::openNetworkStream(const std::string& url, AVDictionary* opt) {
        if (url.empty()) {
            return AVERROR(EINVAL);
        }

        reset();

        AVFormatContext* ctx = nullptr;
        int ret = avformat_open_input(&ctx, url.c_str(), nullptr, &opt);
        if (ret < 0) {
            return ret;
        }

        ret = avformat_find_stream_info(ctx, nullptr);
        if (ret < 0) {
            avformat_close_input(&ctx);
            return ret;
        }

        inputCtx_ = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* p) {
            if (p) {
                avformat_close_input(&p);
            }
            });

        extractParams();
        return 0;
    }

    void MediaInput::reset() {
        duration_ = 0;
        videoParams_ = VideoParams();
        audioParams_ = AudioParams();
        inputFmt_ = nullptr;
        inputCtx_.reset();
    }

    void MediaInput::extractParams() {
        if (!inputCtx_) {
            return;
        }

        if (!inputCtx_->streams || inputCtx_->nb_streams == 0) {
            return;
        }

        for (unsigned int i = 0; i < inputCtx_->nb_streams; ++i) {
            AVStream* s = inputCtx_->streams[i];
            if (!s || !s->codecpar) {
                continue;
            }

            AVCodecParameters* p = s->codecpar;

            if (p->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoParams_.index = static_cast<int>(i);
                videoParams_.width = p->width;
                videoParams_.height = p->height;
                videoParams_.bitrate = p->bit_rate;
                videoParams_.timebase = s->time_base;
                videoParams_.pixfmt = static_cast<AVPixelFormat>(p->format);

                if (p->framerate.num > 0 && p->framerate.den > 0) {
                    videoParams_.framerate = p->framerate;
                }
                else if (s->avg_frame_rate.num > 0 && s->avg_frame_rate.den > 0) {
                    videoParams_.framerate = s->avg_frame_rate;
                }
                else if (s->r_frame_rate.num > 0 && s->r_frame_rate.den > 0) {
                    videoParams_.framerate = s->r_frame_rate;
                }
            }
            else if (p->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioParams_.index = static_cast<int>(i);
                audioParams_.framesize = p->frame_size;
                audioParams_.samplerate = p->sample_rate;
                audioParams_.bitrate = p->bit_rate;
                audioParams_.timebase = s->time_base;
                audioParams_.samplefmt = static_cast<AVSampleFormat>(p->format);

                av_channel_layout_uninit(&audioParams_.chlayout);
                if (av_channel_layout_copy(&audioParams_.chlayout, &p->ch_layout) < 0) {
                    av_channel_layout_default(&audioParams_.chlayout, 2);
                }
            }
        }

        if (inputCtx_->duration != AV_NOPTS_VALUE) {
            duration_ = inputCtx_->duration / AV_TIME_BASE;
        }
        else if (videoParams_.index >= 0) {
            AVStream* vs = inputCtx_->streams[videoParams_.index];
            if (vs->duration != AV_NOPTS_VALUE) {
                duration_ = av_rescale_q(vs->duration, vs->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;
            }
        }
        else if (audioParams_.index >= 0) {
            AVStream* as = inputCtx_->streams[audioParams_.index];
            if (as->duration != AV_NOPTS_VALUE) {
                duration_ = av_rescale_q(as->duration, as->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;
            }
        }
    }

} // namespace media