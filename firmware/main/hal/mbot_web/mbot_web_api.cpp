/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "mbot_web_api.h"
#include "mbot_web_stream.h"

#include <board.h>
#include <display.h>
#include <hal/drivers/mbot/mbot_client.h>
#include <hal/hal.h>
#include <stackchan/stackchan.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

struct ActionResult {
    bool ok = false;
    bool hasValue = false;
    uint32_t value = 0;
    char message[64] = {};
};

void resultToJson(const ActionResult& r, ArduinoJson::JsonObject obj)
{
    obj["ok"]      = r.ok;
    obj["message"] = r.message;
    if (r.hasValue) {
        obj["value"] = r.value;
    }
}

ActionResult makeResult(bool ok, const char* msg, uint32_t val = 0, bool hasVal = false)
{
    ActionResult r;
    r.ok       = ok;
    r.hasValue = hasVal;
    r.value    = val;
    if (msg) {
        snprintf(r.message, sizeof(r.message), "%s", msg);
    }
    return r;
}

bool pathIs(const char* path, const char* expect)
{
    return path && expect && strcmp(path, expect) == 0;
}

ActionResult actionPing()
{
    const bool ok = MbotClient::GetInstance().ping();
    return makeResult(ok, ok ? "PING OK" : "PING FAIL");
}

ActionResult actionStop()
{
    const bool ok = MbotClient::GetInstance().stopMotors();
    return makeResult(ok, ok ? "stopped" : "I2C fail");
}

ActionResult actionMotors(int8_t left, int8_t right)
{
    left  = static_cast<int8_t>(std::clamp<int>(left, -128, 127));
    right = static_cast<int8_t>(std::clamp<int>(right, -128, 127));
    const bool ok = MbotClient::GetInstance().setMotors(left, right);
    return makeResult(ok, ok ? "motors OK" : "motors NG");
}

ActionResult actionDistance()
{
    uint16_t mm = 0;
    const bool ok = MbotClient::GetInstance().getDistanceCm(mm);
    if (!ok) {
        return makeResult(false, "distance NG");
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "mm:%u", static_cast<unsigned>(mm));
    return makeResult(true, buf, mm, true);
}

ActionResult actionLineFollower()
{
    uint8_t st = 0xFF;
    const bool ok = MbotClient::GetInstance().getLineFollower(st);
    if (!ok) {
        return makeResult(false, "Line NG");
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "st:%02X", static_cast<unsigned>(st));
    return makeResult(true, buf, st, true);
}

ActionResult actionClear()
{
    const bool ok = MbotClient::GetInstance().clearDisplay();
    return makeResult(ok, ok ? "CLR OK" : "CLR NG");
}

ActionResult actionShowText(const char* text)
{
    const bool ok = MbotClient::GetInstance().displayText(text ? text : "");
    return makeResult(ok, ok ? "TEXT OK" : "TEXT NG");
}

ActionResult actionRgb(uint8_t r, uint8_t g, uint8_t b)
{
    const bool ok = MbotClient::GetInstance().setRgbLed(r, g, b);
    return makeResult(ok, ok ? "RGB OK" : "RGB NG");
}

ActionResult actionServo(uint8_t angle)
{
    const bool ok = MbotClient::GetInstance().setServoAngle(angle);
    return makeResult(ok, ok ? "SERVO OK" : "SERVO NG");
}

ActionResult actionPlayTone(uint16_t freqHz, uint16_t durationMs)
{
    const bool ok = MbotClient::GetInstance().playTone(freqHz, durationMs);
    return makeResult(ok, ok ? "TONE OK" : "TONE NG");
}

void setResult(ArduinoJson::JsonDocument& doc, const ActionResult& r)
{
    resultToJson(r, doc.to<ArduinoJson::JsonObject>());
}

bool avatarReady()
{
    return GetStackChan().hasAvatar();
}

ActionResult avatarNotReadyResult()
{
    return makeResult(false, "avatar not active");
}

bool emotionAllowed(const char* emotion)
{
    if (!emotion || !emotion[0]) {
        return false;
    }
    static const char* kAllowed[] = {"neutral",  "happy",    "angry",    "sad",      "sleepy",
                                     "thinking", "doubtful", "surprised", "loving",   nullptr};
    for (const char** p = kAllowed; *p; ++p) {
        if (strcmp(emotion, *p) == 0) {
            return true;
        }
    }
    return false;
}

ActionResult actionAvatarEmotion(const char* emotion)
{
    if (!avatarReady()) {
        return avatarNotReadyResult();
    }
    if (!emotionAllowed(emotion)) {
        return makeResult(false, "bad emotion");
    }

    LvglLockGuard lock;
    Board::GetInstance().GetDisplay()->SetEmotion(emotion);
    return makeResult(true, "emotion OK");
}

ActionResult actionAvatarSpeech(const char* text)
{
    if (!avatarReady()) {
        return avatarNotReadyResult();
    }

    LvglLockGuard lock;
    Board::GetInstance().GetDisplay()->SetChatMessage("assistant", text ? text : "");
    return makeResult(true, "speech OK");
}

ActionResult actionAvatarSpeechClear()
{
    if (!avatarReady()) {
        return avatarNotReadyResult();
    }

    LvglLockGuard lock;
    Board::GetInstance().GetDisplay()->ClearChatMessages();
    return makeResult(true, "speech cleared");
}

