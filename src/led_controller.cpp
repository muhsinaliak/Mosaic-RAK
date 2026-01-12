/**
 * @file led_controller.cpp
 * @brief NeoPixel LED Controller Implementation
 * @version 1.0.0
 */

#include "led_controller.h"
#include <Adafruit_NeoPixel.h>

// NeoPixel instance (will be initialized in begin())
static Adafruit_NeoPixel* neoPixel = nullptr;

// Global instance
LEDController statusLED(NEOPIXEL_PIN, NEOPIXEL_COUNT);

// Gamma correction table for smoother brightness
static const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

LEDController::LEDController(uint8_t pin, uint8_t count)
    : _pin(pin)
    , _count(count)
    , _currentColor(LED_COLOR_OFF)
    , _brightness(50)
    , _breathing(false)
    , _breathColor(0)
    , _lastBreathUpdate(0)
    , _breathPhase(0)
    , _breathDirection(true)
    , _pixels(nullptr)
{
}

void LEDController::begin() {
    // Initialize NeoPixel
    if (neoPixel == nullptr) {
        neoPixel = new Adafruit_NeoPixel(_count, _pin, NEO_GRB + NEO_KHZ800);
    }

    neoPixel->begin();
    neoPixel->setBrightness(_brightness);
    neoPixel->clear();
    neoPixel->show();

    LOG_INFO("LED", "NeoPixel initialized");

    // Boot animation
    blink(LED_COLOR_WHITE, 2, 100, 100);
}

void LEDController::setColor(uint32_t color, uint8_t brightness) {
    _currentColor = color;
    _brightness = brightness;
    _breathing = false;

    if (neoPixel) {
        neoPixel->setBrightness(brightness);

        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        for (uint8_t i = 0; i < _count; i++) {
            neoPixel->setPixelColor(i, neoPixel->Color(r, g, b));
        }
        neoPixel->show();
    }
}

void LEDController::setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    setColor(color, brightness);
}

void LEDController::off() {
    setColor(LED_COLOR_OFF, 0);
}

void LEDController::blink(uint32_t color, uint8_t count, uint16_t onTime, uint16_t offTime) {
    _breathing = false;

    for (uint8_t i = 0; i < count; i++) {
        setColor(color, _brightness);
        delay(onTime);
        off();
        if (i < count - 1) {
            delay(offTime);
        }
    }
}

void LEDController::startBreathing(uint32_t color) {
    _breathing = true;
    _breathColor = color;
    _breathPhase = 0;
    _breathDirection = true;
    _lastBreathUpdate = millis();
}

void LEDController::stopBreathing() {
    _breathing = false;
}

void LEDController::setBrightness(uint8_t brightness) {
    // Clamp to 0-100 range, then map to 0-255
    if (brightness > 100) brightness = 100;
    _brightness = map(brightness, 0, 100, 0, 255);

    // Apply to current color if not breathing
    if (!_breathing && neoPixel) {
        neoPixel->setBrightness(_brightness);
        neoPixel->show();
    }

    LOG_INFO("LED", ("Brightness set to " + String(brightness) + "%").c_str());
}

void LEDController::update() {
    if (!_breathing) return;

    uint32_t now = millis();
    if (now - _lastBreathUpdate < 20) return;  // 50 FPS
    _lastBreathUpdate = now;

    // Breathing effect: sine wave approximation
    if (_breathDirection) {
        _breathPhase += 3;
        if (_breathPhase >= 255) {
            _breathPhase = 255;
            _breathDirection = false;
        }
    } else {
        if (_breathPhase <= 3) {
            _breathPhase = 0;
            _breathDirection = true;
        } else {
            _breathPhase -= 3;
        }
    }

    // Apply gamma correction for smoother fade
    uint8_t brightness = pgm_read_byte(&gamma8[_breathPhase]);
    brightness = map(brightness, 0, 255, 5, _brightness);

    if (neoPixel) {
        neoPixel->setBrightness(brightness);

        uint8_t r = (_breathColor >> 16) & 0xFF;
        uint8_t g = (_breathColor >> 8) & 0xFF;
        uint8_t b = _breathColor & 0xFF;

        for (uint8_t i = 0; i < _count; i++) {
            neoPixel->setPixelColor(i, neoPixel->Color(r, g, b));
        }
        neoPixel->show();
    }
}

void LEDController::setStatus(SystemStatus_t status) {
    stopBreathing();

    switch (status) {
        case SYS_STATUS_BOOT:
            setColor(LED_COLOR_WHITE, 50);
            break;

        case SYS_STATUS_ETH_CONNECTING:
        case SYS_STATUS_WIFI_CONNECTING:
            startBreathing(LED_COLOR_BLUE);
            break;

        case SYS_STATUS_AP_MODE:
            startBreathing(LED_COLOR_PURPLE);
            break;

        case SYS_STATUS_MQTT_CONNECTING:
            startBreathing(LED_COLOR_CYAN);
            break;

        case SYS_STATUS_ONLINE:
            setColor(LED_COLOR_GREEN, 30);
            break;

        case SYS_STATUS_OFFLINE:
            startBreathing(LED_COLOR_RED);  // Kirmizi nefes efekti
            break;

        case SYS_STATUS_ERROR:
            setColor(LED_COLOR_RED, 50);
            break;

        case SYS_STATUS_FACTORY_RESET:
            startBreathing(LED_COLOR_ORANGE);
            break;

        case SYS_STATUS_OTA_UPDATE:
            startBreathing(LED_COLOR_CYAN);
            break;

        default:
            setColor(LED_COLOR_WHITE, 20);
            break;
    }
}

uint8_t LEDController::_gamma8(uint8_t x) {
    return pgm_read_byte(&gamma8[x]);
}
