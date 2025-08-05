#include <vector>
#include <SPI.h>
#include <TFT_eSPI.h>  // Hardware-specific library
#include "FT6336U.h"

enum ScreenID {
  SCREEN_MAIN,
  SCREEN_VOLUME,
  // ...
};

// ============== Structs & Others, ex: button ================= //

struct UIElement {
  virtual void update() = 0;
  virtual void draw() = 0;
  virtual void destroy() {}
  virtual void handlePress() {}
  virtual bool contains(int px, int py) = 0;
  virtual ~UIElement() {}
};

struct SimpleButton : public UIElement {
  int x, y, w, h;
  uint16_t fillColor;
  uint16_t borderColor;
  String label;
  uint16_t textColor;
  bool isToggle = false;
  bool toggled = false;
  bool pressed = false;
  unsigned long lastPressTime = 0;
  uint16_t pressDuration = 250;

  bool isTransferBtn = false;

  TFT_eSPI* tft = nullptr;
  TFT_eSprite* sprite = nullptr;  // now a pointer

  SimpleButton() {}

  SimpleButton(int x, int y, int w, int h, uint16_t fill, uint16_t border,
               const String& lbl, uint16_t txtColor, bool toggle = false)
    : x(x), y(y), w(w), h(h),
      fillColor(fill), borderColor(border),
      label(lbl), textColor(txtColor), isToggle(toggle) {}

  void initSprite(TFT_eSPI& display) {
    tft = &display;
    sprite = new TFT_eSprite(tft);
    sprite->setColorDepth(16);
    sprite->setTextDatum(MC_DATUM);
    sprite->setTextColor(textColor);
    sprite->createSprite(w, h);
  }

  void setToTransferButton() {
    isTransferBtn = true;
  }

  void draw() override {
    if (!sprite) return;

    sprite->fillSprite(pressed ? borderColor : fillColor);
    sprite->drawRect(0, 0, w, h, borderColor);
    if (label.length() > 0) {
      sprite->drawString(label, w / 2, h / 2, 2);
    }
  }

  bool contains(int px, int py) override {
    return (px >= x && px <= x + w && py >= y && py <= y + h);
  }

  void handlePress() override {
    if (isToggle) {
      toggled = !toggled;
    } else {
      pressed = true;
      lastPressTime = millis();
    }
  }

  void update() override {
    if (!isToggle && pressed && millis() - lastPressTime >= pressDuration) {
      pressed = false;
      draw();
    }
  }

  void destroy() override {
    if (sprite) {
      sprite->deleteSprite();
      delete sprite;
      sprite = nullptr;
    }
  }
};

struct SimpleSlider : public UIElement {
  int x, y, w, h;
  int minVal, maxVal;
  int value;

  uint16_t trackColor = TFT_DARKGREY;
  uint16_t pointerColor = TFT_RED;
  uint16_t bgColor = TFT_BLACK;

  TFT_eSPI* tft = nullptr;
  TFT_eSprite* sprite = nullptr;

  bool vertical = true;
  bool isDragging = false;

  SimpleSlider() {}

  SimpleSlider(int x, int y, int w, int h, int minVal, int maxVal, int initialVal,
               uint16_t trackCol = TFT_DARKGREY, uint16_t pointerCol = TFT_RED, bool vertical = true)
    : x(x), y(y), w(w), h(h),
      minVal(minVal), maxVal(maxVal), value(initialVal),
      trackColor(trackCol), pointerColor(pointerCol),
      vertical(vertical) {}

  void initSprite(TFT_eSPI& display) {
    tft = &display;
    sprite = new TFT_eSprite(tft);
    sprite->setColorDepth(16);
    sprite->createSprite(w, h);
  }

  void draw() override {
    if (!sprite || !tft) return;

    sprite->fillSprite(bgColor);

    if (vertical) {
      sprite->fillRect(w / 2 - 2, 0, 4, h, trackColor);

      int pointerY = map(value, minVal, maxVal, h, 0);  // invert Y for vertical
      sprite->fillCircle(w / 2, pointerY, 6, pointerColor);
    } else {
      sprite->fillRect(0, h / 2 - 2, w, 4, trackColor);

      int pointerX = map(value, minVal, maxVal, 0, w);
      sprite->fillCircle(pointerX, h / 2, 6, pointerColor);
    }

    sprite->pushSprite(x, y);
  }

