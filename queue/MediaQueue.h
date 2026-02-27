#pragma once

#include <mutex>
#include <deque>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <functional>
#include <condition_variable>

namespace media {

    template<typename T>
    class MediaQueue {
    public:
        MediaQueue(const MediaQueue&) = delete;
        MediaQueue& operator=(const MediaQueue&) = delete;

        using ClearCallback = std::function<void(T*)>;

        MediaQueue(size_t minSize = 0, size_t maxSize = 0)
            : locked_(false)
            , minSize_(minSize)
            , maxSize_(std::max(minSize, maxSize))
            , clearCallback_(nullptr) {
        }

        MediaQueue(MediaQueue&& other) noexcept
            : locked_(other.locked_.load())
            , minSize_(other.minSize_)
            , maxSize_(other.maxSize_)
            , queue_(std::move(other.queue_))
            , clearCallback_(std::move(other.clearCallback_)) {

            other.locked_.store(false);
            other.minSize_ = 0;
            other.maxSize_ = 0;
        }

        MediaQueue& operator=(MediaQueue&& other) noexcept {
            if (this != &other) {
                std::lock(mutex_, other.mutex_);
                std::lock_guard<std::mutex> locker1(mutex_, std::adopt_lock);
                std::lock_guard<std::mutex> locker2(other.mutex_, std::adopt_lock);

                locked_.store(other.locked_.load());
                minSize_ = other.minSize_;
                maxSize_ = other.maxSize_;
                queue_ = std::move(other.queue_);
                clearCallback_ = std::move(other.clearCallback_);

                other.locked_.store(false);
                other.minSize_ = 0;
                other.maxSize_ = 0;
            }

            return *this;
        }

        ~MediaQueue() {
            lock();
            clear();
        }

        void setLimit(size_t minSize, size_t maxSize) {
            std::lock_guard<std::mutex> locker(mutex_);
            minSize_ = minSize;
            maxSize_ = std::max(minSize, maxSize);

            if (queue_.size() < maxSize_) {
                notFull_.notify_all();
            }

            if (!queue_.empty()) {
                notEmpty_.notify_all();
            }
        }

        void setClearCallback(ClearCallback callback) {
            std::lock_guard<std::mutex> locker(mutex_);
            clearCallback_ = std::move(callback);
        }

        bool enqueue(T* item) {
            if (!item || locked_.load()) {
                return false;
            }

            std::unique_lock<std::mutex> locker(mutex_);

            while (!locked_.load()) {
                if (maxSize_ == 0) {
                    return false;
                }

                if (queue_.size() < maxSize_) {
                    queue_.push_back(item);
                    notEmpty_.notify_one();
                    return true;
                }

                if (notFull_.wait_for(locker, std::chrono::milliseconds(50)) == std::cv_status::timeout) {
                    if (locked_.load()) {
                        return false;
                    }
                }
            }

            return false;
        }

        T* dequeue() {
            if (locked_.load()) {
                return nullptr;
            }

            std::unique_lock<std::mutex> locker(mutex_);

            while (!locked_.load()) {
                if (maxSize_ == 0) {
                    return nullptr;
                }

                if (!queue_.empty()) {
                    T* item = queue_.front();
                    queue_.pop_front();
                    if (queue_.size() < maxSize_) {
                        notFull_.notify_one();
                    }
                    return item;
                }

                if (notEmpty_.wait_for(locker, std::chrono::milliseconds(50)) == std::cv_status::timeout) {
                    if (locked_.load()) {
                        return nullptr;
                    }
                }
            }

            return nullptr;
        }

        size_t size() const {
            std::lock_guard<std::mutex> locker(mutex_);
            return queue_.size();
        }

        bool empty() const {
            std::lock_guard<std::mutex> locker(mutex_);
            return queue_.empty();
        }

        bool full() const {
            std::lock_guard<std::mutex> locker(mutex_);
            return maxSize_ > 0 && queue_.size() >= maxSize_;
        }

        void wake() {
            notEmpty_.notify_all();
            notFull_.notify_all();
        }

        void lock() {
            locked_.store(true);
            wake();
        }

        void unlock() {
            locked_.store(false);
            wake();
        }

        void clear() {
            std::lock_guard<std::mutex> locker(mutex_);
            while (!queue_.empty()) {
                T* item = queue_.front();
                queue_.pop_front();
                if (item) {
                    if (clearCallback_) {
                        clearCallback_(item);
                    }
                    else {
                        delete item;
                    }
                }
            }
        }

    private:
        mutable std::mutex mutex_;
        std::atomic<bool> locked_;
        std::condition_variable notEmpty_;
        std::condition_variable notFull_;
        size_t minSize_;
        size_t maxSize_;
        std::deque<T*> queue_;
        ClearCallback clearCallback_;
    };

} // namespace media