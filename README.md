# gbc-rgb-led-2.0
## Version 2 of the Gameboy Color RGB LED mod by NiceMitch. 
This update features a streamlined user interface to customize the color options. 
See this link for a video install guide: https://www.youtube.com/watch?v=BZriLR9Ldtk

For flashing this to an Attiny85, you must set:
- efuse: 0xff
- hfuse: 0xdf
- lfuse: 0xE2
_______________________________
You can cycle through several modes in this kit. Navigation through the menus is done by holding Select and pressing Left or Right. You will know you’ve entered a menu when you see the LEDs blink twice. Here are the different menu options with their respective controls, in order:

  - Default Mode
    - No other controls
  - Brightness Mode
    - Press Left or Right to adjust brightness level
  - Color Mode Selection
    - Press Left or Right to change color mode
  - Color Mode Edit
    - See section 2 for details
  - Save All Settings
      - No other controls. Entering this menu will save all selections and reboot the LED controller.

______________________________

In Color Mode Edit, the controls and options will change depending on which color mode you have selected. See the table below for descriptions of each mode and the different customization options available.

- User Preset Mode - Pick a unique color for each button
  - Press Select once to toggle between BUTTON SELECT, HUE SELECT, and SATURATION SELECT.
  - In BUTTON SELECT, Press Left or Right to change which button you would like to edit
  - In HUE SELECT, Press Left or Right to change the hue.
  - In SATURATION SELECT, Press Left or Right to change the saturation. 


- Rainbow Color Mode - Fades between all colors and repeats
  - Press Select once to toggle between SPEED SELECT and SATURATION SELECT.
  - In SPEED SELECT, Press Left or Right to adjust the fade speed.
  - In SATURATION SELECT, Press Left or Right to change the saturation.
- Fairy Color Mode - Twinkles each button and makes it look magical!
  - Press Select once to toggle between SPEED SELECT, SATURATION SELECT, and HUE SELECT.
  - In SPEED SELECT, Press Left or Right to adjust the fade speed.
  - In SATURATION SELECT, Press Left or Right to change the saturation.
  - In HUE SELECT, Press Left or Right to change the hue.

- Solid Color Mode - Display one color across all LEDs.
  - Press Select once to toggle between HUE SELECT and SATURATION SELECT.
  - In HUE SELECT, Press Left or Right to change the hue.
  - In SATURATION SELECT, Press Left or Right to change the saturation.

- To save all settings, Hold Select and press Right.