  void update() override {
    // if (!tft) return;

    // int localX = touchX - x;
    // int localY = touchY - y;

    // if (touchDown) {
    //   int px = vertical ? w / 2 : map(value, minVal, maxVal, 0, w);
    //   int py = vertical ? map(value, minVal, maxVal, h, 0) : h / 2;
    //   int dx = localX - px;
    //   int dy = localY - py;

    //   if (!isDragging && sqrt(dx * dx + dy * dy) < 10) {
    //     isDragging = true;
    //   }
    // } else {
    //   isDragging = false;
    // }

    // if (isDragging) {
    //   if (vertical) {
    //     int newVal = map(localY, h, 0, minVal, maxVal);
    //     value = constrain(newVal, minVal, maxVal);
    //   } else {
    //     int newVal = map(localX, 0, w, minVal, maxVal);
    //     value = constrain(newVal, minVal, maxVal);
    //   }
    // }
  }

  bool contains(int px, int py) override {}

  void handlePress() override {}

  int getValue() const {
    return value;
  }

  void destroy() override {
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

// ============================ Classes and Advanced Structs ============================ //



class UIScreen {
public:
  virtual void begin(TFT_eSPI& tft) = 0;
  virtual void updateTouch(int tx, int ty) = 0;
  virtual void draw() = 0;
  virtual void update() = 0;
  virtual void destroy() = 0;
  virtual ~UIScreen() {}
};

class MainScreen : public UIScreen {
public:
  SimpleButton startButton;      // manager button
  SimpleButton mediaButtons[3];  // prev song, pause/play, next song
  SimpleButton albumCoverBtn;    // Spotify stuff
  std::vector<UIElement*> uiElements;

  SimpleButton** buttonGrid;
  MainScreen() {
    buttonGrid = new SimpleButton*[2];
    for (int i = 0; i < 2; i++) {
      buttonGrid[i] = new SimpleButton[4];
    }
  }

  void begin(TFT_eSPI& tft) override {
    int gap = 10;
    int screenW = 480;
    int screenH = 320;

    // === Setup Grid Macro Buttons ===
    int topMargin = 100;
    int btnGridW = (screenW - (gap * 5)) / 4;
    int btnGridH = (screenH - topMargin - (gap * 3)) / 2;

    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 4; ++col) {
        int x = gap + col * (btnGridW + gap);
        int y = topMargin + gap + row * (btnGridH + gap);

        String label = "B" + String(row * 4 + col + 1);
        buttonGrid[row][col] = SimpleButton(x, y, btnGridW, btnGridH, TFT_LIGHTGREY, TFT_BLACK, label, TFT_BLACK, false);
        buttonGrid[row][col].initSprite(tft);
        uiElements.push_back(&buttonGrid[row][col]);
        buttonGrid[row][col].draw();
      }
    }

    // === Setup Start Button ===
    int startW = btnGridW;
    int startH = topMargin - gap;  // 90 if gap is 10
    startButton = SimpleButton(gap, gap, startW, startH, TFT_NAVY, TFT_WHITE, "Start", TFT_WHITE, false);
    startButton.initSprite(tft);
    uiElements.push_back(&startButton);
    startButton.draw();

    int coverBtnX = gap + 3 * (btnGridW + gap);  // far-right grid column
    int coverBtnY = gap;
    int coverBtnW = btnGridW;
    int coverBtnH = startH;

    albumCoverBtn = SimpleButton(coverBtnX, coverBtnY, coverBtnW, coverBtnH, TFT_MAROON, TFT_WHITE, "Cover", TFT_WHITE, false);
    albumCoverBtn.initSprite(tft);
    albumCoverBtn.setToTransferButton();
    uiElements.push_back(&albumCoverBtn);
    albumCoverBtn.draw();

    // === Setup Media Buttons (centered between Start and Album Cover) ===
    int mediaBtnW = btnGridW / 2;
    int mediaBtnH = startH / 2;
    int totalMediaW = mediaBtnW * 3 + gap * 2;

    int leftX = gap + startW + gap;
    int rightX = coverBtnX;  // albumCoverBtn starts here
    int centerStartX = leftX + ((rightX - leftX) - totalMediaW) / 2;
    int mediaBtnY = gap + (startH - mediaBtnH) / 2;

    for (int i = 0; i < 3; ++i) {
      int x = centerStartX + i * (mediaBtnW + gap);
      String label = "M" + String(i + 1);
      mediaButtons[i] = SimpleButton(x, mediaBtnY, mediaBtnW, mediaBtnH, TFT_DARKGREY, TFT_WHITE, label, TFT_BLACK, false);
      mediaButtons[i].initSprite(tft);
      uiElements.push_back(&mediaButtons[i]);
      mediaButtons[i].draw();
    }
  }

  void updateTouch(int tx, int ty) {
    for (auto* e : uiElements) {
      if (e->contains(tx, ty)) {
        e->handlePress();
        if (e == &albumCoverBtn) {
          // switchTo()
        }
      }
    }
  }

  void update() override {
    // if needed call draw first will be background and other stuff --- non-active elements
    for (auto* e : uiElements) {
      e->update();
    }
  }

