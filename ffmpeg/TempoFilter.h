#pragma once

#include <cmath>
#include <mutex>
#include <vector>
#include "FFmpeg.h"

namespace media {

    class TempoFilter {
    public:
        TempoFilter(const TempoFilter&) = delete;
        TempoFilter& operator=(const TempoFilter&) = delete;
        TempoFilter(TempoFilter&&) = delete;
        TempoFilter& operator=(TempoFilter&&) = delete;

        static constexpr int   MAX_NODE_COUNT = 4;
        static constexpr float MIN_TEMPO      = 0.5f;
        static constexpr float MAX_TEMPO      = 4.0f;
        static constexpr float MIN_ATEMPO     = 0.5f;
        static constexpr float MAX_ATEMPO     = 2.0f;

        struct FilterNode {
            float tempo = 1.0f;
            AVFilterContext* context = nullptr;
        };

        TempoFilter();
        ~TempoFilter();

        // Init tempo filter (samplerate, timebase, chlayout, samplefmt, threads) >= 0
        int init(int samplerate,
                 AVRational timebase,
                 const AVChannelLayout& chlayout,
                 AVSampleFormat samplefmt,
                 unsigned int threads = 0);

        // Set Tempo (tempo) >= 0
        int setTempo(float tempo);
        float getTempo() const;

        // Add Frame (srcFrame) >= 0
        int addFrame(AVFrame* srcFrame);
        // Get Frame (dstFrame) >= 0
        int getFrame(AVFrame* dstFrame);

        // Flush tempo filter
        void flush();
        // Reset tempo filter
        void reset();

        bool isInited() const;
        AVFilterContext* bufferSrc() const;
        AVFilterContext* bufferSink() const;

    private:
        int buildFilterChain();
        int createBufferSrc();
        int createBufferSink();
        int createTempoChain();
        int linkTempoChain();
        std::vector<float> calculateTempoChain(float tempo);

    private:
        mutable std::mutex mutex_;

        int samplerate_;
        AVRational timebase_;
        AVChannelLayout chlayout_;
        AVSampleFormat samplefmt_;
        unsigned int threads_;

        bool inited_;
        float tempo_;

        AVFilterGraph* filterGraph_;
        AVFilterContext* bufferSrc_;
        std::vector<FilterNode> bufferNodes_;
        AVFilterContext* bufferSink_;
    };

} // namespace media