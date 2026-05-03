/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "../../avatar/avatar.h"
#include "../../avatar/elements/feature.h"
#include "roboeyes_engine.h"
#include <lvgl.h>
#include <smooth_lvgl.hpp>
#include <memory>

namespace stackchan::avatar {

/**
 * @brief RoboEyes-style LVGL avatar (eyes only + DefaultSpeechBubble, no mouth).
 */
class RoboEyesAvatar : public Avatar {
public:
    lv_color_t primaryColor   = lv_color_white();
    lv_color_t secondaryColor = lv_color_black();

    RoboEyesAvatar();
    ~RoboEyesAvatar() override;

    void init(lv_obj_t* parent, const lv_font_t* font = &lv_font_montserrat_16);

    uitk::lvgl_cpp::Container* getPanel() const override;

    void update() override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _viewport;
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye_left_lv;
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye_right_lv;
    std::unique_ptr<RoboEyesEngine>             _engine;
};

/**
 * @brief Feature bridge: one logical eye → shared RoboEyesEngine weights / gaze / mood.
 */
class RoboEyesEyeFeature : public Feature {
public:
    RoboEyesEyeFeature(RoboEyesEngine* engine,
                       bool               is_left);
    ~RoboEyesEyeFeature();

    void setPosition(const uitk::Vector2i& position) override;
    void setWeight(int weight) override;
    void setRotation(int rotation) override;
    void setEmotion(const Emotion& emotion) override;
    void setVisible(bool visible) override;
    void setSize(int size) override;

private:
    RoboEyesEngine*             _engine;
    bool                         _left;
};

/**
 * @brief Placeholder mouth (SpeakingModifier-safe no-op).
 */
class RoboEyesMouthFeature : public Feature {
public:
    void setWeight(int weight) override
    {
        Feature::setWeight(weight);
    }
    void setPosition(const uitk::Vector2i& position) override
    {
        Element::setPosition(position);
    }
    void setRotation(int rotation) override
    {
        Element::setRotation(rotation);
    }
    void setVisible(bool visible) override
    {
        Element::setVisible(visible);
    }
};

}  // namespace stackchan::avatar
