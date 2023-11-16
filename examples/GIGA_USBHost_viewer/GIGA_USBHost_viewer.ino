//=============================================================================
// Simple test viewer app for several of the USB devices on a display
// This sketch as well as all others in this library will only work with the
// Teensy 3.6 and the T4.x boards.
//
// The default display is the ILI9341, which uses the ILI9341_t3n library,
// which is located at:
//    ili9341_t3n that can be located: https://github.com/KurtE/ILI9341_t3n
//
// Alternate display ST7735 or ST7789 using the ST7735_t3 library which comes
// with Teensyduino.
//
// Default pins
//   8 = RST
//   9 = D/C
//  10 = CS
//
// This example is in the public domain
//=============================================================================
//#define USE_ST77XX // define this if you wish to use one of these displays.
#include <Arduino_USBHostMbed5.h>
#include <USBHostMouseEx.h>
#include <USBHostKeyboardEx.h>

REDIRECT_STDOUT_TO(Serial)

#include "SPI.h"
#include <ILI9341_GIGA_n.h>
#include <ili9341_GIGA_n_font_Arial.h>

#define BLACK ILI9341_BLACK
#define WHITE ILI9341_WHITE
#define YELLOW ILI9341_YELLOW
#define GREEN ILI9341_GREEN
#define RED ILI9341_RED

#define TFT_KEY_SCROLL_START_Y 110
#define TFT_KEY_SCROLL_START_X 20
uint16_t tft_key_cursor_y = 0;

//=============================================================================
// Connection configuration of ILI9341 LCD TFT
//=============================================================================
#define TFT_DC 9
#define TFT_RST 8
#define TFT_CS 10

ILI9341_GIGA_n tft(&SPI1, TFT_CS, TFT_DC, TFT_RST);
//=============================================================================
// USB Host Objects
//=============================================================================
USBHostMouseEx mouse;
USBHostKeyboardEx kbd;

bool mouse_was_connected = false;
bool kbd_was_connected = false;
//=============================================================================
// Other state variables.
//=============================================================================

// Save away values for buttons, x, y, wheel, wheelh
uint32_t buttons_cur = 0;
int x_cur = 0,
    y_cur = 0,
    z_cur = 0;
int x2_cur = 0,
    y2_cur = 0,
    z2_cur = 0,
    L1_cur = 0,
    R1_cur = 0;
int wheel_cur = 0;
int wheelH_cur = 0;
int axis_cur[10];


int user_axis[64];
uint32_t buttons_prev = 0;
uint32_t buttons;

bool show_changed_only = false;
bool show_verbose_debout_output = true;
bool titles_displayed = false;
bool gyro_titles_displayed = false;
bool scroll_box_setup = false;

int16_t x_position_after_device_info = 0;
int16_t y_position_after_device_info = 0;

uint8_t joystick_left_trigger_value = 0;
uint8_t joystick_right_trigger_value = 0;
uint64_t joystick_full_notify_mask = (uint64_t)-1;

uint8_t keyboard_battery_level = 0xff;  // default have nto seen it...


//=============================================================================
// Setup
//=============================================================================
void setup() {
  Serial1.begin(115200);
  while (!Serial && millis() < 3000)
    ;  // wait for Arduino Serial Monitor
  Serial.println("\n\nUSB Host Viewer");

  // The below forceBootProtocol will force which ever
  // next keyboard that attaches to this device to be in boot protocol
  // Only try this if you run into keyboard with issues.  If this is a combined
  // device like wireless mouse and keyboard this can cause mouse problems.
  //keyboard1.forceBootProtocol();

  tft.begin();
  // explicitly set the frame buffer
  //  tft.setFrameBuffer(frame_buffer);
  delay(100);
  tft.setRotation(3);  // 180
  delay(100);

  tft.fillScreen(BLACK);
  tft.setTextColor(YELLOW);
  tft.setTextSize(2);
  tft.println("Waiting for Device...");
  tft.useFrameBuffer(true);

  kbd.attachPress(OnPress);
  kbd.attachHIDPress(OnHIDExtrasPress);


  //  Serial.println("\n============================================");
  //  Serial.println("Simple keyboard commands");
  //  Serial.println("\tI - SWITCH joystick test");
  //  Serial.println("\tc - Toggle debug output show changed only fields");
  //  Serial.println("\tv - Toggle debug output verbose output");
}


