#pragma once

#include <mutex>
#include <cmath>
#include <algorithm>
#include "FFmpeg.h"

namespace media {

    struct Clock {
        bool isValid = false;
        double pts = 0.0;
        double duration = 0.0;
        double lastPts = -1.0;
        double lastDuration = 0.0;
        double updateTime = 0.0;
    };

    class AVSyncManager {
    public:
        AVSyncManager(const AVSyncManager&) = delete;
        AVSyncManager& operator=(const AVSyncManager&) = delete;
        AVSyncManager(AVSyncManager&&) = delete;
        AVSyncManager& operator=(AVSyncManager&&) = delete;

        AVSyncManager(double vduration, double aduration);
        ~AVSyncManager();

        void reset();
        void pause();
        void resume();
        void setSpeed(double speed);
        void updateAudioClock(double pts, double duration);
        void updateVideoClock(double pts, double duration, int& msleep);

    private:
        static double systemClock();
        double updateClock(Clock& clock, double pts, double duration, bool audio);

    private:
        mutable std::mutex mutex_;
        double vduration_;
        double aduration_;
        bool paused_;
        double speed_;
        Clock vclock_;
        Clock aclock_;
    };

} // namespace media