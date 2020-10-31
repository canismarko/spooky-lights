/*
  Tomato Timer

  Creates a countdown timer for staying on track during boring work.

  Exampes usage: work for 25 min, then break for 5,
  then work for 25, etc. Every 4 work periods,
  there is a longer 20 minutes break.

  January 2017
  Mark Wolfman
*/

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// Prepare the NeoPixels
#define STRIP_PIN 7
#define RING_PIN 6 // Swap these
#define N_RING 12
#define N_STRIP 75
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_STRIP, STRIP_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel ring = Adafruit_NeoPixel(N_RING, RING_PIN, NEO_GRB + NEO_KHZ800);

// Holds color definitions for both the strip and the ring
struct Color{
  uint32_t strip;
  uint32_t ring;
};

// COLORS
#define BRIGHTNESS 10 // 0-100
#define NEW_BLUE 0, 0, 255
#define BLUE strip.Color(0, 0, 255)
Color TEAL = {strip.Color(0, 128, 64), ring.Color(0, 128, 64)};
Color PURPLE  = {strip.Color(255, 50, 255), ring.Color(255, 50, 255)};
Color GREEN = {strip.Color(0, 255, 0), ring.Color(0, 255, 0)};
Color DARK_PURPLE = {strip.Color(0, 0, 48, 0), strip.Color(0, 48, 48)};
Color DARK_BLUE = {strip.Color(48, 0, 48, 0), strip.Color(48, 0, 48)};
Color BLACK = {strip.Color(0, 0, 0), ring.Color(0, 0, 0)};
Color WHITE = {strip.Color(0, 48, 48, 128), strip.Color(255, 205, 205)};
#define ORANGE 983 // 65536 * 0.015

Color c_stopped;  // Changed by pressing the stop button
#define C_OFF DARK_PURPLE

// Durations for each phase
#define LOOP_TICK 50 // In millisecond
#define DEBUG_SPEEDUP 1 // Set to 1 for normal operation

#define TONE_HZ 200

// Possible states for the system
#define STOPPED 0
#define STARTING 1
#define RUNNING 2
#define BREAK 3
#define EXPIRED 4
#define STOPPING 5

// Button and buzzer pin assignments
#define BTN_START 2
#define BTN_STOP 3
#define BUZZER A1

// Flags for setting different dimming levels for the lights
#define N_DIM_LEVELS 3
#define DIM_OFF 2
#define DIM_LOW 1
#define DIM_HIGH 0
int stopDimLevel = DIM_HIGH;

// Variables for debouncing buttons
#define DEBOUNCE_MS 150
unsigned long lastBtnPress;

// Variable for different light scenes
#define SCENE_DEFAULT 0
#define SCENE_ACTIVE 1
unsigned int scene = SCENE_DEFAULT;

// Variables for interrupting the routine
volatile bool interrupted = false;

// Variable for keeping track of how long this round has been running
unsigned long startTime;

// Variable for keeping track of if the user has silenced the buzzer
bool isSilenced;
#define AUDIBLE 0
#define SILENCED 1

// Function headers
void setAllPixels(Adafruit_NeoPixel &pixel, uint32_t c, bool keepBluePin=false);

bool isNewBtnPress() {
  // Check if it's been long enough since the last button press registered
  //   to consider this a unique button press.
  unsigned long deltaBtnPress = millis() - lastBtnPress;
  lastBtnPress = millis();
  if (deltaBtnPress > DEBOUNCE_MS) {
    return true;
  } else {
    return false;
  }
}

// Function to start a work sesstion when the user presses the button
void start_pressed() {
  if (isNewBtnPress()) {
    scene = SCENE_ACTIVE;
    interrupted = true;
  }
}


void stop_pressed() {
  if (isNewBtnPress()) {
    scene = SCENE_DEFAULT;
    interrupted = true;
  };
}


void setAllPixels(Adafruit_NeoPixel &pixel, uint32_t c, bool keepBluePin=false)
// Set all the neopixels to the given color
{
  for (uint16_t i=0; i < pixel.numPixels(); i++) {
    pixel.setPixelColor(i, c);
  }
  // Set the 12th pin to blue, for the ring timer
  if (keepBluePin) {
    pixel.setPixelColor(pixel.numPixels()-1, BLUE);
  }
  pixel.show();  
}


