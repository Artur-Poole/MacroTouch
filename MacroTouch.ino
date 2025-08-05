#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include "FT6336U.h"

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDGamepad.h"

// ============== Structs & Others, ex: button ================= //

struct SimpleButton {
  int x, y, w, h;
  uint16_t fillColor;
  uint16_t borderColor;
  String label;
  uint16_t textColor;
  bool isToggle = false;
  bool toggled = false;
  bool pressed = false;
  unsigned long lastPressTime = 0;
  uint16_t pressDuration = 150;  // in ms

  SimpleButton() {}

  SimpleButton(int x, int y, int w, int h, uint16_t fill, uint16_t border, const String &lbl, uint16_t txtColor, bool toggle = false)
    : x(x), y(y), w(w), h(h),
      fillColor(fill), borderColor(border), label(lbl), textColor(txtColor), isToggle(toggle) {}

  void draw(TFT_eSPI &tft, bool overridePressed = false) {
    bool active = overridePressed || (isToggle && toggled);
    tft.fillRect(x, y, w, h, active ? borderColor : fillColor);
    tft.drawRect(x, y, w, h, borderColor);
    if (label.length() > 0) {
      tft.setTextColor(textColor);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, x + w/2, y + h/2, 2);
    }
  }

  bool contains(int px, int py) const {
    return (px >= x && px <= x + w && py >= y && py <= y + h);
  }

  void handlePress(TFT_eSPI &tft) {
    if (isToggle) {
      toggled = !toggled;
      draw(tft);
    } else {
      pressed = true;
      lastPressTime = millis();
      draw(tft, true);  // visually press
    }
  }

  void update(TFT_eSPI &tft) {
    if (!isToggle && pressed && millis() - lastPressTime >= pressDuration) {
      pressed = false;
      draw(tft);
    }
  }
};

struct TouchPoint {
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t weight = 0;
    uint8_t event = 0;
    uint8_t id = 0;
    uint8_t misc = 0;

    bool isValid() const {
        return weight > 0;  // You could also check x/y if needed
    }

    bool isNear(const TouchPoint& other, int tolerance = 5) const {
        int dx = abs(x - other.x);
        int dy = abs(y - other.y);
        return dx <= tolerance && dy <= tolerance;
    }
};

// ============================ Initial Variables & Definitions ============================ //

// ==== TFT SPI Display Pins FOR [[ ESP32-S3-DevkitC1-1U  ]]====
#define TFT_CS     17    // LCD_CS - TFT Chip Select
#define TFT_RST    15    // LCD_RST - TFT Reset
#define TFT_DC      8    // LCD_RS - TFT Data/Command
#define TFT_MOSI    3    // SDI (MOSI) - SPI MOSI
#define TFT_SCLK   46    // SCK - SPI Clock
#define TFT_BL     16    // LED BL - TFT Backlight
#define TFT_MISO    9    // SDO (MISO) - SPI MISO

// ==== I2C Touch Controller Pins ====
#define I2C_SCL    10    // CTP_SCL - I2C Clock
#define I2C_SDA    12    // CTP_SDA - I2C Data
#define RST_N_PIN  11    // CTP_RST - Touch Reset
#define INT_N_PIN  13    // CTP_INT - Touch Interrupt

// ==== SD Card (Shared SPI Bus) ====
#define SD_CS      35    // SD Card Chip Select

// ==== HID Control Keys ====
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

bool DEBUG_MODE = true;

TFT_eSPI tft = TFT_eSPI(); // Uses settings from User_Setup.h
FT6336U ft6336u(I2C_SDA, I2C_SCL, RST_N_PIN, INT_N_PIN); // Using custom library for I2C Capactive Touch

String msgToSend = "STARTUP_MSG";

int screenW;
int screenH;

const int numRows = 2;
const int numCols = 4;

SimpleButton buttonGrid[numRows][numCols];
SimpleButton startButton; // manager button
SimpleButton mediaButtons[3]; // prev song, pause/play, next song
SimpleButton albumCoverBtn; // Spotify stuff

TouchPoint lastTouch;
TouchPoint currentTouch;

// =============== SD CARD Code ================ //

void mountSD() {
  // Manually configure SPI before SD.begin (optional but useful for debug)
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, SD_CS);
  Serial.println("Initializing SD card...");

  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card initialization failed!");
    Serial.println("⚠️ Check: wiring, power, and that LCD_CS is not interfering");
    while (1);
  }
  Serial.println("✅ SD card initialization successful!");
}

