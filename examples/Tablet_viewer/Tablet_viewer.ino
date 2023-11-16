//=============================================================================
// Simple test viewer app for the USBHostTablets object
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

#include <Arduino_USBHostMbed5.h>
#include <USBHostTablets.h>
#include <elapsedMillis.h>

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
USBHostTablets digi;

bool digi_was_connected = false;

//=============================================================================
// Other state variables.
//=============================================================================

// Save away values for buttons, x, y, wheel, wheelh
#define MAX_TOUCH 4
uint32_t buttons_cur = 0;
int x_cur = 0,
    x_cur_prev = 0,
    y_cur = 0,
    y_cur_prev = 0,
    z_cur = 0;

int wheel_cur = 0;
int wheelH_cur = 0;
int axis_cur[10];

bool BT = 0;

bool show_alternate_view = false;

int user_axis[64];
uint32_t buttons_prev = 0;
uint32_t buttons;

bool g_redraw_all = false;
int16_t y_position_after_device_info = 0;

//-----------------------------------------------------------------------------
// Remember some stuff between calls
//-----------------------------------------------------------------------------
int g_cnt_pen_buttons;
int g_cnt_frame_buttons;
int g_button_height;
uint32_t g_pen_buttons_prev;
uint32_t g_frame_buttons_prev;
uint8_t g_touch_count_prev = 0;
bool g_switch_view = false;

int tablet_changed_area_x_min = 0;
int tablet_changed_area_x_max = 0;
int tablet_changed_area_y_min = 0;
int tablet_changed_area_y_max = 0;



//=============================================================================
// Setup
//=============================================================================
void setup() {
  Serial1.begin(2000000);
  while (!Serial && millis() < 3000)
    ;  // wait for Arduino Serial Monitor
  Serial.println("\nTablet Viewer");
  tft.begin();
  // explicitly set the frame buffer
  //  tft.setFrameBuffer(frame_buffer);
  delay(100);
  tft.setRotation(3);  // 180
  delay(100);
  // make sure display is working
  tft.fillScreen(GREEN);
  delay(500);
  tft.fillScreen(YELLOW);
  delay(500);
  tft.fillScreen(RED);
  delay(500);

  tft.fillScreen(BLACK);
  tft.setTextColor(YELLOW);
  tft.setTextSize(2);
  tft.setCursor(CENTER, CENTER);
  tft.println("Waiting for Device...");
  tft.useFrameBuffer(true);
  tft.updateChangedAreasOnly(true);
}


//=============================================================================
// Loop
//=============================================================================
void loop() {
  // Update the display with
  UpdateActiveDeviceInfo();

  // Now lets try displaying Tablet data
  ProcessTabletData();

  if (Serial.available()) {
    int ch = Serial.read();
    while (Serial.read() != -1)
      ;
    if (ch == 'f') digi.forceResendSetupControlPackets();
    else switchView();
  }
}


