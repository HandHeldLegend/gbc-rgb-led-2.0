#include <FastLED.h>
#include <EEPROM.h>
#include "config.h"

// PIN setup
// ----------------
#define selPin    PCINT0
#define lPin      PCINT2
#define rPin      PCINT1
#define ledPin    4

// Save check variable to see if save data exists
#define SAVE_CHECK  35

// Led Stuff
// Max brightness to prevent power issues
#define LED_COUNT 9
#define MAX_BRIGHTNESS 80

// Use this boolean to tell the loop when it should update
// the LED colors (so it's not *always* sending color updates)
bool colorSet = false;

// Use this boolean to tell the MCU when to do a button logic check
bool buttonSet = false;

// Definitions for the different color modes
#define COLORMODES      4
#define COLORMODE_USER  0
#define COLORMODE_RGB   1
#define COLORMODE_FAIRY 2
#define COLORMODE_SOLID 3
byte colorMode = 0;
// Set to false to initiate colorInit
bool colorModeSet = false;

// Definitions for the different edit modes
// Idle is when you ain't editin!
#define EDITMODES       4
#define EDIT_IDLE       0
#define EDIT_BRIGHTNESS 1
#define EDIT_COLORMODE  2
#define EDIT_CMSET      3
byte editMode = 0;

// Definitions for the sub-edit slots
// Multi-purpose and vary from mode to mode
#define SED_ZERO  0
#define SED_ONE   1
#define SED_TWO   2
byte seditMode = 0;
// Number used will change per mode.
// Adjust dynamically.
byte seditModes = 0;

// Calculate rainbowcolor VALUE based on saturation
byte scaledRCV(byte saturation)
{
  float tmp = (0.29411 * saturation) + 180;
  return (byte) tmp;
}

// Color related values
// rainbowColor is used whenever we need a chsv value (color in Hue, Saturation, Value/Brightness format)
byte rainbowTime = 192; // Time for adjusting rainbow fade speedl
CHSV rainbowColor = CHSV(0, 255, scaledRCV(255));
byte brightness = 16;

// Primary 'leds' variable which contains all the colors in an array we need to set each LED
CRGB leds[LED_COUNT] = { CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black};
CRGB nextleds[LED_COUNT] = { rainbowColor, rainbowColor, rainbowColor, rainbowColor, rainbowColor, rainbowColor, rainbowColor, rainbowColor, rainbowColor } ;
CRGB oldleds[LED_COUNT];

// LED color preset default. ROYGBBBBP :)
CRGB ledPreset[LED_COUNT] = { 0xff0505, 0xff8605, 0xfff700, 0x22ff00, 0x0022ff, 0x0022ff, 0x0022ff, 0x0022ff, 0xAE00ff };

// User preference object initialization
UserPreference upref;

// button check functions

// Button states for individual buttons
#define NO_PRESS    0
#define HELD        1
#define COMPLETED   2
// For select after another press was done
#define LIMBO       3

byte selState = 0;
byte lState = 0;
byte rState = 0;

// Global button state flag for taking actions in the main loop
// based on different button combos
#define NO_PRESS    0
#define SEL_ONLY    1
#define L_ONLY      2
#define R_ONLY      3
#define SEL_L       4
#define SEL_R       5
byte buttonState = 0;

// LED transition time in ms (not for rainbow or fairy mode)
#define BLEND_TIME  40

// colorChange states
// This is used to help determine which color fade
// type is used and if we should loop
// Idle for no changing
#define COLOR_IDLE    0
// Change for a single change
#define COLOR_CHANGE  1
// Cycle for continuously changing
#define COLOR_CYCLE   2
byte colorState = COLOR_CHANGE;
float colorFade = 0;
int colorDuration = BLEND_TIME;
int counter = 0;

// Function to update button state
byte updateButtonState(byte newValue, byte currentState)
{
    if (currentState == HELD and newValue == HIGH) return COMPLETED;
    else if (currentState == NO_PRESS and newValue == LOW) return HELD;
    else if (currentState == COMPLETED) return NO_PRESS;
    else if (currentState == HELD and newValue == LOW) return HELD;
    else if (currentState == LIMBO and newValue == HIGH) return NO_PRESS;

    return NO_PRESS;
}

