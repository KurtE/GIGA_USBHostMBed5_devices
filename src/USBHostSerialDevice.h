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

#ifndef USBHostSerialDevice_H
#define USBHostSerialDevice_H

#include <Arduino_USBHostMbed5.h>
#include "FasterSafeRingBuffer.h"
#include "USBHost/USBHost.h"

#include "USBHost/USBHostConf.h"

#define ENABLE_BUFFERED_WRITES

// USBSerial formats - Lets encode format into bits
// Bits: 0-4 - Number of data bits
// Bits: 5-7 - Parity (0=none, 1=odd, 2 = even)
// bits: 8-9 - Stop bits. 0=1, 1=2


#define USBHOST_SERIAL_7E1 0x047
#define USBHOST_SERIAL_7O1 0x027
#define USBHOST_SERIAL_8N1 0x08
#define USBHOST_SERIAL_8N2 0x108
#define USBHOST_SERIAL_8E1 0x048
#define USBHOST_SERIAL_8O1 0x028



/**
 * A class to communicate a USB hser
 */
class USBHostSerialDevice : public IUSBEnumerator, public Stream {
public:
  enum { DEFAULT_WRITE_TIMEOUT = 3500, MAX_DEVICES = 2};

  /**
    * Constructor
    */
  USBHostSerialDevice(bool buffer_writes=false);

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


  void begin(uint32_t baud, uint32_t format = USBHOST_SERIAL_8N1);
  bool setDTR(bool fSet);
  bool setRTS(bool fSet);

  uint16_t idVendor() { return (dev != nullptr) ? dev->getVid() : 0; }
  uint16_t idProduct() { return (dev != nullptr) ? dev->getPid() : 0; }
  bool manufacturer(uint8_t *buffer, size_t len);
  bool product(uint8_t *buffer, size_t len);
  bool serialNumber(uint8_t *buffer, size_t len);



  // from Stream
  using Print::write; 
  virtual int available(void);
  virtual int peek(void);
  virtual int read(void);
  virtual int availableForWrite();
  virtual size_t write(uint8_t c);
  virtual size_t write(const uint8_t *buffer, size_t size);
  virtual void flush(void);

  uint32_t writeTimeout() {return write_timeout_;}
  void writeTimeOut(uint32_t write_timeout) {write_timeout_ = write_timeout;} // Will not impact current ones.

protected:
  //From IUSBEnumerator
  virtual void setVidPid(uint16_t vid, uint16_t pid);
  virtual bool parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol);  //Must return true if the interface should be parsed
  virtual bool useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir);                           //Must return true if the endpoint will be used


  void initFTDI();
  void initPL2303(bool fConnect);
  void initCDCACM(bool fConnect);
  void initCH341();
  void initCP210X();


private:
  USBHost* host;
  USBDeviceConnected* dev;
  USBEndpoint* int_in;
  USBEndpoint* bulk_in;
  USBEndpoint* bulk_out;
  uint32_t size_bulk_in_;
  uint32_t size_bulk_out_;

  uint8_t setupdata[16];

  bool dev_connected;
  bool hser_device_found;
  uint8_t intf_SerialDevice;
  int ports_found;

  uint32_t baudrate_ = 115200;  // lets give it a default in case begin is not called
  uint32_t format_ = USBHOST_SERIAL_8N1;
  uint8_t dtr_rts_ = 3;

  // String indexes 
  uint8_t iManufacturer_ = 0xff;
  uint8_t iProduct_ = 0xff;
  uint8_t iSerialNumber_ = 0xff;
  uint16_t wLanguageID_ = 0x409; // english US


  void rxHandler();
  void txHandler();
  void (*onUpdate)(uint8_t x, uint8_t y, uint8_t z, uint8_t rz, uint16_t buttons);
  void init();

  // should be part of higher level stuff...
  bool getStringDesc(uint8_t index, uint8_t *buffer, size_t len);
  bool cacheStringIndexes();



  // The current know serial device types
  typedef enum { UNKNOWN = 0,
                 CDCACM,
                 FTDI,
                 PL2303,
                 CH341,
                 CP210X } sertype_t;

  typedef struct {
    uint16_t idVendor;
    uint16_t idProduct;
    sertype_t sertype;
    int claim_at_type;
  } product_vendor_mapping_t;
  static product_vendor_mapping_t pid_vid_mapping[];

  sertype_t sertype_ = UNKNOWN;

  // TODO: Maybe replace with a version that is safer...
  // RX variables
  SaferRingBufferN<128> rxBuffer_;
  rtos::Mutex rxMut_;
  uint8_t rxUSBBuf_[64];

  // TX variables
  SaferRingBufferN<128> txBuffer_;
  rtos::Mutex txMut_;
  uint8_t txUSBBuf_[64];

  bool buffer_writes_;
  mbed::Timeout writeTO_;

    typedef struct {
        uint8_t event_id;
        //USBHostSerialDevice *serObj;  
    } message_t;

  rtos::Thread  *writeTOThread_ = nullptr;
  rtos::Mail<USBHostSerialDevice::message_t, 10> mail_serobj_event;

  uint32_t write_timeout_ = DEFAULT_WRITE_TIMEOUT;
  volatile uint8_t in_tx_write_ = false;
  volatile uint8_t usb_tx_queued_ = false;
  volatile uint8_t in_tx_flush_ = false;

  // static USBHostSerialDevice *device_list[MAX_DEVICES];
  //typedef void (*timerCallback)() ;
  // static timerCallback timerCB_list[MAX_DEVICES];
  void processTXTimerCB();
  void tx_timeout_thread_proc();

  uint8_t timerCB_index_ = 0xff;
  inline void startWriteTimeout() { 
    writeTO_.attach(mbed::callback(this, &USBHostSerialDevice::processTXTimerCB), std::chrono::microseconds(3500)/*write_timeout_*/);}
  inline void stopWriteTimeout() { writeTO_.detach();}


//  static void txTimerCB0();
//  static void txTimerCB1();

  void submit_async_bulk_write(uint8_t where_called);
};

#endif
