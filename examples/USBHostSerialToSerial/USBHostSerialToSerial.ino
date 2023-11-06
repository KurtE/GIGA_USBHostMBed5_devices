//  Simple Echo USBHost Serial object with the main USB Serial object
#include <elapsedMillis.h>
#include <LibPrintf.h>
#include <USBHostSerialDevice.h>
#include <GIGA_digitalWriteFast.h>
REDIRECT_STDOUT_TO(Serial)

USBHostSerialDevice hser(true);
int Fix_Serial_avalableForWrite = 0;
int Fix_hser_avalableForWrite = 0;

elapsedMillis emBlink;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {}

  Serial.println("Starting USB host Serial device test...");

  // Enable the USBHost
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);

  while (!hser.connect()) {
    Serial.println("No USB host Serial device connected");
    delay(5000);
  }

  printf("USB host Serial device(%x:%x) connected trying begin\n\r", hser.idVendor(), hser.idProduct());

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

  hser.begin(4800);
  //hser.setDTR(false);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  pinMode(5, OUTPUT);
  pinMode(4, OUTPUT);

  Fix_Serial_avalableForWrite = Serial.availableForWrite() ? 0 : 1;
  if (Fix_Serial_avalableForWrite) Serial.println("*** Serial does not support availableForWrite ***");
  Fix_hser_avalableForWrite = hser.availableForWrite() ? 0 : 1;
  if (Fix_hser_avalableForWrite) Serial.println("*** USB Host Serial does not support availableForWrite ***");
}

uint8_t copy_buffer[64];

void loop() {
  int cbAvail;
  int cbAvailForWrite;

  if (emBlink > 500) {
    emBlink = 0;
    digitalToggleFast(LED_GREEN);
  }

  if ((cbAvail = Serial.available())) {
    digitalToggleFast(LED_RED);
    cbAvailForWrite = max(hser.availableForWrite(), Fix_Serial_avalableForWrite);
    int cb = min(cbAvail, min(cbAvailForWrite, (int)sizeof(copy_buffer)));
    int cbRead = Serial.readBytes(copy_buffer, cb);
    //printf("S(%d)->HS(%d): %u %u\n\r", cbAvail, cbAvailForWrite, cb, cbRead);
    if (cbRead > 0) {
      hser.write(copy_buffer, cbRead);
      //hser.flush();
    }
  }
  if ((cbAvail = hser.available())) {
    digitalToggleFast(LED_BLUE);
    cbAvailForWrite = max(Serial.availableForWrite(), Fix_Serial_avalableForWrite);
    int cb = min(cbAvail, min(cbAvailForWrite, (int)sizeof(copy_buffer)));
    //printf("HS(%d)->S(%d): %u\n\r", cbAvail, cbAvailForWrite, cb);
    int cbRead = hser.readBytes(copy_buffer, cb);
    if (cbRead > 0)  Serial.write(copy_buffer, cbRead);
  }
}
