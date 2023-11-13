/* mbed USBHost Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "USBHostKeyboardEx.h"
#include <MemoryHexDump.h>

#if USBHOST_KEYBOARD

static const uint8_t keymap[3][0x39] = {
  { 0, 0, 0, 0, 'a', 'b' /*0x05*/,
    'c', 'd', 'e', 'f', 'g' /*0x0a*/,
    'h', 'i', 'j', 'k', 'l' /*0x0f*/,
    'm', 'n', 'o', 'p', 'q' /*0x14*/,
    'r', 's', 't', 'u', 'v' /*0x19*/,
    'w', 'x', 'y', 'z', '1' /*0x1E*/,
    '2', '3', '4', '5', '6' /*0x23*/,
    '7', '8', '9', '0', 0x0A /*enter*/,                                     /*0x28*/
    0x1B /*escape*/, 0x08 /*backspace*/, 0x09 /*tab*/, 0x20 /*space*/, '-', /*0x2d*/
    '=', '[', ']', '\\', '#',                                               /*0x32*/
    ';', '\'', 0, ',', '.',                                                 /*0x37*/
    '/' },

  /* CTRL MODIFIER */
  {
    0, 0, 0, 0, 1, 2 /*0x05*/,
    3, 4, 5, 6, 7 /*0x0a*/,
    8, 9, 10, 11, 12 /*0x0f*/,
    13, 14, 15, 16, 17 /*0x14*/,
    18, 19, 20, 21, 22 /*0x19*/,
    23, 24, 25, 26, 0 /*0x1E*/,
    0, 0, 0, 0, 0 /*0x23*/,
    0, 0, 0, 0, 0 /*enter*/, /*0x28*/
    0, 0, 0, 0, 0,           /*0x2d*/
    0, 0, 0, 0, 0,           /*0x32*/
    0, 0, 0, 0, 0,           /*0x37*/
    0 },

  /* SHIFT MODIFIER */
  {
    0, 0, 0, 0, 'A', 'B' /*0x05*/,
    'C', 'D', 'E', 'F', 'G' /*0x0a*/,
    'H', 'I', 'J', 'K', 'L' /*0x0f*/,
    'M', 'N', 'O', 'P', 'Q' /*0x14*/,
    'R', 'S', 'T', 'U', 'V' /*0x19*/,
    'W', 'X', 'Y', 'Z', '!' /*0x1E*/,
    '@', '#', '$', '%', '^' /*0x23*/,
    '&', '*', '(', ')', 0,   /*0x28*/
    0, 0, 0, 0, 0,           /*0x2d*/
    '+', '{', '}', '|', '~', /*0x32*/
    ':', '"', 0, '<', '>',   /*0x37*/
    '?' },

  /* ALT MODIFIER */

};

#define KEY_INSERT (73)
#define KEY_HOME (74)
#define KEY_PAGE_UP (75)
#define KEY_DELETE (76)
#define KEY_END (77)
#define KEY_PAGE_DOWN (78)
#define KEY_RIGHT (79)
#define KEY_LEFT (80)
#define KEY_DOWN (81)
#define KEY_UP (82)
#define KEYPAD_SLASH (84)
#define KEYPAD_ASTERIX (85)
#define KEYPAD_MINUS (86)
#define KEYPAD_PLUS (87)
#define KEYPAD_ENTER (88)
#define KEYPAD_1 (89)
#define KEYPAD_2 (90)
#define KEYPAD_3 (91)
#define KEYPAD_4 (92)
#define KEYPAD_5 (93)
#define KEYPAD_6 (94)
#define KEYPAD_7 (95)
#define KEYPAD_8 (96)
#define KEYPAD_9 (97)
#define KEYPAD_0 (98)
#define KEYPAD_PERIOD (99)

typedef struct {
  uint8_t code;
  uint8_t codeNumlockOff;
  uint8_t charNumlockOn;  // We will assume when num lock is on we have all characters...
} keycode_numlock_t;

static const keycode_numlock_t keycode_numlock[] = {
  { KEYPAD_SLASH, '/', '/' },
  { KEYPAD_ASTERIX, '*', '*' },
  { KEYPAD_MINUS, '-', '-' },
  { KEYPAD_PLUS, '+', '+' },
  { KEYPAD_ENTER, '\n', '\n' },
  { KEYPAD_1, 0x80 | KEY_END, '1' },
  { KEYPAD_2, 0x80 | KEY_DOWN, '2' },
  { KEYPAD_3, 0x80 | KEY_PAGE_DOWN, '3' },
  { KEYPAD_4, 0x80 | KEY_LEFT, '4' },
  { KEYPAD_5, 0x00, '5' },
  { KEYPAD_6, 0x80 | KEY_RIGHT, '6' },
  { KEYPAD_7, 0x80 | KEY_HOME, '7' },
  { KEYPAD_8, 0x80 | KEY_UP, '8' },
  { KEYPAD_9, 0x80 | KEY_PAGE_UP, '9' },
  { KEYPAD_0, 0x80 | KEY_INSERT, '0' },
  { KEYPAD_PERIOD, 0x80 | KEY_DELETE, '.' }
};

