#ifndef __USBHOSTHIDPARSER_H__
#define __USBHOSTHIDPARSER_H__
#include <Arduino.h>
#include <Arduino_USBHostMbed5.h>
#include "USBHost/USBHost.h"

class USBHostHIDParserCB {
public:
  virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) {};
  virtual void hid_input_data(uint32_t usage, int32_t value);
  virtual void hid_input_end() {};
};


class USBHostHIDParser {
public:
  bool init(USBHost *host, USBDeviceConnected *dev, uint8_t index, uint16_t len);
  void parse(const uint8_t *data, uint16_t len);

  inline void attach(USBHostHIDParserCB *hidCB) {
    hidCB_ = hidCB;
  }


  bool getHIDDescriptor();

private:
  USBHost *host_;
  USBDeviceConnected *dev_ = nullptr;
  uint8_t index_ = 0xff;
  uint8_t *descriptor_buffer_ = nullptr;
  ;
  uint16_t descriptor_length_ = 0;
  enum { USAGE_LIST_LEN = 24 };

  USBHostHIDParserCB *hidCB_ = nullptr;  // does this Descriptor have report IDS?
  bool use_report_id_ = false;
};
#endif