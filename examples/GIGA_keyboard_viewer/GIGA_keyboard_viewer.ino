//=============================================================================
// Simple test viewer app for several of the USB devices on a display
// This sketch as well as all others in this library will only work with the
// Teensy 3.6 and the T4.x boards.
//
// The default display is the ILI9341, which uses the ILI9341_t3 library,
// which installs with Teensyduino.  Alterntively you could use
// the ILI9341_t3n library which supports different SPI busses and the like
// It is located at:
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

#include <Arduino_USBHostMbed5.h>
#include "USBHostKeyboardEx.h"
REDIRECT_STDOUT_TO(Serial)


#include "SPI.h"
#include <ILI9341_GIGA_n.h>
#include <ili9341_GIGA_n_font_Arial.h>

#define BLACK ILI9341_BLACK
#define WHITE ILI9341_WHITE
#define YELLOW ILI9341_YELLOW
#define GREEN ILI9341_GREEN
#define RED ILI9341_RED

#define LIGHTGREY 0xC618 /* 192, 192, 192 */
#define DARKGREY 0x7BEF  /* 128, 128, 128 */
#define BLUE 0x001F      /*   0,   0, 255 */

#define CENTER ILI9341_GIGA_n::CENTER

//=============================================================================
// Connection configuration of ILI9341 LCD TFT
//=============================================================================
#define TFT_DC 9
#define TFT_RST 8
#define TFT_CS 10

ILI9341_GIGA_n tft(&SPI1, TFT_CS, TFT_DC, TFT_RST);

//=============================================================================
// USB Host Ojbects
//=============================================================================
USBHostKeyboardEx keyboard1;
bool keyboard1_was_connected = false;


//=============================================================================
// Other state variables.
//=============================================================================

bool new_device_detected = false;


//=============================================================================
// Setup
//=============================================================================
void setup() {
  Serial1.begin(2000000);
  while (!Serial && millis() < 3000)
    ;  // wait for Arduino Serial Monitor
  Serial.println("\nUSB Host Keyboard viewer");

  keyboard1.attachRawPress(OnRawPress);
  keyboard1.attachRawRelease(OnRawRelease);
  keyboard1.attachHIDPress(OnHIDExtrasPress);
  keyboard1.attachHIDRelease(OnHIDExtrasRelease);

  tft.begin();

  delay(100);
  tft.setRotation(3);  // 180
  delay(100);

  tft.fillScreen(BLACK);
  tft.setTextColor(YELLOW);
  tft.setTextSize(2);
  TFTDrawTextCentered("Waiting for Device...");
}


//=============================================================================
// Loop
//=============================================================================
void loop() {

  // Update the display with
  UpdateActiveDeviceInfo();
}

//=============================================================================
// TFTDrawTextCentered
//=============================================================================
void TFTDrawTextCentered(const char *psz) {
#ifdef CENTER
  tft.setCursor(CENTER, CENTER);
#else
  uint16_t x = (tft.width() - tft.measureTextWidth(psz)) / 2;
  uint16_t y = (tft.height() - tft.measureTextHeight(psz)) / 2;
  tft.setCursor(x, y);
#endif  
  tft.print(psz);
  
}

//=============================================================================
// UpdateActiveDeviceInfo
//=============================================================================
void UpdateActiveDeviceInfo() {
  //bool new_device_detected = false;

  if (keyboard1_was_connected) {
    if (!keyboard1.connected()) {
      Serial.println("*** Tablet disconnected ***");
      keyboard1_was_connected = false;
    }
  } else {
    if (keyboard1.connect()) {
      keyboard1_was_connected = true;

      Serial.print("*** Device Tablet ");
      Serial.print(keyboard1.idVendor(), HEX);
      Serial.print(":");
      Serial.print(keyboard1.idProduct(), HEX);
      Serial.println("- connected ***");
      tft.fillScreen(BLACK);  // clear the screen.
      tft.setCursor(0, 0);
      tft.setTextColor(YELLOW);
      tft.setFont(Arial_12);
      tft.print("Device Tablet ");
      tft.print(keyboard1.idVendor(), HEX);
      tft.print(":");
      tft.print(keyboard1.idProduct(), HEX);
      tft.println(" - connected");

      uint8_t string_buffer[80];
      if (keyboard1.manufacturer(string_buffer, sizeof(string_buffer))) {
        tft.print("  Manufacturer: ");
        tft.println((char *)string_buffer);
      }

      if (keyboard1.product(string_buffer, sizeof(string_buffer))) {
        tft.print("  Product: ");
        tft.println((char *)string_buffer);
      }
      if (keyboard1.serialNumber(string_buffer, sizeof(string_buffer))) {
        tft.print("  Serial: ");
        tft.println((char *)string_buffer);
      }
      DrawKeyboard();
      //tft.updateScreen();  // update the screen now
    }
  }
}


