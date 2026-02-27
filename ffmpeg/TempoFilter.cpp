#include "TempoFilter.h"

namespace media {

    TempoFilter::TempoFilter()
        : samplerate_(0)
        , timebase_({ 0, 0 })
        , chlayout_(AV_CHANNEL_LAYOUT_STEREO)
        , samplefmt_(AV_SAMPLE_FMT_NONE)
        , threads_(0u)
        , inited_(false)
        , tempo_(1.0f)
        , filterGraph_(nullptr)
        , bufferSrc_(nullptr)
        , bufferSink_(nullptr) {
    }

    TempoFilter::~TempoFilter() {
        reset();
    }

    int TempoFilter::init(int samplerate,
                          AVRational timebase,
                          const AVChannelLayout& chlayout,
                          AVSampleFormat samplefmt,
                          unsigned int threads) {
        if (samplerate <= 0 || samplefmt == AV_SAMPLE_FMT_NONE) {
            return AVERROR(EINVAL);
        }

        reset();

        std::lock_guard<std::mutex> locker(mutex_);

        samplerate_ = samplerate;
        timebase_ = timebase;
        samplefmt_ = samplefmt;
        threads_ = threads > 4u ? 4u : threads;

        int ret = av_channel_layout_copy(&chlayout_, &chlayout);
        if (ret < 0) {
            return ret;
        }

        inited_ = true;
        return 0;
    }

    int TempoFilter::setTempo(float tempo) {
        if (tempo < MIN_TEMPO || tempo > MAX_TEMPO) {
            return AVERROR(EINVAL);
        }

        std::lock_guard<std::mutex> locker(mutex_);

        if (!inited_) {
            return AVERROR(EINVAL);
        }

        tempo_ = tempo;
        return buildFilterChain();
    }

    float TempoFilter::getTempo() const {
        std::lock_guard<std::mutex> locker(mutex_);
        return tempo_;
    }

    int TempoFilter::addFrame(AVFrame* srcFrame) {
        if (!srcFrame) {
            return AVERROR(EINVAL);
        }

        std::lock_guard<std::mutex> locker(mutex_);

        if (!inited_ || !bufferSrc_) {
            return AVERROR(EINVAL);
        }

        return av_buffersrc_add_frame(bufferSrc_, srcFrame);
    }

    int TempoFilter::getFrame(AVFrame* dstFrame) {
        if (!dstFrame) {
            return AVERROR(EINVAL);
        }

        std::lock_guard<std::mutex> locker(mutex_);

        if (!inited_ || !bufferSink_) {
            return AVERROR(EINVAL);
        }

        return av_buffersink_get_frame(bufferSink_, dstFrame);
    }

    void TempoFilter::flush() {
        std::lock_guard<std::mutex> locker(mutex_);

        if (inited_ && bufferSrc_) {
            av_buffersrc_add_frame(bufferSrc_, nullptr);
        }
    }

    void TempoFilter::reset() {
        std::lock_guard<std::mutex> locker(mutex_);

        bufferSrc_ = nullptr;
        bufferNodes_.clear();
        bufferSink_ = nullptr;

        if (filterGraph_) {
            avfilter_graph_free(&filterGraph_);
            filterGraph_ = nullptr;
        }

        av_channel_layout_uninit(&chlayout_);
        chlayout_ = AV_CHANNEL_LAYOUT_STEREO;

        samplerate_ = 0;
        timebase_ = { 0, 0 };
        samplefmt_ = AV_SAMPLE_FMT_NONE;
        threads_ = 0u;
        inited_ = false;
        tempo_ = 1.0f;
    }

    bool TempoFilter::isInited() const {
        std::lock_guard<std::mutex> locker(mutex_);
        return inited_;
    }

    AVFilterContext* TempoFilter::bufferSrc() const {
        std::lock_guard<std::mutex> locker(mutex_);
        return bufferSrc_;
    }

    AVFilterContext* TempoFilter::bufferSink() const {
        std::lock_guard<std::mutex> locker(mutex_);
        return bufferSink_;
    }

    int TempoFilter::buildFilterChain() {
        auto cleanup = [this]() {
            bufferSrc_ = nullptr;
            bufferNodes_.clear();
            bufferSink_ = nullptr;

            if (filterGraph_) {
                avfilter_graph_free(&filterGraph_);
                filterGraph_ = nullptr;
            }
            };

        cleanup();

        filterGraph_ = avfilter_graph_alloc();
        if (!filterGraph_) {
            return AVERROR(ENOMEM);
        }
        filterGraph_->nb_threads = threads_;

        int ret = 0;
        do {

            ret = createBufferSrc();
            if (ret < 0) {
                break;
            }

            ret = createBufferSink();
            if (ret < 0) {
                break;
            }

            ret = createTempoChain();
            if (ret < 0) {
                break;
            }

            ret = linkTempoChain();
            if (ret < 0) {
                break;
            }

            ret = avfilter_graph_config(filterGraph_, nullptr);
            if (ret < 0) {
                break;
            }

        } while (0);

        if (ret < 0) {
            cleanup();
        }

        return ret;
    }

