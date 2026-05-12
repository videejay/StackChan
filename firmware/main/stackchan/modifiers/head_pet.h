/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
//
// HeadPetModifier — touch-driven affect + non-visual wake fallback.
//
// Touch start: fires head_pet_started perception event + HeartDecorator + Happy
// emotion (existing behavior).
// Held for >= kHoldToListenMs (2 s default): fires WakeWordInvoke("head_pet_hold")
// once per session.
//   - Opens the listen window (mic uplink to server) just like face-detected.
//   - Works in the dark, where face detection can't fire.
// Touch end: fires head_pet_ended perception event.
//
// kHoldToListenMs lives at the top of head_pet.cpp for one-line tuning.
//
#include "../modifiable.h"
#include "../avatar/decorators/decorators.h"
#include "application.h"
#include <hal/hal.h>
#include <cstdint>

namespace stackchan {

/**
 * @brief
 *
 */
class HeadPetModifier : public Modifier {
public:
    HeadPetModifier(uint32_t restoreDelayMs = 3000);
    ~HeadPetModifier();

    void _update(Modifiable& stackchan) override;

private:
    void handle_swipe(Modifiable& stackchan);
    void restore_original_state(Modifiable& stackchan);
    void perform_pet_motion(Modifiable& stackchan);
    // Quick green pulse on the listen-mode color so the user gets
    // confirmation the 2 s hold registered. The actual listen-state
    // colors (driven by stackchan_display when the WS responds) take
    // over immediately after.
    void flashWakeFeedback(Modifiable& stackchan);

    // Signals
    int _signal_connection;
    volatile bool _event_press   = false;
    volatile bool _event_swipe   = false;
    volatile bool _event_release = false;

    // Affect state machine
    bool _in_happy_state     = false;
    bool _is_waiting_restore = false;
    uint32_t _restore_tick   = 0;
    uint32_t _restore_delay_ms;
    int _heart_decorator_id = -1;
    int _shy_decorator_id   = -1;

    // Hold-to-listen state
    bool _is_touched         = false;  // Press received, Release not yet
    bool _hold_wake_fired    = false;  // wake invoked this touch session
    uint32_t _touch_start_ms = 0;

    // Memory of pre-pet pose
    avatar::Emotion _prev_emotion = avatar::Emotion::Neutral;
    int32_t _prev_yaw             = 0;
    int32_t _prev_pitch           = 0;
};

namespace {

constexpr uint32_t kHeadPetHoldToListenMs = 2000;

}  // namespace

inline HeadPetModifier::HeadPetModifier(uint32_t restoreDelayMs) : _restore_delay_ms(restoreDelayMs)
{
    _signal_connection = GetHAL().onHeadPetGesture.connect([this](HeadPetGesture gesture) {
        if (gesture == HeadPetGesture::Press) {
            _event_press = true;
        } else if (gesture == HeadPetGesture::Release) {
            _event_release = true;
        } else if (gesture == HeadPetGesture::SwipeForward || gesture == HeadPetGesture::SwipeBackward) {
            _event_swipe = true;
        }
    });
}

inline HeadPetModifier::~HeadPetModifier()
{
    GetHAL().onHeadPetGesture.disconnect(_signal_connection);
}

inline void HeadPetModifier::_update(Modifiable& stackchan)
{
    const uint32_t now = GetHAL().millis();

    if (_event_press) {
        _event_press = false;
        _is_touched = true;
        _hold_wake_fired = false;
        _touch_start_ms = now;
        perform_pet_motion(stackchan);
        Application::GetInstance().SendEvent("head_pet_started", "{}");
    }

    if (_event_swipe) {
        _event_swipe = false;
        handle_swipe(stackchan);
    }

    if (_is_touched && !_hold_wake_fired && now - _touch_start_ms >= kHeadPetHoldToListenMs) {
        _hold_wake_fired = true;
        flashWakeFeedback(stackchan);
        Application::GetInstance().WakeWordInvoke("head_pet_hold");
    }

    if (_event_release) {
        _event_release = false;
        _is_touched = false;
        Application::GetInstance().SendEvent("head_pet_ended", "{}");
    }

    if (_is_waiting_restore && now >= _restore_tick) {
        restore_original_state(stackchan);
    }
}

inline void HeadPetModifier::handle_swipe(Modifiable& stackchan)
{
    auto& avatar = stackchan.avatar();
    avatar.removeDecorator(_shy_decorator_id);
    _shy_decorator_id = avatar.addDecorator(std::make_unique<avatar::ShyDecorator>(lv_screen_active(), 1800));
    avatar.setEmotion(avatar::Emotion::Happy);
    _restore_tick = GetHAL().millis() + _restore_delay_ms;
    _is_waiting_restore = true;
}

inline void HeadPetModifier::restore_original_state(Modifiable& stackchan)
{
    auto& avatar = stackchan.avatar();
    auto& motion = stackchan.motion();

    avatar.setEmotion(_prev_emotion);
    avatar.removeDecorator(_heart_decorator_id);
    avatar.removeDecorator(_shy_decorator_id);
    _heart_decorator_id = -1;
    _shy_decorator_id = -1;

    motion.moveWithSpeed(_prev_yaw, _prev_pitch, 400);
    motion.setModifyLock(false);
    avatar.setModifyLock(false);

    _in_happy_state = false;
    _is_waiting_restore = false;
}

inline void HeadPetModifier::perform_pet_motion(Modifiable& stackchan)
{
    auto& avatar = stackchan.avatar();
    auto& motion = stackchan.motion();

    if (!_in_happy_state) {
        _prev_emotion = avatar.getEmotion();
        _prev_yaw = motion.getCurrentYawAngle();
        _prev_pitch = motion.getCurrentPitchAngle();
        _in_happy_state = true;
    }

    avatar.setEmotion(avatar::Emotion::Happy);
    avatar.removeDecorator(_heart_decorator_id);
    _heart_decorator_id = avatar.addDecorator(std::make_unique<avatar::HeartDecorator>(lv_screen_active(), 4000, 500));

    motion.setModifyLock(true);
    avatar.setModifyLock(true);
    motion.moveWithSpeed(0, -120, 500);

    _restore_tick = GetHAL().millis() + _restore_delay_ms;
    _is_waiting_restore = true;
}

inline void HeadPetModifier::flashWakeFeedback(Modifiable& stackchan)
{
    stackchan.leftNeonLight().setColor(0, 50, 0);
}

}  // namespace stackchan