  void destroy() override {
    for (auto* el : uiElements) {
      el->destroy();
      delete el;
    }
    uiElements.clear();
  }

  void draw() override {}
};



// ============================ Initial Variables & Definitions ============================ //

// ==== TFT SPI Display Pins FOR [[ ESP32-S3-DevkitC1-1U  ]]====
#define TFT_CS 17    // LCD_CS - TFT Chip Select
#define TFT_RST 15   // LCD_RST - TFT Reset
#define TFT_DC 8     // LCD_RS - TFT Data/Command
#define TFT_MOSI 3   // SDI (MOSI) - SPI MOSI
#define TFT_SCLK 46  // SCK - SPI Clock
#define TFT_BL 16    // LED BL - TFT Backlight
#define TFT_MISO 9   // SDO (MISO) - SPI MISO

// ==== I2C Touch Controller Pins ====
#define I2C_SCL 10    // CTP_SCL - I2C Clock
#define I2C_SDA 12    // CTP_SDA - I2C Data
#define RST_N_PIN 11  // CTP_RST - Touch Reset
#define INT_N_PIN 13  // CTP_INT - Touch Interrupt

// ==== SD Card (Shared SPI Bus) ====
#define SD_CS 35  // SD Card Chip Select

bool DEBUG_MODE = true;

TFT_eSPI tft = TFT_eSPI();                                // Uses settings from User_Setup.h
FT6336U ft6336u(I2C_SDA, I2C_SCL, RST_N_PIN, INT_N_PIN);  // Using custom library for I2C Capactive Touch

String msgToSend = "STARTUP_MSG";

int screenW;
int screenH;

const int numRows = 2;
const int numCols = 4;


TouchPoint lastTouch;
TouchPoint currentTouch;


// ============================== MAIN SETUP & LOOP ============================ //

SimpleSlider* volumeSlider;
UIScreen* currentScreen = nullptr;

void setScreen(UIScreen* screen, TFT_eSPI& tft) {
  if (currentScreen) {
    delete currentScreen;
  }
  currentScreen = screen;
  currentScreen->begin(tft);
}

void switchTo(ScreenID id, TFT_eSPI& tft) {
  switch (id) {
    case SCREEN_MAIN:
      setScreen(new MainScreen(), tft);
      break;
      // case SCREEN_VOLUME:
      //   setScreen(new DiscordScreen(), tft);
      //   break;
      // ...
  }
}

void launchVolumeScreen() {
  tft.fillScreen(TFT_WHITE);

  volumeSlider = new SimpleSlider(60, 40, 40, 200, 0, 100, 50);
  volumeSlider->initSprite(tft);
  volumeSlider->draw();
}

void dimTFT() {
  pinMode(TFT_BL, OUTPUT);
  ledcAttachPin(TFT_BL, 1);  // Attach to PWM channel 1
  ledcSetup(1, 30000, 8);    // Channel 1, 5kHz, 8-bit resolution
  ledcWrite(1, 128);         // Set brightness (0–255)
}

void setupTFT() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  screenW = TFT_HEIGHT;
  screenH = TFT_WIDTH;

  // dimTFT();
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

void setup() {
  Serial.begin(115200);
  // while (!Serial) {delay(20);}

  Serial.println("------------- ESP32-Startup --------------");

  delay(50);  // changed from 1000 -> 50

  setupTFT();

  setupTouchScreen();

  Serial.println("------------- Touch Setup --------------");

  delay(500);

  switchTo(SCREEN_MAIN, tft);

  delay(50);  // changed from 1000 -> 50

  Serial.println("------------- ESP32-S3-C1 --------------");
  Serial.println("------------ TFT, Touch, SD : DISABLED ------------");
}

void handleTouchScreen() {
  uint8_t touches = ft6336u.read_td_status();

  bool msgWaitingOnDelivery = false;

  if (touches > 0) {
    currentTouch.id = ft6336u.read_touch1_id();
    currentTouch.event = ft6336u.read_touch1_event();
    currentTouch.x = ft6336u.read_touch1_y();
    currentTouch.y = 320 - ft6336u.read_touch1_x();  // rotated -45
    currentTouch.weight = ft6336u.read_touch1_weight();
    currentTouch.misc = ft6336u.read_touch1_misc();

    // Compare to previous
    if (!currentTouch.isNear(lastTouch)) {
      // Serial.printf("New Touch at (%d, %d)\n", currentTouch.x, currentTouch.y);

      lastTouch = currentTouch;  // update history
      currentScreen->updateTouch(currentTouch.x, currentTouch.y);
    }
  }
}

void loop() {

  handleTouchScreen();

  currentScreen->update();
}