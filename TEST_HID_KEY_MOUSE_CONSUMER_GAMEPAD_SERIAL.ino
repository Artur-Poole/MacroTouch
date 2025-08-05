#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDGamepad.h"

#define HID_CONSUMER_CONTROL            0x0001
#define HID_CONSUMER_PLAY_PAUSE         0x00CD
#define HID_CONSUMER_SCAN_NEXT_TRACK    0x00B5
#define HID_CONSUMER_SCAN_PREV_TRACK    0x00B6
#define HID_CONSUMER_STOP               0x00B7
#define HID_CONSUMER_MUTE               0x00E2
#define HID_CONSUMER_VOLUME_INCREMENT   0x00E9
#define HID_CONSUMER_VOLUME_DECREMENT   0x00EA

USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
USBHIDConsumerControl Consumer;
USBHIDGamepad Gamepad;

void setup() {
  Serial.begin(115200);    // CDC Serial over USB

  USB.begin();             // Start USB stack
  delay(1000);             // Let host enumerate

  Keyboard.begin();
  Mouse.begin();
  Consumer.begin();
  Gamepad.begin();

  Serial.println("✅ All HID Interfaces Initialized.");

  // Send Play/Pause
  Consumer.press(HID_CONSUMER_PLAY_PAUSE);
  delay(100);  // short press
  Consumer.release();
}

void loop() {
  // No need to send anything yet. Just testing detection.
    // Continuous status updates
  Serial.println("Looping...");
  delay(2000);
}
