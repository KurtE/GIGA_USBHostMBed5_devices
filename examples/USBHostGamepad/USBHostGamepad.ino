#define USBHOST_OTHER
REDIRECT_STDOUT_TO(Serial)

#include <Arduino_USBHostMbed5.h>
#include <LibPrintf.h>
#include "USBHostGamepad.h"

USBHostGamepad gamepad;

uint8_t joystick_left_trigger_value = 0;
uint8_t joystick_right_trigger_value = 0;
uint32_t buttons = 0;
uint32_t buttons_prev = 0;
uint8_t ltv, rtv;
int psAxis[64];

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {}

  Serial.println("Starting gamepad test...");

  // Enable the USBHost
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);
  delay(100);

  while (!gamepad.connect()) {
    Serial.println("No gamepad connected");
    delay(5000);
  }

  Serial.println("Joystick connected:");
 
  uint8_t string_buffer[80];
  if (gamepad.manufacturer(string_buffer, sizeof(string_buffer))) {
    Serial.print("Manufacturer: ");
    Serial.println((char*)string_buffer);
  }
  if (gamepad.product(string_buffer, sizeof(string_buffer))) {
    Serial.print("Product: ");
    Serial.println((char*)string_buffer);
  }
  if (gamepad.serialNumber(string_buffer, sizeof(string_buffer))) {
    Serial.print("Serial Number: ");
    Serial.println((char*)string_buffer);
  }
  delay(2000);
}

void loop() {
  if (gamepad.available()) {
    for (uint8_t i = 0; i < 64; i++) {
        psAxis[i] = gamepad.getAxis(i);
    }

    //printf("  Joystick Data: ");
    //for (uint16_t i = 0; i < 40; i++) printf("%d:%02x ", i, psAxis[i]);
    //printf("\r\n");

    if(gamepad.gamepadType() != USBHostGamepad::LogiExtreme3DPro){
      buttons = gamepad.getButtons();
      printf("gamepad buttons = %x\n", buttons);
      //axis values:
      printf("lx: %d, ly: %d, rx: %d, ry: %d\n", gamepad.getAxis(0), gamepad.getAxis(1), gamepad.getAxis(2), gamepad.getAxis(3));
    }
    
    switch (gamepad.gamepadType()) {
      default:
        break;
      case USBHostGamepad::PS4:
        //left and right triggers
        ltv = gamepad.getAxis(7);
        rtv = gamepad.getAxis(8);
        printf("ltv: %d, rtv: %d\n", ltv, rtv);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          gamepad.setRumble(ltv, rtv);
        }
        printf("\n");
        break;
      case USBHostGamepad::SWITCH:
        //left and right triggers
        ltv = gamepad.getAxis(6);
        rtv = gamepad.getAxis(7);
        printf("ltv: %d, rtv: %d\n", ltv, rtv);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          gamepad.setRumble(ltv, rtv);
        }
        break;
      case USBHostGamepad::PS3:
        ltv = gamepad.getAxis(18);
        rtv = gamepad.getAxis(19);
        printf("  ltv: %d,   rtv: %d\n", ltv, rtv);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          gamepad.setRumble(ltv, rtv, 50);
          printf(" Set Rumble %d %d\n", ltv, rtv);
        }
        float angles[2];
        getAnglesPS3(angles);
        printf("  Pitch: %f\n", angles[0]);
        printf("  Roll: %f\n", angles[1]);
        break;
      case USBHostGamepad::XBOXONE:
      case USBHostGamepad::XBOX360:
        ltv = gamepad.getAxis(6);
        rtv = gamepad.getAxis(7);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          gamepad.setRumble(ltv, rtv, 50);
          printf(" Set Rumble %d %d", ltv, rtv);
        }
        break;
      case USBHostGamepad::LogiExtreme3DPro:
        printf("rx: %d, ry: %d, rz: %d, hat: %x\n", gamepad.getAxis(0), gamepad.getAxis(1), gamepad.getAxis(2), gamepad.getAxis(3));
        printf("Slider: %d, ButtonsA: %x, ButtonsB: %d\n", gamepad.getAxis(4), gamepad.getAxis(5), gamepad.getAxis(6));
        uint8_t b[16]; int16_t v;
        //Button A breakout
        v =  gamepad.getAxis(5);
        for (uint8_t j = 0;  j < 8;  ++j) {
          b [j] =  0 != (v & (1 << j));
        }
        //button B group breakout.
        v =  gamepad.getAxis(6);
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

    if(gamepad.gamepadType() != USBHostGamepad::LogiExtreme3DPro) {
      if (buttons != buttons_prev) {
        if (gamepad.gamepadType() == USBHostGamepad::PS3) {
          //joysticks[joystick_index].setLEDs((buttons >> 12) & 0xf); //  try to get to TRI/CIR/X/SQuare
          uint8_t leds = 0;
          if (buttons & 0x8000) leds = 6;  //Srq
          if (buttons & 0x2000) leds = 2;  //Cir
          if (buttons & 0x1000) leds = 4;  //Tri
          if (buttons & 0x4000) leds = 8;  //X  //Tri
          gamepad.setLEDs(leds, leds, leds);
        } else {
          uint8_t lr = (buttons & 1) ? 0xff : 0;
          uint8_t lg = (buttons & 2) ? 0xff : 0;
          uint8_t lb = (buttons & 4) ? 0xff : 0;
          gamepad.setLEDs(lr, lg, lb);
        }
        buttons_prev = buttons;
      }
    }
  }
  delay(500);

  if(!gamepad.connected()) {
    gamepad.disconnect();
    while(!gamepad.connect()) {
      Serial.println("No gamepad");
      delay(5000);
    }
  }
}