//======================================================
// Keycode information
//======================================================

typedef struct {
  uint8_t key_code;  // sort of duplicate in table, but...
  uint8_t row;       // which row to draw it in.  we will scale
  uint8_t col;       // which column
  const char *sz;    // text to display.
} KEYCODE_SCAN_DATA_t;

enum {
  KEY_A=4, KEY_B=5, KEY_C=6, KEY_D=7, KEY_E=8, KEY_F=9, KEY_G=10, KEY_H=11,
  KEY_I=12, KEY_J=13, KEY_K=14, KEY_L=15, KEY_M=16, KEY_N=17, KEY_O=18, KEY_P=19,
  KEY_Q=20, KEY_R=21, KEY_S=22, KEY_T=23, KEY_U=24, KEY_V=25, KEY_W=26, KEY_X=27,
  KEY_Y=28, KEY_Z=29, KEY_1=30, KEY_2=31, KEY_3=32, KEY_4=33, KEY_5=34, KEY_6=35,
  KEY_7=36, KEY_8=37, KEY_9=38, KEY_0=39, KEY_ENTER=40, KEY_ESC=41, KEY_BACKSPACE=42, KEY_TAB=43, KEY_SPACE=44,
  KEY_MINUS=45, KEY_EQUAL=46, KEY_LEFT_BRACE=47, KEY_RIGHT_BRACE=48, KEY_BACKSLASH=49, KEY_NON_US_NUM=50, 
  KEY_SEMICOLON=51, KEY_QUOTE=52, KEY_TILDE=53, KEY_COMMA=54, KEY_PERIOD=55, KEY_SLASH=56,
  KEY_CAPS_LOCK=57, KEY_F1=58, KEY_F2=59, KEY_F3=60, KEY_F4=61, KEY_F5=62, KEY_F6=63, KEY_F7=64,
  KEY_F8=65, KEY_F9=66, KEY_F10=67, KEY_F11=68, KEY_F12=69, KEY_PRINTSCREEN=70, KEY_SCROLL_LOCK=71, KEY_PAUSE=72,
  KEY_INSERT=73, KEY_HOME=74, KEY_PAGE_UP=75, KEY_DELETE=76, KEY_END=77, KEY_PAGE_DOWN=78, KEY_RIGHT=79, 
  KEY_LEFT=80, KEY_DOWN=81, KEY_UP=82, KEY_NUM_LOCK=83, KEYPAD_SLASH=84,
  KEYPAD_ASTERIX=85, KEYPAD_MINUS=86, KEYPAD_PLUS=87, KEYPAD_ENTER=88, KEYPAD_1=89, KEYPAD_2=90, KEYPAD_3=91,
  KEYPAD_4=92, KEYPAD_5=93, KEYPAD_6=94, KEYPAD_7=95, KEYPAD_8=96, KEYPAD_9=97, KEYPAD_0=98,
  KEYPAD_PERIOD=99, KEY_NON_US_BS=100, KEY_MENU=101
  };


