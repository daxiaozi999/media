#pragma once

#include <string>
#include <memory>
#include "FFmpeg.h"

namespace media {

    class MediaOutput {
    public:
        MediaOutput(const MediaOutput&) = delete;
        MediaOutput& operator=(const MediaOutput&) = delete;
        MediaOutput(MediaOutput&&) = delete;
        MediaOutput& operator=(MediaOutput&&) = delete;

        MediaOutput();
        ~MediaOutput();

        // Write file (filename, mp4, videoEncoder, audioEncoder) >= 0
        int writeFile(const std::string& url,
                      const std::string& format = "",
                      AVCodecContext* videoEncoder = nullptr,
                      AVCodecContext* audioEncoder = nullptr);
        // Write Network (RTSP, HLS)/(HTTP, HLS)/(RTMP, FLV)/...+(videoEncoder, audioEncoder, opt) >= 0
        int writeNetwork(const std::string& url,
                         const std::string& format,
                         AVCodecContext* videoEncoder = nullptr,
                         AVCodecContext* audioEncoder = nullptr,
                         AVDictionary* opt = nullptr);

        // Reset current write
        void reset();

        int videoIndex()                 const { return videoIndex_; }
        int audioIndex()                 const { return audioIndex_; }
        AVStream* videoStream()          const { return videoStream_; }
        AVStream* audioStream()          const { return audioStream_; }
        AVFormatContext* outputContext() const { return outputCtx_.get(); }

    private:
        int videoIndex_;
        int audioIndex_;
        AVStream* videoStream_;
        AVStream* audioStream_;
        std::shared_ptr<AVFormatContext> outputCtx_;
    };

} // namespace media