//=============================================================================
// Loop
//=============================================================================
void loop() {
  // Update the display with
  UpdateActiveDeviceInfo();

  // Process Mouse Data
  ProcessMouseData();

  // Process Extra keyboard data
  ProcessKeyboardData();
}

//=============================================================================
// UpdateActiveDeviceInfo
//=============================================================================
void UpdateActiveDeviceInfo() {
  // First see if any high level devices
  bool new_device_detected = false;

  // only handle one at a time.
  if (!kbd_was_connected) {
    if (mouse_was_connected) {
      if (!mouse.connected()) {
        Serial.println("*** Mouse disconnected ***");
        mouse_was_connected = false;
      }
    } else {
      if (mouse.connect()) {
        mouse_was_connected = true;
        new_device_detected = true;

        Serial.print("*** Device Mouse ");
        Serial.print(mouse.idVendor(), HEX);
        Serial.print(":");
        Serial.print(mouse.idProduct(), HEX);
        Serial.println("- connected ***");
        tft.fillScreen(BLACK);  // clear the screen.
        tft.setCursor(0, 0);
        tft.setTextColor(YELLOW);
        tft.setFont(Arial_12);
        tft.print("Device Mouse ");
        tft.print(mouse.idVendor(), HEX);
        tft.print(":");
        tft.print(mouse.idProduct(), HEX);
        tft.println(" - connected");

        uint8_t string_buffer[80];
        if (mouse.manufacturer(string_buffer, sizeof(string_buffer))) {
          tft.print("  Manufacturer: ");
          tft.println((char *)string_buffer);
        }

        if (mouse.product(string_buffer, sizeof(string_buffer))) {
          tft.print("  Product: ");
          tft.println((char *)string_buffer);
        }
        if (mouse.serialNumber(string_buffer, sizeof(string_buffer))) {
          tft.print("  Serial: ");
          tft.println((char *)string_buffer);
        }
        tft.updateScreen();  // update the screen now
      }
    }
  }

  if (!mouse_was_connected) {
    // keyboard --- bugbug - will setup combined object soon.
    if (kbd_was_connected) {
      if (!kbd.connected()) {
        Serial.println("*** keyboard disconnected ***");
        kbd_was_connected = false;
      }
    } else {
      if (kbd.connect()) {
        kbd_was_connected = true;
        new_device_detected = true;

        Serial.print("*** Device Keyboard ");
        Serial.print(kbd.idVendor(), HEX);
        Serial.print(":");
        Serial.print(kbd.idProduct(), HEX);
        Serial.println("- connected ***");
        tft.fillScreen(BLACK);  // clear the screen.
        tft.setCursor(0, 0);
        tft.setTextColor(YELLOW);
        tft.setFont(Arial_12);
        tft.print("Device Keyboard ");
        tft.print(kbd.idVendor(), HEX);
        tft.print(":");
        tft.print(kbd.idProduct(), HEX);
        tft.println(" - connected");

        uint8_t string_buffer[80];
        if (kbd.manufacturer(string_buffer, sizeof(string_buffer))) {
          tft.print("  Manufacturer: ");
          tft.println((char *)string_buffer);
        }

        if (kbd.product(string_buffer, sizeof(string_buffer))) {
          tft.print("  Product: ");
          tft.println((char *)string_buffer);
        }
        if (kbd.serialNumber(string_buffer, sizeof(string_buffer))) {
          tft.print("  Serial: ");
          tft.println((char *)string_buffer);
        }
        tft.updateScreen();  // update the screen now
      }
    }
  }


  if (new_device_detected) {
    titles_displayed = false;
    scroll_box_setup = false;
    gyro_titles_displayed = false;


    tft.getCursor(&x_position_after_device_info, &y_position_after_device_info);
    printf("TFT pos after device info: x:%u y:%u\n", x_position_after_device_info, y_position_after_device_info);
  }
}


//=============================================================================
// OutputNumberField
//=============================================================================
void OutputNumberField(int16_t x, int16_t y, int val, int16_t field_width) {
  int16_t x2, y2;
  tft.setCursor(x, y);
  tft.print(val, DEC);
  tft.getCursor(&x2, &y2);
  tft.fillRect(x2, y, field_width - (x2 - x), Arial_12.line_space, BLACK);
}

