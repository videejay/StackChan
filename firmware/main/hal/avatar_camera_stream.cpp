/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "avatar_camera_stream.h"

#include <hal/board/hal_bridge.h>
#include <hal/hal.h>
#include <jpg/image_to_jpeg.h>
#include <mooncake_log.h>
#include <stackchan/privacy/camera_peripheral_guard.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <linux/videodev2.h>

#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace {

constexpr int kJpegQuality = 20;

bool isValidJpeg(const uint8_t* data, size_t len)
{
    return data && len >= 4 && data[0] == 0xFF && data[1] == 0xD8;
}

size_t expectedRawFrameBytes(int width, int height, v4l2_pix_fmt_t format)
{
    if (width <= 0 || height <= 0) {
        return 0;
    }
    switch (format) {
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_YUV422P:
        case V4L2_PIX_FMT_RGB565:
        case V4L2_PIX_FMT_RGB565X:
            return static_cast<size_t>(width) * static_cast<size_t>(height) * 2U;
        case V4L2_PIX_FMT_RGB24:
            return static_cast<size_t>(width) * static_cast<size_t>(height) * 3U;
        case V4L2_PIX_FMT_YUV420:
            return static_cast<size_t>(width) * static_cast<size_t>(height) * 3U / 2U;
        case V4L2_PIX_FMT_GREY:
            return static_cast<size_t>(width) * static_cast<size_t>(height);
        default:
            return 0;
    }
}

bool captureAndEncodeJpeg(std::vector<uint8_t>& out)
{
    auto* camera = hal_bridge::board_get_camera();
    if (!camera || !camera->StreamCaptures()) {
        return false;
    }

    const uint8_t* frameData = camera->GetFrameData();
    const size_t frameSize   = camera->GetFrameSize();
    const int width          = camera->GetFrameWidth();
    const int height         = camera->GetFrameHeight();
    const int format         = camera->GetFrameFormat();

    const size_t expected = expectedRawFrameBytes(width, height, static_cast<v4l2_pix_fmt_t>(format));
    if (expected > 0 && frameSize < expected) {
        return false;
    }

    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len    = 0;
    if (!image_to_jpeg(const_cast<uint8_t*>(frameData), frameSize, width, height,
                       static_cast<v4l2_pix_fmt_t>(format), kJpegQuality, &jpeg_data, &jpeg_len)) {
        return false;
    }

    bool ok = false;
    if (isValidJpeg(jpeg_data, jpeg_len)) {
        out.assign(jpeg_data, jpeg_data + jpeg_len);
        ok = true;
    }
    if (jpeg_data) {
        free(jpeg_data);
    }
    return ok;
}

std::mutex g_mutex;
int g_refcount = 0;
std::unique_ptr<stackchan::privacy::CameraPeripheralGuard> g_guard;
uint32_t g_last_capture_ms = 0;
std::vector<uint8_t> g_latest_jpeg;
bool g_has_frame = false;

void syncGuardLocked()
{
    if (g_refcount > 0 && !g_guard) {
        g_guard = std::make_unique<stackchan::privacy::CameraPeripheralGuard>();
    } else if (g_refcount <= 0 && g_guard) {
        g_guard.reset();
        g_has_frame = false;
        g_latest_jpeg.clear();
    }
}

}  // namespace

void avatarCameraStreamAcquire()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_refcount;
    syncGuardLocked();
}

void avatarCameraStreamRelease()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_refcount > 0) {
        --g_refcount;
    }
    syncGuardLocked();
}

bool avatarCameraStreamIsActive()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_refcount > 0;
}

int avatarCameraStreamRefCount()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_refcount;
}

bool avatarCameraStreamTick(uint32_t intervalMs)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_refcount <= 0) {
        return false;
    }

    const uint32_t now = GetHAL().millis();
    if (now - g_last_capture_ms < intervalMs) {
        return false;
    }
    g_last_capture_ms = now;

    if (!captureAndEncodeJpeg(g_latest_jpeg)) {
        return false;
    }
    g_has_frame = true;
    return true;
}

bool avatarCameraStreamCaptureFrame(std::vector<uint8_t>& out, uint32_t intervalMs)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_refcount <= 0) {
        return false;
    }

    const uint32_t now = GetHAL().millis();
    if (now - g_last_capture_ms < intervalMs) {
        return false;
    }
    g_last_capture_ms = now;

    out.clear();
    return captureAndEncodeJpeg(out);
}

bool avatarCameraStreamCopyLatestJpeg(std::vector<uint8_t>& out)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_has_frame || g_latest_jpeg.empty()) {
        return false;
    }
    out = g_latest_jpeg;
    return true;
}

void avatarCameraStreamClearLatestJpeg()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_has_frame = false;
    g_latest_jpeg.clear();
}
