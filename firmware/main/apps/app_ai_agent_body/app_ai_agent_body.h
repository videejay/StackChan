/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <mooncake.h>
#include <cstdint>
#include <memory>

namespace view {
class LoadingPage;
}

class AppAiAgentBody : public mooncake::AppAbility {
public:
    AppAiAgentBody();
    ~AppAiAgentBody() override;

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<view::LoadingPage> loading_page_;
    uint32_t next_connect_attempt_ms_ = 0;
    uint32_t connected_at_ms_         = 0;
    uint32_t connect_backoff_ms_      = 500;
    bool mbot_ok_sent_                = false;
};
