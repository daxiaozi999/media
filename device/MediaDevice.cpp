#include "MediaDevice.h"

#include <cstring>
#include <sstream>
#include <algorithm>

#if defined(_WIN32)
#include <dshow.h>
#include <comdef.h>
#include <strmif.h>
#include <initguid.h>
#include <wrl/client.h>
#include <locale>
#include <codecvt>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "strmiids.lib")
#elif defined(__linux__)
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#elif defined(__APPLE__)
#include <AVFoundation/AVFoundation.h>
#endif

namespace media {

    std::mutex MediaDevice::mutex_;
    std::list<DeviceInfo> MediaDevice::cameraList_;
    std::list<DeviceInfo> MediaDevice::microphoneList_;
    std::unique_ptr<MediaDevice::Enumerator> MediaDevice::enumerator_;

    void MediaDevice::initEnumerator() {
        std::lock_guard<std::mutex> locker(mutex_);
        if (!enumerator_) {
#if defined(_WIN32)
            enumerator_ = std::make_unique<WindowsEnumerator>();
#elif defined(__linux__)
            enumerator_ = std::make_unique<LinuxEnumerator>();
#elif defined(__APPLE__)
            enumerator_ = std::make_unique<MacOSEnumerator>();
#endif
        }
    }

    void MediaDevice::uninitEnumerator() {
        std::lock_guard<std::mutex> locker(mutex_);
        enumerator_.reset();
    }

    bool MediaDevice::enumCameraDevice() {
        try {
            initEnumerator();

            std::lock_guard<std::mutex> locker(mutex_);
            if (!enumerator_) {
                return false;
            }

            cameraList_ = enumerator_->enumCameraDevice();
            return !cameraList_.empty();
        }
        catch (...) {
            return false;
        }
    }

    bool MediaDevice::enumMicrophoneDevice() {
        try {
            initEnumerator();

            std::lock_guard<std::mutex> locker(mutex_);
            if (!enumerator_) {
                return false;
            }

            microphoneList_ = enumerator_->enumMicrophoneDevice();
            return !microphoneList_.empty();
        }
        catch (...) {
            return false;
        }
    }

    std::list<DeviceInfo> MediaDevice::getCameraList() {
        std::lock_guard<std::mutex> locker(mutex_);
        return cameraList_;
    }

    std::list<DeviceInfo> MediaDevice::getMicrophoneList() {
        std::lock_guard<std::mutex> locker(mutex_);
        return microphoneList_;
    }

#if defined(_WIN32)
    namespace {
        std::string wstringToString(const std::wstring& wstr) {
            if (wstr.empty()) {
                return std::string();
            }

            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.to_bytes(wstr);
        }
    }

    std::list<DeviceInfo> MediaDevice::WindowsEnumerator::enumCameraDevice() {
        std::list<DeviceInfo> deviceList;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        struct ComReleaser {
            bool success;
            ComReleaser(bool success) : success(success) {}
            ~ComReleaser() { if (success) CoUninitialize(); }
        } comReleaser(SUCCEEDED(hr));

        Microsoft::WRL::ComPtr<ICreateDevEnum> pDevEnum;
        hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
        if (FAILED(hr) || !pDevEnum) {
            return deviceList;
        }

        Microsoft::WRL::ComPtr<IEnumMoniker> pEnum;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (hr != S_OK || !pEnum) {
            return deviceList;
        }

        pEnum->Reset();

        while (true) {
            Microsoft::WRL::ComPtr<IMoniker> pMoniker;
            ULONG fetched = 0;
            hr = pEnum->Next(1, &pMoniker, &fetched);
            if (hr != S_OK || !pMoniker) {
                break;
            }

            Microsoft::WRL::ComPtr<IPropertyBag> pPropBag;
            hr = pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag));
            if (SUCCEEDED(hr) && pPropBag) {
                VARIANT var;
                VariantInit(&var);
                hr = pPropBag->Read(L"FriendlyName", &var, nullptr);
                if (SUCCEEDED(hr) && var.vt == VT_BSTR) {
                    DeviceInfo device;
                    device.name = wstringToString(var.bstrVal);

                    LPOLESTR displayName = nullptr;
                    hr = pMoniker->GetDisplayName(nullptr, nullptr, &displayName);
                    if (SUCCEEDED(hr) && displayName) {
                        device.path = wstringToString(displayName);
                        CoTaskMemFree(displayName);
                    }

                    deviceList.push_back(device);
                }
                VariantClear(&var);
            }
        }

        return deviceList;
    }

    std::list<DeviceInfo> MediaDevice::WindowsEnumerator::enumMicrophoneDevice() {
        std::list<DeviceInfo> deviceList;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        struct ComReleaser {
            bool success;
            ComReleaser(bool success) : success(success) {}
            ~ComReleaser() { if (success) CoUninitialize(); }
        } comReleaser(SUCCEEDED(hr));

        Microsoft::WRL::ComPtr<ICreateDevEnum> pDevEnum;
        hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
        if (FAILED(hr) || !pDevEnum) {
            return deviceList;
        }

        Microsoft::WRL::ComPtr<IEnumMoniker> pEnum;
        hr = pDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEnum, 0);
        if (hr != S_OK || !pEnum) {
            return deviceList;
        }

        pEnum->Reset();

        while (true) {
            Microsoft::WRL::ComPtr<IMoniker> pMoniker;
            ULONG fetched = 0;
            hr = pEnum->Next(1, &pMoniker, &fetched);
            if (hr != S_OK || !pMoniker) {
                break;
            }

            Microsoft::WRL::ComPtr<IPropertyBag> pPropBag;
            hr = pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag));
            if (SUCCEEDED(hr) && pPropBag) {
                VARIANT var;
                VariantInit(&var);
                hr = pPropBag->Read(L"FriendlyName", &var, nullptr);
                if (SUCCEEDED(hr) && var.vt == VT_BSTR) {
                    DeviceInfo device;
                    device.name = wstringToString(var.bstrVal);

                    LPOLESTR displayName = nullptr;
                    hr = pMoniker->GetDisplayName(nullptr, nullptr, &displayName);
                    if (SUCCEEDED(hr) && displayName) {
                        device.path = wstringToString(displayName);
                        CoTaskMemFree(displayName);
                    }

                    deviceList.push_back(device);
                }
                VariantClear(&var);
            }
        }

        return deviceList;
    }