// Variables for reading the pins
volatile byte tmpSel;
volatile byte tmpL;
volatile byte tmpR;

// Interrupt function
ISR(PCINT0_vect)
{ 
    // Since an interrupt was triggered
    // One of the pin values changed; let's figure out 
    // which one(s) it was!
    tmpSel = digitalRead(selPin);
    tmpL = digitalRead(lPin);
    tmpR = digitalRead(rPin);

    // Set the buttonSet flag to true so we can run a button logic check!
    buttonSet = true;
}

// Perform a button logic check if the flag is set by the interrupt
void buttonLogicCheck()
{
    // Reset button state
    buttonState = NO_PRESS;
  
    // Update all button states accordingly
    selState = updateButtonState(tmpSel, selState);
    lState = updateButtonState(tmpL, lState);
    rState = updateButtonState(tmpR, rState);

    // Check and update the button state byte flag
    if (selState == HELD or selState == LIMBO)
    {
        if (lState == COMPLETED)
        {
          lState = NO_PRESS;
          selState = LIMBO;
          buttonState = SEL_L;
        }
        else if (rState == COMPLETED)
        {
          rState = NO_PRESS;
          selState = LIMBO;
          buttonState = SEL_R;
        }
    }
    
    else if (selState == COMPLETED)
    {
      selState = NO_PRESS;
      buttonState = SEL_ONLY;
    }
    
    else if (lState == COMPLETED)
    {
      lState = NO_PRESS;
      buttonState = L_ONLY;
    }
    
    else if (rState == COMPLETED) 
    {
      rState = NO_PRESS;
      buttonState = R_ONLY;
    }

    buttonSet = false;
}

// LED Groups
// Subgroups defined. Each number represents one in the LED count
// THE FIRST NUMBER IS THE NUMBER OF ITEMS IN THE ARRAY
// This is a space-saving measure since we have precious space :)
// Full groups for iterating through all button-oriented SG's
#define BUTTON_COUNT  6
byte fg_buttons[6][5] = { 
  { 1, 0 },               // A
  { 1, 1 },               // B
  { 1, 2 },               // Start
  { 1, 3 },               // Select
  { 4, 4,5,6,7 },         // Dpad
  { 1, 8 }                // Power LED
  };
byte buttonIndex = 0;

// Set color of one group. 
// group is the button group 2d array
// color is the color you want
// loadDefault is a bool for whether or not the group is set to load the preset default
void setGroupColor(byte* group, CRGB color, bool loadDefault)
{   
    fill_solid(nextleds, LED_COUNT, CHSV(0,0,0));
  
    for(byte i = 0; i < group[0]; i ++)
    {
        if (loadDefault == true)
        {
          nextleds[group[i+1]] = ledPreset[group[i+1]];
        }
        else 
        {
          nextleds[group[i+1]] = color;
          ledPreset[group[i+1]] = color;
        }
    }

    
}


void setup() {

  pinMode(selPin, INPUT_PULLUP);
  pinMode(lPin, INPUT_PULLUP);
  pinMode(rPin, INPUT_PULLUP);
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);

  delay(20);
  EEPROM.get(0, upref);
  delay(20);

  // Try to load upref data
  if (upref.saved == SAVE_CHECK)
  {
    brightness = upref.brightness;
    colorMode = upref.colorMode;
    rainbowTime = upref.rainbowTime;
    rainbowColor.hue = upref.hue;
    rainbowColor.saturation = upref.saturation;
    rainbowColor.value = scaledRCV(rainbowColor.saturation);
    memcpy(ledPreset, upref.ledPreset, sizeof(ledPreset));
  }

  // Initialize fastLED library. GRB is neo pixel 2020's typically but not always. Check your specifications.
  FastLED.addLeds<WS2812B, ledPin, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(brightness);

  colorInit();

  cli();
  // Set up interrupts
  GIMSK = 0b00100000;    // turns on pin change interrupts
  PCMSK |= (1 << selPin);
  PCMSK |= (1 << lPin);
  PCMSK |= (1 << rPin);
  sei(); 
}

