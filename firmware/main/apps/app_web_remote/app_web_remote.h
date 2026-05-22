/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/common/loading_page/loading_page.h>
#include <mooncake.h>
#include <cstdint>
#include <memory>

class AppWebRemote : public mooncake::AppAbility {
public:
    AppWebRemote();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum class Phase {
        EnsureWifi,
        ConnectMbot,
        StartHttp,
        ShowUrl,
        AvatarIdle,
        Error,
    };

    void showAvatar();
    void formatUrlMessage(std::string& out) const;
    /** mBot I2C keepalive — runs before LVGL/camera work. */
    void serviceMbotKeepalive(uint32_t now);

    Phase phase_ = Phase::EnsureWifi;
    std::unique_ptr<view::LoadingPage> loading_page_;
    bool network_start_attempted_ = false;
    bool avatar_attached_       = false;
    uint32_t next_connect_attempt_ms_ = 0;
    uint32_t connect_backoff_ms_      = 500;
    uint32_t url_shown_at_ms_         = 0;
    uint32_t last_keepalive_ms_       = 0;
    static constexpr uint32_t kUrlDisplayMs    = 10000;
    /** Must stay well below MbotClient link idle drop (10 s). */
    static constexpr uint32_t kKeepaliveMs   = 3000;
    static constexpr uint32_t kThemeColor    = 0x4A90D9;
    static constexpr uint32_t kThemeColorDark = 0x1A3A5C;
};