// prints contents of root directory      TODO: Add one with given directory if helpful
void printDirectoryContents() {
  // Try opening root dir
  File root = SD.open("/");
  if (!root) {
    Serial.println("❌ Failed to open root directory");
  } else {
    Serial.println("📁 Root directory contents:");
    File file = root.openNextFile();
    while (file) {
      Serial.print(" - ");
      Serial.println(file.name());
      file = root.openNextFile();
    }
  }
}

void dismountSD() {
  SD.end();
}

void runSetupSDScript() {

  delay(50);

  mountSD();
  
  delay(50);

  printDirectoryContents();
  
  delay(50);

  dismountSD();

  delay(50);
}

// ============================== MAIN SETUP & LOOP ============================ //

void launchHomeScreenTFT() {
  int gap = 10;

  // === Setup Grid Macro Buttons ===
  int topMargin = 100;
  int btnGridW = (screenW - (gap * 5)) / 4;
  int btnGridH = (screenH - topMargin - (gap * 3)) / 2;

  for (int row = 0; row < numRows; ++row) {
    for (int col = 0; col < numCols; ++col) {
      int x = gap + col * (btnGridW + gap);
      int y = topMargin + gap + row * (btnGridH + gap);

      String label = "B" + String(row * numCols + col + 1);
      buttonGrid[row][col] = SimpleButton(x, y, btnGridW, btnGridH, TFT_LIGHTGREY, TFT_BLACK, label, TFT_BLACK, false);
      buttonGrid[row][col].draw(tft);
    }
  }

  // === Setup Start Button ===
  int startW = btnGridW;
  int startH = topMargin - gap;  // 90 if gap is 10
  startButton = SimpleButton(gap, gap, startW, startH, TFT_NAVY, TFT_WHITE, "Start", TFT_WHITE, false);
  startButton.draw(tft);

  int coverBtnX = gap + 3 * (btnGridW + gap); // far-right grid column
  int coverBtnY = gap;
  int coverBtnW = btnGridW;
  int coverBtnH = startH;

  albumCoverBtn = SimpleButton(coverBtnX, coverBtnY, coverBtnW, coverBtnH, TFT_MAROON, TFT_WHITE, "Cover", TFT_WHITE, false);
  albumCoverBtn.draw(tft);

  // === Setup Media Buttons (centered between Start and Album Cover) ===
  int mediaBtnW = btnGridW / 2;
  int mediaBtnH = startH / 2;
  int totalMediaW = mediaBtnW * 3 + gap * 2;

  int leftX = gap + startW + gap;
  int rightX = coverBtnX; // albumCoverBtn starts here
  int centerStartX = leftX + ((rightX - leftX) - totalMediaW) / 2;
  int mediaBtnY = gap + (startH - mediaBtnH) / 2;

  for (int i = 0; i < 3; ++i) {
    int x = centerStartX + i * (mediaBtnW + gap);
    String label = "M" + String(i + 1);
    mediaButtons[i] = SimpleButton(x, mediaBtnY, mediaBtnW, mediaBtnH, TFT_DARKGREY, TFT_WHITE, label, TFT_BLACK, false);
    mediaButtons[i].draw(tft);
  }
}

void dimTFT() {
  pinMode(TFT_BL, OUTPUT);
  ledcAttachPin(TFT_BL, 1);      // Attach to PWM channel 1 
  ledcSetup(1, 30000, 8);         // Channel 1, 5kHz, 8-bit resolution
  ledcWrite(1, 128);             // Set brightness (0–255)
}

void setupTFT() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  screenW = TFT_HEIGHT;
  screenH = TFT_WIDTH;

  dimTFT();
}

void setupTouchScreen() {
  ft6336u.begin();

  if (DEBUG_MODE) {
    Serial.print("FT6336U Firmware Version: ");
    Serial.println(ft6336u.read_firmware_id());
    Serial.print("FT6336U Device Mode: ");
    Serial.println(ft6336u.read_device_mode());
  }
  
}

void setupUSBComposite() {
  USB.begin();             // Start USB stack
  delay(1000);             // Let host enumerate

  Keyboard.begin();
  Mouse.begin();
  Consumer.begin();
  Gamepad.begin();

  Serial.println("✅ All HID Interfaces Initialized.");
}

