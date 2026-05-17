/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "body_motion.h"

#include <hal/hal.h>
#include <hal/drivers/mbot/mbot_client.h>

namespace stackchan {

namespace {
constexpr uint32_t kUpdatePeriodMs        = 120;
constexpr uint16_t kObstacleDistanceCm   = 25;
constexpr uint16_t kTooCloseDistanceCm   = 5;
constexpr int kYawTurnThreshold          = 120;
constexpr int8_t kForwardSpeed           = 28;
constexpr int8_t kTurnSpeed              = 24;
constexpr int8_t kObstacleTurnSpeed      = 32;
constexpr uint8_t kNoGroundDetectedState = 0x03;
}  // namespace

void BodyMotionModifier::_update(Modifiable& stackchan)
{
    auto now = GetHAL().millis();
    if (now < next_update_ms_) {
        return;
    }
    next_update_ms_ = now + kUpdatePeriodMs;

    auto& mbot = MbotClient::GetInstance();
    if (!mbot.isReady() && !mbot.init()) {
        return;
    }

    uint8_t line_state = kNoGroundDetectedState;
    if (!mbot.getLineFollower(line_state) || line_state == kNoGroundDetectedState) {
        mbot.stopMotors();
        return;
    }

    uint16_t distance_cm = 0;
    if (!mbot.getDistanceCm(distance_cm)) {
        mbot.stopMotors();
        return;
    }

    if (distance_cm > 0 && distance_cm < kTooCloseDistanceCm) {
        mbot.stopMotors();
        if (!too_close_active_ || now >= next_too_close_feedback_ms_) {
            mbot.displayText("DIST TOO SMALL");
            mbot.playTone(880, 120);
            next_too_close_feedback_ms_ = now + 1200;
        }
        too_close_active_ = true;
        return;
    }

    if (too_close_active_) {
        too_close_active_ = false;
        mbot.clearDisplay();
    }

    if (distance_cm > 0 && distance_cm <= kObstacleDistanceCm) {
        if (turn_left_next_) {
            mbot.setMotors(-kObstacleTurnSpeed, kObstacleTurnSpeed);
        } else {
            mbot.setMotors(kObstacleTurnSpeed, -kObstacleTurnSpeed);
        }
        turn_left_next_ = !turn_left_next_;
        return;
    }

    const int yaw = stackchan.motion().getCurrentYawAngle();
    if (yaw > kYawTurnThreshold) {
        mbot.setMotors(-kTurnSpeed, kTurnSpeed);
    } else if (yaw < -kYawTurnThreshold) {
        mbot.setMotors(kTurnSpeed, -kTurnSpeed);
    } else {
        mbot.setMotors(kForwardSpeed, kForwardSpeed);
    }
}

}  // namespace stackchan
