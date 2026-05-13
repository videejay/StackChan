/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 *
 * FluxGarage RoboEyes animation logic ported to LVGL (no Adafruit GFX).
 * Based on FluxGarage/RoboEyes — mood eyelid triangles omitted; shapes express mood only.
 */
#pragma once

#include <cstdint>
#include <lvgl.h>
#include <smooth_lvgl.hpp>

namespace stackchan::avatar {

enum class RoboEyesCardinalPosition : unsigned char {
    Default = 0,
    N  = 1,
    NE = 2,
    E  = 3,
    SE = 4,
    S  = 5,
    SW = 6,
    W  = 7,
    NW = 8,
};

enum class RoboEyesMood : unsigned char {
    Default = 0,
    Tired   = 1,
    Angry   = 2,
    Happy   = 3,
};

/**
 * @brief RoboEyes state + tween; drives two LVGL rounded-rect containers.
 */
class RoboEyesEngine {
public:
    RoboEyesEngine(uitk::lvgl_cpp::Container& viewport,
                   uitk::lvgl_cpp::Container& eye_left,
                   uitk::lvgl_cpp::Container& eye_right);

    /** Logical drawing area matching viewport pixel size */
    void begin(int width, int height, lv_color_t main_color);

    /** One frame of tween + apply to LVGL (call from RoboEyesAvatar::update) */
    void tick();

    void setDisplayColors(lv_color_t main);

    /* -------- Geometry -------- */
    void setWidth(unsigned char left, unsigned char right);
    void setHeight(unsigned char left, unsigned char right);
    void setBorderRadius(unsigned char left, unsigned char right);
    void setSpaceBetween(int space);
    void setCuriosity(bool on);
    void setCyclops(bool on);

    void setSweat(bool on);   // NOP render in this port
    void setHFlicker(bool on, unsigned char amplitude = 2);
    void setVFlicker(bool on, unsigned char amplitude = 10);

    void setAutoblinker(bool active, int interval_sec, int variation_sec);
    void setAutoblinker(bool active);
    void setIdleMode(bool active, int interval_sec, int variation_sec);
    void setIdleMode(bool active);

    void setMood(RoboEyesMood mood);

    /** Predefined gaze (applied before gaze offset from Feature position) */
    void setCardinal(RoboEyesCardinalPosition pos);

    /** Offsets mapped from Avatar Feature coordinates (-100..100) onto pixel gaze */
    void setGazeOffsetNormalized(int vx, int vy);

    /** Per-eye openness for BlinkModifier: low = squinted/closed height */
    void setLeftWeight(int weight_zero_to_hundred);
    void setRightWeight(int weight_zero_to_hundred);

    /** Scale multiplier from Feature::setSize(-100..100) */
    void setSizeNormalized(int sz);

    void close();
    void open();
    void blink();
    void close(bool left, bool right);
    void open(bool left, bool right);
    void blink(bool left, bool right);

    void animConfused();
    void animLaugh();

    void setLeftShapeVisible(bool v);
    void setRightShapeVisible(bool v);

private:
    void refreshDefaultGeometryFromMood();
    int getScreenConstraintX() const;
    int getScreenConstraintY() const;
    void applyCardinalBaseline();

    uitk::lvgl_cpp::Container* _viewport;
    uitk::lvgl_cpp::Container* _eye_left_shape;
    uitk::lvgl_cpp::Container* _eye_right_shape;

    int _screen_w = 300;
    int _screen_h = 120;

    bool _eye_l_open    = true;
    bool _eye_r_open    = true;
    bool _tired_shape   = false;
    bool _angry_shape   = false;
    bool _happy_shape   = false;
    bool _curious       = false;
    bool _cyclops       = false;
    bool _sweat         = false;
    RoboEyesCardinalPosition _cardinal = RoboEyesCardinalPosition::Default;

    bool _h_flicker            = false;
    bool _h_flicker_alternate  = false;
    unsigned char _h_flick_amp = 2;
    bool _v_flicker            = false;
    bool _v_flicker_alternate  = false;
    unsigned char _v_flick_amp = 10;

    bool _left_shape_visible = true;
    bool _right_shape_visible = true;

    bool _autoblinker          = false;
    int _blink_interval        = 1;
    int _blink_interval_variation = 0;
    unsigned long _blink_timer = 0;

    bool _idle                 = false;
    int _idle_interval         = 1;
    int _idle_interval_variation = 0;
    unsigned long _idle_animation_timer = 0;

    bool _confused    = false;
    unsigned long _confused_animation_timer = 0;
    int _confused_duration_ms               = 500;
    bool _confused_toggle                   = true;

    bool _laugh       = false;
    unsigned long _laugh_animation_timer = 0;
    int _laugh_duration_ms               = 500;
    bool _laugh_toggle                   = true;

    int _eye_l_width_default{};
    int _eye_l_height_default{};
    int _eye_l_height_current{};
    int _eye_l_width_current{};
    int _eye_l_width_next{};
    int _eye_l_height_next{};
    int _eye_l_height_offset = 0;
    unsigned char _eye_l_bo_radius_current{};
    unsigned char _eye_l_bo_radius_next{};

    int _eye_r_width_default{};
    int _eye_r_height_default{};
    int _eye_r_height_current{};
    int _eye_r_width_current{};
    int _eye_r_width_next{};
    int _eye_r_height_next{};
    int _eye_r_height_offset = 0;
    unsigned char _eye_r_bo_radius_current{};
    unsigned char _eye_r_bo_radius_next{};

    int _space_default = 14;
    int _space_current = 14;
    int _space_next    = 14;

    int _eye_lx{};
    int _eye_ly{};
    int _eye_lx_next{};
    int _eye_ly_next{};
    int _eye_rx{};
    int _eye_ry{};
    int _eye_rx_next{};
    int _eye_ry_next{};

    int _gaze_vx             = 0;
    int _gaze_vy             = 0;
    unsigned char _l_weight_blend = 100;
    unsigned char _r_weight_blend = 100;
    float _size_scale                                  = 1.f;

    /* Base dims before mood (saved when begin() runs) */
    int _base_l_w{}, _base_l_h{}, _base_r_w{}, _base_r_h{};
    unsigned char _base_l_r{};
    unsigned char _base_r_r{};
    int _base_space{};

    lv_color_t _main_color{};
};

}  // namespace stackchan::avatar