KEYCODE_SCAN_DATA_t keycode_scan_data[] = {
  { 0, 255 }, { 1, 255 }, { 2, 255 }, { 3, 255 },    // 0-3 are not used,
  { KEY_A, 3, 2, "A" },                    //(   4 )
  { KEY_B, 4, 6, "B" },                    //(   5 )
  { KEY_C, 4, 4, "C" },                    //(   6 )
  { KEY_D, 3, 4, "D" },                    //(   7 )
  { KEY_E, 2, 4, "E" },                    //(   8 )
  { KEY_F, 3, 5, "F" },                    //(   9 )
  { KEY_G, 3, 6, "G" },                    //(  10 )
  { KEY_H, 3, 7, "H" },                    //(  11 )
  { KEY_I, 2, 9, "I" },                    //(  12 )
  { KEY_J, 3, 8, "J" },                    //(  13 )
  { KEY_K, 3, 9, "K" },                   //(  14 )
  { KEY_L, 3, 10, "L" },                   //(  15 )
  { KEY_M, 4, 8, "M" },                    //(  16 )
  { KEY_N, 4, 7, "N" },                    //(  17 )
  { KEY_O, 2, 10, "O" },                   //(  18 )
  { KEY_P, 2, 11, "P" },                   //(  19 )
  { KEY_Q, 2, 2, "Q" },                    //(  20 )
  { KEY_R, 2, 5, "R" },                    //(  21 )
  { KEY_S, 3, 3, "S" },                    //(  22 )
  { KEY_T, 2, 6, "T" },                    //(  23 )
  { KEY_U, 2, 8, "U" },                    //(  24 )
  { KEY_V, 4, 5, "V" },                    //(  25 )
  { KEY_W, 2, 3, "W" },                    //(  26 )
  { KEY_X, 4, 3, "X" },                    //(  27 )
  { KEY_Y, 2, 7, "Y" },                    //(  28 )
  { KEY_Z, 4, 2, "Z" },                    //(  29 )
  { KEY_1, 1, 1, "1" },                    //(  30 )
  { KEY_2, 1, 2, "2" },                    //(  31 )
  { KEY_3, 1, 3, "3" },                    //(  32 )
  { KEY_4, 1, 4, "4" },                    //(  33 )
  { KEY_5, 1, 5, "5" },                    //(  34 )
  { KEY_6, 1, 6, "6" },                    //(  35 )
  { KEY_7, 1, 7, "7" },                    //(  36 )
  { KEY_8, 1, 8, "8" },                    //(  37 )
  { KEY_9, 1, 9, "9" },                    //(  38 )
  { KEY_0, 1, 10, "0" },                   //(  39 )
  { KEY_ENTER, 3, 13, "EN" },             //(  40 )
  { KEY_ESC, 0, 0, "ESC" },                //(  41 )
  { KEY_BACKSPACE, 1, 13, "BS" },          //(  42 )
  { KEY_TAB, 2, 0, "TAB" },                //(  43 )
  { KEY_SPACE, 5, 6, "SP" },               //(  44 )
  { KEY_MINUS, 1, 11, "-" },               //(  45 )
  { KEY_EQUAL, 1, 12, "=" },               //(  46 )
  { KEY_LEFT_BRACE, 2, 12, "[" },          //(  47 )
  { KEY_RIGHT_BRACE, 2, 13, "]" },         //(  48 )
  { KEY_BACKSLASH, 4, 11, "\\" },          //(  49 )
  { KEY_NON_US_NUM, 255, 0, "??" },        //(  50 )
  { KEY_SEMICOLON, 3, 11, ";" },           //(  51 )
  { KEY_QUOTE, 3, 12, "'" },               //(  52 )
  { KEY_TILDE, 1, 0, "`" },                //(  53 )
  { KEY_COMMA, 4, 9, "," },                //(  54 )
  { KEY_PERIOD, 4, 10, "." },              //(  55 )
  { KEY_SLASH, 4, 11, "/" },               //(  56 )
  { KEY_CAPS_LOCK, 3, 0, "CAP" },          //(  57 )
  { KEY_F1, 0, 2, "F1" },                  //(  58 )
  { KEY_F2, 0, 3, "F2" },                  //(  59 )
  { KEY_F3, 0, 4, "F3" },                  //(  60 )
  { KEY_F4, 0, 5, "F4" },                  //(  61 )
  { KEY_F5, 0, 6, "F5" },                  //(  62 )
  { KEY_F6, 0, 7, "F6" },                  //(  63 )
  { KEY_F7, 0, 8, "F7" },                  //(  64 )
  { KEY_F8, 0, 9, "F8" },                  //(  65 )
  { KEY_F9, 0, 10, "F9" },                 //(  66 )
  { KEY_F10, 0, 11, "10" },                //(  67 )
  { KEY_F11, 0, 12, "11" },                //(  68 )
  { KEY_F12, 0, 13, "12" },                //(  69 )
  { KEY_PRINTSCREEN, 0, 14, "PR" },        //(  70 )
  { KEY_SCROLL_LOCK, 0, 15, "SL" },        //(  71 )
  { KEY_PAUSE, 0, 16, "PA" },              //(  72 )
  { KEY_INSERT, 6, 0, "INS" },             //(  73 )
  { KEY_HOME, 6, 4, "HM" },                //(  74 )
  { KEY_PAGE_UP, 6, 8, "PU" },             //(  75 )
  { KEY_DELETE, 6, 2, "DEL" },             //(  76 )
  { KEY_END, 6, 6, "END" },                //(  77 )
  { KEY_PAGE_DOWN, 6, 10, "PD" },          //(  78 )
  { KEY_RIGHT, 6, 17, "RT" },              //(  79 )
  { KEY_LEFT, 6, 16, "LT" },               //(  80 )
  { KEY_DOWN, 6, 14, "DN" },               //(  81 )
  { KEY_UP, 6, 12, "UP" },                 //(  82 )
  { KEY_NUM_LOCK, 1, 14, "NL" },           //(  83 )
  { KEYPAD_SLASH, 1, 15, "/" },            //(  84 )
  { KEYPAD_ASTERIX, 1, 16, "*" },          //(  85 )
  { KEYPAD_MINUS, 1, 17, "-" },            //(  86 )
  { KEYPAD_PLUS, 2, 17, "+" },             //(  87 )
  { KEYPAD_ENTER, 5, 16, "EN" },           //(  88 )
  { KEYPAD_1, 4, 14, "1" },                //(  89 )
  { KEYPAD_2, 4, 15, "2" },                //(  90 )
  { KEYPAD_3, 4, 16, "3" },                //(  91 )
  { KEYPAD_4, 3, 14, "4" },                //(  92 )
  { KEYPAD_5, 3, 15, "5" },                //(  93 )
  { KEYPAD_6, 3, 16, "6" },                //(  94 )
  { KEYPAD_7, 2, 14, "7" },                //(  95 )
  { KEYPAD_8, 2, 15, "8" },                //(  96 )
  { KEYPAD_9, 2, 16, "9" },                //(  97 )
  { KEYPAD_0, 5, 14, "0" },                //(  98 )
  { KEYPAD_PERIOD, 5, 15, "." },           //(  99 )
  { KEY_NON_US_BS, 255, 0, "NON_US_BS" },  //( 100 )
  { KEY_MENU, 255, 0, "MENU" },            //( 101 )
  { 102 },                                           //( 102 )
                                                     // Note the ctrl/shift/alt appear to take over F13-F20
  { 103, 5, 0, "CTL" },                              //( 103 ) left ctrl
  { 104, 4, 0, "SHF" },                              //( 104 ) left shift
  { 105, 5, 4, "ALT" },                              //( 105 ) left alt
  { 106, 5, 2, "WIN" },                              //( 106 ) left GUI
  { 107, 5, 12, "CTL" },                             //( 107 ) right ctrl
  { 108, 4, 12, "SHF" },                             //( 108 ) right shift
  { 109, 5, 8, "ALT" },                              //( 109 ) right alt
  { 110, 5, 10, "GUI" }                              //( 110 ) right GUI
};

