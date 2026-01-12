/**
 * @file led_controller.h
 * @brief NeoPixel LED Controller for Status Indication
 * @version 1.0.0
 */

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

class LEDController {
public:
    /**
     * @brief Constructor
     * @param pin NeoPixel data pin
     * @param count Number of LEDs
     */
    LEDController(uint8_t pin = NEOPIXEL_PIN, uint8_t count = NEOPIXEL_COUNT);

    /**
     * @brief Initialize LED controller
     */
    void begin();

    /**
     * @brief Set solid color
     * @param color 24-bit RGB color (0xRRGGBB)
     * @param brightness Brightness (0-255)
     */
    void setColor(uint32_t color, uint8_t brightness = 50);

    /**
     * @brief Set color from RGB components
     */
    void setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 50);

    /**
     * @brief Turn off LED
     */
    void off();

    /**
     * @brief Blink LED
     * @param color Color to blink
     * @param count Number of blinks
     * @param onTime On duration (ms)
     * @param offTime Off duration (ms)
     */
    void blink(uint32_t color, uint8_t count = 3, uint16_t onTime = 200, uint16_t offTime = 200);

    /**
     * @brief Start breathing effect (non-blocking)
     * @param color Base color
     */
    void startBreathing(uint32_t color);

    /**
     * @brief Stop breathing effect
     */
    void stopBreathing();

    /**
     * @brief Update LED state (call in loop for effects)
     */
    void update();

    /**
     * @brief Set status-based color
     * @param status System status
     */
    void setStatus(SystemStatus_t status);

    /**
     * @brief Get current color
     */
    uint32_t getCurrentColor() const { return _currentColor; }

    /**
     * @brief Set maximum brightness
     * @param brightness Brightness level (0-100)
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Get current brightness
     */
    uint8_t getBrightness() const { return _brightness; }

private:
    uint8_t     _pin;
    uint8_t     _count;
    uint32_t    _currentColor;
    uint8_t     _brightness;

    // Breathing effect
    bool        _breathing;
    uint32_t    _breathColor;
    uint32_t    _lastBreathUpdate;
    uint8_t     _breathPhase;
    bool        _breathDirection;

    // Internal NeoPixel data
    uint8_t*    _pixels;

    void        _show();
    void        _setPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
    uint8_t     _gamma8(uint8_t x);
};

// Global instance
extern LEDController statusLED;

#endif // LED_CONTROLLER_H
