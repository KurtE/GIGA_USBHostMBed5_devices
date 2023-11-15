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

#ifndef USBHostMouseEx_H
#define USBHostMouseEx_H

#include "USBHost/USBHostConf.h"
#include "USBHostHIDParser.h"

#include "USBHost/USBHost.h"
#include "IUSBEnumeratorEx.h"
/**
 * A class to communicate a USB keyboard
 */
class USBHostMouseEx : public IUSBEnumeratorEx, public USBHostHIDParserCB {
public:

  // The host helper adds methods, to retrieve the VID, PID and device strings.


  /**
    * Constructor
    */
  USBHostMouseEx();

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

  bool available() {
    return mouseEvent;
  }
  void mouseDataClear();
  uint8_t getButtons() {
    return buttons;
  }
  int getMouseX() {
    return mouseX;
  }
  int getMouseY() {
    return mouseY;
  }
  int getWheel() {
    return wheel;
  }
  int getWheelH() {
    return wheelH;
  }



protected:
  //From IUSBEnumerator
  virtual void setVidPid(uint16_t vid, uint16_t pid);
  virtual bool parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol);  //Must return true if the interface should be parsed
  virtual bool useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir);                           //Must return true if the endpoint will be used

  // From USBHostHIDParser
  virtual void hid_input_data(uint32_t usage, int32_t value);
  virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
  virtual void hid_input_end();

private:
  // first two are in the host helper
  //USBHost* host;
  //USBDeviceConnected* dev;
  USBEndpoint* int_in;

  uint8_t buf_in_[64];
  uint32_t size_in_;
  uint16_t hid_descriptor_size_;

  int mouse_intf;
  bool mouse_device_found;

  bool dev_connected;

  void rxHandler();
  void rxExtrasHandler();
  uint8_t mapKeycodeToKey(uint8_t modifier, uint8_t keycode);

  void process_hid_data(uint32_t usage, uint32_t value);
  void init();

  volatile bool mouseEvent = false;
  volatile bool hid_input_begin_ = false;
  uint8_t buttons = 0;
  int mouseX = 0;
  int mouseY = 0;
  int wheel = 0;
  int wheelH = 0;

  USBHostHIDParser hidParser;
};

#endif
