
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

#include "USBHostDeviceHelper.h"
#include <LibPrintf.h>
#include <MemoryHexDump.h>
#include <GIGA_digitalWriteFast.h>


USBHostDeviceHelper::USBHostDeviceHelper() {
  host = USBHost::getHostInst();
  initHelper();
}


void USBHostDeviceHelper::initHelper() {
  iManufacturer_ = 0xff;
}

bool USBHostDeviceHelper::manufacturer(uint8_t *buffer, size_t len) {
  cacheStringIndexes();
  return getStringDesc(iManufacturer_, buffer, len);
}

bool USBHostDeviceHelper::product(uint8_t *buffer, size_t len){
  cacheStringIndexes();
  return getStringDesc(iProduct_, buffer, len);
}

bool USBHostDeviceHelper::serialNumber(uint8_t *buffer, size_t len){
  cacheStringIndexes();
  return getStringDesc(iSerialNumber_, buffer, len);
}


#define STRING_DESCRIPTOR  (3)

bool USBHostDeviceHelper::cacheStringIndexes() {
  if (iManufacturer_ != 0xff) return true; // already done

  //printf(">>>>> USBHostDeviceHelper::cacheStringIndexes() called <<<<< \n\r");
  DeviceDescriptor device_descriptor;

  USB_TYPE res = host->controlRead(  dev,
                         USB_DEVICE_TO_HOST | USB_RECIPIENT_DEVICE,
                         GET_DESCRIPTOR,
                         (DEVICE_DESCRIPTOR << 8) | (0),
                         0, (uint8_t*)&device_descriptor, DEVICE_DESCRIPTOR_LENGTH );

  if (res != USB_TYPE_OK) {
    //printf("\t Read device descriptor failed: %u\n\r",  res);
    return false;
  }

  iManufacturer_ = device_descriptor.iManufacturer;
  iProduct_ = device_descriptor.iProduct;
  iSerialNumber_ = device_descriptor.iSerialNumber;
  //printf("\tiMan:%u iProd:%u iSer:%u\n\r", iManufacturer_, iProduct_, iSerialNumber_);

  // Now lets try to get the default language ID:
  uint8_t read_buffer[64]; 
  //printf(">>>>> Get Language ID <<<<<<\n\r");
  res = host->controlRead(  dev,
                      USB_DEVICE_TO_HOST | USB_RECIPIENT_DEVICE,
                      GET_DESCRIPTOR,
                      0x300,
                      0, read_buffer, sizeof(read_buffer));
  if (res != USB_TYPE_OK) {
    //printf("\tFailed default to  0x0409 English");
    wLanguageID_ = 0x409;
  } else {
    //MemoryHexDump(Serial, read_buffer, sizeof(read_buffer), true);
    wLanguageID_ = read_buffer[2] | (read_buffer[3] << 8);
    //printf("\tLanguage ID: %x\n\r", wLanguageID_);
  }

  return true;
}


bool USBHostDeviceHelper::getStringDesc(uint8_t index, uint8_t *buffer, size_t len) {

  //printf(">>>>> USBHostDeviceHelper::getStringDesc(%u) called <<<<< \n\r", index);
  if ((index == 0xff) || (index == 0)) return false;


  // Lets reserve space on stack to read in the string, note it is Unicode. so twice the len +
  uint8_t read_len = len * 2 + 2;
  uint8_t read_buffer[read_len]; // will probably give compiler warning about variable length...

   USB_TYPE res = host->controlRead(  dev,
                      USB_DEVICE_TO_HOST | USB_RECIPIENT_DEVICE,
                      GET_DESCRIPTOR,
                      (STRING_DESCRIPTOR << 8) | (index),
                      wLanguageID_, read_buffer, read_len);

  if (res != USB_TYPE_OK) {
    //printf("\t Read string descriptor failed: %u\n\r",  res);
    return false;
  }
  //MemoryHexDump(Serial, read_buffer, read_len, true);

  if (read_buffer[1] != 0x03) return false;

  if (read_buffer[0] > (read_len)) read_buffer[0] = read_len;

  for (uint8_t i = 2; i < read_buffer[0]; i += 2) {
    *buffer++ = read_buffer[i];
  }
  *buffer = '\0';
  return true;

}
