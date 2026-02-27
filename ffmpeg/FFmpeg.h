#pragma once

extern "C" {

// Libavdevice
#include <libavdevice/avdevice.h>

// Libavformat
#include <libavformat/avformat.h>

// Libavcodec
#include <libavcodec/avcodec.h>

// Libswscale
#include <libswscale/swscale.h>

// Libswresample
#include <libswresample/swresample.h>

// Libavfilter
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>

// Libavutil
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/fifo.h>
#include <libavutil/error.h>
#include <libavutil/avutil.h>
#include <libavutil/bprint.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>

}

namespace media {

	static enum AVHWDeviceType HW_Types[] = {
		AV_HWDEVICE_TYPE_CUDA,
		AV_HWDEVICE_TYPE_VULKAN,
		AV_HWDEVICE_TYPE_OPENCL,
#if defined(_WIN32)
		AV_HWDEVICE_TYPE_D3D11VA
#elif defined(__linux__)
		AV_HWDEVICE_TYPE_VAAPI
#elif defined(__APPLE__)
		AV_HWDEVICE_TYPE_VIDEOTOOLBOX
#endif
	};

	struct Resolution {
		const char* name;
		int width;
		int height;
		int framerate;
		int64_t bitrate;
	};

	static const Resolution Resolution_Preset[] = {
		{"360p",  640,  360,  24, 500000},	// 0
		{"480p",  854,  480,  25, 1000000},	// 1
		{"720p",  1280, 720,  30, 2500000}, // 2
		{"1080p", 1920, 1080, 30, 4000000},	// 3
		{"audio",    0,    0,  0, 128000},	// 4
	};

} // namespace media