// Function that performs a visual double flash to mark a change
void switchIndicator(CRGB col) {
  FastLED.setBrightness(30);
  for (byte i = 0; i < 2; i++)
  {
    fill_solid(leds, LED_COUNT, col);
    FastLED.show();
    delay(100);
    FastLED.clear();
    FastLED.show();
    delay(100);
  }
  FastLED.setBrightness(brightness);
}


// List out all the button function types here
#define SAT_CHANGE      0
#define HUE_CHANGE      1
#define BUTTON_CHANGE   2
#define SPEED_CHANGE    3

#define COLORMODE_CHANGE  4
#define EDITMODE_CHANGE   5
#define SEDITMODE_CHANGE  6

#define BRIGHTNESS_CHANGE 7
#define SAVE_SETTINGS     8

// List out modifier parameters here
static int SAT_INCREMENT = 8;
static int HUE_INCREMENT = 8;
static int BRIGHTNESS_INCREMENT = 8;
static int RAINBOW_INCREMENT = 8;
static int SPEED_INCREMENT = 8;

// Define function for value changing
byte adjustValue(int changing, int cap, int increment, bool loop)
{
  byte adjusted = 0;
  
  if (changing + increment > cap)
  {
    if (loop == true) 
    {
      return 0;
    }
    else 
    {
      return cap;
    }
  }
  else if (changing + increment < 0)
  {
    if (loop == true) 
    {
      return cap;
    }
    else 
    {
      return 0;
    }
  }
  else adjusted = changing + increment;

  return adjusted;
}

// Define what each button function does and the param it takes in
void buttonFunction(byte functionType, int parameter)
{
  if (functionType == SAT_CHANGE)
  {
    rainbowColor.saturation = adjustValue(rainbowColor.saturation, 255, parameter, false);
    rainbowColor.value = scaledRCV(rainbowColor.saturation);
    colorInit();
  }
  else if (functionType == HUE_CHANGE)
  {
    rainbowColor.hue = adjustValue(rainbowColor.hue, 255, parameter, true);
    colorInit();
  }
  else if (functionType == COLORMODE_CHANGE)
  {
    colorMode = adjustValue(colorMode, COLORMODES-1, parameter, true);
    colorModeSet = false;
    colorInit();
  }
  else if (functionType == EDITMODE_CHANGE)
  {
    switchIndicator(CRGB::Green);
    editMode = adjustValue(editMode, EDITMODES-1, parameter, false);
    seditMode = SED_ZERO;
    colorModeSet = false;
    colorInit();
  }
  else if (functionType == BRIGHTNESS_CHANGE)
  {
    brightness = adjustValue(brightness, MAX_BRIGHTNESS, parameter, false);
    FastLED.setBrightness(brightness);
    FastLED.show();
  }
  else if (functionType == SEDITMODE_CHANGE)
  {
    switchIndicator(CRGB::Red);
    seditMode = adjustValue(seditMode, seditModes-1, parameter, true);
    colorInit();
  }
  else if (functionType == BUTTON_CHANGE)
  {
    buttonIndex = adjustValue(buttonIndex, BUTTON_COUNT-1, parameter, true);
    colorInit();
  }
  else if (functionType == SAVE_SETTINGS)
  {
    if (colorMode != COLORMODE_RGB and colorMode != COLORMODE_USER)
    {
      upref.hue = rainbowColor.hue;
    }

    if (colorMode != COLORMODE_USER)
    {
      upref.saturation = rainbowColor.saturation;
    }
    
    upref.brightness = brightness;
    upref.colorMode = colorMode;
    upref.rainbowTime = rainbowTime;
    upref.saved = SAVE_CHECK;
    memcpy(upref.ledPreset, ledPreset, sizeof(ledPreset));

    EEPROM.put(0,upref);
    delay(100);
    editMode = EDIT_IDLE;
    seditMode = SED_ZERO;
    
    switchIndicator(CRGB::Blue);
    colorInit();
    
  }
  else if (functionType == SPEED_CHANGE)
  {
    rainbowTime = adjustValue(rainbowTime, 248, parameter, false);
    if (rainbowTime < 8) rainbowTime = 8;
    colorInit();
  }

}