//=============================================================================
// ProcessMouseData
//=============================================================================
void ProcessMouseData() {
  if (mouse.available()) {
    if (!titles_displayed) {
      // Lets display the titles.
      tft.setCursor(x_position_after_device_info, y_position_after_device_info);
      tft.setTextColor(YELLOW);
      tft.print("Buttons:\nX:\nY:\nWheel:\nWheel H:");
      titles_displayed = true;
    }

    bool something_changed = false;
    if (mouse.getButtons() != buttons_cur) {
      buttons_cur = mouse.getButtons();
      something_changed = true;
    }
    if (mouse.getMouseX() != x_cur) {
      x_cur = mouse.getMouseX();
      something_changed = true;
    }
    if (mouse.getMouseY() != y_cur) {
      y_cur = mouse.getMouseY();
      something_changed = true;
    }
    if (mouse.getWheel() != wheel_cur) {
      wheel_cur = mouse.getWheel();
      something_changed = true;
    }
    if (mouse.getWheelH() != wheelH_cur) {
      wheelH_cur = mouse.getWheelH();
      something_changed = true;
    }
    if (something_changed) {
#define MOUSE_DATA_X 100
      int16_t x, y2;
      unsigned char line_space = Arial_12.line_space;
      tft.setTextColor(WHITE, BLACK);
      //tft.setTextDatum(BR_DATUM);
      int16_t y = y_position_after_device_info;
      tft.setCursor(MOUSE_DATA_X, y);
      tft.print(buttons_cur, DEC);
      tft.print("(");
      tft.print(buttons_cur, HEX);
      tft.print(")");
      tft.getCursor(&x, &y2);
      tft.fillRect(x, y, 320, line_space, BLACK);

      y += line_space;
      OutputNumberField(MOUSE_DATA_X, y, x_cur, 320);
      y += line_space;
      OutputNumberField(MOUSE_DATA_X, y, y_cur, 320);
      y += line_space;
      OutputNumberField(MOUSE_DATA_X, y, wheel_cur, 320);
      y += line_space;
      OutputNumberField(MOUSE_DATA_X, y, wheelH_cur, 320);
      tft.updateScreen();  // update the screen now
    }

    mouse.mouseDataClear();
  }
}
void MaybeSetupTextScrollArea() {
  tft.setFont(Arial_11);
  if (!scroll_box_setup) {
    scroll_box_setup = true;

    int16_t x1, y1;
    uint16_t w, h_line_space;
    tft.getTextBounds((const uint8_t *)"\n", 1, 0, 0, &x1, &y1, &w, &h_line_space);

    tft.enableScroll();
    tft.setScrollTextArea(TFT_KEY_SCROLL_START_X, TFT_KEY_SCROLL_START_Y, tft.width() - (2 * TFT_KEY_SCROLL_START_X),
                          tft.height() - (TFT_KEY_SCROLL_START_Y + h_line_space));
    tft.setScrollBackgroundColor(GREEN);
    tft_key_cursor_y = tft.height() - (2 * h_line_space);
  }

  tft.setTextColor(BLACK);
  tft.setCursor(TFT_KEY_SCROLL_START_X, tft_key_cursor_y);
}

