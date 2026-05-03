/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "roboeyes.h"
#include "../default/default.h"

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar;

RoboEyesAvatar::RoboEyesAvatar() = default;

RoboEyesAvatar::~RoboEyesAvatar()
{
    /* Features reference _engine; release before engine / LVGL subtree */
    _key_elements.speechBubble.reset();
    _key_elements.mouth.reset();
    _key_elements.rightEye.reset();
    _key_elements.leftEye.reset();
}

void RoboEyesAvatar::init(lv_obj_t* parent,
                          const lv_font_t* font)
{
    const lv_font_t* f = font ? font : (&lv_font_montserrat_16);

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER,
                  0,
                  0);
    _panel->setSize(320,
                     240);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setBgColor(secondaryColor);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _viewport =
        std::make_unique<Container>(_panel->get());
    _viewport->setBgOpa(LV_OPA_TRANSP);
    _viewport->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    /* Size set in engine.begin */

    _eye_left_lv =
        std::make_unique<Container>(_viewport->get());
    _eye_right_lv =
        std::make_unique<Container>(_viewport->get());

    _engine =
        std::make_unique<RoboEyesEngine>(*_viewport,
                                         *_eye_left_lv,
                                         *_eye_right_lv);
    _engine->begin(300,
                   126,
                   primaryColor);
    _engine->setIdleMode(false);
    _engine->setAutoblinker(false);

    _key_elements.leftEye  = std::make_unique<RoboEyesEyeFeature>(_engine.get(),
                                                                  true);
    _key_elements.rightEye = std::make_unique<RoboEyesEyeFeature>(_engine.get(),
                                                                  false);
    _key_elements.mouth    = std::make_unique<RoboEyesMouthFeature>();
    _key_elements.speechBubble =
        std::make_unique<DefaultSpeechBubble>(_panel->get(),
                                              primaryColor,
                                              secondaryColor,
                                              f);
}

Container* RoboEyesAvatar::getPanel() const
{
    return _panel.get();
}

void RoboEyesAvatar::update()
{
    if (_engine) {
        _engine->tick();
    }
    Avatar::update();
}

/* ---------------- RoboEyesEyeFeature ---------------- */

RoboEyesEyeFeature::RoboEyesEyeFeature(RoboEyesEngine* engine,
                                       bool              is_left)
    : _engine(engine),
      _left(is_left)
{
    Feature::setWeight(100);
    if (_engine) {
        if (_left) {
            _engine->setLeftWeight(100);
        } else {
            _engine->setRightWeight(100);
        }
    }
}

RoboEyesEyeFeature::~RoboEyesEyeFeature() = default;

void RoboEyesEyeFeature::setPosition(const uitk::Vector2i& position)
{
    Element::setPosition(position);
    if (!_engine) return;
    /* Shared gaze: last writer wins — idle sets both eyes equally */
    _engine->setGazeOffsetNormalized(position.x,
                                     position.y);
}

void RoboEyesEyeFeature::setWeight(int weight)
{
    Feature::setWeight(weight);
    if (!_engine) return;
    if (_left) {
        _engine->setLeftWeight(weight);
    }
    else {
        _engine->setRightWeight(weight);
    }
}

void RoboEyesEyeFeature::setRotation(int rotation)
{
    Element::setRotation(rotation);
}

void RoboEyesEyeFeature::setEmotion(const Emotion& emotion)
{
    if (_engine == nullptr ||
        getIgnoreEmotion()) return;

    RoboEyesMood m = RoboEyesMood::Default;
    switch (emotion) {
        case Emotion::Happy:
            m = RoboEyesMood::Happy;
            break;
        case Emotion::Angry:
            m = RoboEyesMood::Angry;
            break;
        case Emotion::Sad:
        case Emotion::Sleepy:
            m = RoboEyesMood::Tired;
            break;
        case Emotion::Neutral:
        case Emotion::Doubt:
        default:
            m = RoboEyesMood::Default;
            break;
    }
    _engine->setMood(m);
}

void RoboEyesEyeFeature::setVisible(bool visible)
{
    Element::setVisible(visible);
    if (!_engine) return;
    if (_left) {
        _engine->setLeftShapeVisible(visible);
    }
    else {
        _engine->setRightShapeVisible(visible);
    }
}

void RoboEyesEyeFeature::setSize(int size)
{
    Feature::setSize(size);
    if (!_engine) return;
    _engine->setSizeNormalized(size);
}