// Use this to handle color changing etc
void colorTick(byte duration)
{
    if (colorState == COLOR_IDLE) return;

    if (colorState != COLOR_IDLE and counter == 0)
    {
      memcpy(oldleds, leds, sizeof(leds));
      colorInit();
      colorFade = 0;
    }
    
    if (colorState != COLOR_IDLE and counter < duration)
    {
      colorFade += 256/duration;
      
      for (byte x = 0; x < LED_COUNT; x++)
      {
        leds[x] = blend(oldleds[x], nextleds[x], (int) colorFade-1);
      }
      FastLED.delay(1);
      FastLED.show();
      counter++;
    }
    else
    {
      colorModeSet = true;
      memcpy(leds, nextleds, sizeof(leds));
      FastLED.show();
      counter = 0;
      colorFade = 0;
      
      if (colorState == COLOR_CHANGE)
      {
        colorState = COLOR_IDLE;
        return;
      }
      
    }
}

// Use to initialize colors when changing modes or booting
void colorInit()
{
  counter = 0;

  if (!colorModeSet and upref.saved == SAVE_CHECK)
  {
    rainbowColor = CHSV(upref.hue, upref.saturation, scaledRCV(upref.saturation));
    
  }
  else if (!colorModeSet)
  {
    rainbowColor = CHSV(0,255,scaledRCV(255));
  }
  
  if (colorMode == COLORMODE_USER)
  {
    if (!colorModeSet)
    {
      seditModes = 3;
      buttonIndex = 0;
      if (upref.saved == SAVE_CHECK)
      {
        memcpy(ledPreset, upref.ledPreset, sizeof(ledPreset));
      }
      colorDuration = BLEND_TIME;
    }

    if (editMode != EDIT_CMSET)
    {
      memcpy(nextleds, ledPreset, sizeof(ledPreset));
    }
    else
    {
      setGroupColor(fg_buttons[buttonIndex], rainbowColor, seditMode == SED_ZERO);
    }

    colorState = COLOR_CHANGE;  
  }
  else if (colorMode == COLORMODE_RGB)
  {
    if (!colorModeSet)
    {
      colorDuration = BLEND_TIME;
      seditModes = 2;
      rainbowColor.hue = 0;
    }
    else
    {
      rainbowColor.hue = adjustValue(rainbowColor.hue, 255, RAINBOW_INCREMENT, true);
      colorDuration = rainbowTime;
    }
    colorState = COLOR_CYCLE;
    fill_solid(nextleds, LED_COUNT, rainbowColor);
    
  }
  else if (colorMode == COLORMODE_FAIRY)
  {
    if (!colorModeSet)
    {
      seditModes = 3;
      colorDuration = BLEND_TIME;
      fill_solid(nextleds, LED_COUNT, rainbowColor);
    }
    else
    {
      colorDuration = rainbowTime;
      for (byte i = 0; i < LED_COUNT; i++)
      {
        nextleds[i] = CHSV((byte) random(rainbowColor.hue-32,rainbowColor.hue+32), rainbowColor.saturation, scaledRCV(rainbowColor.saturation));
      }
    }
    colorState = COLOR_CYCLE;
    
  }
  else if (colorMode == COLORMODE_SOLID)
  {
    if (!colorModeSet)
    {
      seditModes = 2;
      colorDuration = BLEND_TIME;
    }

    fill_solid(nextleds, LED_COUNT, rainbowColor);
    colorState = COLOR_CHANGE;
  }
}

