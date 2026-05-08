/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "roboeyes.h"

#include <cstdint>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar;

namespace {

static constexpr int kScreenWidth = 320;
static constexpr int kScreenHeight = 240;
static constexpr int kBaseEyeWidth = 74;
static constexpr int kBaseEyeHeight = 54;
static constexpr int kBaseEyeRadius = 14;
static constexpr int kEyeBaseY = -8;
static constexpr int kEyeBaseLeftX = -47;
static constexpr int kEyeBaseRightX = 47;
static constexpr int kLookRangeX = 28;
static constexpr int kLookRangeY = 22;
static constexpr int kTweenDivisor = 3;

static int approach(int current, int target)
{
    int delta = target - current;
    if (delta == 0) {
        return current;
    }
    if (delta > 0 && delta < kTweenDivisor) {
        return target;
    }
    if (delta < 0 && delta > -kTweenDivisor) {
        return target;
    }
    return current + delta / kTweenDivisor;
}

static void style_plain(Container& obj)
{
    obj.setBorderWidth(0);
    obj.removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    obj.setPadding(0, 0, 0, 0);
}

}  // namespace

void RoboEyesAvatar::init(lv_obj_t* parent, const lv_font_t* font)
{
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(kScreenWidth, kScreenHeight);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setBgColor(secondaryColor);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _key_elements.leftEye = std::make_unique<RoboEyesEye>(_panel->get(), primaryColor, secondaryColor, true);
    _key_elements.rightEye = std::make_unique<RoboEyesEye>(_panel->get(), primaryColor, secondaryColor, false);
    _key_elements.mouth = std::make_unique<RoboEyesMouth>(_panel->get(), primaryColor, secondaryColor);
    _key_elements.speechBubble = std::make_unique<DefaultSpeechBubble>(_panel->get(), primaryColor, secondaryColor, font);
}

Container* RoboEyesAvatar::getPanel() const
{
    if (_panel) {
        return _panel.get();
    }
    return nullptr;
}

RoboEyesEye::RoboEyesEye(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor, bool isLeftEye)
{
    _is_left_eye = isLeftEye;

    _container = std::make_unique<Container>(parent);
    _container->setAlign(LV_ALIGN_CENTER);
    _container->setSize(kBaseEyeWidth + 36, kBaseEyeHeight + 36);
    _container->setRadius(0);
    _container->setBgOpa(0);
    _container->setTransformPivot((kBaseEyeWidth + 36) / 2, (kBaseEyeHeight + 36) / 2);
    style_plain(*_container);

    _eye = std::make_unique<Container>(_container->get());
    _eye->setAlign(LV_ALIGN_CENTER);
    _eye->setBgColor(primaryColor);
    style_plain(*_eye);

    _top_cover = std::make_unique<Container>(_container->get());
    _top_cover->setAlign(LV_ALIGN_CENTER);
    _top_cover->setBgColor(secondaryColor);
    _top_cover->setTransformPivot(kBaseEyeWidth / 2, 0);
    style_plain(*_top_cover);

    _bottom_cover = std::make_unique<Container>(_container->get());
    _bottom_cover->setAlign(LV_ALIGN_CENTER);
    _bottom_cover->setBgColor(secondaryColor);
    style_plain(*_bottom_cover);

    setPosition({0, 0});
    setSize(0);
    setWeight(100);
    setRotation(0);
    render();
}

RoboEyesEye::~RoboEyesEye()
{
    _bottom_cover.reset();
    _top_cover.reset();
    _eye.reset();
    _container.reset();
}

void RoboEyesEye::setPosition(const Vector2i& position)
{
    Element::setPosition(position);
    applyTargetFromState();
}

void RoboEyesEye::setWeight(int weight)
{
    Feature::setWeight(weight);
    applyTargetFromState();
}

void RoboEyesEye::setRotation(int rotation)
{
    Element::setRotation(rotation);
    _container->setRotation(rotation);
}

void RoboEyesEye::setEmotion(const Emotion& emotion)
{
    if (getIgnoreEmotion()) {
        return;
    }

    _emotion = emotion;
    switch (emotion) {
        case Emotion::Happy:
            Feature::setWeight(78);
            break;
        case Emotion::Sleepy:
            Feature::setWeight(36);
            break;
        case Emotion::Angry:
        case Emotion::Sad:
        case Emotion::Doubt:
        case Emotion::Neutral:
        default:
            Feature::setWeight(100);
            break;
    }
    applyTargetFromState();
}

void RoboEyesEye::setVisible(bool visible)
{
    Element::setVisible(visible);
    _container->setHidden(!visible);
}

void RoboEyesEye::setSize(int size)
{
    Feature::setSize(size);
    applyTargetFromState();
}

