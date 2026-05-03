/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 *
 * RoboEyes-style tween (FluxGarage) + LVGL; no GFX dependency.
 */
#include "roboeyes_engine.h"

#include "../../utils/random.h"
#include <hal/hal.h>
#include <lvgl.h>
#include <smooth_lvgl.hpp>
#include <smooth_ui_toolkit.hpp>

#include <algorithm>
#include <cmath>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan;

namespace stackchan::avatar {

static int scl_dim(int dim,
                   float scale)
{
    return std::max(4,
                    static_cast<int>(std::lround(static_cast<float>(dim) * scale)));
}

/** Modifier weight (~25 blink closed, ~100 open) → next height pixel target */
static int height_from_blend(unsigned char blend,
                             int mood_full_height)
{
    const int mh = std::max(14,
                            mood_full_height);
    return uitk::map_range(static_cast<int>(blend),
                           26,
                           100,
                           8,
                           mh);
}

RoboEyesEngine::RoboEyesEngine(Container& viewport,
                               Container& eye_left,
                               Container& eye_right)
    : _viewport(&viewport),
      _eye_left_shape(&eye_left),
      _eye_right_shape(&eye_right)
{
}

void RoboEyesEngine::refreshDefaultGeometryFromMood()
{
    _eye_l_width_default = scl_dim(_base_l_w,
                                   _size_scale);
    _eye_r_width_default = scl_dim(_base_r_w,
                                   _size_scale);

    const int tired_cut_l = _tired_shape ? (_base_l_h * 40 / 100) : 0;
    const int happy_cut_l = (!_tired_shape && _happy_shape) ? (_base_l_h * 13 / 100) : 0;
    _eye_l_height_default  = scl_dim(_base_l_h - tired_cut_l - happy_cut_l,
                                    _size_scale);

    const int tired_cut_r = _tired_shape ? (_base_r_h * 40 / 100) : 0;
    const int happy_cut_r = (!_tired_shape && _happy_shape) ? (_base_r_h * 13 / 100) : 0;
    _eye_r_height_default  = scl_dim(_base_r_h - tired_cut_r - happy_cut_r,
                                    _size_scale);

    _eye_l_height_default = std::max(14,
                                      _eye_l_height_default);
    _eye_r_height_default = std::max(14,
                                      _eye_r_height_default);

    int br_base = static_cast<int>(_base_l_r) - (_angry_shape ? 7 : 0);
    br_base +=
        (!_angry_shape && _happy_shape) ? std::min(10,
                                                    _eye_l_height_default / 5) :
                                         0;
    br_base = std::clamp(br_base,
                         4,
                         std::max(4,
                                 std::min(_eye_l_width_default,
                                           _eye_l_height_default) / 2));
    _eye_l_bo_radius_next    = static_cast<unsigned char>(br_base);
    _eye_r_bo_radius_next    = static_cast<unsigned char>(br_base);
    _eye_l_bo_radius_current = static_cast<unsigned char>(br_base);
    _eye_r_bo_radius_current = static_cast<unsigned char>(br_base);

    if (_angry_shape) {
        _eye_l_width_default += (_eye_l_width_default / 14);
        _eye_r_width_default += (_eye_r_width_default / 14);
    }

    /* When width default changes, keep next in sync for smooth tween */
    _eye_l_width_next = _eye_l_width_default;
    _eye_r_width_next = _eye_r_width_default;
}

void RoboEyesEngine::begin(int width,
                           int height,
                           lv_color_t main_color)
{
    _screen_w   = width;
    _screen_h   = height;
    _main_color = main_color;
    _size_scale =
        1.f;

    _base_l_w     = (_screen_w >= 260) ? 74 : 52;
    _base_l_h     = _base_l_w;
    _base_r_w     = _base_l_w;
    _base_r_h     = _base_l_h;
    _base_l_r     = static_cast<unsigned char>(std::min(26,
                                                        std::max(4,
                                                                 _base_l_h / 6)));
    _base_r_r     = _base_l_r;
    _space_default = (_screen_w >= 260) ? 22 : 14;
    _space_next    = _space_default;
    _space_current = _space_default;

    refreshDefaultGeometryFromMood();

    _l_weight_blend = _r_weight_blend = 100;

    _eye_l_width_current  = (_eye_l_width_next = _eye_l_width_default);
    _eye_r_width_current  = (_eye_r_width_next = _eye_r_width_default);

    _eye_l_height_next    = height_from_blend(_l_weight_blend,
                                               _eye_l_height_default);
    _eye_r_height_next    = height_from_blend(_r_weight_blend,
                                               _eye_r_height_default);
    _eye_l_height_current = _eye_l_height_next;
    _eye_r_height_current = _eye_r_height_next;

    _eye_l_open = true;
    _eye_r_open = true;

    applyCardinalBaseline();
    _eye_rx_next = _eye_lx_next + _eye_l_width_current + _space_current;
    _eye_ry_next =
        _eye_ly_next;
    _eye_lx =
        _eye_lx_next;
    _eye_ly =
        _eye_ly_next;
    _eye_rx =
        _eye_rx_next;
    _eye_ry =
        _eye_ry_next;

    _blink_timer          =
        GetHAL().millis();
    _idle_animation_timer =
        GetHAL().millis();

    _viewport->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _viewport->setSize(_screen_w,
                       _screen_h);
    lv_obj_align(_viewport->get(),
                 LV_ALIGN_CENTER,
                 0,
                 -26);

    for (lv_obj_t* o : {_eye_left_shape->get(),
                        _eye_right_shape->get()}) {
        lv_obj_remove_flag(o,
                           LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_align(o,
                         LV_ALIGN_TOP_LEFT);
    }

    _eye_left_shape->setBorderWidth(0);
    _eye_right_shape->setBorderWidth(0);
    setDisplayColors(main_color);
    tick();
}

void RoboEyesEngine::setDisplayColors(lv_color_t main)
{
    _main_color = main;
    _eye_left_shape->setBgColor(main);
    _eye_right_shape->setBgColor(main);
}

void RoboEyesEngine::setWidth(unsigned char left,
                              unsigned char right)
{
    _base_l_w = static_cast<int>(left);
    _base_r_w = static_cast<int>(right);
    refreshDefaultGeometryFromMood();
}

void RoboEyesEngine::setHeight(unsigned char left,
                               unsigned char right)
{
    _base_l_h = static_cast<int>(left);
    _base_r_h = static_cast<int>(right);
    refreshDefaultGeometryFromMood();
}

void RoboEyesEngine::setBorderRadius(unsigned char left,
                                     unsigned char right)
{
    _base_l_r = left;
    _base_r_r = right;
    refreshDefaultGeometryFromMood();
}

void RoboEyesEngine::setSpaceBetween(int space)
{
    _space_next    = space;
    _space_default = space;
}

void RoboEyesEngine::setCuriosity(bool on)
{
    _curious =
        on;
}

void RoboEyesEngine::setCyclops(bool on)
{
    _cyclops =
        on;
}

void RoboEyesEngine::setSweat(bool on)
{
    _sweat =
        on;
}

void RoboEyesEngine::setHFlicker(bool on,
                                 unsigned char amplitude)
{
    _h_flicker            = on;
    _h_flick_amp           = amplitude;
    _h_flicker_alternate =
        false;
}

void RoboEyesEngine::setVFlicker(bool on,
                                  unsigned char amplitude)
{
    _v_flicker            = on;
    _v_flick_amp           = amplitude;
    _v_flicker_alternate =
        false;
}

void RoboEyesEngine::setAutoblinker(bool active,
                                   int interval_sec,
                                   int variation_sec)
{
    _autoblinker =
        active;
    _blink_interval =
        interval_sec;
    _blink_interval_variation =
        variation_sec;
}

void RoboEyesEngine::setAutoblinker(bool active)
{
    _autoblinker =
        active;
}

void RoboEyesEngine::setIdleMode(bool active,
                                 int interval_sec,
                                 int variation_sec)
{
    _idle =
        active;
    _idle_interval =
        interval_sec;
    _idle_interval_variation =
        variation_sec;
}

void RoboEyesEngine::setIdleMode(bool active)
{
    _idle =
        active;
}

void RoboEyesEngine::setMood(RoboEyesMood mood)
{
    _tired_shape =
        _angry_shape =
            _happy_shape =
                false;
    switch (mood) {
        case RoboEyesMood::Tired:
            _tired_shape =
                true;
            break;
        case RoboEyesMood::Angry:
            _angry_shape =
                true;
            break;
        case RoboEyesMood::Happy:
            _happy_shape =
                true;
            break;
        default:
            break;
    }
    refreshDefaultGeometryFromMood();
}

void RoboEyesEngine::setCardinal(RoboEyesCardinalPosition pos)
{
    _cardinal =
        pos;
}

void RoboEyesEngine::setGazeOffsetNormalized(int vx,
                                             int vy)
{
    _gaze_vx =
        vx;
    _gaze_vy =
        vy;
}

void RoboEyesEngine::setLeftWeight(int w)
{
    _l_weight_blend = static_cast<unsigned char>(uitk::clamp(w,
                                                           0,
                                                           100));
}

void RoboEyesEngine::setRightWeight(int w)
{
    _r_weight_blend = static_cast<unsigned char>(uitk::clamp(w,
                                                             0,
                                                             100));
}

void RoboEyesEngine::setSizeNormalized(int sz)
{
    _size_scale =
        uitk::map_range(sz,
                        -100,
                        100,
                        76,
                        124) /
        100.f;
    refreshDefaultGeometryFromMood();
}

void RoboEyesEngine::close()
{
    close(true,
          true);
}

void RoboEyesEngine::open()
{
    open(true,
          true);
}

void RoboEyesEngine::blink()
{
    blink(true,
          true);
}

void RoboEyesEngine::close(bool left,
                            bool right)
{
    if (left) {
        _eye_l_height_next = 1;
        _eye_l_open        = false;
    }
    if (right) {
        _eye_r_height_next = 1;
        _eye_r_open        = false;
    }
}

void RoboEyesEngine::open(bool left,
                          bool right)
{
    if (left) {
        _eye_l_open =
            true;
    }
    if (right) {
        _eye_r_open =
            true;
    }
}

void RoboEyesEngine::blink(bool left,
                           bool right)
{
    close(left,
          right);
    open(left,
          right);
}

void RoboEyesEngine::animConfused()
{
    _confused        =
        true;
    _confused_toggle =
        true;
}

void RoboEyesEngine::animLaugh()
{
    _laugh        =
        true;
    _laugh_toggle =
        true;
}

void RoboEyesEngine::setLeftShapeVisible(bool v)
{
    _left_shape_visible = v;
}

void RoboEyesEngine::setRightShapeVisible(bool v)
{
    _right_shape_visible = v;
}

int RoboEyesEngine::getScreenConstraintX() const
{
    const int cx = _screen_w - _eye_l_width_current -
        _space_current - _eye_r_width_current;
    return std::max(0,
                    cx);
}

int RoboEyesEngine::getScreenConstraintY() const
{
    /* Match FluxGarage: use nominal left default height */
    return std::max(0,
                    _screen_h - _eye_l_height_default);
}

void RoboEyesEngine::applyCardinalBaseline()
{
    const int cx =
        std::max(0,
                getScreenConstraintX());
    const int cy =
        std::max(0,
                getScreenConstraintY());

    switch (_cardinal) {
        case RoboEyesCardinalPosition::N:
            _eye_lx_next =
                cx / 2;
            _eye_ly_next =
                0;
            break;
        case RoboEyesCardinalPosition::NE:
            _eye_lx_next =
                cx;
            _eye_ly_next =
                0;
            break;
        case RoboEyesCardinalPosition::E:
            _eye_lx_next =
                cx;
            _eye_ly_next =
                cy / 2;
            break;
        case RoboEyesCardinalPosition::SE:
            _eye_lx_next =
                cx;
            _eye_ly_next =
                cy;
            break;
        case RoboEyesCardinalPosition::S:
            _eye_lx_next =
                cx / 2;
            _eye_ly_next =
                cy;
            break;
        case RoboEyesCardinalPosition::SW:
            _eye_lx_next =
                0;
            _eye_ly_next =
                cy;
            break;
        case RoboEyesCardinalPosition::W:
            _eye_lx_next =
                0;
            _eye_ly_next =
                cy / 2;
            break;
        case RoboEyesCardinalPosition::NW:
            _eye_lx_next =
                0;
            _eye_ly_next =
                0;
            break;
        default:
            _eye_lx_next =
                cx / 2;
            _eye_ly_next =
                cy / 2;
            break;
    }
}

void RoboEyesEngine::tick()
{
    const unsigned long ms =
        GetHAL().millis();

    refreshDefaultGeometryFromMood();
    applyCardinalBaseline();
    _eye_rx_next =
        _eye_lx_next + _eye_l_width_current +
        _space_current;
    _eye_ry_next =
        _eye_ly_next;

    /* Modifier-driven height targets (blink / breath) */
    if (_eye_l_open) {
        _eye_l_height_next =
            height_from_blend(_l_weight_blend,
                              _eye_l_height_default);
    }
    else {
        _eye_l_height_next =
            1;
    }
    if (_eye_r_open) {
        _eye_r_height_next =
            height_from_blend(_r_weight_blend,
                              _eye_r_height_default);
    }
    else {
        _eye_r_height_next =
            1;
    }

    /* Curiosity */
    if (_curious) {
        if (_eye_lx_next <= 10) {
            _eye_l_height_offset =
                8;
        } else if (_eye_lx_next >= (getScreenConstraintX() - 10) && _cyclops) {
            _eye_l_height_offset =
                8;
        } else {
            _eye_l_height_offset =
                0;
        }
        if (_eye_rx_next >= _screen_w - _eye_r_width_current - 10 && !_cyclops) {
            _eye_r_height_offset =
                8;
        } else {
            _eye_r_height_offset =
                0;
        }
    } else {
        _eye_l_height_offset =
            _eye_r_height_offset =
                0;
    }

    /* FluxGarage tween (heights / vertical center correction) */
    _eye_l_height_current =
        (_eye_l_height_current + _eye_l_height_next +
         _eye_l_height_offset) /
        2;
    _eye_ly +=
        ((_eye_l_height_default - _eye_l_height_current) / 2);
    _eye_ly -= (_eye_l_height_offset / 2);

    _eye_r_height_current =
        (_eye_r_height_current + _eye_r_height_next +
         _eye_r_height_offset) /
        2;
    _eye_ry +=
        ((_eye_r_height_default - _eye_r_height_current) / 2);
    _eye_ry -= (_eye_r_height_offset / 2);

    if (_eye_l_open) {
        if (_eye_l_height_current <= (1 + _eye_l_height_offset)) {
            _eye_l_height_next =
                height_from_blend(_l_weight_blend,
                                  _eye_l_height_default);
        }
    }
    if (_eye_r_open) {
        if (_eye_r_height_current <= (1 + _eye_r_height_offset)) {
            _eye_r_height_next =
                height_from_blend(_r_weight_blend,
                                  _eye_r_height_default);
        }
    }

    _eye_l_width_current  = (_eye_l_width_current + _eye_l_width_next) / 2;
    _eye_r_width_current  = (_eye_r_width_current + _eye_r_width_next) / 2;
    _space_current        = (_space_current + _space_next) / 2;

    const int gdx =
        uitk::map_range(_gaze_vx,
                        -100,
                        100,
                        -(_screen_w / 10),
                        _screen_w / 10);
    const int gdy =
        uitk::map_range(_gaze_vy,
                        -100,
                        100,
                        -(_screen_h / 12),
                        _screen_h / 12);

    _eye_lx = (_eye_lx + _eye_lx_next + gdx) / 2;
    _eye_ly = (_eye_ly + _eye_ly_next + gdy) / 2;

    _eye_rx_next = _eye_lx_next +
        _eye_l_width_current + _space_current;
    _eye_ry_next =
        _eye_ly_next;

    _eye_rx = (_eye_rx + _eye_rx_next + gdx) / 2;
    _eye_ry = (_eye_ry + _eye_ry_next + gdy) / 2;

    _eye_l_bo_radius_current =
        (_eye_l_bo_radius_current +
         _eye_l_bo_radius_next) /
        2;
    _eye_r_bo_radius_current =
        (_eye_r_bo_radius_current +
         _eye_r_bo_radius_next) /
        2;

    if (_autoblinker &&
        ms >= _blink_timer) {
        blink();
        _blink_timer =
            ms +
            static_cast<unsigned long>(_blink_interval * 1000) +
            static_cast<unsigned long>(
                Random::getInstance().getInt(0,
                                             std::max(0,
                                                     _blink_interval_variation))) *
                1000UL;
    }

    if (_laugh) {
        if (_laugh_toggle) {
            setVFlicker(true,
                        6);
            _laugh_animation_timer =
                ms;
            _laugh_toggle =
                false;
        } else if (ms >= _laugh_animation_timer +
                               static_cast<unsigned long>(_laugh_duration_ms)) {
            setVFlicker(false,
                        0);
            _laugh_toggle =
                true;
            _laugh =
                false;
        }
    }

    if (_confused) {
        if (_confused_toggle) {
            setHFlicker(true,
                       12);
            _confused_animation_timer =
                ms;
            _confused_toggle =
                false;
        } else if (ms >= _confused_animation_timer +
                               static_cast<unsigned long>(_confused_duration_ms)) {
            setHFlicker(false,
                       0);
            _confused_toggle =
                true;
            _confused =
                false;
        }
    }

    if (_idle &&
        ms >= _idle_animation_timer) {
        const int cxx =
            getScreenConstraintX();
        const int cyy =
            getScreenConstraintY();
        _eye_lx_next =
            cxx > 0 ? Random::getInstance().getInt(0,
                                                    cxx)
                    : 0;
        _eye_ly_next =
            cyy > 0 ? Random::getInstance().getInt(0,
                                                    cyy)
                    : 0;
        _idle_animation_timer =
            ms +
            static_cast<unsigned long>(_idle_interval * 1000) +
            static_cast<unsigned long>(
                Random::getInstance().getInt(0,
                                            std::max(0,
                                                    _idle_interval_variation))) *
                1000UL;
    }

    int lx =
        _eye_lx,
        ly =
            _eye_ly;
    int rx =
        _eye_rx,
        ry =
            _eye_ry;

    if (_h_flicker) {
        const int a =
            static_cast<int>(_h_flick_amp);
        if (_h_flicker_alternate) {
            lx += a;
            rx += a;
        } else {
            lx -= a;
            rx -= a;
        }
        _h_flicker_alternate =
            !_h_flicker_alternate;
    }

    if (_v_flicker) {
        const int a =
            static_cast<int>(_v_flick_amp);
        if (_v_flicker_alternate) {
            ly +=
                a;
            ry +=
                a;
        } else {
            ly -=
                a;
            ry -=
                a;
        }
        _v_flicker_alternate =
            !_v_flicker_alternate;
    }

    int rw =
        _eye_r_width_current;
    int rh =
        _eye_r_height_current;

    if (_cyclops) {
        rw =
            rh =
                0;
    }

    if (!_left_shape_visible) {
        _eye_left_shape->setHidden(true);
    }
    else {
        _eye_left_shape->setHidden(false);
        _eye_left_shape->setPos(lx,
                                ly);
        _eye_left_shape->setSize(_eye_l_width_current,
                                 _eye_l_height_current);

        const lv_coord_t lrad =
            LV_CLAMP(
                0,
                static_cast<lv_coord_t>(_eye_l_bo_radius_current),
                static_cast<lv_coord_t>(std::min(_eye_l_width_current,
                                                 _eye_l_height_current) / 2));
        _eye_left_shape->setRadius(lrad);
    }

    if (!_right_shape_visible) {
        _eye_right_shape->setHidden(true);
    }
    else if (!_cyclops &&
             rw >
                 1 &&
             rh >
                 1) {
        _eye_right_shape->setHidden(false);
        _eye_right_shape->setPos(rx,
                                 ry);
        _eye_right_shape->setSize(rw,
                                   rh);

        const lv_coord_t rrad =
            LV_CLAMP(
                0,
                static_cast<lv_coord_t>(_eye_r_bo_radius_current),
                static_cast<lv_coord_t>(std::min(rw,
                                                 rh) / 2));

        _eye_right_shape->setRadius(rrad);
    }
    else {
        _eye_right_shape->setHidden(true);
    }
}

}  // namespace stackchan::avatar
