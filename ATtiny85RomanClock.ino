#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

/*  Defines  */

#define FRAME_MAX           8
#define SECOND_MAX          60
#define MINUTE_MAX          60
#define HOUR_MAX            24
#define HOUR_HALF           (HOUR_MAX / 2)
#define ADJUSTER_MAX        85

#define MILLIS_PER_FRAME    (1000 / FRAME_MAX)

#define PIXELS_PIN          1
#define PIXELS_NUMBER       16
#define PIXELS_NUMBER_HOUR  6

#define BUTTON_PIN          0
#define BUTTON_LONG_PRESS   4
#define BUTTON_LONGER_PRESS 16

enum : uint8_t {
    STATE_CLOCK = 0,
    STATE_ADJUST_HOUR,
    STATE_ADJUST_MINUTE,
};

#define BRIGHTNESS_MAX      3

/*  Constants  */

PROGMEM static const uint8_t patternHour[HOUR_HALF] = {
    0x12, 0x01, 0x02, 0x03, 0x0C, 0x04, 0x05, 0x06, 0x07, 0x30, 0x10, 0x18
};

PROGMEM static const uint16_t patternMinute[MINUTE_MAX + 1] = {
    0x000, 0x200, 0x100, 0x300, 0x0C0, 0x080, 0x280, 0x180, 0x380, 0x030, 0x020, 0x060,
    0x120, 0x320, 0x0E0, 0x0A0, 0x2A0, 0x1A0, 0x3A0, 0x038, 0x028, 0x068, 0x128, 0x328,
    0x0E8, 0x0A8, 0x2A8, 0x1A8, 0x3A8, 0x03C, 0x02C, 0x06C, 0x12C, 0x32C, 0x0EC, 0x0AC,
    0x2AC, 0x1AC, 0x3AC, 0x03D, 0x003, 0x013, 0x053, 0x143, 0x0C3, 0x083, 0x283, 0x183,
    0x383, 0x033, 0x002, 0x012, 0x052, 0x142, 0x0C2, 0x082, 0x282, 0x182, 0x382, 0x032, 0x006
};

/*  Variables  */

static Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXELS_NUMBER, PIXELS_PIN, NEO_GRB + NEO_KHZ800);
static unsigned long targetMillis;
static int8_t hour, minute, second, frame;
static uint8_t state, brightness, adjuster;

/*----------------------------------------------------------------------------*/

void setup(void)
{
    initDevices();
    hour = minute = second = frame = adjuster = 0;
    targetMillis = millis() + MILLIS_PER_FRAME;
    state = STATE_CLOCK;
}

void loop(void)
{
    handleButton();
    if (++frame >= FRAME_MAX) {
        frame = 0;
        if (state == STATE_CLOCK) forwardTime();
    }
    updatePixels();
    long waitMillis = targetMillis - millis();
    if (waitMillis > 0) delay(waitMillis);
    targetMillis += MILLIS_PER_FRAME;
    if (++adjuster >= ADJUSTER_MAX) {
        adjuster = 0;
        targetMillis++;
    }
}

/*----------------------------------------------------------------------------*/

static void initDevices(void)
{
    pixels.begin();
    pixels.clear();
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    brightness = BRIGHTNESS_MAX;
    pixels.setBrightness(brightness * 85);
}

static void handleButton(void)
{
    static int8_t counter = 0;
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (counter < BUTTON_LONGER_PRESS) {
            counter++;
            if (counter == BUTTON_LONG_PRESS) onLongPress();
            if (counter == BUTTON_LONGER_PRESS) onLongerPress();
        }
    } else {
        if (counter > 0 && counter < BUTTON_LONG_PRESS) onShortPress();
        counter = 0;
    }
}

static void onShortPress(void)
{
    switch (state) {
        case STATE_CLOCK:
            if (--brightness == 0) brightness = BRIGHTNESS_MAX;
            pixels.setBrightness(brightness * 85);
            break;
        case STATE_ADJUST_HOUR:
            forwardHour();
            break;
        case STATE_ADJUST_MINUTE:
            forwardMinute();
            break;
    }
}

static void onLongPress(void)
{
    if (state != STATE_CLOCK) {
        state = (state == STATE_ADJUST_HOUR) ? STATE_ADJUST_MINUTE : STATE_ADJUST_HOUR;
    }
}

static void onLongerPress(void)
{
    state = (state == STATE_CLOCK) ? STATE_ADJUST_HOUR : STATE_CLOCK;
    second = frame = 0;
}

static void forwardTime(void)
{
    if (++second >= SECOND_MAX) {
        second = 0;
        forwardMinute();
        if (minute == 0) {
            forwardHour();
        }
    }
}

static void forwardMinute(void)
{
    if (++minute >= MINUTE_MAX) minute = 0;
}

static void forwardHour(void)
{
    if (++hour >= HOUR_MAX) hour = 0;
}

static void updatePixels(void)
{
    uint16_t pattern = 0;
    int8_t blinkPos = -1;
    if (state == STATE_CLOCK && frame < brightness) {
        blinkPos = (18 - second * PIXELS_NUMBER / SECOND_MAX) % PIXELS_NUMBER; 
    }
    if (state != STATE_ADJUST_HOUR || (frame & 2) == 0) {
        pattern |= pgm_read_byte(&patternHour[hour % HOUR_HALF]);
    }
    if (state != STATE_ADJUST_MINUTE || (frame & 2) == 0) {
        int8_t tmpMinute = minute;
        if (minute == 0 && second == 0 && state == STATE_CLOCK) tmpMinute = MINUTE_MAX;
        pattern |= pgm_read_word(&patternMinute[tmpMinute]) << PIXELS_NUMBER_HOUR;
    }
    for (int8_t i = 0; i < PIXELS_NUMBER; i++) {
        uint32_t color;
        if (bitRead(pattern, i)) {
            if (i == blinkPos) {
                color = pixels.Color(32, 32, 32);
            } else if (i < PIXELS_NUMBER_HOUR) {
                color = (hour < HOUR_HALF) ? pixels.Color(48, 16, 0) : pixels.Color(0, 16, 64);
            } else {
                color = pixels.Color(8, 32, 8);
            }
        } else if (state == STATE_ADJUST_HOUR && i < PIXELS_NUMBER_HOUR ||
                   state == STATE_ADJUST_MINUTE && i >= PIXELS_NUMBER_HOUR) {
            uint8_t c = 4 - brightness;
            color = pixels.Color(c, c, c);
        } else {
            color = (i == blinkPos) ? pixels.Color(6, 6, 6) : pixels.Color(0, 0, 0);
        }
        if (state == STATE_CLOCK) {
            color = (pixels.getPixelColor(i) + color) >> 1 & 0x7F7F7F;
        }
        pixels.setPixelColor(i, color);
    }
    pixels.show();
}
