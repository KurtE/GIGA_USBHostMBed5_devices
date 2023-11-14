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

/**
 * A class to communicate a USB keyboard
 */
class USBHostKeyboardEx : public IUSBEnumerator, public USBHostHIDParserCB {
public:

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

protected:
  //From IUSBEnumerator
  virtual void setVidPid(uint16_t vid, uint16_t pid);
  virtual bool parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol);  //Must return true if the interface should be parsed
  virtual bool useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir);                           //Must return true if the endpoint will be used

  // From USBHostHIDParser
  virtual void hid_input_data(uint32_t usage, int32_t value);
  virtual void hid_input_end();


private:
  USBHost* host;
  USBDeviceConnected* dev;
  USBEndpoint* int_in;
  USBEndpoint* int_extras_in;
  uint8_t report[9];
  uint8_t prev_report[9];
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

  int report_id;

  void init();

  USBHostHIDParser hidParser;
};

#endif