// UI TREE
// HERES ALL THE USER INTERFACE AND BUTTON ACTION :)
void uiTree()
{

  // EDIT MODE IDLE
  
  if (editMode == EDIT_IDLE)
  {
    if (buttonState == SEL_R) buttonFunction(EDITMODE_CHANGE, 1);
  }
  else
  {

    // RETURN FUNCTION GLOBAL IF NOT IDLE    
    if (buttonState == SEL_L) buttonFunction(EDITMODE_CHANGE, -1);

    // EDIT BRIGHTNESS
    else if (editMode == EDIT_BRIGHTNESS)
    {
      if (buttonState == L_ONLY)
      {
        buttonFunction(BRIGHTNESS_CHANGE, -1*BRIGHTNESS_INCREMENT);
      }
      else if (buttonState == R_ONLY) 
      {
        buttonFunction(BRIGHTNESS_CHANGE, BRIGHTNESS_INCREMENT);
      }
      else if (buttonState == SEL_R) 
      {
        buttonFunction(EDITMODE_CHANGE, 1);
      }
    }

    // EDIT COLOR MODE
    else if (editMode == EDIT_COLORMODE)
    {
      if (buttonState == L_ONLY) buttonFunction(COLORMODE_CHANGE, -1);
      else if (buttonState == R_ONLY) buttonFunction(COLORMODE_CHANGE, 1);
      else if (buttonState == SEL_R) buttonFunction(EDITMODE_CHANGE, 1);
    }

    // EDIT COLORMODE SET
    else if (editMode == EDIT_CMSET)
    {

      // CMSET GLOBAL SELECT BUTTON CHANGE OPTION
      if (buttonState == SEL_ONLY) buttonFunction(SEDITMODE_CHANGE, 1);

      // CMSET GLOBAL SAVE FUNCTION
      if (buttonState == SEL_R) buttonFunction(SAVE_SETTINGS, 0);

      // CMSET USER PRESET EDIT MODE
      if (colorMode == COLORMODE_USER)
      {
        if (seditMode == SED_ZERO)
        {
          if (buttonState == R_ONLY) buttonFunction(BUTTON_CHANGE, 1);
          else if (buttonState == L_ONLY) buttonFunction(BUTTON_CHANGE, -1);
        }
        else if (seditMode == SED_ONE)
        {
          if (buttonState == R_ONLY) buttonFunction(HUE_CHANGE, HUE_INCREMENT);
          else if (buttonState == L_ONLY) buttonFunction(HUE_CHANGE, -HUE_INCREMENT);
        }
        else if (seditMode == SED_TWO)
        {
          if (buttonState == R_ONLY) buttonFunction(SAT_CHANGE, SAT_INCREMENT);
          else if (buttonState == L_ONLY) buttonFunction(SAT_CHANGE, -SAT_INCREMENT);
        }
        
      }

      // FAIRY AND RGB COMMON OPTION FOR SED_ZERO
      else if ((colorMode == COLORMODE_FAIRY or colorMode == COLORMODE_RGB) and seditMode == SED_ZERO)
      {
        if (buttonState == R_ONLY) buttonFunction(SPEED_CHANGE, SPEED_INCREMENT);
        else if (buttonState == L_ONLY) buttonFunction(SPEED_CHANGE, -SPEED_INCREMENT);
      }

      // Solid color SED_ZERO options and fairy option for hue
      else if ((colorMode == COLORMODE_SOLID and seditMode == SED_ZERO) or (colorMode == COLORMODE_FAIRY and seditMode == SED_TWO))
      {
        if (buttonState == R_ONLY) buttonFunction(HUE_CHANGE, HUE_INCREMENT);
        else if (buttonState == L_ONLY) buttonFunction(HUE_CHANGE, -HUE_INCREMENT);
      }

      // Common options for fairy, rgb, and solid color modes on SED_ONE
      else if ((colorMode == COLORMODE_FAIRY or colorMode == COLORMODE_RGB or colorMode == COLORMODE_SOLID) and seditMode == SED_ONE)
      {
        if (buttonState == R_ONLY) buttonFunction(SAT_CHANGE, SAT_INCREMENT);
        else if (buttonState == L_ONLY) buttonFunction(SAT_CHANGE, -SAT_INCREMENT);
      }

    }
  }

  buttonState = NO_PRESS;
}

// Main program loop
void loop() {

  // When the button flag gets set by an interrupt, run the button logic check.
  if (buttonSet) buttonLogicCheck();

  if (buttonState != NO_PRESS)
  {
    uiTree();
  }

  // If color is not set, set it
  if (colorState != COLOR_IDLE)
  {
    colorTick(colorDuration);
  }

}
