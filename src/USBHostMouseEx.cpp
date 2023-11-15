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

#include "USBHostMouseEx.h"
#include <MemoryHexDump.h>


USBHostMouseEx::USBHostMouseEx() {
  init();
}


void USBHostMouseEx::init() {
  initHelper(); // call the helper method...
  dev = NULL;
  int_in = NULL;
  dev_connected = false;
  mouse_intf = -1;
  mouse_device_found = false;
}

bool USBHostMouseEx::connected() {
  return dev_connected;
}


bool USBHostMouseEx::connect() {

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


      if (mouse_device_found) {
        {
          /* As this is done in a specific thread
                     * this lock is taken to avoid to process the device
                     * disconnect in usb process during the device registering */
          USBHost::Lock Lock(host);

          int_in = dev->getEndpoint(mouse_intf, INTERRUPT_ENDPOINT, IN);

          if (!int_in) {
            break;
          }

          USB_INFO("New Mouse device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, mouse_intf);
          dev->setName("Mouse", mouse_intf);
          host->registerDriver(dev, mouse_intf, this, &USBHostMouseEx::init);

          int_in->attach(this, &USBHostMouseEx::rxHandler);
          size_in_ = int_in->getSize();

          hidParser.init(host, dev, mouse_intf, hid_descriptor_size_);
          hidParser.attach(this);
        }
        host->interruptRead(dev, int_in, buf_in_, size_in_, false);

        dev_connected = true;
        return true;
      }
    }
  }
  init();
  return false;
}

void USBHostMouseEx::rxHandler() {
  int len = int_in->getLengthTransferred();

  if (len) {
    //Serial.println("$$$ Extras HID RX $$$");
    //MemoryHexDump(Serial, buf_in_, len, true, nullptr, -1, 0);
    hidParser.parse(buf_in_, len);
  }

  host->interruptRead(dev, int_in, buf_in_, size_in_, false);
}



/*virtual*/ void USBHostMouseEx::setVidPid(uint16_t vid, uint16_t pid) {
  // we don't check VID/PID for keyboard driver
}

/*virtual*/ bool USBHostMouseEx::parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol)  //Must return true if the interface should be parsed
{
  //printf("intf_class: %d\n", intf_class);
  //printf("intf_subclass: %d\n", intf_subclass);
  //printf("intf_protocol: %d\n", intf_protocol);
  // Is this a HID BOOT Mouse.
  if ((mouse_intf == -1) && (intf_class == HID_CLASS) && (intf_subclass == 0x01) && (intf_protocol == 0x02)) {
    // primary interface
    mouse_intf = intf_nb;
    return true;
  }
  return false;
}

/*virtual*/ bool USBHostMouseEx::useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir)  //Must return true if the endpoint will be used
{
  //printf("intf_nb: %d\n", intf_nb);
  //printf(" ??? HID Report size: %u\n", host->getLengthReportDescr());
  if (intf_nb == mouse_intf) {
    if (type == INTERRUPT_ENDPOINT && dir == IN) {
      mouse_device_found = true;
      hid_descriptor_size_ = host->getLengthReportDescr();
      return true;
    }
  }
  return false;
}

/*virtual*/ void USBHostMouseEx::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) {
  //printf("Mouse HID Begin(%lx %lx %d %d)\n", topusage, type, lgmin, lgmax);
  hid_input_begin_ = true;
}

/*virtual*/ void USBHostMouseEx::hid_input_end() {
  //printf("Mouse HID end()\n");
  if (hid_input_begin_) {
    mouseEvent = true;
    hid_input_begin_ = false;
  }
}

/*virtual*/ void USBHostMouseEx::hid_input_data(uint32_t usage, int32_t value) {
  //printf("Mouse: usage=%lX, value=%ld\n", usage, value);
  uint32_t usage_page = usage >> 16;
  usage &= 0xFFFF;
  if (usage_page == 9 && usage >= 1 && usage <= 8) {
    if (value == 0) {
      buttons &= ~(1 << (usage - 1));
    } else {
      buttons |= (1 << (usage - 1));
    }
  } else if (usage_page == 1) {
    switch (usage) {
      case 0x30:
        mouseX = value;
        break;
      case 0x31:
        mouseY = value;
        break;
      case 0x32:  // Apple uses this for horizontal scroll
        wheelH = value;
        break;
      case 0x38:
        wheel = value;
        break;
    }
  } else if (usage_page == 12) {
    if (usage == 0x238) {  // Microsoft uses this for horizontal scroll
      wheelH = value;
    }
  }
}

void USBHostMouseEx::mouseDataClear() {
  mouseEvent = false;
  buttons = 0;
  mouseX = 0;
  mouseY = 0;
  wheel = 0;
  wheelH = 0;
}