void setRingLights(int numOn, uint32_t color)
// Set a certain number of LEDs on the ring to on
{
  for (int i=0; i<numOn; i++) {
    ring.setPixelColor(i, color);
  }
  // Set the remaining LEDs off
  for (int i=numOn; i<N_RING; i++) {
    ring.setPixelColor(i, C_OFF.ring);
  }
  ring.show();
}



uint32_t Wheel(byte WheelPos)
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


void pulsate() {
  uint32_t color;
  uint16_t hue = ORANGE; // Orange
  uint8_t val;
  uint32_t delay_time = 0;
  // Fade in
  for (val=0; val < 255; val++) {
    // Serial.print("Seting value to: ");
    // Serial.println(val, DEC);
    color = strip.ColorHSV(hue, 255, val);
    setAllPixels(strip, color);
    strip.show();
    color = ring.ColorHSV(hue, 255, val);
    setAllPixels(ring, color);
    ring.show();
    if (interrupted) {
      interrupted = false;
      return;
    }
    delay(delay_time);
  }
  // Fade out
  for (val=255; val >= 1; val--) {
    // Serial.print("Seting value to: ");
    // Serial.println(val, DEC);
    color = strip.ColorHSV(hue, 255, val);
    setAllPixels(strip, color);
    strip.show();
    color = ring.ColorHSV(hue, 255, val);
    setAllPixels(ring, color);
    ring.show();
    if (interrupted) {
      interrupted = false;
      return;
    }
    delay(delay_time);
  }
}


// Function that fluorishes the lights, then waits for the user to press the "Go" button
void flourish() {
  Serial.print("Pausing init\n");
  uint32_t bg_strip = strip.ColorHSV(ORANGE, 255, 255);
  uint32_t bg_ring = ring.ColorHSV(ORANGE, 255, 255);
  // Cycle through the lights
  for (byte i=0; i < 255; i=i+1) {
    uint32_t color = Wheel(i);
    // Set the strip LED to on
    uint16_t pxIdx = i * N_STRIP / 255;
    strip.setPixelColor(pxIdx, color);
    // Set the ring LED to on
    pxIdx = i * N_RING / 255;
    ring.setPixelColor(pxIdx, color);
    // Update the device and wait for a bit
    strip.show();
    ring.show();
    if (interrupted) {
      interrupted = false;
      return;
    }
    delay(0.5);
  }
  // Cycle off the lights
  for (byte i=0; i < 255; i=i+1) {
    // Set the strip LED to on
    // uint16_t pxIdx = N_STRIP - (i * N_STRIP / 255) - 1;
    uint16_t pxIdx = i * N_STRIP / 255;
    strip.setPixelColor(pxIdx, bg_strip);
    // Set the ring LED off
    // pxIdx = N_RING - (i * N_RING / 255) - 1;
    pxIdx = i * N_RING / 255;
    ring.setPixelColor(pxIdx, bg_ring);
    // Update the device and wait for a bit
    strip.show();
    ring.show();
    delay(0.5);
  }
}


// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(9600);

  // Initialize the Neo Pixels
  strip.begin();
  ring.setBrightness(BRIGHTNESS);
  strip.show(); // Initialize all pixels to 'off'
  ring.begin();
  ring.setBrightness(BRIGHTNESS);
  ring.show(); // Initialize all pixels to 'off'
  
  // Set the interrupt for when the various buttons are pressed
  // attachInterrupt(digitalPinToInterrupt(MUTE_PIN), mute_buzzer, RISING);
  attachInterrupt(digitalPinToInterrupt(BTN_START), start_pressed, RISING);
  attachInterrupt(digitalPinToInterrupt(BTN_STOP), stop_pressed, RISING);

  // Set initial state
  lastBtnPress = millis();
}

void loop() {

  // Figure out how long we've been working so far
  unsigned long mSecondsIn = (millis() - startTime) * DEBUG_SPEEDUP;

  if (scene == SCENE_ACTIVE) {
    flourish();
  } else if (scene == SCENE_DEFAULT) {
    pulsate();
  }
  
  // Sleep until the next round
  delay(LOOP_TICK);
}
