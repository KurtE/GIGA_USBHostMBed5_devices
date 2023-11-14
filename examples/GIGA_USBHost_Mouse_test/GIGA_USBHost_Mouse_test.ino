// Simple test of USB Host Mouse/Keyboard
//
// This example is in the public domain

#include <Arduino_USBHostMbed5.h>
#include "USBHostMouseEx.h"
REDIRECT_STDOUT_TO(Serial)

USBHostMouseEx mouse;
bool show_changed_only = false;

void setup()
{
   Serial.begin(115200);
    while (!Serial && millis() < 5000) {}

    Serial.println("Starting mouse test...");

    // Enable the USBHost 
    pinMode(PA_15, OUTPUT);
    digitalWrite(PA_15, HIGH);

    // if you are using a Max Carrier uncomment the following line
    // start_hub();

    while (!mouse.connect()) {
      Serial.println("No mouse connected");        
        delay(5000);
    }
}


void loop()
{
  if (mouse.available()) {
    Serial.print("Mouse: buttons = ");
    Serial.print(mouse.getButtons());
    Serial.print(",  mouseX = ");
    Serial.print(mouse.getMouseX());
    Serial.print(",  mouseY = ");
    Serial.print(mouse.getMouseY());
    Serial.print(",  wheel = ");
    Serial.print(mouse.getWheel());
    Serial.print(",  wheelH = ");
    Serial.print(mouse.getWheelH());
    Serial.println();
    mouse.mouseDataClear();
  }
}