#define ROW_LAST_MSG 70
#define COL_LAST_MSG 65

#define ROW_START 100
#define COL_START 5
#define ROW_SIZE 20
#define COL_SIZE 17

uint16_t cursor_last_key_end = 0;

//======================================================
// DrawKeyboard
//======================================================
void DrawKeyboard() {
  tft.setFont(Arial_9);
  
  tft.setCursor(0, ROW_LAST_MSG);
  tft.print("Last key:");
  cursor_last_key_end = 0;
  
  tft.setTextColor(RED, BLACK);
  for (uint8_t i = 0; i < (sizeof(keycode_scan_data) / sizeof(keycode_scan_data[0])); i++) {
    if ((keycode_scan_data[i].sz == nullptr) || (keycode_scan_data[i].row == 255)) continue;  // not something to display.
    uint16_t y = ROW_START + keycode_scan_data[i].row * ROW_SIZE;
    uint16_t x = COL_START + keycode_scan_data[i].col * COL_SIZE;
    tft.setCursor(x, y);
    tft.print(keycode_scan_data[i].sz);
  }
}

//======================================================
// Show the last key pressed or released
//======================================================
void ShowLastKeyChange(const char *sz, bool press) {
  tft.setTextColor(press ? YELLOW : RED, BLACK);
  tft.setCursor(COL_LAST_MSG, ROW_LAST_MSG);
  #ifdef _ILI9341_t3H_
  tft.fillRect(COL_LAST_MSG, ROW_LAST_MSG, 320, ROW_SIZE, BLACK);
  tft.print(sz);
  #else
  tft.print(sz);
  uint16_t cursor_x = tft.getCursorX();
  if (cursor_x < cursor_last_key_end) {
    tft.fillRect(cursor_x, ROW_LAST_MSG, cursor_last_key_end - cursor_x + 1, ROW_SIZE, BLACK);
  }
  cursor_last_key_end = cursor_x;
  #endif
}



