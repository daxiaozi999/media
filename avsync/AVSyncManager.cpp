#include "AVSyncManager.h"

namespace media {

    AVSyncManager::AVSyncManager(double vduration, double aduration)
        : vduration_(vduration)
        , aduration_(aduration)
        , paused_(false)
        , speed_(1.0) {

        vclock_.duration = vduration;
        aclock_.duration = aduration;
    }

    AVSyncManager::~AVSyncManager() {}

    void AVSyncManager::reset() {
        std::lock_guard<std::mutex> locker(mutex_);

        vclock_ = Clock();
        aclock_ = Clock();
        vclock_.duration = vduration_;
        aclock_.duration = aduration_;
        paused_ = false;
        speed_ = 1.0;
    }

    void AVSyncManager::pause() {
        std::lock_guard<std::mutex> locker(mutex_);

        if (!paused_) {
            double time = systemClock();
            paused_ = true;
            vclock_.updateTime = time;
            aclock_.updateTime = time;
        }
    }

    void AVSyncManager::resume() {
        std::lock_guard<std::mutex> locker(mutex_);

        if (paused_) {
            double time = systemClock();
            paused_ = false;
            vclock_.updateTime = time;
            aclock_.updateTime = time;
        }
    }

    void AVSyncManager::setSpeed(double speed) {
        std::lock_guard<std::mutex> locker(mutex_);

        if (speed > 0.0 && std::fabs(speed_ - speed) > 0.01) {
            speed_ = speed;
        }
    }

    void AVSyncManager::updateAudioClock(double pts, double duration) {
        std::lock_guard<std::mutex> locker(mutex_);

        if (paused_) {
            return;
        }

        updateClock(aclock_, pts, duration, true);
    }

    void AVSyncManager::updateVideoClock(double pts, double duration, int& msleep) {
        std::lock_guard<std::mutex> locker(mutex_);

        if (paused_) {
            msleep = 100;
            return;
        }

        updateClock(vclock_, pts, duration, false);

        double diff = 0.0;
        double delay = vclock_.duration / speed_;

        if (aclock_.isValid && vclock_.isValid) {
            diff = (vclock_.pts - aclock_.pts) / speed_;
        }
        else {
            diff = 0.0;
        }

        if (diff <= -0.1) {
            msleep = 1;
            return;
        }

        if (speed_ <= 0.8) {
            if (diff > 0.05) {
                delay *= 1.5;
            }
            else if (diff > 0.02) {
                delay *= 1.1;
            }
            else if (diff < -0.05) {
                delay *= 0.5;
            }
            else if (diff < -0.02) {
                delay *= 0.9;
            }
        }
        else if (speed_ >= 1.2) {
            if (diff > 0.15) {
                delay *= 1.5;
            }
            else if (diff > 0.06) {
                delay *= 1.1;
            }
            else if (diff < -0.15) {
                delay *= 0.5;
            }
            else if (diff < -0.06) {
                delay *= 0.9;
            }
        }
        else {
            if (diff > 0.02) {
                delay *= 1.2;
            }
            else if (diff > 0.01) {
                delay *= 1.05;
            }
            else if (diff < -0.02) {
                delay *= 0.8;
            }
            else if (diff < -0.01) {
                delay *= 0.95;
            }
        }

        delay = std::max(0.001, std::min(delay, 0.1));
        msleep = static_cast<int>(delay * 1000);
    }

    double AVSyncManager::systemClock() {
        return av_gettime() / 1000000.0;
    }

    double AVSyncManager::updateClock(Clock& clock, double pts, double duration, bool audio) {
        double time = systemClock();
        double srcd = audio ? aduration_ : vduration_;

        if (srcd <= 0.0) {
            clock.isValid = false;
            clock.updateTime = time;
            return time;
        }

        clock.lastPts = clock.pts;
        clock.lastDuration = clock.duration;

        if (duration <= 0) {
            clock.duration = srcd;
        }
        else {
            clock.duration = duration;

            if (std::fabs(clock.duration - clock.lastDuration) >= srcd / 2 ||
                std::fabs(clock.duration - srcd) >= srcd / 2) {
                clock.duration = srcd;
            }
        }

        if (pts < 0.0) {
            if (clock.lastPts >= 0.0) {
                clock.pts = clock.lastPts + clock.duration;
                clock.isValid = true;
            }
            else {
                clock.pts = 0.0;
                clock.isValid = false;
            }
        }
        else if (pts == 0.0) {
            if (clock.lastPts == 0.0) {
                clock.pts = pts;
                clock.isValid = true;
            }
            else {
                clock.pts = 0.0;
                clock.isValid = false;
            }
        }
        else {
            clock.pts = pts;

            if (std::fabs(clock.pts - clock.lastPts) >= clock.duration / 2) {
                if (clock.lastPts >= 0.0) {
                    clock.pts = clock.lastPts + clock.duration;
                    clock.isValid = true;
                }
                else {
                    clock.pts = 0.0;
                    clock.isValid = false;
                }
            }
        }

        clock.updateTime = time;
        return time;
    }

} // namespace media