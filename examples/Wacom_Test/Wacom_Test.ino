// Test for some Wacom Tablets
// This is a WIP.
//
// This example is in the public domain

#include "USBHostTablets.h"
#include <LibPrintf.h>
REDIRECT_STDOUT_TO(Serial)

USBHostTablets digi;

int axis_prev[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

bool show_changed_only = false;

void setup() {
  Serial1.begin(2000000);
  while (!Serial)
    ;  // wait for Arduino Serial Monitor
  Serial.println("\n\nWacom Tablet Test Program");

  // Enable the USBHost
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);

  // if you are using a Max Carrier uncomment the following line
  // start_hub();
}


void loop() {

  if (Serial.available()) {
    int ch = Serial.read();  // get the first char.
    while (Serial.read() != -1)
      ;
    if (ch == 'c') {
      show_changed_only = !show_changed_only;
      if (show_changed_only) Serial.println("\n** Turned on Show Changed Only mode **");
      else
        Serial.println("\n** Turned off Show Changed Only mode **");
    } else {
      if (digi.debugPrint()) {
        digi.debugPrint(false);
        Serial.println("\n*** Turned off debug printing ***");
      } else {
        digi.debugPrint(true);
        Serial.println("\n*** Turned on debug printing ***");
      }
    }
  }

  // Update the display with
  UpdateActiveDeviceInfo();

  if (digi.available()) {
    int touch_count = digi.getTouchCount();
    int touch_index;
    uint16_t buttons_bin = digi.getPenButtons();
    bool pen_button[3];
    Serial.print("Digitizer: ");
    for (int i = 0; i < 3; i++) {
      pen_button[i] = (buttons_bin >> i) & 0x1;

      Serial.print("Pen_Btn");
      Serial.print(i, DEC);
      Serial.print(":");
      Serial.print(pen_button[i], DEC);
      Serial.print(" ");
    }

    switch (digi.eventType()) {
      case USBHostTablets::TOUCH:
        Serial.print(" Touch:");
        for (touch_index = 0; touch_index < touch_count; touch_index++) {
          Serial.print(" (");
          Serial.print(digi.getX(touch_index), DEC);
          Serial.print(", ");
          Serial.print(digi.getY(touch_index), DEC);
          Serial.print(")");
        }
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
          //buttons are saved within one byte for all the 8 buttons, here is a decoding example
          uint16_t button_touch_bin = digi.getFrameTouchButtons();
          uint16_t button_press_bin = digi.getFrameButtons();
          bool button_touched[8];
          bool button_pressed[8];
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
        }
        break;
      default:
        Serial.print(",  X = ");
        Serial.print(digi.getX(), DEC);
        Serial.print(", Y = ");
        Serial.print(digi.getY(), DEC);
        printf(",  X = %u, Y = %u", digi.getX(), digi.getY());
        Serial.print(",  Pressure: = ");
        Serial.print(",  wheel = ");
        Serial.print(digi.getWheel());
        Serial.print(",  wheelH = ");
        Serial.print(digi.getWheelH());
        if (show_changed_only) Serial.print("\t: ");
        else
          Serial.println();
        for (uint8_t i = 0; i < 16; i++) {
          int axis = digi.getAxis(i);
          if (show_changed_only) {
            if (axis != axis_prev[i]) {
              Serial.print(" ");
              Serial.print(i, DEC);
              Serial.print("#");
              Serial.print(axis, DEC);
              Serial.print("(");
              Serial.print(axis, HEX);
              Serial.print(")");
            }
          } else {
            Serial.print(" ");
            Serial.print(i, DEC);
            Serial.write((axis == axis_prev[i]) ? ':' : '#');
            Serial.print(axis, DEC);
            Serial.print("(");
            Serial.print(axis, HEX);
            Serial.print(")");
          }
          axis_prev[i] = axis;
        }
        break;
    }
    Serial.println();
    digi.digitizerDataClear();
  }
}

void UpdateActiveDeviceInfo() {
  if (digi.connected()) return;

  while (!digi.connect()) {
    Serial.println("No tablet connected");
    delay(5000);
  }
  Serial.print("\nTablet (");
  Serial.print(digi.idVendor(), HEX);
  Serial.print(":");
  Serial.print(digi.idProduct(), HEX);
  Serial.println("): connected\n");

  uint8_t string_buffer[80];
  if (digi.manufacturer(string_buffer, sizeof(string_buffer))) {
    Serial.print("Manufacturer: ");
    Serial.println((char *)string_buffer);
  }

  if (digi.product(string_buffer, sizeof(string_buffer))) {
    Serial.print("Product: ");
    Serial.println((char *)string_buffer);
  }
  if (digi.serialNumber(string_buffer, sizeof(string_buffer))) {
    Serial.print("Serial Number: ");
    Serial.println((char *)string_buffer);
  }
}