//======================================================
// Raw Press
//======================================================

void OnRawPress(uint8_t keycode, uint8_t mod) {
  tft.setTextColor(YELLOW, BLACK);
  if ((keycode_scan_data[keycode].sz == nullptr) || (keycode_scan_data[keycode].row == 255)) return;  // not something to display.
  uint16_t y = ROW_START + keycode_scan_data[keycode].row * ROW_SIZE;
  uint16_t x = COL_START + keycode_scan_data[keycode].col * COL_SIZE;
  tft.setCursor(x, y);
  tft.print(keycode_scan_data[keycode].sz);
  ShowLastKeyChange(keycode_scan_data[keycode].sz, true);
}

//======================================================
// Raw Relese
//======================================================
void OnRawRelease(uint8_t keycode) {
  tft.setTextColor(RED, BLACK);
  if ((keycode_scan_data[keycode].sz == nullptr) || (keycode_scan_data[keycode].row == 255)) return;  // not something to display.
  uint16_t y = ROW_START + keycode_scan_data[keycode].row * ROW_SIZE;
  uint16_t x = COL_START + keycode_scan_data[keycode].col * COL_SIZE;
  tft.setCursor(x, y);
  tft.print(keycode_scan_data[keycode].sz);
  ShowLastKeyChange(keycode_scan_data[keycode].sz, false);
}