#elif defined(__linux__)
    namespace {
        std::string trim(const std::string& str) {
            size_t first = str.find_first_not_of(" \t\n\r");
            if (first == std::string::npos) {
                return "";
            }

            size_t last = str.find_last_not_of(" \t\n\r");
            return str.substr(first, (last - first + 1));
        }
    }

    std::list<DeviceInfo> MediaDevice::LinuxEnumerator::enumCameraDevice() {
        std::list<DeviceInfo> deviceList;

        DIR* dir = opendir("/dev");
        if (!dir) {
            return deviceList;
        }

        struct DirCloser {
            DIR* d;
            DirCloser(DIR* dir) : d(dir) {}
            ~DirCloser() { if (d) closedir(d); }
        } dirCloser(dir);

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (std::strncmp(entry->d_name, "video", 5) != 0) {
                continue;
            }

            std::string path = std::string("/dev/") + entry->d_name;
            int fd = open(path.c_str(), O_RDWR);
            if (fd < 0) {
                continue;
            }

            struct FdCloser {
                int fd;
                FdCloser(int f) : fd(f) {}
                ~FdCloser() { if (fd >= 0) close(fd); }
            } fdCloser(fd);

            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                continue;
            }

            DeviceInfo device;
            device.name = reinterpret_cast<const char*>(cap.card);
            device.path = path;
            deviceList.push_back(device);
        }

        return deviceList;
    }

    std::list<DeviceInfo> MediaDevice::LinuxEnumerator::enumMicrophoneDevice() {
        std::list<DeviceInfo> deviceList;

        FILE* pipe = popen("arecord -l 2>/dev/null | grep card", "r");
        if (!pipe) {
            return deviceList;
        }

        struct PipeCloser {
            FILE* p;
            PipeCloser(FILE* pipe) : p(pipe) {}
            ~PipeCloser() { if (p) pclose(p); }
        } pipeCloser(pipe);

        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line = trim(buffer);
            if (line.empty() || line.find("card") == std::string::npos) {
                continue;
            }

            DeviceInfo device;

            size_t cardPos = line.find("card");
            if (cardPos == std::string::npos) {
                continue;
            }

            size_t numStart = cardPos + 5;
            size_t colonPos = line.find(":", cardPos);
            if (colonPos == std::string::npos) {
                continue;
            }

            std::string cardNum = trim(line.substr(numStart, colonPos - numStart));

            size_t devicePos = line.find("device", colonPos);
            if (devicePos == std::string::npos) {
                continue;
            }

            size_t devNumStart = devicePos + 7;
            size_t devColonPos = line.find(":", devicePos);
            if (devColonPos == std::string::npos) {
                continue;
            }

            std::string deviceNum = trim(line.substr(devNumStart, devColonPos - devNumStart));
            device.name = trim(line.substr(devColonPos + 1));
            device.path = "plughw:" + cardNum + "," + deviceNum;
            deviceList.push_back(device);
        }

        return deviceList;
    }

#elif defined(__APPLE__)
    std::list<DeviceInfo> MediaDevice::MacOSEnumerator::enumCameraDevice() {
        std::list<DeviceInfo> deviceList;

        @autoreleasepool{
            NSArray * videoDevices = [AVCaptureDevice devicesWithMediaType : AVMediaTypeVideo];
            if (!videoDevices || [videoDevices count] == 0) {
                return deviceList;
            }

            for (AVCaptureDevice* device in videoDevices) {
                DeviceInfo info;
                info.name = std::string([device.localizedName UTF8String]);
                info.path = std::string([device.uniqueID UTF8String]);
                deviceList.push_back(info);
            }
        }

        return deviceList;
    }

    std::list<DeviceInfo> MediaDevice::MacOSEnumerator::enumMicrophoneDevice() {
        std::list<DeviceInfo> deviceList;

        @autoreleasepool{
            NSArray * audioDevices = [AVCaptureDevice devicesWithMediaType : AVMediaTypeAudio];
            if (!audioDevices || [audioDevices count] == 0) {
                return deviceList;
            }

            for (AVCaptureDevice* device in audioDevices) {
                DeviceInfo info;
                info.name = std::string([device.localizedName UTF8String]);
                info.path = std::string([device.uniqueID UTF8String]);
                deviceList.push_back(info);
            }
        }

        return deviceList;
    }
#endif

} // namespace media