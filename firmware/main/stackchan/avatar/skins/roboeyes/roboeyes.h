/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "../../avatar/avatar.h"
#include "../../avatar/elements/feature.h"
#include "../default/default.h"

#include <lvgl.h>
#include <smooth_lvgl.hpp>
#include <memory>

namespace stackchan::avatar {

class RoboEyesAvatar : public Avatar {
public:
    lv_color_t primaryColor   = lv_color_white();
    lv_color_t secondaryColor = lv_color_black();

    void init(lv_obj_t* parent, const lv_font_t* font = &lv_font_montserrat_16);
    uitk::lvgl_cpp::Container* getPanel() const override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
};

class RoboEyesEye : public Feature {
public:
    RoboEyesEye(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor, bool isLeftEye);
    ~RoboEyesEye();

    void setPosition(const uitk::Vector2i& position) override;
    void setWeight(int weight) override;
    void setRotation(int rotation) override;
    void setEmotion(const Emotion& emotion) override;
    void setVisible(bool visible) override;
    void setSize(int size) override;
    void _update() override;

private:
    void applyTargetFromState();
    void render();
    int moodTopCoverHeight() const;
    int moodTopCoverRotation() const;

    bool _is_left_eye = false;
    Emotion _emotion = Emotion::Neutral;

    int _current_x = 0;
    int _current_y = 0;
    int _current_width = 74;
    int _current_height = 54;
    int _current_radius = 14;
    int _current_top_cover = 0;
    int _current_bottom_cover = 0;

    int _target_x = 0;
    int _target_y = 0;
    int _target_width = 74;
    int _target_height = 54;
    int _target_radius = 14;
    int _target_top_cover = 0;
    int _target_bottom_cover = 0;

    std::unique_ptr<uitk::lvgl_cpp::Container> _container;
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye;
    std::unique_ptr<uitk::lvgl_cpp::Container> _top_cover;
    std::unique_ptr<uitk::lvgl_cpp::Container> _bottom_cover;
};

class RoboEyesMouth : public Feature {
public:
    RoboEyesMouth(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor);
    ~RoboEyesMouth();

    void setPosition(const uitk::Vector2i& position) override;
    void setWeight(int weight) override;
    void setRotation(int rotation) override;
    void setVisible(bool visible) override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _mouth;
};

}  // namespace stackchan::avatar
