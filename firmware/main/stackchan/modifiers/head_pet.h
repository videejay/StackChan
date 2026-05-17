/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "../modifiable.h"

#include <cstdint>

namespace stackchan {

class HeadPetModifier : public Modifier {
public:
    explicit HeadPetModifier(uint32_t restoreDelayMs = 3000);
    ~HeadPetModifier() override;

    void _update(Modifiable& stackchan) override;

private:
    void handle_swipe(Modifiable& stackchan);
    void restore_original_state(Modifiable& stackchan);
    void perform_pet_motion(Modifiable& stackchan);
    void flashWakeFeedback(Modifiable& stackchan);
    void update_mbot_heart_feedback();

    size_t _signal_connection = 0;

    volatile bool _event_press   = false;
    volatile bool _event_swipe   = false;
    volatile bool _event_release = false;

    bool _is_touched        = false;
    bool _hold_wake_fired   = false;
    uint32_t _touch_start_ms = 0;
    uint32_t _mbot_heart_until_ms = 0;
    uint32_t _mbot_next_pulse_ms  = 0;
    bool _mbot_heart_lit          = false;

    bool _in_happy_state     = false;
    bool _is_waiting_restore = false;
    uint32_t _restore_tick    = 0;
    uint32_t _restore_delay_ms = 3000;

    int _heart_decorator_id = -1;
    int _shy_decorator_id   = -1;

    avatar::Emotion _prev_emotion = avatar::Emotion::Neutral;
    int32_t _prev_yaw              = 0;
    int32_t _prev_pitch            = 0;
};

}  // namespace stackchan