void switchView() {
  show_alternate_view = !show_alternate_view;
  if (show_alternate_view) Serial.println("Switched to simple graphic mode");
  else {
    Serial.println("switched back to Text mode");
  }
  tft.fillScreen(BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(YELLOW);
  tft.setFont(Arial_12);
  tft.print("(");
  tft.print(digi.idVendor(), HEX);
  tft.print(":");
  tft.print(digi.idProduct(), HEX);
  tft.print("): ");

  uint8_t string_buffer[80];
  if (digi.product(string_buffer, sizeof(string_buffer))) {
    tft.print((char *)string_buffer);
  }
  tft.println();
  tft.updateScreen();
  g_redraw_all = true;
}

//=============================================================================
// ProcessTabletData
//=============================================================================
void ProcessTabletData() {
  if (g_switch_view) {
    switchView();
    g_switch_view = false;
  }

  digi.debugPrint(true);
  bool update_screen;
  if (digi.available()) {
    elapsedMicros em;
    if (show_alternate_view) update_screen = ShowSimpleGraphicScreen();
    else update_screen = showDataScreen();
    if (update_screen) {
      tft.updateScreen();  // update the screen now
      Serial.println(em);
    }
    digi.digitizerDataClear();
  }
}

//=============================================================================
// Show using raw data output.
//=============================================================================
bool showDataScreen() {
  int16_t tilt_x_cur = 0,
          tilt_y_cur = 0;
  uint16_t pen_press_cur = 0,
           pen_dist_cur = 0;
  int touch_x_cur[4] = { 0, 0, 0, 0 },
      touch_y_cur[4] = { 0, 0, 0, 0 };
  bool touch = false;

  if (g_redraw_all) {
    // Lets display the titles.
    int16_t x;
    tft.getCursor(&x, &y_position_after_device_info);
    tft.setTextColor(YELLOW);
    tft.print("Buttons:\nPen (X, Y):\nTouch (X, Y):\n\nPress/Dist:\nTiltXY:\nFPress:\nFTouch:\nAxis:");
    g_redraw_all = false;
  }

  int touch_count = digi.getTouchCount();
  int touch_index;
  uint16_t buttons_bin = digi.getPenButtons();

  Serial.println(buttons_bin);
  bool pen_button[4];
  bool button_touched[8];
  bool button_pressed[8];
  Serial.print("Digitizer: ");
  for (int i = 0; i < 4; i++) {
    pen_button[i] = (buttons_bin >> i) & 0x1;
    Serial.print("Pen_Btn");
    Serial.print(i, DEC);
    Serial.print(":");
    Serial.print(pen_button[i], DEC);
    Serial.print(" ");
  }

  bool something_changed = false;

  //buttons are saved within one byte for all the 8 buttons, here is a decoding example
  uint16_t button_touch_bin = digi.getFrameTouchButtons();
  uint16_t button_press_bin = digi.getFrameButtons();
  for (int i = 0; i < 8; i++) {
    button_touched[i] = (button_touch_bin >> i) & 0x1;
    button_pressed[i] = (button_press_bin >> i) & 0x1;
    Serial.print("Btn");
    Serial.print(i, DEC);
    Serial.print(": T:");
    Serial.print(button_touched[i], DEC);
    Serial.print(" P:");
    Serial.print(button_pressed[i], DEC);
    Serial.print(" ");
  }

  USBHostTablets::event_type_t evt = digi.eventType();
  switch (evt) {
    case USBHostTablets::TOUCH:
      Serial.print(" Touch:");
      for (touch_index = 0; touch_index < touch_count; touch_index++) {
        Serial.print(" (");
        Serial.print(digi.getX(touch_index), DEC);
        Serial.print(", ");
        Serial.print(digi.getY(touch_index), DEC);
        Serial.print(")");
        touch_x_cur[touch_index] = digi.getX(touch_index);
        touch_y_cur[touch_index] = digi.getY(touch_index);
      }
      touch = true;
      break;
    case USBHostTablets::PEN:
      Serial.print(" Pen: (");
      Serial.print(digi.getX(), DEC);
      Serial.print(", ");
      Serial.print(digi.getY(), DEC);
      Serial.print(" Pressure: ");
      Serial.print(digi.getPenPressure(), DEC);
      Serial.print(" Distance: ");
      Serial.print(digi.getPenDistance(), DEC);
      Serial.print(" TiltX: ");
      Serial.print(digi.getPenTiltX(), DEC);
      Serial.print(" TiltY: ");
      Serial.print(digi.getPenTiltY(), DEC);
      x_cur = digi.getX();
      y_cur = digi.getY();
      pen_press_cur = digi.getPenPressure();
      pen_dist_cur = digi.getPenDistance();
      touch = false;
      break;
    case USBHostTablets::FRAME:
      {
        //wheel data 0-71
        Serial.print(" Whl: ");
        Serial.print(digi.getFrameWheel(), DEC);
        //wheel button binary, no touch functionality
        Serial.print(" WhlBtn: ");
        Serial.print(digi.getFrameWheelButton(), DEC);
        Serial.print(" ");
        something_changed = true;
      }
      break;
    default:
      Serial.print(",  X = ");
      Serial.print(digi.getX(), DEC);
      Serial.print(", Y = ");
      Serial.print(digi.getY(), DEC);
      Serial.print(",  X = ");

      Serial.print(",  Pressure: = ");
      Serial.print(",  wheel = ");
      Serial.print(digi.getWheel());
      Serial.print(",  wheelH = ");
      Serial.print(digi.getWheelH());
      break;
  }

  Serial.println();

  if (buttons_bin != buttons_cur) {
    buttons_cur = buttons_bin;
    something_changed = true;
  }
  if (x_cur != x_cur_prev) {
    x_cur_prev = x_cur;
    something_changed = true;
  }
  if (y_cur != y_cur_prev) {
    y_cur_prev = y_cur;
    something_changed = true;
  }
  if (digi.getPenPressure() != pen_press_cur) {
    pen_press_cur = digi.getPenPressure();
    something_changed = true;
  }
  if (digi.getPenDistance() != pen_dist_cur) {
    pen_dist_cur = digi.getPenDistance();
    something_changed = true;
  }
  if (digi.getPenTiltX() != tilt_x_cur) {
    tilt_x_cur = digi.getPenTiltX();
    something_changed = true;
  }
  if (digi.getPenTiltY() != tilt_y_cur) {
    tilt_y_cur = digi.getPenTiltY();
    something_changed = true;
  }
  if (touch) {
    for (touch_index = 0; touch_index < touch_count; touch_index++) {
      touch_x_cur[touch_index] = digi.getX(touch_index);
      touch_y_cur[touch_index] = digi.getY(touch_index);
    }
    something_changed = true;
  }
  // BUGBUG:: play with some Axis...
  for (uint8_t i = 0; i < 10; i++) {
    int axis = digi.getAxis(i);
    if (axis != axis_cur[i]) {
      axis_cur[i] = axis;
      something_changed = true;
    }
  }

  if (something_changed) {
#define TABLET_DATA_X 100
    unsigned char line_space = Arial_12.line_space;
    tft.setTextColor(WHITE, BLACK);
    //tft.setTextDatum(BR_DATUM);
    int16_t y = y_position_after_device_info;
    tft.setCursor(TABLET_DATA_X, y);
    tft.fillRect(TABLET_DATA_X, y, 320, line_space, BLACK);
    for (int i = 0; i < 4; i++) {
      tft.print("(");
      tft.print(pen_button[i], DEC);
      tft.print(") ");
    }

    y += line_space;
    if (touch == false) {
      tft.setCursor(TABLET_DATA_X, y);
      tft.fillRect(TABLET_DATA_X, y, 320, line_space, BLACK);
      tft.print("(");
      tft.print(x_cur, DEC);
      tft.print(", ");
      tft.print(y_cur, DEC);
      tft.print(")");
    }
    y += line_space;
    if (touch == true) {
      tft.setCursor(TABLET_DATA_X, y);
      tft.fillRect(TABLET_DATA_X, y, tft.width(), line_space, BLACK);
      for (int i = 0; i < touch_count; i++) {
        if (i == 2) {
          y += line_space;
          tft.setCursor(TABLET_DATA_X, y);
          tft.fillRect(TABLET_DATA_X, y, tft.width(), line_space, BLACK);
        }
        tft.print("(");
        tft.print(touch_x_cur[i], DEC);
        tft.print(", ");
        tft.print(touch_y_cur[i], DEC);
        tft.print(")");
      }
    }
    if (touch_count < 2) {
      y += line_space;
      tft.setCursor(TABLET_DATA_X, y);
    }
    y += line_space;
    tft.setCursor(TABLET_DATA_X, y);
    tft.fillRect(TABLET_DATA_X, y, 320, line_space, BLACK);
    tft.print("(");
    tft.print(pen_press_cur, DEC);
    tft.print(", ");
    tft.print(pen_dist_cur, DEC);
    tft.print(")");

    y += line_space;
    tft.fillRect(TABLET_DATA_X, y, 320, line_space, BLACK);
    tft.setCursor(TABLET_DATA_X, y);
    tft.print("(");
    tft.print(tilt_x_cur, DEC);
    tft.print(", ");
    tft.print(tilt_y_cur, DEC);
    tft.print(") ");

    y += line_space;
    tft.fillRect(TABLET_DATA_X, y, tft.width(), line_space, BLACK);
    if (evt == USBHostTablets::FRAME) {
      tft.setCursor(TABLET_DATA_X, y);
      for (int i = 0; i < 4; i++) {
        tft.print("(");
        tft.print(button_pressed[i], DEC);
        tft.print(") ");
      }
    }
    y += line_space;
    tft.fillRect(TABLET_DATA_X, y, tft.width(), line_space, BLACK);
    if (evt == USBHostTablets::FRAME) {
      tft.setCursor(TABLET_DATA_X, y);
      for (int i = 0; i < 4; i++) {
        tft.print("(");
        tft.print(button_touched[i], DEC);
        tft.print(") ");
      }
    }
    //y += line_space; OutputNumberField(TABLET_DATA_X, y, wheel_cur, 320);
    //y += line_space; OutputNumberField(TABLET_DATA_X, y, wheelH_cur, 320);
    /*
    // Output other Axis data
    for (uint8_t i = 0; i < 9; i += 3) {
      y += line_space;
      OutputNumberField(TABLET_DATA_X, y, axis_cur[i], 75);
      OutputNumberField(TABLET_DATA_X + 75, y, axis_cur[i + 1], 75);
      OutputNumberField(TABLET_DATA_X + 150, y, axis_cur[i + 2], 75);
    }
*/
  }

  g_switch_view = ((g_pen_buttons_prev & 0x3) == 3) && ((buttons_bin & 0x1) == 0);
  g_pen_buttons_prev = buttons_bin;
  g_frame_buttons_prev = button_touch_bin;

  return something_changed;
}

//=============================================================================
// Show using psuedo graphic screen
//=============================================================================
#define BUTTON_WIDTH 15
#define BUTTON_HEIGHT 30


bool ShowSimpleGraphicScreen() {
  if (g_redraw_all) {
    // Lets display the titles.
    y_position_after_device_info = tft.getCursorY();
    tft.setTextColor(YELLOW);
    g_cnt_pen_buttons = digi.getCntPenButtons();
    g_cnt_frame_buttons = digi.getCntFrameButtons();

    if (g_cnt_frame_buttons >= g_cnt_pen_buttons) g_button_height = (tft.height() - y_position_after_device_info) / g_cnt_frame_buttons;
    else g_button_height = (tft.height() - y_position_after_device_info) / g_cnt_pen_buttons;
    if (g_button_height > BUTTON_HEIGHT) g_button_height = BUTTON_HEIGHT;
  }

  int y_start_graphics = y_position_after_device_info;


  //Serial.printf("P:%d F:%d H:%d\n", g_cnt_pen_buttons, g_cnt_frame_buttons, g_button_height);
  // Lets output pen buttons first
  uint32_t buttons = digi.getPenButtons();
  bool pen_touching = buttons & 1;
  uint16_t x = 5;
  uint16_t y = tft.height() - g_button_height - 2;
  uint8_t index = 0;
  bool buttons_changed = false;

  if (g_redraw_all) {
    g_pen_buttons_prev = buttons;
    for (index = 0; index < g_cnt_pen_buttons; index++) {
      tft.drawRect(x, y, BUTTON_WIDTH, g_button_height, GREEN);
      if (buttons & 1) tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, BLUE);
      else tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, BLACK);
      buttons >>= 1;
      y -= g_button_height;
    }
  } else if (buttons != g_pen_buttons_prev) {
    buttons_changed = true;
    g_switch_view = ((g_pen_buttons_prev & 0x3) == 3) && ((buttons & 0x1) == 0);
    uint32_t buttons_prev = g_pen_buttons_prev;
    g_pen_buttons_prev = buttons;
    for (index = 0; index < g_cnt_pen_buttons; index++) {
      if ((buttons & 1) != (buttons_prev & 1)) {
        if (buttons & 1) tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, BLUE);
        else tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, BLACK);
      }
      buttons >>= 1;
      buttons_prev >>= 1;
      y -= g_button_height;
    }
  }

  buttons = digi.getFrameButtons();
  x += BUTTON_WIDTH + 5;
  y = tft.height() - g_button_height - 2;
  if (g_redraw_all) {
    //uint32_t butprev = g_frame_buttons_prev
    g_frame_buttons_prev = buttons;
    for (index = 0; index < g_cnt_frame_buttons; index++) {
      tft.drawRect(x, y, BUTTON_WIDTH, g_button_height, GREEN);
      if (buttons & 1) tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, RED);
      else tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, BLACK);
      buttons >>= 1;
      y -= g_button_height;
    }
  } else if (buttons != g_frame_buttons_prev) {
    buttons_changed = true;
    uint32_t buttons_prev = g_frame_buttons_prev;
    g_frame_buttons_prev = buttons;
    for (index = 0; index < g_cnt_frame_buttons; index++) {
      if ((buttons & 1) != (buttons_prev & 1)) {
        if (buttons & 1) tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, RED);
        else tft.fillRect(x + 1, y + 1, BUTTON_WIDTH - 2, g_button_height - 2, BLACK);
      }
      buttons >>= 1;
      buttons_prev >>= 1;
      y -= g_button_height;
    }
  }

  // now lets output a rough tablet image:
  x += BUTTON_WIDTH + 5;
  int tab_draw_width = tft.width() - x - 5;
  int tab_draw_height = tft.height() - y_start_graphics - 5;

  if (g_redraw_all) tft.fillRect(x, y_start_graphics, tab_draw_width, tab_draw_height, DARKGREY);
  else if (buttons_changed) tft.updateScreen();

  x += 3;
  y_start_graphics += 3;
  tab_draw_width -= 6;
  tab_draw_height -= 6;
  if (g_redraw_all) tft.fillRect(x, y_start_graphics, tab_draw_width, tab_draw_height, LIGHTGREY);
  tft.setClipRect(x, y_start_graphics, tab_draw_width, tab_draw_height);

  // if we changed something in previous run, than fill it back
  if ((tablet_changed_area_x_min < tft.width()) && !g_redraw_all)
    tft.fillRect(tablet_changed_area_x_min, tablet_changed_area_y_min, tablet_changed_area_x_max - tablet_changed_area_x_min + 1,
                 tablet_changed_area_y_max - tablet_changed_area_y_min + 1, LIGHTGREY);

  tablet_changed_area_x_min = tft.width();
  tablet_changed_area_x_max = 0;
  tablet_changed_area_y_min = tft.height();
  tablet_changed_area_y_max = 0;


  USBHostTablets::event_type_t evt = digi.eventType();
  if (evt == USBHostTablets::PEN) {
    if (pen_touching) {
      int pen_x = digi.getX();
      int pen_y = digi.getY();
      int x_in_tablet = map(pen_x, 0, digi.width(), 0, tab_draw_width) + x;
      int y_in_tablet = map(pen_y, 0, digi.height(), 0, tab_draw_height) + y_start_graphics;
      //Serial.printf("Pen: (%d, %d) W:%u, H:%u -> (%d, %d)\n", pen_x, pen_y, digi.width(), digi.height(), x_in_tablet, y_in_tablet);
      tft.fillRect(x_in_tablet - 1, y_in_tablet - 10, 3, 21, BLUE);
      tft.fillRect(x_in_tablet - 10, y_in_tablet - 1, 21, 3, BLUE);
      if ((x_in_tablet - 10) < tablet_changed_area_x_min) tablet_changed_area_x_min = x_in_tablet - 10;
      if ((x_in_tablet + 10) > tablet_changed_area_x_max) tablet_changed_area_x_max = x_in_tablet + 10;
      if ((y_in_tablet - 10) < tablet_changed_area_y_min) tablet_changed_area_y_min = y_in_tablet - 10;
      if ((y_in_tablet + 10) > tablet_changed_area_y_max) tablet_changed_area_y_max = y_in_tablet + 10;
    }

  } else if (evt == USBHostTablets::TOUCH) {
    uint8_t touch_count = digi.getTouchCount();
    for (uint8_t i = 0; i < touch_count; i++) {
      int finger_x = digi.getX(i);
      int finger_y = digi.getY(i);
      int x_in_tablet = map(finger_x, 0, digi.touchWidth(), 0, tab_draw_width) + x;
      int y_in_tablet = map(finger_y, 0, digi.touchHeight(), 0, tab_draw_height) + y_start_graphics;
      //Serial.printf("Pen: (%d, %d) W:%u, H:%u -> (%d, %d)\n", finger_x, finger_y, digi.width(), digi.height(), x_in_tablet, y_in_tablet);
      tft.fillRect(x_in_tablet - 1, y_in_tablet - 10, 3, 21, RED);
      tft.fillRect(x_in_tablet - 10, y_in_tablet - 1, 21, 3, RED);
      if ((x_in_tablet - 10) < tablet_changed_area_x_min) tablet_changed_area_x_min = x_in_tablet - 10;
      if ((x_in_tablet + 10) > tablet_changed_area_x_max) tablet_changed_area_x_max = x_in_tablet + 10;
      if ((y_in_tablet - 10) < tablet_changed_area_y_min) tablet_changed_area_y_min = y_in_tablet - 10;
      if ((y_in_tablet + 10) > tablet_changed_area_y_max) tablet_changed_area_y_max = y_in_tablet + 10;
    }
    if ((touch_count == 0) && (g_touch_count_prev > 0) && (g_frame_buttons_prev == 1)) g_switch_view = 1;
    g_touch_count_prev = touch_count;
  }
  tft.setClipRect();
  g_redraw_all = false;

  return true;
}

