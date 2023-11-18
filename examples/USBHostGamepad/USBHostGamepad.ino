#define USBHOST_OTHER
REDIRECT_STDOUT_TO(Serial)

#include <Arduino_USBHostMbed5.h>
#include "USBHostJoystickEX.h"

USBHostJoystickEX joystick;

uint8_t joystick_left_trigger_value = 0;
uint8_t joystick_right_trigger_value = 0;
uint32_t buttons = 0;
uint32_t buttons_prev = 0;
uint8_t ltv, rtv;
int psAxis[64];

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {}

  Serial.println("Starting joystick test...");

  // Enable the USBHost
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);
  delay(100);

  delay(2000);
}

void loop() {

  UpdateActiveDeviceInfo();

  if (joystick.available()) {
    for (uint8_t i = 0; i < 64; i++) {
        psAxis[i] = joystick.getAxis(i);
    }

    printf("  Joystick Data: ");
    for (uint16_t i = 0; i < 40; i++) printf("%d:%02x ", i, psAxis[i]);
    printf("\r\n");
  
    buttons = joystick.getButtons();
    printf("joystick buttons = %x\n", buttons);

    switch (joystick.joystickType()) {
      default:
        break;
      case USBHostJoystickEX::PS4:
        //axis values:
        printf("lx: %d, ly: %d, rx: %d, ry: %d\n", joystick.getAxis(0), joystick.getAxis(1), joystick.getAxis(2), joystick.getAxis(5));

        //left and right triggers
        ltv = joystick.getAxis(3);
        rtv = joystick.getAxis(4);
        printf("ltv: %d, rtv: %d\n", ltv, rtv);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          joystick.setRumble(ltv, rtv);
        }
        printf("\n");
        break;
      case USBHostJoystickEX::SWITCH:
        //axis values:
        printf("lx: %d, ly: %d, rx: %d, ry: %d\n", joystick.getAxis(0), joystick.getAxis(1), joystick.getAxis(2), joystick.getAxis(3));

        //left and right triggers
        ltv = joystick.getAxis(6);
        rtv = joystick.getAxis(7);
        printf("ltv: %d, rtv: %d\n", ltv, rtv);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          joystick.setRumble(ltv, rtv);
        }
        break;
      case USBHostJoystickEX::PS3:
        //axis values:
        printf("lx: %d, ly: %d, rx: %d, ry: %d\n", joystick.getAxis(0), joystick.getAxis(1), joystick.getAxis(2), joystick.getAxis(3));
        ltv = joystick.getAxis(18);
        rtv = joystick.getAxis(19);
        printf("  ltv: %d,   rtv: %d\n", ltv, rtv);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          joystick.setRumble(ltv, rtv, 50);
          printf(" Set Rumble %d %d\n", ltv, rtv);
        }
        float angles[2];
        getAnglesPS3(angles);
        printf("  Pitch: %f\n", angles[0]);
        printf("  Roll: %f\n", angles[1]);
        break;
      case USBHostJoystickEX::XBOXONE:
      case USBHostJoystickEX::XBOX360:
        //axis values:
        printf("lx: %d, ly: %d, rx: %d, ry: %d\n", joystick.getAxis(0), joystick.getAxis(1), joystick.getAxis(2), joystick.getAxis(5));
        ltv = joystick.getAxis(6);
        rtv = joystick.getAxis(7);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          joystick.setRumble(ltv, rtv, 50);
          printf(" Set Rumble %d %d\n", ltv, rtv);
        }
        break;
    }
    Serial.println();

    if (buttons != buttons_prev) {
      if (joystick.joystickType() == USBHostJoystickEX::PS3) {
        //joysticks[joystick_index].setLEDs((buttons >> 12) & 0xf); //  try to get to TRI/CIR/X/SQuare
        uint8_t leds = 0;
        if (buttons & 0x8000) leds = 6;  //Srq
        if (buttons & 0x2000) leds = 2;  //Cir
        if (buttons & 0x1000) leds = 4;  //Tri
        if (buttons & 0x4000) leds = 8;  //X  //Tri
        joystick.setLEDs(leds, leds, leds);
      } else {
        uint8_t lr = (buttons & 1) ? 0xff : 0;
        uint8_t lg = (buttons & 2) ? 0xff : 0;
        uint8_t lb = (buttons & 4) ? 0xff : 0;
        joystick.setLEDs(lr, lg, lb);
      }
      buttons_prev = buttons;
    }
    
    joystick.joystickDataClear();

  }
  delay(500);

  if(!joystick.connected()) {
    joystick.disconnect();
    while(!joystick.connect()) {
      Serial.println("No joystick");
      delay(5000);
    }
  }
}


void UpdateActiveDeviceInfo() {
  if (joystick.connected()) return;

  while (!joystick.connect()) {
    Serial.println("No Joystick connected");
    delay(5000);
  }
  Serial.print("\nJoystick (");
  Serial.print(joystick.idVendor(), HEX);
  Serial.print(":");
  Serial.print(joystick.idProduct(), HEX);
  Serial.println("): connected\n");

  uint8_t string_buffer[80];
  if (joystick.manufacturer(string_buffer, sizeof(string_buffer))) {
    Serial.print("Manufacturer: ");
    Serial.println((char *)string_buffer);
  }

  if (joystick.product(string_buffer, sizeof(string_buffer))) {
    Serial.print("Product: ");
    Serial.println((char *)string_buffer);
  }
  if (joystick.serialNumber(string_buffer, sizeof(string_buffer))) {
    Serial.print("Serial Number: ");
    Serial.println((char *)string_buffer);
  }
}