    int TempoFilter::createBufferSrc() {
        AVBPrint bp;
        av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);
        av_channel_layout_describe_bprint(&chlayout_, &bp);

        char* channel_layout = nullptr;
        int ret = av_bprint_finalize(&bp, &channel_layout);
        if (ret < 0 || !channel_layout) {
            return ret < 0 ? ret : AVERROR(ENOMEM);
        }

        char args[512];
        snprintf(args, sizeof(args),
                "sample_rate=%d:sample_fmt=%s:time_base=%d/%d:channel_layout=%s",
                 samplerate_, av_get_sample_fmt_name(samplefmt_),
                 timebase_.num, timebase_.den, channel_layout);

        ret = avfilter_graph_create_filter(&bufferSrc_, avfilter_get_by_name("abuffer"),
                                           "ffmpeg_abuffer", args, nullptr, filterGraph_);

        av_free(channel_layout);
        return ret;
    }

    int TempoFilter::createBufferSink() {
        int ret = avfilter_graph_create_filter(&bufferSink_, avfilter_get_by_name("abuffersink"),
                                               "ffmpeg_abuffersink", nullptr, nullptr, filterGraph_);
        if (ret < 0) {
            return ret;
        }

        AVBPrint bp;
        av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);
        av_channel_layout_describe_bprint(&chlayout_, &bp);

        char* channel_layout = nullptr;
        ret = av_bprint_finalize(&bp, &channel_layout);
        if (ret < 0 || !channel_layout) {
            return ret < 0 ? ret : AVERROR(ENOMEM);
        }

        int samplerates[] = { samplerate_, -1 };
        AVSampleFormat samplefmts[] = { samplefmt_, AV_SAMPLE_FMT_NONE };

        ret = 0;
        ret |= av_opt_set(bufferSink_, "ch_layouts", channel_layout, AV_OPT_SEARCH_CHILDREN);
        ret |= av_opt_set_int_list(bufferSink_, "sample_rates", samplerates, -1, AV_OPT_SEARCH_CHILDREN);
        ret |= av_opt_set_int_list(bufferSink_, "sample_fmts", samplefmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

        av_free(channel_layout);
        return ret;
    }

    int TempoFilter::createTempoChain() {
        std::vector<float> chain = calculateTempoChain(tempo_);
        if (chain.empty()) {
            return AVERROR(EINVAL);
        }

        if (chain.size() == 1 && std::fabs(chain[0] - 1.0f) < 0.001f) {
            return 0;
        }

        for (size_t i = 0; i < chain.size() && i < MAX_NODE_COUNT; ++i) {
            FilterNode node;

            char name[32];
            snprintf(name, sizeof(name), "atempo_%zu", i);

            char atempo[32];
            snprintf(atempo, sizeof(atempo), "%.3f", chain[i]);

            int ret = avfilter_graph_create_filter(&node.context, avfilter_get_by_name("atempo"),
                                                    name, atempo, nullptr, filterGraph_);
            if (ret < 0) {
                return ret;
            }

            node.tempo = chain[i];
            bufferNodes_.push_back(std::move(node));
        }

        return 0;
    }

    int TempoFilter::linkTempoChain() {
        if (bufferNodes_.empty()) {
            return avfilter_link(bufferSrc_, 0, bufferSink_, 0);
        }

        int ret = avfilter_link(bufferSrc_, 0, bufferNodes_[0].context, 0);
        if (ret < 0) {
            return ret;
        }

        for (size_t i = 1; i < bufferNodes_.size(); ++i) {
            ret = avfilter_link(bufferNodes_[i - 1].context, 0, bufferNodes_[i].context, 0);
            if (ret < 0) {
                return ret;
            }
        }

        return avfilter_link(bufferNodes_.back().context, 0, bufferSink_, 0);
    }

    std::vector<float> TempoFilter::calculateTempoChain(float tempo) {
        std::vector<float> chain;

        if (std::fabs(tempo - 1.0f) < 0.001f) {
            chain.push_back(1.0f);
            return chain;
        }

        float remain = tempo;
        while (std::fabs(remain - 1.0f) > 0.001f && chain.size() < MAX_NODE_COUNT) {
            if (remain >= MIN_ATEMPO && remain <= MAX_ATEMPO) {
                chain.push_back(remain);
                break;
            }
            else if (remain < MIN_ATEMPO) {
                chain.push_back(MIN_ATEMPO);
                remain /= MIN_ATEMPO;
            }
            else {
                chain.push_back(MAX_ATEMPO);
                remain /= MAX_ATEMPO;
            }
        }

        return chain;
    }

} // namespace media