//=============================================================================
// UpdateActiveDeviceInfo
//=============================================================================
void UpdateActiveDeviceInfo() {
  //bool new_device_detected = false;

  if (digi_was_connected) {
    if (!digi.connected()) {
      Serial.println("*** Tablet disconnected ***");
      digi_was_connected = false;
    }
  } else {
    if (digi.connect()) {
      digi_was_connected = true;

      Serial.print("*** Device Tablet ");
      Serial.print(digi.idVendor(), HEX);
      Serial.print(":");
      Serial.print(digi.idProduct(), HEX);
      Serial.println("- connected ***");
      tft.fillScreen(BLACK);  // clear the screen.
      tft.setCursor(0, 0);
      tft.setTextColor(YELLOW);
      tft.setFont(Arial_12);
      tft.print("Device Tablet ");
      tft.print(digi.idVendor(), HEX);
      tft.print(":");
      tft.print(digi.idProduct(), HEX);
      tft.println(" - connected");

      uint8_t string_buffer[80];
      if (digi.manufacturer(string_buffer, sizeof(string_buffer))) {
        tft.print("  Manufacturer: ");
        tft.println((char *)string_buffer);
      }

      if (digi.product(string_buffer, sizeof(string_buffer))) {
        tft.print("  Product: ");
        tft.println((char *)string_buffer);
      }
      if (digi.serialNumber(string_buffer, sizeof(string_buffer))) {
        tft.print("  Serial: ");
        tft.println((char *)string_buffer);
      }
      tft.updateScreen();  // update the screen now
    }
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


void MaybeSetupTextScrollArea() {
  if (g_redraw_all) {
    BT = 0;
    g_redraw_all = false;
  }
  if (BT == 0) {
    tft.enableScroll();
    tft.setScrollTextArea(20, 70, 280, 140);
    tft.setScrollBackgroundColor(GREEN);
    tft.setFont(Arial_11);
    tft.setTextColor(BLACK);
    tft.setCursor(20, 70);
    BT = 1;
  }
}