void OnPress(uint8_t key) {
  MaybeSetupTextScrollArea();
  tft.print("key: ");
  switch (key) {
    case USBHostKeyboardEx::KEYD_UP: tft.print("UP"); break;
    case USBHostKeyboardEx::KEYD_DOWN: tft.print("DN"); break;
    case USBHostKeyboardEx::KEYD_LEFT: tft.print("LEFT"); break;
    case USBHostKeyboardEx::KEYD_RIGHT: tft.print("RIGHT"); break;
    case USBHostKeyboardEx::KEYD_INSERT: tft.print("Ins"); break;
    case USBHostKeyboardEx::KEYD_DELETE: tft.print("Del"); break;
    case USBHostKeyboardEx::KEYD_PAGE_UP: tft.print("PUP"); break;
    case USBHostKeyboardEx::KEYD_PAGE_DOWN: tft.print("PDN"); break;
    case USBHostKeyboardEx::KEYD_HOME: tft.print("HOME"); break;
    case USBHostKeyboardEx::KEYD_END: tft.print("END"); break;
    case USBHostKeyboardEx::KEYD_F1: tft.print("F1"); break;
    case USBHostKeyboardEx::KEYD_F2: tft.print("F2"); break;
    case USBHostKeyboardEx::KEYD_F3: tft.print("F3"); break;
    case USBHostKeyboardEx::KEYD_F4: tft.print("F4"); break;
    case USBHostKeyboardEx::KEYD_F5: tft.print("F5"); break;
    case USBHostKeyboardEx::KEYD_F6: tft.print("F6"); break;
    case USBHostKeyboardEx::KEYD_F7: tft.print("F7"); break;
    case USBHostKeyboardEx::KEYD_F8: tft.print("F8"); break;
    case USBHostKeyboardEx::KEYD_F9: tft.print("F9"); break;
    case USBHostKeyboardEx::KEYD_F10: tft.print("F10"); break;
    case USBHostKeyboardEx::KEYD_F11: tft.print("F11"); break;
    case USBHostKeyboardEx::KEYD_F12: tft.print("F12"); break;
    default: tft.print((char)key); break;
  }
  tft.print("'  ");
  tft.print(key);
  tft.print(" MOD: ");
    tft.print(kbd.getModifiers(), HEX);
    tft.print(" OEM: ");
    tft.print(kbd.getOemKey(), HEX);
    tft.print(" LEDS: ");
    tft.println(kbd.LEDS(), HEX);
  tft.updateScreen();  // update the screen now
}

