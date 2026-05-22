/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <vector>

/** Increment stream consumer refcount; starts V4L2 on 0→1 via CameraPeripheralGuard. */
void avatarCameraStreamAcquire();

/** Decrement refcount; stops V4L2 on 1→0. */
void avatarCameraStreamRelease();

bool avatarCameraStreamIsActive();

/** Returns number of active consumers (WS, HTTP, etc.). */
int avatarCameraStreamRefCount();

/**
 * Capture + JPEG encode when interval elapsed and refcount > 0.
 * @param intervalMs minimum ms since last capture (350 normal, 700 video mode for WS)
 * @return true if a new frame was stored
 */
bool avatarCameraStreamTick(uint32_t intervalMs);

/** Copy latest JPEG frame; returns false if none available yet. */
bool avatarCameraStreamCopyLatestJpeg(std::vector<uint8_t>& out);

/**
 * Capture + encode into @p out when @p intervalMs elapsed (for HTTP MJPEG handler).
 * Avoids sharing a frame buffer between the main loop and httpd task.
 */
bool avatarCameraStreamCaptureFrame(std::vector<uint8_t>& out, uint32_t intervalMs);

/** Clear stored frame (e.g. on stream stop). */
void avatarCameraStreamClearLatestJpeg();
