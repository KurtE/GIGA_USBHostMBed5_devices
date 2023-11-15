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

#ifndef __IUSBEnumeratorEx_H__
#define __IUSBEnumeratorEx_H__

#include <Arduino_USBHostMbed5.h>
#include "USBHost/USBHost.h"
#include "USBHost/USBHostConf.h"

/**
 * A class to communicate a USB hser
 */
class IUSBEnumeratorEx: public IUSBEnumerator  {
public:
  void initHelper();
  IUSBEnumeratorEx();

  uint16_t idVendor() { return (dev != nullptr) ? dev->getVid() : 0; }
  uint16_t idProduct() { return (dev != nullptr) ? dev->getPid() : 0; }
  bool manufacturer(uint8_t *buffer, size_t len);
  bool product(uint8_t *buffer, size_t len);
  bool serialNumber(uint8_t *buffer, size_t len);

  bool getStringDesc(uint8_t index, uint8_t *buffer, size_t len);


protected:
  USBHost* host;
  USBDeviceConnected* dev;

// String indexes 
  uint8_t iManufacturer_ = 0xff;
  uint8_t iProduct_ = 0xff;
  uint8_t iSerialNumber_ = 0xff;
  uint16_t wLanguageID_ = 0x409; // english US

  // should be part of higher level stuff...
  bool cacheStringIndexes();

};

#endif
