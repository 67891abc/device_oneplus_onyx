/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Onyx Light HAL"

#include <log/log.h>

#include "Light.h"

#include <fstream>

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

#define LEDS "/sys/class/leds/"

#define LCD_LED         LEDS "lcd-backlight/"
#define BUTTON_LED      LEDS "button-backlight/"
#define BUTTON1_LED     LEDS "button-backlight1/"
#define RED_LED         LEDS "red/"
#define GREEN_LED       LEDS "green/"
#define BLUE_LED        LEDS "blue/"
#define RGB_LED         LEDS "rgb/"

#define BRIGHTNESS      "brightness"
#define DUTY_PCTS       "duty_pcts"
#define START_IDX       "start_idx"
#define PAUSE_LO        "pause_lo"
#define PAUSE_HI        "pause_hi"
#define RAMP_STEP_MS    "ramp_step_ms"
#define RGB_BLINK       "rgb_blink"

#define RAMP_SIZE 21
#define RAMP_STEP_DURATION 50
static int32_t BRIGHTNESS_RAMP[RAMP_SIZE];

static void set(std::string path, std::string value) {
    std::ofstream file(path);
    file << value;
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

static void handleBacklight(const LightState& state) {
    uint32_t brightness = state.color & 0xFF;
    set(LCD_LED BRIGHTNESS, brightness);
}

static void handleButtons(const LightState& state) {
    uint32_t brightness = state.color & 0xFF;
    set(BUTTON_LED BRIGHTNESS, brightness);
    set(BUTTON1_LED BRIGHTNESS, brightness);
}

static std::string getScaledRamp(uint32_t brightness) {
    std::string ramp, pad;

    for(auto const& step: BRIGHTNESS_RAMP) {
        int32_t scaledStep = step * brightness / 0xFF;
        ramp += pad + std::to_string(scaledStep);
        pad = ",";
    }

    return ramp;
}

static void handleNotification(const LightState& state) {
    uint32_t redBrightness, greenBrightness, blueBrightness, brightness;

    redBrightness = (state.color >> 16) & 0xFF;
    greenBrightness = (state.color >> 8) & 0xFF;
    blueBrightness = state.color & 0xFF;

    brightness = (state.color >> 24) & 0xFF;

    if (brightness != 0xFF) {
        redBrightness = (redBrightness * brightness) / 0xFF;
        greenBrightness = (greenBrightness * brightness) / 0xFF
        blueBrightness = (blueBrightness * brightness) / 0xFF
    }

    /* Disable blinking. */
    set(RGB_LED RGB_BLINK, 0);

    if (state.flashMode == Flash::TIMED) {
        int32_t stepDuration = RAMP_STEP_DURATION;
        int32_t pauseHi = state.flashOnMs - (stepDuration * RAMP_SIZE * 2);
        int32_t pauseLo = state.flashOffMs;

        if (pauseHi < 0) {
            stepDuration = state.flashOnMs / (RAMP_SIZE * 2);
            pauseHi = 0;
        }

        /* Red */
        set(RED_LED START_IDX, 0 * RAMP_SIZE);
        set(RED_LED DUTY_PCTS, getScaledRamp(redBrightness));
        set(RED_LED PAUSE_LO, pauseLo);
        set(RED_LED PAUSE_HI, pauseHi);
        set(RED_LED RAMP_STEP_MS, stepDuration);

        /* Green */
        set(GREEN_LED START_IDX, 1 * RAMP_SIZE);
        set(GREEN_LED DUTY_PCTS, getScaledRamp(greenBrightness));
        set(GREEN_LED PAUSE_LO, pauseLo);
        set(GREEN_LED PAUSE_HI, pauseHi);
        set(GREEN_LED RAMP_STEP_MS, stepDuration);

        /* Blue */
        set(BLUE_LED START_IDX, 2 * RAMP_SIZE);
        set(BLUE_LED DUTY_PCTS, getScaledRamp(blueBrightness));
        set(BLUE_LED PAUSE_LO, pauseLo);
        set(BLUE_LED PAUSE_HI, pauseHi);
        set(BLUE_LED RAMP_STEP_MS, stepDuration);

        /* Enable blinking. */
        set(RGB_LED RGB_BLINK, 1);
    } else {
        set(RED_LED BRIGHTNESS, redBrightness);
        set(GREEN_LED BRIGHTNESS, greenBrightness);
        set(BLUE_LED BRIGHTNESS, blueBrightness);
    }
}

static std::map<Type, std::function<void(const LightState&)>> lights = {
    {Type::BACKLIGHT, handleBacklight},
    {Type::BUTTONS, handleButtons},
    {Type::BATTERY, handleNotification},
    {Type::NOTIFICATIONS, handleNotification},
    {Type::ATTENTION, handleNotification},
};

Light::Light() {
    int i;
    for (i = 0; i < RAMP_SIZE; i++) {
        BRIGHTNESS_RAMP[i] = 100 * i / (RAMP_SIZE - 1);
    }

    ALOGI("YumeMichi: Initial Brightness Ramp...");
    std::lock_guard<std::mutex> lock(globalLock);
}

Return<Status> Light::setLight(Type type, const LightState& state)  {
    auto it = lights.find(type);

    if (it == lights.end()) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    std::lock_guard<std::mutex> lock(globalLock);

    it->second(state);

    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb)  {
    std::vector<Type> types;

    for(auto const& light: lights)
        types.push_back(light.first);

    _hidl_cb(types);

    return Void();
}

} // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
