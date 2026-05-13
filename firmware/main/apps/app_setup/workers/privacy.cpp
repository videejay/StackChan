/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "workers.h"
#include <mooncake_log.h>
#include <hal/hal.h>

using namespace smooth_ui_toolkit::lvgl_cpp;
using namespace setup_workers;

static std::string _tag = "Setup-Privacy";

FaceDetectionToggleWorker::FaceDetectionToggleWorker()
{
    mclog::info("FaceDetectionToggleWorker start");

    _config = GetHAL().getXiaozhiConfig();

    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setBgColor(lv_color_hex(0xEDF4FF));
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setBorderWidth(0);
    _panel->setSize(320, 240);
    _panel->setRadius(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _label_title = std::make_unique<Label>(*_panel);
    _label_title->setText("Face detection");
    _label_title->setTextFont(&lv_font_montserrat_20);
    _label_title->setTextColor(lv_color_hex(0x26206A));
    _label_title->align(LV_ALIGN_CENTER, 0, -60);

    _label_hint = std::make_unique<Label>(*_panel);
    _label_hint->setText("Camera + ML inference. Off saves RAM.\nConfirm reboots to apply.");
    _label_hint->setTextFont(&lv_font_montserrat_16);
    _label_hint->setTextColor(lv_color_hex(0x615B9E));
    _label_hint->setWidth(280);
    _label_hint->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _label_hint->align(LV_ALIGN_CENTER, 0, -8);

    _switch = std::make_unique<Switch>(*_panel);
    _switch->setSize(64, 36);
    _switch->align(LV_ALIGN_CENTER, 0, 50);
    _switch->setBgColor(lv_color_hex(0xB8D3FD), LV_PART_MAIN);
    _switch->setBgColor(lv_color_hex(0x615B9E), LV_PART_INDICATOR | LV_STATE_CHECKED);
    _switch->setBgColor(lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    _switch->setValue(_config.faceDetectionEnabled);

    _btn_confirm = std::make_unique<Button>(*_panel);
    apply_button_common_style(*_btn_confirm);
    _btn_confirm->align(LV_ALIGN_CENTER, 0, 110);
    _btn_confirm->setSize(150, 48);
    _btn_confirm->label().setText("Confirm");
    _btn_confirm->onClick().connect([this]() { _confirm_flag = true; });
}

void FaceDetectionToggleWorker::update()
{
    if (!_confirm_flag) {
        return;
    }
    _confirm_flag                     = false;
    _config.faceDetectionEnabled      = _switch->getValue();
    GetHAL().setXiaozhiConfig(_config);
    mclog::tagInfo(_tag, "faceDetectionEnabled={}", _config.faceDetectionEnabled);
    _is_done = true;
}
