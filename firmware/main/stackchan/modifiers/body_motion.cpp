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
/** Both line sensors "out" — no line / often no module on Port 2; do not stop the drive loop. */
constexpr uint8_t kLineBothOutState = 0x03;
}  // namespace

void BodyMotionModifier::stopMotorsIfRunning(MbotClient& mbot)
{
    if (!motors_active_) {
        return;
    }
    if (mbot.stopMotors()) {
        motors_active_ = false;
    }
}

void BodyMotionModifier::setMotorsTracked(MbotClient& mbot, int8_t left, int8_t right)
{
    if (!mbot.setMotors(left, right)) {
        return;
    }
    motors_active_ = (left != 0 || right != 0);
}

void BodyMotionModifier::_update(Modifiable& stackchan)
{
    auto now = GetHAL().millis();
    if (now < next_update_ms_) {
        return;
    }
    next_update_ms_ = now + kUpdatePeriodMs;

    auto& mbot = MbotClient::GetInstance();
    if (!mbot.maintainLink()) {
        motors_active_ = false;
        return;
    }

    uint8_t line_state = kLineBothOutState;
    if (!mbot.getLineFollower(line_state)) {
        return;
    }
    // 0x03 = both sensors out (no line). Without a line follower on Port 2 this is the
    // default reading — continue with distance + yaw instead of stopping every tick.

    uint16_t distance_cm = 0;
    if (!mbot.getDistanceCm(distance_cm)) {
        return;
    }

    if (distance_cm > 0 && distance_cm < kTooCloseDistanceCm) {
        stopMotorsIfRunning(mbot);
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
            setMotorsTracked(mbot, -kObstacleTurnSpeed, kObstacleTurnSpeed);
        } else {
            setMotorsTracked(mbot, kObstacleTurnSpeed, -kObstacleTurnSpeed);
        }
        turn_left_next_ = !turn_left_next_;
        return;
    }

    const int yaw = stackchan.motion().getCurrentYawAngle();
    if (yaw > kYawTurnThreshold) {
        setMotorsTracked(mbot, -kTurnSpeed, kTurnSpeed);
    } else if (yaw < -kYawTurnThreshold) {
        setMotorsTracked(mbot, kTurnSpeed, -kTurnSpeed);
    } else {
        setMotorsTracked(mbot, kForwardSpeed, kForwardSpeed);
    }
}

}  // namespace stackchan