void RoboEyesEye::_update()
{
    _current_x = approach(_current_x, _target_x);
    _current_y = approach(_current_y, _target_y);
    _current_width = approach(_current_width, _target_width);
    _current_height = approach(_current_height, _target_height);
    _current_radius = approach(_current_radius, _target_radius);
    _current_top_cover = approach(_current_top_cover, _target_top_cover);
    _current_bottom_cover = approach(_current_bottom_cover, _target_bottom_cover);
    render();
}

void RoboEyesEye::applyTargetFromState()
{
    int size_delta = map_range(_size, -100, 100, -24, 30);
    int look_x = map_range(_position.x, -100, 100, -kLookRangeX, kLookRangeX);
    int look_y = map_range(_position.y, -100, 100, -kLookRangeY, kLookRangeY);

    bool outer_gaze = (_is_left_eye && _position.x < -45) || (!_is_left_eye && _position.x > 45);
    int curiosity_height = outer_gaze ? 12 : 0;

    _target_width = uitk::clamp(kBaseEyeWidth + size_delta, 28, 112);
    _target_height = uitk::clamp(map_range(_weight, 0, 100, 4, kBaseEyeHeight + size_delta / 2 + curiosity_height), 4, 88);
    _target_radius = uitk::clamp(_target_height / 4 + 2, 4, 22);

    _target_x = (_is_left_eye ? kEyeBaseLeftX : kEyeBaseRightX) + look_x;
    _target_y = kEyeBaseY + look_y + (kBaseEyeHeight - _target_height) / 2;

    _target_top_cover = moodTopCoverHeight();
    _target_bottom_cover = (_emotion == Emotion::Happy) ? _target_height / 2 + 7 : 0;
}

void RoboEyesEye::render()
{
    _container->setPos(_current_x, _current_y);

    _eye->setSize(_current_width, _current_height);
    _eye->setRadius(_current_radius);

    int cover_width = _current_width + 28;
    int top_cover_height = _current_top_cover + 20;
    _top_cover->setSize(cover_width, top_cover_height);
    _top_cover->setRadius(0);
    _top_cover->setY(-(_current_height / 2) - top_cover_height / 2 + _current_top_cover);
    _top_cover->setRotation(moodTopCoverRotation());
    _top_cover->setHidden(_current_top_cover <= 0);

    int bottom_cover_height = _current_bottom_cover + 24;
    _bottom_cover->setSize(cover_width, bottom_cover_height);
    _bottom_cover->setRadius(_current_radius);
    _bottom_cover->setY((_current_height / 2) - _current_bottom_cover + bottom_cover_height / 2);
    _bottom_cover->setHidden(_current_bottom_cover <= 0);
}

int RoboEyesEye::moodTopCoverHeight() const
{
    switch (_emotion) {
        case Emotion::Angry:
        case Emotion::Sad:
        case Emotion::Doubt:
            return _target_height / 2;
        case Emotion::Sleepy:
            return _target_height / 3;
        default:
            return 0;
    }
}

int RoboEyesEye::moodTopCoverRotation() const
{
    if (_emotion == Emotion::Angry || _emotion == Emotion::Doubt) {
        return _is_left_eye ? -180 : 180;
    }
    if (_emotion == Emotion::Sad || _emotion == Emotion::Sleepy) {
        return _is_left_eye ? 150 : -150;
    }
    return 0;
}

RoboEyesMouth::RoboEyesMouth(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor)
{
    _mouth = std::make_unique<Container>(parent);
    _mouth->setAlign(LV_ALIGN_CENTER);
    _mouth->setBgColor(primaryColor);
    style_plain(*_mouth);

    setPosition({0, 0});
    setWeight(0);
    setRotation(0);
}

RoboEyesMouth::~RoboEyesMouth()
{
    _mouth.reset();
}

void RoboEyesMouth::setPosition(const Vector2i& position)
{
    Element::setPosition(position);
    int x = map_range(_position.x, -100, 100, -12, 12);
    int y = 54 + map_range(_position.y, -100, 100, -8, 14);
    _mouth->setPos(x, y);
}

void RoboEyesMouth::setWeight(int weight)
{
    Feature::setWeight(weight);
    int width = map_range(_weight, 0, 100, 42, 54);
    int height = map_range(_weight, 0, 100, 4, 18);
    _mouth->setSize(width, height);
    _mouth->setRadius(height / 2);
}

void RoboEyesMouth::setRotation(int rotation)
{
    Element::setRotation(rotation);
    _mouth->setTransformPivot(_mouth->getWidth() / 2, _mouth->getHeight() / 2);
    _mouth->setRotation(rotation);
}

void RoboEyesMouth::setVisible(bool visible)
{
    Element::setVisible(visible);
    _mouth->setHidden(!visible);
}
