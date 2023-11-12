#include <Arduino_USBHostMbed5.h>
#include <LibPrintf.h>
#include "USBDumperDevice.h"

REDIRECT_STDOUT_TO(Serial)

USBDumperDevice hser;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {}

  Serial.println("\n\n*** starting Device information sketch ***");

  // Enable the USBHost
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);
  delay(500);


}

void loop() {
  while (!hser.connect()) {
    Serial.println("No USB host device connected");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(1000);
  }

  printf("USB host device(%x:%x) connected\n", hser.idVendor(), hser.idProduct());

  uint8_t string_buffer[80];
  if (hser.manufacturer(string_buffer, sizeof(string_buffer))) {
    Serial.print("Manufacturer: ");
    Serial.println((char*)string_buffer);
  }

  if (hser.product(string_buffer, sizeof(string_buffer))) {
    Serial.print("Product: ");
    Serial.println((char*)string_buffer);
  }
  if (hser.serialNumber(string_buffer, sizeof(string_buffer))) {
    Serial.print("Serial Number: ");
    Serial.println((char*)string_buffer);
  }

  // wait to see if the device is removed or not.
  while (hser.connected()) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(1000);
  }

  Serial.println("\n*** Device disconnected ***\n");
  hser.disconnect();

}
