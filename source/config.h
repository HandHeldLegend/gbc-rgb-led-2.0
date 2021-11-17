// USER PREFERENCES STRUCTURE
// ----------------
typedef struct 
{
  byte saved = 0;
  byte brightness;
  byte colorMode;
  byte rainbowTime = 192;
  byte hue = 0;
  byte saturation = 255;
  CRGB ledPreset[9];
} UserPreference;