#define KEY_CAPS_LOCK (0x39)
#define KEY_SCROLL_LOCK (0x47)
#define KEY_NUM_LOCK (0x53)


USBHostKeyboardEx::USBHostKeyboardEx() {
  // Don't reset these each time...
  onKey = NULL;
  onKeyCode = NULL;
  onKeyRelease = NULL;
  init();
}


void USBHostKeyboardEx::init() {
  dev = NULL;
  int_in = NULL;
  int_extras_in = NULL;
  report_id = 0;
  dev_connected = false;
  keyboard_intf = -1;
  keyboard_extras_intf = -1;
  keyboard_device_found = false;
}

bool USBHostKeyboardEx::connected() {
  return dev_connected;
}


bool USBHostKeyboardEx::connect() {

  if (dev_connected) {
    return true;
  }
  host = USBHost::getHostInst();

  for (uint8_t i = 0; i < MAX_DEVICE_CONNECTED; i++) {
    if ((dev = host->getDevice(i)) != NULL) {

      int ret = host->enumerate(dev, this);
      if (ret) {
        break;
      }


      if (keyboard_device_found) {
        {
          /* As this is done in a specific thread
                     * this lock is taken to avoid to process the device
                     * disconnect in usb process during the device registering */
          USBHost::Lock Lock(host);

          int_in = dev->getEndpoint(keyboard_intf, INTERRUPT_ENDPOINT, IN);

          if (!int_in) {
            break;
          }

          USB_INFO("New Keyboard device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, keyboard_intf);
          dev->setName("Keyboard", keyboard_intf);
          host->registerDriver(dev, keyboard_intf, this, &USBHostKeyboardEx::init);

          int_in->attach(this, &USBHostKeyboardEx::rxHandler);

          // Now see if we found a keyboard extras
          if (keyboard_extras_intf != -1) {
            int_extras_in = dev->getEndpoint(keyboard_extras_intf, INTERRUPT_ENDPOINT, IN);
            if (int_extras_in) {
              int_extras_in->attach(this, &USBHostKeyboardEx::rxExtrasHandler);

              size_extras_in_ = int_extras_in->getSize();
              printf("\n\n&&&&&&&&&&&&&& >>> HID Extras endpoint %p size:%lu\n", int_extras_in, size_extras_in_);
            }
            
          }
        }
        host->interruptRead(dev, int_in, report, int_in->getSize(), false);

        if (int_extras_in)host->interruptRead(dev, int_extras_in, buf_extras, size_extras_in_);


        dev_connected = true;
        return true;
      }
    }
  }
  init();
  return false;
}

static bool contains(uint8_t b, const uint8_t *data) {
  if (data[2] == b || data[3] == b || data[4] == b) return true;
  if (data[5] == b || data[6] == b || data[7] == b) return true;
  return false;
}


void USBHostKeyboardEx::rxHandler() {
  int len = int_in->getLengthTransferred();
  //int index = (len == 9) ? 1 : 0;
  int len_listen = int_in->getSize();
  uint8_t key = 0;
  if (len == 8 || len == 9) {
    // boot format: byte 0 mod, 1=skip, 2-7 keycodes
    if (memcmp(report, prev_report, len)) {
      //printf("USBHostKeyboardEx::rxHandler l:%u %u = ", len, len_listen);
      //for (uint8_t i = 0; i < len; i++) printf("%02X ", report[i]);
      //printf("\n");


      uint8_t modifier = report[0];
      // first check for new key presses
      for (uint8_t i = 2; i < 8; i++) {
        uint8_t keycode = report[i];
        if (keycode == 0) break;  // no more keys pressed
        if (!contains(keycode, prev_report)) {
          // new key press
          if (onKey) {
            // This is pretty lame... and buggy
            key = mapKeycodeToKey(modifier, keycode);
            //printf("key: %x(%c)\n", key, key);
            if (key) (*onKey)(key);
          }
          if (onKeyCode) (*onKeyCode)(report[i], modifier);
        }
      }
      // next check for releases.
      for (uint8_t i = 2; i < 8; i++) {
        uint8_t keycode = prev_report[i];
        if (keycode == 0) break;  // no more keys pressed
        if (!contains(keycode, report)) {
          if (onKeyRelease) {
            key = mapKeycodeToKey(prev_report[0], prev_report[i]);
            if (key) (*onKeyRelease)(key);
          }
          // Now see if this is one of the modifier keys
          if (keycode == KEY_NUM_LOCK) {
            numLock(!leds_.numLock);
            // Lets toggle Numlock
          } else if (keycode == KEY_CAPS_LOCK) {
            capsLock(!leds_.capsLock);

          } else if (keycode == KEY_SCROLL_LOCK) {
            scrollLock(!leds_.scrollLock);
          }
        }
      }
      memcpy(prev_report, report, 8);
    }
  }
  if (dev && int_in) {
    host->interruptRead(dev, int_in, report, len_listen, false);
  }
}

uint8_t USBHostKeyboardEx::mapKeycodeToKey(uint8_t modifier, uint8_t keycode) {
  // first hack, handle keypad to see if we need to map characters
  for (uint8_t i = 0; i < (sizeof(keycode_numlock) / sizeof(keycode_numlock[0])); i++) {
    if (keycode_numlock[i].code == keycode) {
      // See if the user is using numlock or not...
      if (leds_.numLock) {
        return keycode_numlock[i].charNumlockOn;
      } else {
        keycode = keycode_numlock[i].codeNumlockOff;
        if (!(keycode & 0x80)) return keycode;  // we have hard coded value
        keycode &= 0x7f;                    // mask off the extra and break out to process as other characters...
        break;
      }
    }
  }

  //[rg ra rs rc lg la ls lc]
  modifier = (modifier | (modifier >> 4)) & 0xf;  // merge left and right modifiers.
  if (keycode <= 0x39) {
    if (modifier <= 2) return keymap[modifier][keycode];
    return 0;
  }

  // any other case return 0;
  return 0;
}
void USBHostKeyboardEx::numLock(bool f) {
  if (leds_.numLock != f) {
    leds_.numLock = f;
    updateLEDS();
  }
}

void USBHostKeyboardEx::capsLock(bool f) {
  if (leds_.capsLock != f) {
    leds_.capsLock = f;
    updateLEDS();
  }
}

void USBHostKeyboardEx::scrollLock(bool f) {
  if (leds_.scrollLock != f) {
    leds_.scrollLock = f;
    updateLEDS();
  }
}

void USBHostKeyboardEx::LEDS(uint8_t leds) {
  printf("Keyboard setLEDS %x\n", leds);
  leds_.byte = leds;
  updateLEDS();
}

void USBHostKeyboardEx::updateLEDS() {
  if (host && dev) {
    printf("$$$ updateLEDS: %x\n", leds_.byte);
    host->controlWrite(dev, 0x21, 9, 0x200, 0, (uint8_t *)&leds_.byte, sizeof(leds_.byte));
  }
}


void USBHostKeyboardEx::rxExtrasHandler() {
  int len = int_extras_in->getLengthTransferred();

  if (len) {
    Serial.println("$$$ Extras HID RX $$$");
    MemoryHexDump(Serial, buf_extras, len, true, nullptr, -1, 0);
  }

  host->interruptRead(dev, int_extras_in, buf_extras, size_extras_in_);
}



/*virtual*/ void USBHostKeyboardEx::setVidPid(uint16_t vid, uint16_t pid) {
  // we don't check VID/PID for keyboard driver
}

/*virtual*/ bool USBHostKeyboardEx::parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol)  //Must return true if the interface should be parsed
{
  //printf("intf_class: %d\n", intf_class);
  //printf("intf_subclass: %d\n", intf_subclass);
  //printf("intf_protocol: %d\n", intf_protocol);

  if ((keyboard_intf == -1) && (intf_class == HID_CLASS) && (intf_subclass == 0x01) && (intf_protocol == 0x01)) {
    // primary interface
    keyboard_intf = intf_nb;
    return true;
  }
  // See if we have a secondary HID interface that is not marked something special
  if ((keyboard_extras_intf == -1) && (intf_class == HID_CLASS) && (intf_subclass == 0x00) && (intf_protocol == 0x00)) {
    // primary interface
    keyboard_extras_intf = intf_nb;
    return true;
  }
  return false;
}

/*virtual*/ bool USBHostKeyboardEx::useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir)  //Must return true if the endpoint will be used
{
  printf("intf_nb: %d\n", intf_nb);
  if (intf_nb == keyboard_intf) {
    if (type == INTERRUPT_ENDPOINT && dir == IN) {
      keyboard_device_found = true;
      return true;
    }
  }

  if (intf_nb == keyboard_extras_intf) {
    if (type == INTERRUPT_ENDPOINT && dir == IN) {
      return true;
    }
  }

  return false;
}

#endif
