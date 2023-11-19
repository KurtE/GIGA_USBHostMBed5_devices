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

#ifndef USBHostKeyboardEx_H
#define USBHostKeyboardEx_H

#include "USBHost/USBHostConf.h"
#include "USBHostHIDParser.h"

#include "USBHost/USBHost.h"
#include "IUSBEnumeratorEx.h"

/**
 * A class to communicate a USB keyboard
 */
class USBHostKeyboardEx : public IUSBEnumeratorEx, public USBHostHIDParserCB {
public:

  // The host helper adds methods, to retrieve the VID, PID and device strings.

  /**
    * Constructor
    */
  USBHostKeyboardEx();

  /**
     * Try to connect a keyboard device
     *
     * @return true if connection was successful
     */
  bool connect();

  /**
    * Check if a keyboard is connected
    *
    * @returns true if a keyboard is connected
    */
  bool connected();


  /**
    * Some keyboards require us to force them into boot mode
    *
    *  @param set - true to force connections into boot mode
    */
  void     forceBootProtocol(bool set=true) {force_boot_mode_ = set;}


  /**
     * Attach a callback called when a Key is pressed.  Only those keys
     * who convert to ASCII are returned, and the translation uses
     * the current state of modifiers and caps lock and num lock
     * 
     *
     * @param ptr function pointer
     */
  inline void attachPress(void (*ptr)(uint8_t key)) {
    onKey = ptr;
  }

  /**
     * Attach a callback called when a key is pressed, that
     * is translated to ASCII.
     *
     * @param ptr function pointer
     */
  inline void attachRelease(void (*ptr)(uint8_t key)) {
    onKeyRelease = ptr;
  }


  /**
     * Attach a callback called when a keyboard event is received
     * This function is passed the raw keycodes that are returned
     * from the keyboard.
     *
     * @param ptr function pointer
     */
  inline void attachRawPress(void (*ptr)(uint8_t keyCode, uint8_t modifier)) {
    onKeyCode = ptr;
  }

  /**
     * Attach a callback called when a key is released
     * This function is passed the raw keycodes 
     *
     * @param ptr function pointer
     */
  inline void attachRawRelease(void (*ptr)(uint8_t keyCode)) {
    onKeyCodeRelease = ptr;
  }

  /**
     * Attach a callback called when a key is pressed
     * from other keys that are typically multimedia or system keys
     * You are passed two values, the HID page the key is on
     * and the index within that page.
     *
     * @param ptr function pointer
     */
  inline void attachHIDPress(void (*ptr)(uint32_t top, uint16_t code)) {
    onExtrasPress = ptr;
  }

  /**
     * Attach a callback called when a key is released
     * from other keys that are typically multimedia or system keys
     * You are passed two values, the HID page the key is on
     * and the index within that page.
     *
     * @param ptr function pointer
     */
  inline void attachHIDRelease(void (*ptr)(uint32_t top, uint16_t code)) {
    onExtrasRelease = ptr;
  }



  typedef union {
    struct {
      uint8_t numLock : 1;
      uint8_t capsLock : 1;
      uint8_t scrollLock : 1;
      uint8_t compose : 1;
      uint8_t kana : 1;
      uint8_t reserved : 3;
    };
    uint8_t byte;
  } __attribute__((packed)) KBDLeds_t;

  KBDLeds_t leds_ = { 0 };
  void LEDS(uint8_t leds);
  uint8_t LEDS() {
    return leds_.byte;
  }
  void updateLEDS(void);
  bool numLock() {
    return leds_.numLock;
  }
  bool capsLock() {
    return leds_.capsLock;
  }
  bool scrollLock() {
    return leds_.scrollLock;
  }
  void numLock(bool f);
  void capsLock(bool f);
  void scrollLock(bool f);

  uint8_t  getModifiers() { return modifiers_; }
  uint8_t  getOemKey() { return keyOEM_; }

  // Keyboard special Keys
  enum {KEYD_UP = 0xDA, KEYD_DOWN = 0xD9, KEYD_LEFT = 0xD8, KEYD_RIGHT = 0xD7, KEYD_INSERT = 0xD1, KEYD_DELETE = 0xD4,
        KEYD_PAGE_UP = 0xD3, KEYD_PAGE_DOWN = 0xD6, KEYD_HOME = 0xD2, KEYD_END = 0xD5, KEYD_F1 = 0xC2, KEYD_F2 = 0xC3,
        KEYD_F3 = 0xC4, KEYD_F4 = 0xC5, KEYD_F5 = 0xC6, KEYD_F6 = 0xC7, KEYD_F7 = 0xC8, KEYD_F8 = 0xC9, KEYD_F9 = 0xCA, 
        KEYD_F10 = 0xCB, KEYD_F11 = 0xCC, KEYD_F12 = 0xCD };


protected:
  //From IUSBEnumerator
  virtual void setVidPid(uint16_t vid, uint16_t pid);
  virtual bool parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol);  //Must return true if the interface should be parsed
  virtual bool useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir);                           //Must return true if the endpoint will be used

  // From USBHostHIDParser
  virtual void hid_input_data(uint32_t usage, int32_t value);
  virtual void hid_input_end();



private:
  // first two are in the host helper
  //USBHost* host;
  //USBDeviceConnected* dev;
  USBEndpoint* int_in;
  USBEndpoint* int_extras_in;
  uint8_t report[9];
  uint8_t prev_report[9];
  uint8_t modifiers_ = 0;
  uint8_t keyOEM_;

  uint8_t buf_extras[64];
  uint32_t size_extras_in_;
  uint16_t hid_extras_descriptor_size_;

  int keyboard_intf;
  int keyboard_extras_intf;
  bool keyboard_device_found;

  bool dev_connected;

  void rxHandler();
  void rxExtrasHandler();
  uint8_t mapKeycodeToKey(uint8_t modifier, uint8_t keycode);

  void process_hid_data(uint32_t usage, uint32_t value);

  void (*onKey)(uint8_t key) = nullptr;
  void (*onKeyRelease)(uint8_t key) = nullptr;
  void (*onKeyCode)(uint8_t key, uint8_t modifier) = nullptr;
  void (*onKeyCodeRelease)(uint8_t key) = nullptr;
  void (*onExtrasPress)(uint32_t top, uint16_t code) = nullptr;
  void (*onExtrasRelease)(uint32_t top, uint16_t code) = nullptr;
  enum {MAX_KEYS_DOWN = 4};
  uint32_t keys_down_[MAX_KEYS_DOWN];
  uint8_t count_keys_down_ = 0;
  bool force_boot_mode_ = false;
  uint16_t idVendor_;
  uint16_t idProduct_;

  int report_id;

  void init();

  USBHostHIDParser hidParser;
};

#endif
