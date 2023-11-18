#define USBHOST_OTHER
//REDIRECT_STDOUT_TO(Serial)

#include <Arduino_USBHostMbed5.h>
#include <LibPrintf.h>
#include "USBHostJoystickEX.h"

USBHostJoystickEX nes;

uint32_t buttons = 0;
String direction[13] = {"None", "UP", "RIGHT", "NE", "DOWN", "", "SE", "", "LEFT", "NW", "", "", "SW"};

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {}

  Serial.println("Starting nes test...");

  // Enable the USBHost
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);
  delay(100);

  while (!nes.connect()) {
    Serial.println("NES Not  connected");
    delay(5000);
  }

  Serial.println("NES connected:");
 
  uint8_t string_buffer[80];
  if (nes.manufacturer(string_buffer, sizeof(string_buffer))) {
    Serial.print("Manufacturer: ");
    Serial.println((char*)string_buffer);
  }
  if (nes.product(string_buffer, sizeof(string_buffer))) {
    Serial.print("Product: ");
    Serial.println((char*)string_buffer);
  }
  if (nes.serialNumber(string_buffer, sizeof(string_buffer))) {
    Serial.print("Serial Number: ");
    Serial.println((char*)string_buffer);
  }
  delay(2000);
}

void loop() {
  if (nes.available()) {

    buttons = nes.getButtons();
    printf("NES buttons = %x\n", buttons);

    uint8_t dpad = (buttons & 0xff);
    if(dpad > 0){
      printf("DPAD: %d -> %s\n", dpad, direction[dpad].c_str());
    }

    uint8_t buttonAB = ((buttons & 0x00ff00) >> 8);
    if(buttonAB == 0x02) Serial.println("A Button Pressed");
    if(buttonAB == 0x04) Serial.println("B Button Pressed");
    if(buttonAB == 0x06) Serial.println("A & B Buttons Pressed");

    uint8_t buttonSS = ((buttons & 0xff0000)) >> 16;
    if(buttonSS == 0x10) Serial.println("Select Button Pressed");
    if(buttonSS == 0x20) Serial.println("Start Button Pressed");
    if(buttonSS == 0x30) Serial.println("Select & Start Buttons Pressed");
    Serial.println();

    nes.joystickDataClear();

  }

  if(!nes.connected()) {
    nes.disconnect();
    while(!nes.connect()) {
      Serial.println("No gamepad");
      delay(5000);
    }
  }

  delay(500);
}