//======================================================
// HIDS extra press
//======================================================
const char * MapExtraKeyToString(uint32_t top, uint16_t key) {
  if (top == 0xc0000) {
    switch (key) {
      case  0x20 : return " - +10";
      case  0x21 : return " - +100";
      case  0x22 : return " - AM/PM";
      case  0x30 : return " - Power";
      case  0x31 : return " - Reset";
      case  0x32 : return " - Sleep";
      case  0x33 : return " - Sleep After";
      case  0x34 : return " - Sleep Mode";
      case  0x35 : return " - Illumination";
      case  0x36 : return " - Function Buttons";
      case  0x40 : return " - Menu";
      case  0x41 : return " - Menu  Pick";
      case  0x42 : return " - Menu Up";
      case  0x43 : return " - Menu Down";
      case  0x44 : return " - Menu Left";
      case  0x45 : return " - Menu Right";
      case  0x46 : return " - Menu Escape";
      case  0x47 : return " - Menu Value Increase";
      case  0x48 : return " - Menu Value Decrease";
      case  0x60 : return " - Data On Screen";
      case  0x61 : return " - Closed Caption";
      case  0x62 : return " - Closed Caption Select";
      case  0x63 : return " - VCR/TV";
      case  0x64 : return " - Broadcast Mode";
      case  0x65 : return " - Snapshot";
      case  0x66 : return " - Still";
      case  0x80 : return " - Selection";
      case  0x81 : return " - Assign Selection";
      case  0x82 : return " - Mode Step";
      case  0x83 : return " - Recall Last";
      case  0x84 : return " - Enter Channel";
      case  0x85 : return " - Order Movie";
      case  0x86 : return " - Channel";
      case  0x87 : return " - Media Selection";
      case  0x88 : return " - Media Select Computer";
      case  0x89 : return " - Media Select TV";
      case  0x8A : return " - Media Select WWW";
      case  0x8B : return " - Media Select DVD";
      case  0x8C : return " - Media Select Telephone";
      case  0x8D : return " - Media Select Program Guide";
      case  0x8E : return " - Media Select Video Phone";
      case  0x8F : return " - Media Select Games";
      case  0x90 : return " - Media Select Messages";
      case  0x91 : return " - Media Select CD";
      case  0x92 : return " - Media Select VCR";
      case  0x93 : return " - Media Select Tuner";
      case  0x94 : return " - Quit";
      case  0x95 : return " - Help";
      case  0x96 : return " - Media Select Tape";
      case  0x97 : return " - Media Select Cable";
      case  0x98 : return " - Media Select Satellite";
      case  0x99 : return " - Media Select Security";
      case  0x9A : return " - Media Select Home";
      case  0x9B : return " - Media Select Call";
      case  0x9C : return " - Channel Increment";
      case  0x9D : return " - Channel Decrement";
      case  0x9E : return " - Media Select SAP";
      case  0xA0 : return " - VCR Plus";
      case  0xA1 : return " - Once";
      case  0xA2 : return " - Daily";
      case  0xA3 : return " - Weekly";
      case  0xA4 : return " - Monthly";
      case  0xB0 : return " - Play";
      case  0xB1 : return " - Pause";
      case  0xB2 : return " - Record";
      case  0xB3 : return " - Fast Forward";
      case  0xB4 : return " - Rewind";
      case  0xB5 : return " - Scan Next Track";
      case  0xB6 : return " - Scan Previous Track";
      case  0xB7 : return " - Stop";
      case  0xB8 : return " - Eject";
      case  0xB9 : return " - Random Play";
      case  0xBA : return " - Select DisC";
      case  0xBB : return " - Enter Disc";
      case  0xBC : return " - Repeat";
      case  0xBD : return " - Tracking";
      case  0xBE : return " - Track Normal";
      case  0xBF : return " - Slow Tracking";
      case  0xC0 : return " - Frame Forward";
      case  0xC1 : return " - Frame Back";
      case  0xC2 : return " - Mark";
      case  0xC3 : return " - Clear Mark";
      case  0xC4 : return " - Repeat From Mark";
      case  0xC5 : return " - Return To Mark";
      case  0xC6 : return " - Search Mark Forward";
      case  0xC7 : return " - Search Mark Backwards";
      case  0xC8 : return " - Counter Reset";
      case  0xC9 : return " - Show Counter";
      case  0xCA : return " - Tracking Increment";
      case  0xCB : return " - Tracking Decrement";
      case  0xCD : return " - Pause/Continue";
      case  0xE0 : return " - Volume";
      case  0xE1 : return " - Balance";
      case  0xE2 : return " - Mute";
      case  0xE3 : return " - Bass";
      case  0xE4 : return " - Treble";
      case  0xE5 : return " - Bass Boost";
      case  0xE6 : return " - Surround Mode";
      case  0xE7 : return " - Loudness";
      case  0xE8 : return " - MPX";
      case  0xE9 : return " - Volume Up";
      case  0xEA : return " - Volume Down";
      case  0xF0 : return " - Speed Select";
      case  0xF1 : return " - Playback Speed";
      case  0xF2 : return " - Standard Play";
      case  0xF3 : return " - Long Play";
      case  0xF4 : return " - Extended Play";
      case  0xF5 : return " - Slow";
      case  0x100: return " - Fan Enable";
      case  0x101: return " - Fan Speed";
      case  0x102: return " - Light";
      case  0x103: return " - Light Illumination Level";
      case  0x104: return " - Climate Control Enable";
      case  0x105: return " - Room Temperature";
      case  0x106: return " - Security Enable";
      case  0x107: return " - Fire Alarm";
      case  0x108: return " - Police Alarm";
      case  0x150: return " - Balance Right";
      case  0x151: return " - Balance Left";
      case  0x152: return " - Bass Increment";
      case  0x153: return " - Bass Decrement";
      case  0x154: return " - Treble Increment";
      case  0x155: return " - Treble Decrement";
      case  0x160: return " - Speaker System";
      case  0x161: return " - Channel Left";
      case  0x162: return " - Channel Right";
      case  0x163: return " - Channel Center";
      case  0x164: return " - Channel Front";
      case  0x165: return " - Channel Center Front";
      case  0x166: return " - Channel Side";
      case  0x167: return " - Channel Surround";
      case  0x168: return " - Channel Low Frequency Enhancement";
      case  0x169: return " - Channel Top";
      case  0x16A: return " - Channel Unknown";
      case  0x170: return " - Sub-channel";
      case  0x171: return " - Sub-channel Increment";
      case  0x172: return " - Sub-channel Decrement";
      case  0x173: return " - Alternate Audio Increment";
      case  0x174: return " - Alternate Audio Decrement";
      case  0x180: return " - Application Launch Buttons";
      case  0x181: return " - AL Launch Button Configuration Tool";
      case  0x182: return " - AL Programmable Button Configuration";
      case  0x183: return " - AL Consumer Control Configuration";
      case  0x184: return " - AL Word Processor";
      case  0x185: return " - AL Text Editor";
      case  0x186: return " - AL Spreadsheet";
      case  0x187: return " - AL Graphics Editor";
      case  0x188: return " - AL Presentation App";
      case  0x189: return " - AL Database App";
      case  0x18A: return " - AL Email Reader";
      case  0x18B: return " - AL Newsreader";
      case  0x18C: return " - AL Voicemail";
      case  0x18D: return " - AL Contacts/Address Book";
      case  0x18E: return " - AL Calendar/Schedule";
      case  0x18F: return " - AL Task/Project Manager";
      case  0x190: return " - AL Log/Journal/Timecard";
      case  0x191: return " - AL Checkbook/Finance";
      case  0x192: return " - AL Calculator";
      case  0x193: return " - AL A/V Capture/Playback";
      case  0x194: return " - AL Local Machine Browser";
      case  0x195: return " - AL LAN/WAN Browser";
      case  0x196: return " - AL Internet Browser";
      case  0x197: return " - AL Remote Networking/ISP Connect";
      case  0x198: return " - AL Network Conference";
      case  0x199: return " - AL Network Chat";
      case  0x19A: return " - AL Telephony/Dialer";
      case  0x19B: return " - AL Logon";
      case  0x19C: return " - AL Logoff";
      case  0x19D: return " - AL Logon/Logoff";
      case  0x19E: return " - AL Terminal Lock/Screensaver";
      case  0x19F: return " - AL Control Panel";
      case  0x1A0: return " - AL Command Line Processor/Run";
      case  0x1A1: return " - AL Process/Task Manager";
      case  0x1A2: return " - AL Select Tast/Application";
      case  0x1A3: return " - AL Next Task/Application";
      case  0x1A4: return " - AL Previous Task/Application";
      case  0x1A5: return " - AL Preemptive Halt Task/Application";
      case  0x200: return " - Generic GUI Application Controls";
      case  0x201: return " - AC New";
      case  0x202: return " - AC Open";
      case  0x203: return " - AC Close";
      case  0x204: return " - AC Exit";
      case  0x205: return " - AC Maximize";
      case  0x206: return " - AC Minimize";
      case  0x207: return " - AC Save";
      case  0x208: return " - AC Print";
      case  0x209: return " - AC Properties";
      case  0x21A: return " - AC Undo";
      case  0x21B: return " - AC Copy";
      case  0x21C: return " - AC Cut";
      case  0x21D: return " - AC Paste";
      case  0x21E: return " - AC Select All";
      case  0x21F: return " - AC Find";
      case  0x220: return " - AC Find and Replace";
      case  0x221: return " - AC Search";
      case  0x222: return " - AC Go To";
      case  0x223: return " - AC Home";
      case  0x224: return " - AC Back";
      case  0x225: return " - AC Forward";
      case  0x226: return " - AC Stop";
      case  0x227: return " - AC Refresh";
      case  0x228: return " - AC Previous Link";
      case  0x229: return " - AC Next Link";
      case  0x22A: return " - AC Bookmarks";
      case  0x22B: return " - AC History";
      case  0x22C: return " - AC Subscriptions";
      case  0x22D: return " - AC Zoom In";
      case  0x22E: return " - AC Zoom Out";
      case  0x22F: return " - AC Zoom";
      case  0x230: return " - AC Full Screen View";
      case  0x231: return " - AC Normal View";
      case  0x232: return " - AC View Toggle";
      case  0x233: return " - AC Scroll Up";
      case  0x234: return " - AC Scroll Down";
      case  0x235: return " - AC Scroll";
      case  0x236: return " - AC Pan Left";
      case  0x237: return " - AC Pan Right";
      case  0x238: return " - AC Pan";
      case  0x239: return " - AC New Window";
      case  0x23A: return " - AC Tile Horizontally";
      case  0x23B: return " - AC Tile Vertically";
      case  0x23C: return " - AC Format";
    }
  }
  static char szUnk[32];
  sprintf(szUnk, "EX(%lx,%x)", top, key);
  return szUnk;
}    


void OnHIDExtrasPress(uint32_t top, uint16_t key) {
  const char *sz = MapExtraKeyToString(top, key);
  //Serial.printf("OnHIDExtrasPress(%x, %x): %s\n", top, key, sz);
  ShowLastKeyChange(sz, true);
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key) {
  const char *sz = MapExtraKeyToString(top, key);
  ShowLastKeyChange(sz, false);
}


