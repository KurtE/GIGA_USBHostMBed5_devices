#define USBHOST_OTHER
REDIRECT_STDOUT_TO(Serial)

#include <Arduino_USBHostMbed5.h>
#include <LibPrintf.h>
#include "USBHostGamepadDevice.h"

USBHostGamepad joystick;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {}

  Serial.println("Starting joystick test...");

  // Enable the USBHost
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);
  delay(100);

  while (!joystick.connect()) {
    Serial.println("No  connected");
    delay(5000);
  }

  Serial.println("Joystick connected:");
 
  uint8_t string_buffer[80];
  if (joystick.manufacturer(string_buffer, sizeof(string_buffer))) {
    Serial.print("Manufacturer: ");
    Serial.println((char*)string_buffer);
  }
  if (joystick.product(string_buffer, sizeof(string_buffer))) {
    Serial.print("Product: ");
    Serial.println((char*)string_buffer);
  }
  if (joystick.serialNumber(string_buffer, sizeof(string_buffer))) {
    Serial.print("Serial Number: ");
    Serial.println((char*)string_buffer);
  }
  delay(2000);
}

void loop() {
  if (joystick.available()) {

    //printf("  Joystick Data: ");
    //for (uint16_t i = 0; i < 40; i++) printf("%d:%02x ", i, psAxis[i]);
    //printf("\r\n");

    switch (joystick.gamepadType()) {
      default:
        break;
      case USBHostGamepad::LogiExtreme3DPro:
        printf("rx: %d, ry: %d, rz: %d, hat: %x\n", joystick.getAxis(0), joystick.getAxis(1), joystick.getAxis(2), joystick.getAxis(3));
        printf("Slider: %d, ButtonsA: %x, ButtonsB: %d\n", joystick.getAxis(4), joystick.getAxis(5), joystick.getAxis(6));
        uint8_t b[16]; int16_t v;
        //Button A breakout
        v =  joystick.getAxis(5);
        for (uint8_t j = 0;  j < 8;  ++j) {
          b [j] =  0 != (v & (1 << j));
        }
        //button B group breakout.
        v =  joystick.getAxis(6);
        for (uint8_t j = 0;  j < 8;  ++j) {
          b [j+8] =  0 != (v & (1 << j));
        }
        //print button map
        Serial.print("Button Map: ");
        for(uint16_t j = 0; j < 12; ++j) {
          Serial.print(b[j]); Serial.print(", ");
        } 
        Serial.println();
        break;
    }
    Serial.println();

  }
  delay(500);

  if(!joystick.connected()) {
    joystick.disconnect();
    while(!joystick.connect()) {
      Serial.println("No gamepad");
      delay(5000);
    }
  }
}