ActionResult actionAvatarHead(int yaw, int pitch, int speed)
{
    if (!avatarReady()) {
        return avatarNotReadyResult();
    }

    yaw   = std::clamp(yaw, -1280, 1280);
    pitch = std::clamp(pitch, 0, 900);
    speed = std::clamp(speed, 100, 1000);

    char json[160];
    snprintf(json, sizeof(json),
             R"({"yawServo":{"angle":%d,"speed":%d},"pitchServo":{"angle":%d,"speed":%d}})", yaw, speed, pitch,
             speed);

    LvglLockGuard lock;
    GetStackChan().updateMotionFromJson(json);
    return makeResult(true, "head OK");
}

ActionResult actionAvatarHeadHome()
{
    return actionAvatarHead(0, 0, 400);
}

void actionAvatarStatus(ArduinoJson::JsonDocument& outDoc)
{
    ArduinoJson::JsonObject obj = outDoc.to<ArduinoJson::JsonObject>();
    obj["ok"]        = true;
    obj["message"]   = "avatar status";
    obj["hasAvatar"] = avatarReady();
    obj["streaming"] = mbotWebStreamIsEnabled();
    if (avatarReady()) {
        LvglLockGuard lock;
        auto& motion      = GetStackChan().motion();
        obj["yaw"]        = motion.yawServo().getCurrentAngle();
        obj["pitch"]      = motion.pitchServo().getCurrentAngle();
    }
}

}  // namespace

void mbotWebHandleApi(const char* method, const char* path, const char* body, ArduinoJson::JsonDocument& outDoc)
{
    ArduinoJson::JsonDocument req;
    if (body && body[0]) {
        deserializeJson(req, body);
    }

    if (pathIs(path, "/api/ping") && (!method || strcmp(method, "GET") == 0)) {
        setResult(outDoc, actionPing());
        return;
    }
    if (pathIs(path, "/api/stop")) {
        setResult(outDoc, actionStop());
        return;
    }
    if (pathIs(path, "/api/distance") && (!method || strcmp(method, "GET") == 0)) {
        setResult(outDoc, actionDistance());
        return;
    }
    if (pathIs(path, "/api/line") && (!method || strcmp(method, "GET") == 0)) {
        setResult(outDoc, actionLineFollower());
        return;
    }
    if (pathIs(path, "/api/clear")) {
        setResult(outDoc, actionClear());
        return;
    }
    if (pathIs(path, "/api/motors")) {
        const int l = req["left"] | 0;
        const int r = req["right"] | 0;
        setResult(outDoc, actionMotors(static_cast<int8_t>(l), static_cast<int8_t>(r)));
        return;
    }
    if (pathIs(path, "/api/text")) {
        const char* t = req["text"] | "";
        setResult(outDoc, actionShowText(t));
        return;
    }
    if (pathIs(path, "/api/rgb")) {
        const uint8_t r = req["r"] | 0;
        const uint8_t g = req["g"] | 0;
        const uint8_t b = req["b"] | 0;
        setResult(outDoc, actionRgb(r, g, b));
        return;
    }
    if (pathIs(path, "/api/servo")) {
        const uint8_t a = req["angle"] | 90;
        setResult(outDoc, actionServo(a));
        return;
    }
    if (pathIs(path, "/api/tone")) {
        const uint16_t f  = req["freq"] | 523;
        const uint16_t ms = req["ms"] | 150;
        setResult(outDoc, actionPlayTone(f, ms));
        return;
    }
    if (pathIs(path, "/api/status") && (!method || strcmp(method, "GET") == 0)) {
        ArduinoJson::JsonObject obj = outDoc.to<ArduinoJson::JsonObject>();
        obj["ok"]      = true;
        obj["message"] = "status";
        obj["ip"]      = GetHAL().getWifiIpAddress().c_str();
        obj["mode"]    = 1;  // STA
        return;
    }
    if (pathIs(path, "/api/avatar/emotion")) {
        const char* e = req["emotion"] | "";
        setResult(outDoc, actionAvatarEmotion(e));
        return;
    }
    if (pathIs(path, "/api/avatar/speech")) {
        const char* t = req["text"] | "";
        setResult(outDoc, actionAvatarSpeech(t));
        return;
    }
    if (pathIs(path, "/api/avatar/speech/clear")) {
        setResult(outDoc, actionAvatarSpeechClear());
        return;
    }
    if (pathIs(path, "/api/avatar/head")) {
        const int yaw   = req["yaw"] | 0;
        const int pitch = req["pitch"] | 0;
        const int speed = req["speed"] | 500;
        setResult(outDoc, actionAvatarHead(yaw, pitch, speed));
        return;
    }
    if (pathIs(path, "/api/avatar/head/home")) {
        setResult(outDoc, actionAvatarHeadHome());
        return;
    }
    if (pathIs(path, "/api/avatar/status") && (!method || strcmp(method, "GET") == 0)) {
        actionAvatarStatus(outDoc);
        return;
    }
    if (pathIs(path, "/api/camera/stream")) {
        mbotWebStreamHandleToggle(body, outDoc);
        return;
    }

    ArduinoJson::JsonObject obj = outDoc.to<ArduinoJson::JsonObject>();
    obj["ok"]      = false;
    obj["message"] = "not found";
}
