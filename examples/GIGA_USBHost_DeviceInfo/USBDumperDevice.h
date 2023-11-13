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

#ifndef USBDumperDevice_H
#define USBDumperDevice_H

#include <Arduino_USBHostMbed5.h>
#include "USBHost/USBHost.h"

#include "USBHost/USBHostConf.h"


/**
 * A class to communicate a USB hser
 */
class USBDumperDevice : public IUSBEnumerator {
public:

  /**
    * Constructor
    */
  USBDumperDevice();

  /**
     * Try to connect a hser device
     *
     * @return true if connection was successful
     */
  bool connect();
  void disconnect();

  /**
    * Check if a hser is connected
    *
    * @returns true if a hser is connected
    */
  bool connected();

  uint16_t idVendor() {
    return (dev != nullptr) ? dev->getVid() : 0;
  }
  uint16_t idProduct() {
    return (dev != nullptr) ? dev->getPid() : 0;
  }
  bool manufacturer(uint8_t* buffer, size_t len);
  bool product(uint8_t* buffer, size_t len);
  bool serialNumber(uint8_t* buffer, size_t len);

protected:
  //From IUSBEnumerator
  virtual void setVidPid(uint16_t vid, uint16_t pid);
  virtual bool parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol);  //Must return true if the interface should be parsed
  virtual bool useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir);                           //Must return true if the endpoint will be used
  
  void device_disconnected();

private:
  USBHost* host;
  USBDeviceConnected* dev;
  USBDeviceConnected* dev_enum;

  uint8_t buf[64];
  uint8_t setupdata[16];

  volatile bool dev_connected;
  uint8_t intf_SerialDevice;

  void rxHandler();
  void txHandler();
  int report_id;
  void init();
  // String indexes
  uint8_t iManufacturer_ = 0xff;
  uint8_t iProduct_ = 0xff;
  uint8_t iSerialNumber_ = 0xff;
  uint16_t wLanguageID_ = 0x409;  // english US
  // should be part of higher level stuff...
  bool getStringDesc(uint8_t index, uint8_t* buffer, size_t len);
  bool cacheStringIndexes();
  bool getHIDDesc(uint8_t index, uint8_t* buffer, size_t len);

  void read_and_print_configuration();
  uint16_t getConfigurationDescriptor(uint8_t* buf, uint16_t max_len_buf);

  void dumpHIDReportDescriptor(uint8_t iInterface, uint16_t descsize);
  void printUsageInfo(uint8_t usage_page, uint16_t usage);
  void print_input_output_feature_bits(uint8_t val);
  enum { MAX_FEATURE_REPORTS = 20 };
  uint8_t feature_report_ids_[MAX_FEATURE_REPORTS];
  uint8_t cnt_feature_reports_ = 0;
};

#endif
