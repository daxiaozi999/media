#pragma once

#include <list>
#include <mutex>
#include <memory>
#include <string>

namespace media {

    struct DeviceInfo {
        std::string name;  // Friendly name
        std::string path;  // Display  name
    };

    class MediaDevice {
    public:
        // Init device enumerator
        static void initEnumerator();
        // Uninit device enumerator
        static void uninitEnumerator();

        // Enum camera device
        static bool enumCameraDevice();
        // Enum microphone device
        static bool enumMicrophoneDevice();

        // Get camera device list
        static std::list<DeviceInfo> getCameraList();
        // Get microphone device list
        static std::list<DeviceInfo> getMicrophoneList();

    private:
        // Enumerator base class
        class Enumerator {
        public:
            Enumerator() = default;
            virtual ~Enumerator() = default;
            virtual std::list<DeviceInfo> enumCameraDevice() = 0;
            virtual std::list<DeviceInfo> enumMicrophoneDevice() = 0;
        };

#if defined(_WIN32)
        // Windows enumerator
        class WindowsEnumerator : public Enumerator {
        public:
            WindowsEnumerator() = default;
            ~WindowsEnumerator() override = default;
            std::list<DeviceInfo> enumCameraDevice() override;
            std::list<DeviceInfo> enumMicrophoneDevice() override;
        };
#elif defined(__linux__)
        // Linux enumerator
        class LinuxEnumerator : public Enumerator {
        public:
            LinuxEnumerator() = default;
            ~LinuxEnumerator() override = default;
            std::list<DeviceInfo> enumCameraDevice() override;
            std::list<DeviceInfo> enumMicrophoneDevice() override;
        };
#elif defined(__APPLE__)
        // MacOS enumerator
        class MacOSEnumerator : public Enumerator {
        public:
            MacOSEnumerator() = default;
            ~MacOSEnumerator() override = default;
            std::list<DeviceInfo> enumCameraDevice() override;
            std::list<DeviceInfo> enumMicrophoneDevice() override;
        };
#endif

    private:
        static std::mutex mutex_;
        static std::list<DeviceInfo> cameraList_;
        static std::list<DeviceInfo> microphoneList_;
        static std::unique_ptr<Enumerator> enumerator_;

    private:
        MediaDevice() = delete;
        ~MediaDevice() = delete;
        MediaDevice(const MediaDevice&) = delete;
        MediaDevice& operator=(const MediaDevice&) = delete;
        MediaDevice(MediaDevice&&) = delete;
        MediaDevice& operator=(MediaDevice&&) = delete;
    };

} // namespace media