void setup() {
  Serial.begin(115200);
  // while (!Serial) {} // wait for serial to initialize // DONT USE FOR TESTING, this will connect tho with the WINUI program somehow idk how but it will and that will allow for the rest of program to initialize....

  setupUSBComposite();

  runSetupSDScript(); // use SD SPI first????

  delay(50); // changed from 1000 -> 50

  setupTFT();

  setupTouchScreen();

  launchHomeScreenTFT();

  delay(1000); // changed from 2000 -> 1000 -- Confirmed IS OK to have small delay instead of 1 sec

  runSetupSDScript();

  delay(50); // changed from 1000 -> 50

  Serial.println("------------- ESP32-S3-C1 --------------");
  Serial.println("------------ TFT, Touch, SD : ENABLED ------------");
  Serial.println("------------ HID Key, Mouse, Media, Composite : Enalbed ------------");

}

void handleTouchScreen() {
  uint8_t touches = ft6336u.read_td_status();

  bool msgWaitingOnDelivery = false;

  if (touches > 0) {
    currentTouch.id     = ft6336u.read_touch1_id();
    currentTouch.event  = ft6336u.read_touch1_event();
    currentTouch.x      = ft6336u.read_touch1_y(); 
    currentTouch.y      = 320 - ft6336u.read_touch1_x(); // rotated -45
    currentTouch.weight = ft6336u.read_touch1_weight();
    currentTouch.misc   = ft6336u.read_touch1_misc();

    // Compare to previous
    if (!currentTouch.isNear(lastTouch)) {
      // Serial.printf("New Touch at (%d, %d)\n", currentTouch.x, currentTouch.y); 

      // Handle new touch behavior here...

      lastTouch = currentTouch; // update history

      // check if any MACRO button was pressed
      for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < numCols; ++col) {
          if (buttonGrid[row][col].contains(currentTouch.x, currentTouch.y)) {
            buttonGrid[row][col].handlePress(tft);

            int btnIndex = row * 4 + col + 1;  // 1-based index G1–G8
            msgWaitingOnDelivery = true;
            msgToSend = "0:0:G" + String(btnIndex);
            Serial.println("BLE Payload: " + msgToSend);

          }
        }
      }

      // Check Start button
      if (startButton.contains(currentTouch.x, currentTouch.y)) {
        startButton.handlePress(tft);

        msgWaitingOnDelivery = true;

        msgToSend = "2:0:X0";  // or whatever you use for control/start
        Serial.println("BLE Payload: " + msgToSend);

        runSetupSDScript();
      }

      // Check media buttons
      for (int i = 0; i < 3; ++i) {
        if (mediaButtons[i].contains(currentTouch.x, currentTouch.y)) {
          mediaButtons[i].handlePress(tft);

          msgWaitingOnDelivery = true;
          
          switch (i) {
            case 0: 
              Consumer.press(HID_CONSUMER_SCAN_PREV_TRACK); // Play/Pause
              delay(50);  // tap press
              Consumer.release();
              msgToSend = "1:0:MD1"; 
              break; // Prev Song
            case 1:
              // Send Play/Pause
              Consumer.press(HID_CONSUMER_PLAY_PAUSE); // Play/Pause
              delay(50);  // tap press
              Consumer.release();
              msgToSend = "1:0:MD2";
              break;
            case 2: 
              Consumer.press(HID_CONSUMER_SCAN_NEXT_TRACK); // Play/Pause
              delay(50);  // tap press
              Consumer.release();
              msgToSend = "1:0:MD3"; 
              break; // Next Song
          }
          Serial.println("BLE Payload: " + msgToSend);
        }
      }

      if (albumCoverBtn.contains(currentTouch.x, currentTouch.y)) {
        Serial.println("Album cover button pressed");
        albumCoverBtn.handlePress(tft);
      }
      albumCoverBtn.update(tft);
    }
  }

  // update momentary buttons
  for (int row = 0; row < numRows; ++row) {
    for (int col = 0; col < numCols; ++col) {
      buttonGrid[row][col].update(tft);
    }
  }

  startButton.update(tft);
  for (int i = 0; i < 3; ++i) {
    mediaButtons[i].update(tft);
  }
}

void loop() {
  // IDK WHAT TO DO BUT SOMEHOW IT WORKS
  handleTouchScreen();
}