void OnHIDExtrasPress(uint32_t top, uint16_t key) {
  MaybeSetupTextScrollArea();
  tft.print("HID (");
  tft.print(top, HEX);
  tft.print(") key press:");
  tft.print(key, HEX);
  if (top == 0xc) {
    switch (key) {
      case 0x20: tft.print(" - +10"); break;
      case 0x21: tft.print(" - +100"); break;
      case 0x22: tft.print(" - AM/PM"); break;
      case 0x30: tft.print(" - Power"); break;
      case 0x31: tft.print(" - Reset"); break;
      case 0x32: tft.print(" - Sleep"); break;
      case 0x33: tft.print(" - Sleep After"); break;
      case 0x34: tft.print(" - Sleep Mode"); break;
      case 0x35: tft.print(" - Illumination"); break;
      case 0x36: tft.print(" - Function Buttons"); break;
      case 0x40: tft.print(" - Menu"); break;
      case 0x41: tft.print(" - Menu  Pick"); break;
      case 0x42: tft.print(" - Menu Up"); break;
      case 0x43: tft.print(" - Menu Down"); break;
      case 0x44: tft.print(" - Menu Left"); break;
      case 0x45: tft.print(" - Menu Right"); break;
      case 0x46: tft.print(" - Menu Escape"); break;
      case 0x47: tft.print(" - Menu Value Increase"); break;
      case 0x48: tft.print(" - Menu Value Decrease"); break;
      case 0x60: tft.print(" - Data On Screen"); break;
      case 0x61: tft.print(" - Closed Caption"); break;
      case 0x62: tft.print(" - Closed Caption Select"); break;
      case 0x63: tft.print(" - VCR/TV"); break;
      case 0x64: tft.print(" - Broadcast Mode"); break;
      case 0x65: tft.print(" - Snapshot"); break;
      case 0x66: tft.print(" - Still"); break;
      case 0x80: tft.print(" - Selection"); break;
      case 0x81: tft.print(" - Assign Selection"); break;
      case 0x82: tft.print(" - Mode Step"); break;
      case 0x83: tft.print(" - Recall Last"); break;
      case 0x84: tft.print(" - Enter Channel"); break;
      case 0x85: tft.print(" - Order Movie"); break;
      case 0x86: tft.print(" - Channel"); break;
      case 0x87: tft.print(" - Media Selection"); break;
      case 0x88: tft.print(" - Media Select Computer"); break;
      case 0x89: tft.print(" - Media Select TV"); break;
      case 0x8A: tft.print(" - Media Select WWW"); break;
      case 0x8B: tft.print(" - Media Select DVD"); break;
      case 0x8C: tft.print(" - Media Select Telephone"); break;
      case 0x8D: tft.print(" - Media Select Program Guide"); break;
      case 0x8E: tft.print(" - Media Select Video Phone"); break;
      case 0x8F: tft.print(" - Media Select Games"); break;
      case 0x90: tft.print(" - Media Select Messages"); break;
      case 0x91: tft.print(" - Media Select CD"); break;
      case 0x92: tft.print(" - Media Select VCR"); break;
      case 0x93: tft.print(" - Media Select Tuner"); break;
      case 0x94: tft.print(" - Quit"); break;
      case 0x95: tft.print(" - Help"); break;
      case 0x96: tft.print(" - Media Select Tape"); break;
      case 0x97: tft.print(" - Media Select Cable"); break;
      case 0x98: tft.print(" - Media Select Satellite"); break;
      case 0x99: tft.print(" - Media Select Security"); break;
      case 0x9A: tft.print(" - Media Select Home"); break;
      case 0x9B: tft.print(" - Media Select Call"); break;
      case 0x9C: tft.print(" - Channel Increment"); break;
      case 0x9D: tft.print(" - Channel Decrement"); break;
      case 0x9E: tft.print(" - Media Select SAP"); break;
      case 0xA0: tft.print(" - VCR Plus"); break;
      case 0xA1: tft.print(" - Once"); break;
      case 0xA2: tft.print(" - Daily"); break;
      case 0xA3: tft.print(" - Weekly"); break;
      case 0xA4: tft.print(" - Monthly"); break;
      case 0xB0: tft.print(" - Play"); break;
      case 0xB1: tft.print(" - Pause"); break;
      case 0xB2: tft.print(" - Record"); break;
      case 0xB3: tft.print(" - Fast Forward"); break;
      case 0xB4: tft.print(" - Rewind"); break;
      case 0xB5: tft.print(" - Scan Next Track"); break;
      case 0xB6: tft.print(" - Scan Previous Track"); break;
      case 0xB7: tft.print(" - Stop"); break;
      case 0xB8: tft.print(" - Eject"); break;
      case 0xB9: tft.print(" - Random Play"); break;
      case 0xBA: tft.print(" - Select DisC"); break;
      case 0xBB: tft.print(" - Enter Disc"); break;
      case 0xBC: tft.print(" - Repeat"); break;
      case 0xBD: tft.print(" - Tracking"); break;
      case 0xBE: tft.print(" - Track Normal"); break;
      case 0xBF: tft.print(" - Slow Tracking"); break;
      case 0xC0: tft.print(" - Frame Forward"); break;
      case 0xC1: tft.print(" - Frame Back"); break;
      case 0xC2: tft.print(" - Mark"); break;
      case 0xC3: tft.print(" - Clear Mark"); break;
      case 0xC4: tft.print(" - Repeat From Mark"); break;
      case 0xC5: tft.print(" - Return To Mark"); break;
      case 0xC6: tft.print(" - Search Mark Forward"); break;
      case 0xC7: tft.print(" - Search Mark Backwards"); break;
      case 0xC8: tft.print(" - Counter Reset"); break;
      case 0xC9: tft.print(" - Show Counter"); break;
      case 0xCA: tft.print(" - Tracking Increment"); break;
      case 0xCB: tft.print(" - Tracking Decrement"); break;
      case 0xCD: tft.print(" - Pause/Continue"); break;
      case 0xE0: tft.print(" - Volume"); break;
      case 0xE1: tft.print(" - Balance"); break;
      case 0xE2: tft.print(" - Mute"); break;
      case 0xE3: tft.print(" - Bass"); break;
      case 0xE4: tft.print(" - Treble"); break;
      case 0xE5: tft.print(" - Bass Boost"); break;
      case 0xE6: tft.print(" - Surround Mode"); break;
      case 0xE7: tft.print(" - Loudness"); break;
      case 0xE8: tft.print(" - MPX"); break;
      case 0xE9: tft.print(" - Volume Up"); break;
      case 0xEA: tft.print(" - Volume Down"); break;
      case 0xF0: tft.print(" - Speed Select"); break;
      case 0xF1: tft.print(" - Playback Speed"); break;
      case 0xF2: tft.print(" - Standard Play"); break;
      case 0xF3: tft.print(" - Long Play"); break;
      case 0xF4: tft.print(" - Extended Play"); break;
      case 0xF5: tft.print(" - Slow"); break;
      case 0x100: tft.print(" - Fan Enable"); break;
      case 0x101: tft.print(" - Fan Speed"); break;
      case 0x102: tft.print(" - Light"); break;
      case 0x103: tft.print(" - Light Illumination Level"); break;
      case 0x104: tft.print(" - Climate Control Enable"); break;
      case 0x105: tft.print(" - Room Temperature"); break;
      case 0x106: tft.print(" - Security Enable"); break;
      case 0x107: tft.print(" - Fire Alarm"); break;
      case 0x108: tft.print(" - Police Alarm"); break;
      case 0x150: tft.print(" - Balance Right"); break;
      case 0x151: tft.print(" - Balance Left"); break;
      case 0x152: tft.print(" - Bass Increment"); break;
      case 0x153: tft.print(" - Bass Decrement"); break;
      case 0x154: tft.print(" - Treble Increment"); break;
      case 0x155: tft.print(" - Treble Decrement"); break;
      case 0x160: tft.print(" - Speaker System"); break;
      case 0x161: tft.print(" - Channel Left"); break;
      case 0x162: tft.print(" - Channel Right"); break;
      case 0x163: tft.print(" - Channel Center"); break;
      case 0x164: tft.print(" - Channel Front"); break;
      case 0x165: tft.print(" - Channel Center Front"); break;
      case 0x166: tft.print(" - Channel Side"); break;
      case 0x167: tft.print(" - Channel Surround"); break;
      case 0x168: tft.print(" - Channel Low Frequency Enhancement"); break;
      case 0x169: tft.print(" - Channel Top"); break;
      case 0x16A: tft.print(" - Channel Unknown"); break;
      case 0x170: tft.print(" - Sub-channel"); break;
      case 0x171: tft.print(" - Sub-channel Increment"); break;
      case 0x172: tft.print(" - Sub-channel Decrement"); break;
      case 0x173: tft.print(" - Alternate Audio Increment"); break;
      case 0x174: tft.print(" - Alternate Audio Decrement"); break;
      case 0x180: tft.print(" - Application Launch Buttons"); break;
      case 0x181: tft.print(" - AL Launch Button Configuration Tool"); break;
      case 0x182: tft.print(" - AL Programmable Button Configuration"); break;
      case 0x183: tft.print(" - AL Consumer Control Configuration"); break;
      case 0x184: tft.print(" - AL Word Processor"); break;
      case 0x185: tft.print(" - AL Text Editor"); break;
      case 0x186: tft.print(" - AL Spreadsheet"); break;
      case 0x187: tft.print(" - AL Graphics Editor"); break;
      case 0x188: tft.print(" - AL Presentation App"); break;
      case 0x189: tft.print(" - AL Database App"); break;
      case 0x18A: tft.print(" - AL Email Reader"); break;
      case 0x18B: tft.print(" - AL Newsreader"); break;
      case 0x18C: tft.print(" - AL Voicemail"); break;
      case 0x18D: tft.print(" - AL Contacts/Address Book"); break;
      case 0x18E: tft.print(" - AL Calendar/Schedule"); break;
      case 0x18F: tft.print(" - AL Task/Project Manager"); break;
      case 0x190: tft.print(" - AL Log/Journal/Timecard"); break;
      case 0x191: tft.print(" - AL Checkbook/Finance"); break;
      case 0x192: tft.print(" - AL Calculator"); break;
      case 0x193: tft.print(" - AL A/V Capture/Playback"); break;
      case 0x194: tft.print(" - AL Local Machine Browser"); break;
      case 0x195: tft.print(" - AL LAN/WAN Browser"); break;
      case 0x196: tft.print(" - AL Internet Browser"); break;
      case 0x197: tft.print(" - AL Remote Networking/ISP Connect"); break;
      case 0x198: tft.print(" - AL Network Conference"); break;
      case 0x199: tft.print(" - AL Network Chat"); break;
      case 0x19A: tft.print(" - AL Telephony/Dialer"); break;
      case 0x19B: tft.print(" - AL Logon"); break;
      case 0x19C: tft.print(" - AL Logoff"); break;
      case 0x19D: tft.print(" - AL Logon/Logoff"); break;
      case 0x19E: tft.print(" - AL Terminal Lock/Screensaver"); break;
      case 0x19F: tft.print(" - AL Control Panel"); break;
      case 0x1A0: tft.print(" - AL Command Line Processor/Run"); break;
      case 0x1A1: tft.print(" - AL Process/Task Manager"); break;
      case 0x1A2: tft.print(" - AL Select Tast/Application"); break;
      case 0x1A3: tft.print(" - AL Next Task/Application"); break;
      case 0x1A4: tft.print(" - AL Previous Task/Application"); break;
      case 0x1A5: tft.print(" - AL Preemptive Halt Task/Application"); break;
      case 0x200: tft.print(" - Generic GUI Application Controls"); break;
      case 0x201: tft.print(" - AC New"); break;
      case 0x202: tft.print(" - AC Open"); break;
      case 0x203: tft.print(" - AC Close"); break;
      case 0x204: tft.print(" - AC Exit"); break;
      case 0x205: tft.print(" - AC Maximize"); break;
      case 0x206: tft.print(" - AC Minimize"); break;
      case 0x207: tft.print(" - AC Save"); break;
      case 0x208: tft.print(" - AC Print"); break;
      case 0x209: tft.print(" - AC Properties"); break;
      case 0x21A: tft.print(" - AC Undo"); break;
      case 0x21B: tft.print(" - AC Copy"); break;
      case 0x21C: tft.print(" - AC Cut"); break;
      case 0x21D: tft.print(" - AC Paste"); break;
      case 0x21E: tft.print(" - AC Select All"); break;
      case 0x21F: tft.print(" - AC Find"); break;
      case 0x220: tft.print(" - AC Find and Replace"); break;
      case 0x221: tft.print(" - AC Search"); break;
      case 0x222: tft.print(" - AC Go To"); break;
      case 0x223: tft.print(" - AC Home"); break;
      case 0x224: tft.print(" - AC Back"); break;
      case 0x225: tft.print(" - AC Forward"); break;
      case 0x226: tft.print(" - AC Stop"); break;
      case 0x227: tft.print(" - AC Refresh"); break;
      case 0x228: tft.print(" - AC Previous Link"); break;
      case 0x229: tft.print(" - AC Next Link"); break;
      case 0x22A: tft.print(" - AC Bookmarks"); break;
      case 0x22B: tft.print(" - AC History"); break;
      case 0x22C: tft.print(" - AC Subscriptions"); break;
      case 0x22D: tft.print(" - AC Zoom In"); break;
      case 0x22E: tft.print(" - AC Zoom Out"); break;
      case 0x22F: tft.print(" - AC Zoom"); break;
      case 0x230: tft.print(" - AC Full Screen View"); break;
      case 0x231: tft.print(" - AC Normal View"); break;
      case 0x232: tft.print(" - AC View Toggle"); break;
      case 0x233: tft.print(" - AC Scroll Up"); break;
      case 0x234: tft.print(" - AC Scroll Down"); break;
      case 0x235: tft.print(" - AC Scroll"); break;
      case 0x236: tft.print(" - AC Pan Left"); break;
      case 0x237: tft.print(" - AC Pan Right"); break;
      case 0x238: tft.print(" - AC Pan"); break;
      case 0x239: tft.print(" - AC New Window"); break;
      case 0x23A: tft.print(" - AC Tile Horizontally"); break;
      case 0x23B: tft.print(" - AC Tile Vertically"); break;
      case 0x23C: tft.print(" - AC Format"); break;
    }
  }
  tft.println();
  tft.updateScreen();  // update the screen now
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key) {
  tft.print("HID (");
  tft.print(top, HEX);
  tft.print(") key release:");
  tft.println(key, HEX);
}

void ProcessKeyboardData() {
#if 0
  uint8_t cur_battery_level = 0xff;
  if (keyboard1) {
    cur_battery_level = keyboard1.batteryLevel();
  } else {
    cur_battery_level = keyboard2.batteryLevel();
  }
  if (cur_battery_level != keyboard_battery_level) {
    MaybeSetupTextScrollArea();
    tft.printf("<<< Battery %u %% >>>\n", cur_battery_level);
    keyboard_battery_level = cur_battery_level;
    tft.updateScreen();  // update the screen now
  }
#endif  
}