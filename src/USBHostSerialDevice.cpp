#include <MemoryHexDump.h>
#include <GIGA_digitalWriteFast.h>

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

#include "USBHostSerialDevice.h"
#include <LibPrintf.h>

enum {LATENCY_TIMEOUT_MSG = 1};

/************************************************************/
//  Define mapping VID/PID - to Serial Device type.
/************************************************************/
USBHostSerialDevice::product_vendor_mapping_t USBHostSerialDevice::pid_vid_mapping[] = {
  // FTDI mappings.
  { 0x0403, 0x6001, USBHostSerialDevice::FTDI, 0 },
  { 0x0403, 0x8088, USBHostSerialDevice::FTDI, 1 },  // 2 devices try to claim at interface level
  { 0x0403, 0x6010, USBHostSerialDevice::FTDI, 1 },  // Also Dual Serial, so claim at interface level

  // PL2303
  { 0x67B, 0x2303, USBHostSerialDevice::PL2303, 0 },

  // CH341
  { 0x4348, 0x5523, USBHostSerialDevice::CH341, 0 },
  { 0x1a86, 0x7523, USBHostSerialDevice::CH341, 0 },
  { 0x1a86, 0x5523, USBHostSerialDevice::CH341, 0 },

  // Silex CP210...
  { 0x10c4, 0xea60, USBHostSerialDevice::CP210X, 0 },
  { 0x10c4, 0xea70, USBHostSerialDevice::CP210X, 0 }
};

#if 0
USBHostSerialDevice *USBHostSerialDevice::device_list[MAX_DEVICES] = {nullptr, nullptr};

USBHostSerialDevice::timerCallback USBHostSerialDevice::timerCB_list[MAX_DEVICES] = {&USBHostSerialDevice::txTimerCB0,
    &USBHostSerialDevice::txTimerCB1};
#endif

USBHostSerialDevice::USBHostSerialDevice(bool buffer_writes) : buffer_writes_(buffer_writes) {
  host = USBHost::getHostInst();
  init();
}

void USBHostSerialDevice::init() {
  dev = NULL;
  bulk_in = NULL;
  bulk_out = NULL;
  int_in = NULL;
  onUpdate = NULL;
  dev_connected = false;
  hser_device_found = false;
  intf_SerialDevice = -1;
  ports_found = 0;
  iManufacturer_ = 0xff; // make sure we get them again...
  iProduct_ = 0xff; // make sure we get them again...
  iSerialNumber_ = 0xff; // make sure we get them again...
}

bool USBHostSerialDevice::connected() {
  return dev_connected;
}

bool USBHostSerialDevice::connect() {
  USB_TYPE res;
  USB_INFO(" USBHostSerialDevice::connect() called\r\n");
  if (dev) {
    for (uint8_t i = 0; i < MAX_DEVICE_CONNECTED; i++) {
      USBDeviceConnected* d = host->getDevice(i);
      if (dev == d)
        return true;
    }
    disconnect();
  }
  host = USBHost::getHostInst();
  for (uint8_t i = 0; i < MAX_DEVICE_CONNECTED; i++) {
    USBDeviceConnected* d = host->getDevice(i);
    USB_DBG("\tDev: %p\r\n", d);
    if (d != NULL) {
      USB_INFO("Device:%p\r\n", d);
      if ((res = host->enumerate(d, this)) != USB_TYPE_OK) {
        USB_INFO("Enumerate returned status: %u", res);
        continue;  //break;  what if multiple devices?
      }

      printf("\tconnect hser_device_found\n\r");

      bulk_in = d->getEndpoint(intf_SerialDevice, BULK_ENDPOINT, IN);
      USB_INFO("bulk in:%p", bulk_in);

      bulk_out = d->getEndpoint(intf_SerialDevice, BULK_ENDPOINT, OUT);
      USB_INFO(" out:%p\r\n", bulk_out);

      //printf("\tAfter get end points\n\r");
      if (bulk_in && bulk_out) {
        dev = d;
        dev_connected = true;
        //        USB_INFO("New hser device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, intf_SerialDevice);
        //printf("New hser device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, intf_SerialDevice);
        dev->setName("Serial", intf_SerialDevice);
        host->registerDriver(dev, intf_SerialDevice, this, &USBHostSerialDevice::init);
        size_bulk_in_ = bulk_in->getSize();
        size_bulk_out_ = bulk_out->getSize();

        bulk_in->attach(this, &USBHostSerialDevice::rxHandler);
        bulk_out->attach(this, &USBHostSerialDevice::txHandler);
        host->bulkRead(dev, bulk_in, rxUSBBuf_, size_bulk_in_, false);
        //printf("\n\r>>>>>>>>>>>>>> connected returning true <<<<<<<<<<<<<<<<<<<<\n\r");

        // Each serial type might have their own init sequence required.
        baudrate_ = 115200;
        format_ = USBHOST_SERIAL_8N1;
        switch (sertype_) {
          default: break;  // don't do anything for the rest of them
          case CDCACM: initCDCACM(true); break;
          case FTDI: initFTDI(); break;
          case PL2303: initPL2303(true); break;
          case CH341: initCH341(true); break;
          case CP210X: initCP210X(); break;
        }


        return true;
      }
    }
  }
  init();
  return false;
}

void USBHostSerialDevice::disconnect() {
  USB_INFO(" USBHostSerialDevice::disconnect() called\r\n");
  init();  // clear everything
}

void USBHostSerialDevice::rxHandler() {
  if (bulk_in) {
    int len = bulk_in->getLengthTransferred();
    //printf("USBHostSerialDevice::rxHandler() called len:%d\n\r", len);
    //MemoryHexDump(Serial, rxUSBBuf_, len, true);
    uint8_t *p = rxUSBBuf_; // pointer from input buffer 
    if (sertype_ == FTDI) {
      // We ignore first two bytes on FTDI
      if (len > 2) {
        p += 2;
        len -= 2;
      } else {
        len = 0;
      }
    }
    if (len > 0) {
      rxMut_.lock();
      for (int i = 0; i < len; i++) {
        rxBuffer_.store_char(*p++);
      }
      rxMut_.unlock();
    }

    // Setup the next read.
    host->bulkRead(dev, bulk_in, rxUSBBuf_, size_bulk_in_, false);
  }
}


/*virtual*/ void USBHostSerialDevice::setVidPid(uint16_t vid, uint16_t pid) {
  // we don't check VID/PID for hser driver
  USB_INFO("VID: %X, PID: %X\n\r", vid, pid);
  printf("VID: %X, PID: %X", vid, pid);
  sertype_ = UNKNOWN;
  for (uint16_t i = 0; i < (sizeof(pid_vid_mapping) / sizeof(pid_vid_mapping[0])); i++) {
    if ((pid_vid_mapping[i].idVendor == vid) && (pid_vid_mapping[i].idProduct == pid)) {
      sertype_ = pid_vid_mapping[i].sertype;
      break;
    }
  }
  switch (sertype_) {
    default: printf(" Unknown\n\r"); break;
    case FTDI: printf(" FTDI\n\r"); break;
    case PL2303: printf(" PL2303\n\r"); break;
    case CH341: printf(" CH341\n\r"); break;
    case CP210X: printf(" Silex CP210X\n\r"); break;
  }
}



/*virtual*/ bool USBHostSerialDevice::parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol)  //Must return true if the interface should be parsed
{
  // PL2303 nb:0, cl:255 isub:0 iprot:0
  USB_INFO("USBHostSerialDevice::parseInterface nb:%d, cl:%u isub:%u iprot:%u\n\r", intf_nb, intf_class, intf_subclass, intf_protocol);
  //printf("USBHostSerialDevice::parseInterface nb:%d, cl:%u isub:%u iprot:%u\n\r", intf_nb, intf_class, intf_subclass, intf_protocol);
  if (sertype_ != UNKNOWN) {
    intf_SerialDevice = intf_nb;
    return true;
  } else {
    if ((intf_class == 0x0a) && (intf_subclass == 0)) {
      //printf("CDC ACM interface\n\r");
      intf_SerialDevice = intf_nb;
      return true;
    }
  }
  return false;
}

/*virtual*/ bool USBHostSerialDevice::useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir)  //Must return true if the endpoint will be used
{
  USB_INFO("USBHostSerialDevice::useEndpoint(%u, %u, %u)\n\r", intf_nb, type, dir);
  //printf("USBHostSerialDevice::useEndpoint(%u, %u, %u)\n\r", intf_nb, type, dir);
  if (intf_nb == intf_SerialDevice) {
    //if (type == INTERRUPT_ENDPOINT && dir == IN) return true; // see if we can ignore it later

    if (type == BULK_ENDPOINT && dir == IN) {
      hser_device_found = true;
      return true;
    }
    if (type == BULK_ENDPOINT && dir == OUT) {
      hser_device_found = true;
      return true;
    }
  }
  return false;
}


void USBHostSerialDevice::initCDCACM(bool fConnect) {
  (void)fConnect;
  //printf("Control - CDCACM LINE_CODING\n\r");
  setupdata[0] = (baudrate_) & 0xff;  // Setup baud rate 115200 - 0x1C200
  setupdata[1] = (baudrate_ >> 8) & 0xff;
  setupdata[2] = (baudrate_ >> 16) & 0xff;
  setupdata[3] = (baudrate_ >> 24) & 0xff;
  setupdata[4] = (format_ & 0x100)? 2 : 0;  // 0 - 1 stop bit, 1 - 1.5 stop bits, 2 - 2 stop bits
  setupdata[5] = (format_ & 0xe0) >> 5;     // 0 - None, 1 - Odd, 2 - Even, 3 - Mark, 4 - Space
  setupdata[6] = format_ & 0x1f;        // Data bits (5, 6, 7, 8 or 16)
  host->controlWrite(dev, 0x21, 0x20, 0, 0, setupdata, 7);

  // pending & 4
  println("Control - 0x21,0x22, 0x3");
  // Need to setup  the data the line coding data
  host->controlWrite(dev, 0x21, 0x22, 3, 0, nullptr, 0);
  dtr_rts_ = 3;
}


void USBHostSerialDevice::initFTDI() {
  // in connect
  host->controlWrite(dev, 0x40, 0, 0, 0, nullptr, 0); // reset port

  // & 1  Format
  uint16_t ftdi_format = format_ & 0xf; // This should give us the number of bits.
  // now lets extract the parity from our encoding
  ftdi_format |= (format_ & 0xe0) << 3; // they encode bits 9-11
  // See if two stop bits
  if (format_ & 0x100) ftdi_format |= (0x2 << 11);
  host->controlWrite(dev, 0x40, 4, ftdi_format, 0, nullptr, 0); // data format 8N1

  // set baud rate & 2
  uint32_t baudval = 3000000 / baudrate_;
  host->controlWrite(dev, 0x40, 3, baudval, 0, nullptr, 0);

  // configure flow control
  host->controlWrite(dev, 0x40, 2, 0, 1, nullptr, 0);

  // set DTR
  host->controlWrite(dev, 0x40, 1, 0x0101, 0, nullptr, 0);
  dtr_rts_ = 1;
}

void USBHostSerialDevice::initPL2303(bool fConnect) {
  // first part done first time:
  if (fConnect) {
    //printf("Init PL2303 - strange stuff\n\r");
    host->controlRead(dev, 0xc0, 1, 0x8484, 0, setupdata, 1);  //claim
    host->controlWrite(dev, 0x40, 1, 0x0404, 0, nullptr, 0); // setup state = 1
    host->controlRead(dev, 0xc0, 1, 0x8484, 0, setupdata, 1); // 2
    host->controlRead(dev, 0xc0, 1, 0x8383, 0, setupdata, 1); // 3
    //uint8_t pl2303_v1 = setupdata[0]; // save the first bye of version

    host->controlRead(dev, 0xc0, 1, 0x8484, 0, setupdata, 1); // 4
    host->controlWrite(dev, 0x40, 1, 0x0404, 1, nullptr, 0); // 5
    host->controlRead(dev, 0xc0, 1, 0x8484, 0, setupdata, 1); // 6
    host->controlRead(dev, 0xc0, 1, 0x8383, 0, setupdata, 1); // 7
    //uint8_t pl2303_v2 = setupdata[0]; // save the first bye of version
    //("PL2303 Version %x : %x\n\r", pl2303_v1, pl2303_v2);

    host->controlWrite(dev, 0x40, 1, 0, 1, nullptr, 0);  // 8
    host->controlWrite(dev, 0x40, 1, 1, 0, nullptr, 0);  // 9
    host->controlWrite(dev, 0x40, 1, 2, 0x24, nullptr, 0); // 10

    // USB host shield 2... does not output
//    host->controlWrite(dev, 0x40, 1, 8, 0, nullptr, 0); // 11
//    host->controlWrite(dev, 0x40, 1, 9, 0, nullptr, 0); // 12
//    host->controlRead(dev, 0xA1, 0x21, 0, 0, setupdata, 7); // 13
//  }
  // Now stuff common to connect and begin

  MemoryHexDump(Serial, setupdata, 7, false, "baud/control before\n");
  // pending control bit &2
  setupdata[0] = (baudrate_) & 0xff;  // Setup baud rate 115200 - 0x1C200
  setupdata[1] = (baudrate_ >> 8) & 0xff;
  setupdata[2] = (baudrate_ >> 16) & 0xff;
  setupdata[3] = (baudrate_ >> 24) & 0xff;
  setupdata[4] = (format_ & 0x100) ? 2 : 0;  // 0 - 1 stop bit, 1 - 1.5 stop bits, 2 - 2 stop bits
  setupdata[5] = (format_ & 0xe0) >> 5;      // 0 - None, 1 - Odd, 2 - Even, 3 - Mark, 4 - Space
  setupdata[6] = format_ & 0x1f;             // Data bits (5, 6, 7, 8 or 16)
  MemoryHexDump(Serial, setupdata, 7, false, "baud/control after\n");
  host->controlWrite(dev, 0x21, 0x20, 0, 0, setupdata, 7);

  // pending control 0x4
  host->controlWrite(dev, 0x40, 1, 0, 0, nullptr, 0);

  // pending control 0x8
  memset(setupdata, 0, sizeof(setupdata));  // clear it to see if we read it...
  host->controlRead(dev, 0xA1, 0x21, 0, 0, setupdata, 7);
  MemoryHexDump(Serial, setupdata, 7, false, "baud/control read back\n");

  // pending control 0x10
  // This sets the control lines (0x1=DTR, 0x2=RTS)
  //printf("PL2303: 0x21, 0x22, 0x3\n\r");
  dtr_rts_ = 1;
  //host->controlWrite(dev, 0x21, 0x22, 1, 0, nullptr, 0);
  setDTR(1);

  // Only on connect?
//  if (fConnect) {
//    printf("PL2303: 0x21, 0x22, 0x3 again\n\r");
//    host->controlWrite(dev, 0x21, 0x22, 1, 0, nullptr, 0);
  }

}


#define CH341_BAUDBASE_FACTOR 1532620800
#define CH341_BAUDBASE_DIVMAX 3
void USBHostSerialDevice::ch341_setBaud() {
  uint32_t factor;
  uint16_t divisor;

  factor = (CH341_BAUDBASE_FACTOR / baudrate_);
  divisor = CH341_BAUDBASE_DIVMAX;

  while ((factor > 0xfff0) && divisor) {
    factor >>= 3;
    divisor--;
  }

  factor = 0x10000 - factor;

  factor = (factor & 0xff00) | divisor;

  uint8_t factor2 = factor & 0xff;  // save away the low byte for 2nd message
  
  //printf("CH341: 40, 0x9a, 0x1312... (Baud word 0):%lx\n\r", factor);

  host->controlWrite(dev, 0x40, 0x9a, 0x1312, factor, setupdata, 0); // 
  
  // output the 2nd byte;
  //printf("CH341: 40, 0x9a, 0x0f2c... (Baud word 1):%x\n\r", factor2);
  host->controlWrite(dev, 0x40, 0x9a, 0x0f2c, factor2, setupdata, 0); // 
}



void USBHostSerialDevice::initCH341(bool fConnect) {
  printf("initCH341(%u)\n\r", fConnect);

  // Need to setup  the data the line coding data
  if (fConnect) {
    // & 1...
    host->controlRead(dev, 0xC0, 0x5f, 0, 0, setupdata, sizeof(setupdata)); 
    //MemoryHexDump(Serial, setupdata, sizeof(setupdata), true); 
    host->controlWrite(dev, 0x40, 0xa1, 0, 0, nullptr, 0); // 
    ch341_setBaud(); // send the baud bytes
    host->controlRead(dev, 0xc0, 0x95, 0x2518, 0, setupdata, sizeof(setupdata)); // 
    //MemoryHexDump(Serial, setupdata, sizeof(setupdata), true); 

    host->controlWrite(dev, 0x40, 0x9a, 0x2518, 0x0050, nullptr, 0); // 
    host->controlRead(dev, 0xc0, 0x95, 0x706, 0, setupdata, sizeof(setupdata)); // 
    //MemoryHexDump(Serial, setupdata, sizeof(setupdata), true); 
    host->controlWrite(dev, 0x40, 0xa1, 0x501f, 0xd90a, nullptr, 0); // 
  }
  
  // pending & 2 and 4
  ch341_setBaud(); // send the baud bytes

  // 8
  uint16_t ch341_format = 0;
  switch (format_) {
    default:
    // These values were observed when used on PC... Need to flush out others. 
    case USBHOST_SERIAL_8N1: ch341_format = 0xc3; break;
    case USBHOST_SERIAL_7E1: ch341_format = 0xda; break;
    case USBHOST_SERIAL_7O1: ch341_format = 0xca; break;
    case USBHOST_SERIAL_8N2: ch341_format = 0xc7; break;
  }
  host->controlWrite(dev, 0x40, 0x9a, 0x2518, ch341_format, nullptr, 0); // 0x08

  // This is setting handshake need to figure out what...
  // 0x20=DTR, 0x40=RTS send ~ of values. 
  //println("CH341: 0x40, 0xa4, 0xff9f, 0, 0 - Handshake");
  host->controlWrite(dev, 0x40, 0xa4, 0xff9f, 0, nullptr, 0); //  0x10 

  if (fConnect) {
  // 0x20 
    // This is setting handshake need to figure out what...
    //println("CH341: c0, 95, 0x706, 0, 8 - get status");
    host->controlRead(dev, 0xc0, 0x95, 0x706, 0, setupdata, sizeof(setupdata)); // 

    // This is setting handshake need to figure out what... 0x40
    //println("CH341: c0, 95, 0x706, 0, 8 - get status");
    host->controlWrite(dev, 0x40, 0x9a, 0x2727, 0, nullptr, 0); // 40
  }
}

void USBHostSerialDevice::initCP210X() {
  //printf("CP210X:  0x41, 0x11, 0, 0, 0 - reset port\n\r");
  host->controlWrite(dev, 0x41, 0x11, 0, 0, nullptr, 0);

  // set data format
  uint16_t cp210x_format = (format_ & 0xf) << 8;  // This should give us the number of bits.

  // now lets extract the parity from our encoding bits 5-7 and in theres 4-7
  cp210x_format |= (format_ & 0xe0) >> 1;   // they encode bits 9-11
  if (format_ & 0x100) cp210x_format |= 2;  // See if two stop bits
  //printf("CP210x setup, cp210x_format %x\n\r", cp210x_format);
  host->controlWrite(dev, 0x41, 3, cp210x_format, 0, nullptr, 0);  // data format 8N1

  // set baud rate
  setupdata[0] = (baudrate_)&0xff;  // Setup baud rate 115200 - 0x1C200
  setupdata[1] = (baudrate_ >> 8) & 0xff;
  setupdata[2] = (baudrate_ >> 16) & 0xff;
  setupdata[3] = (baudrate_ >> 24) & 0xff;
  //printf("CP210x Set Baud 0x40, 0x1e\n");
  host->controlWrite(dev, 0x40, 0x1e, 0, 0, setupdata, 4);

  // Appears to be an enable command
  memset(setupdata, 0, sizeof(setupdata));  // clear out the data
  printf("CP210x 0x41, 0, 1\n\r");
  host->controlWrite(dev, 0x41, 0, 1, 0, nullptr, 0);

  // MHS_REQUEST
  host->controlWrite(dev, 0x41, 7, 0x0303, 0, nullptr, 0);
  //dtr_rts_ = 3;
  return;
}



/*virtual */ int USBHostSerialDevice::available(void) {
  return rxBuffer_.available();
}
/*virtual */ int USBHostSerialDevice::peek(void) {
  return rxBuffer_.peek();
}
/*virtual */ int USBHostSerialDevice::read(void) {
  rxMut_.lock();
  auto ret = rxBuffer_.read_char();
  rxMut_.unlock();
  return ret;
}

/*virtual */ int USBHostSerialDevice::availableForWrite() {
  if (buffer_writes_) {
    if (!usb_tx_queued_) {
      return txBuffer_.availableForStore() + size_bulk_out_;
    }    
    return txBuffer_.availableForStore();
  }
  // return txBuffer_.availableForStore();
  return size_bulk_out_; // hacks to start
}


/*virtual */ size_t USBHostSerialDevice::write(uint8_t c) {
  // bugbug not sure how to setup timer yet...
  //txBuffer_.store_char(c);
  return write(&c, 1);
}

/*virtual*/ size_t USBHostSerialDevice::write(const uint8_t *buffer, size_t size) {
  size_t cb_left = size;
  //printf("USBHostSerialDevice::write(%p, %u)\n\r", buffer, size);
  //MemoryHexDump(Serial, buffer, size, true);
  if (size == 0) return 0; // bail if nothing to do

  if (buffer_writes_) {
    stopWriteTimeout(); // turn off the timer.
    //printf("\tAfter detach TO\n\r");
    in_tx_write_ = true; // not sure yet if needed. 
    while (cb_left) {
      while (!txBuffer_.availableForStore()) {} // should we do something like yield()?
      //txMut_.lock();
      txBuffer_.store_char(*buffer++);
      //txMut_.unlock();
      cb_left--;

      if (!usb_tx_queued_ && ((uint32_t)txBuffer_.available() >= size_bulk_out_)) {
        submit_async_bulk_write(0);
      }
    }
    //printf("\tAfter store loop\n\r");
    in_tx_write_ = false;  
    if (txBuffer_.available()) {
      //printf("\tBefore restart timer\n\r");
      startWriteTimeout();
    }

  } else {
    while (cb_left) {
      size_t count_write = (cb_left <= size_bulk_out_)? cb_left : size_bulk_out_;

      USB_TYPE ret;
      //printf("\t%p %p %u\n\r", bulk_out, buffer, count_write);
      if ((ret = host->bulkWrite(dev, bulk_out, (uint8_t *)buffer, count_write)) != USB_TYPE_OK) {
        //printf("bulkwrite(%p, %u) failed %u\n\r", buffer, count_write, ret);
        return size - cb_left;
      }
      cb_left -= count_write;
      buffer += count_write;
    }
  }

  return size;
}

void USBHostSerialDevice::submit_async_bulk_write(uint8_t where_called) {
  digitalWriteFast(5, HIGH);
  //txMut_.lock();
  uint16_t buffer_index = 0;
  for (; buffer_index < size_bulk_out_; buffer_index++) {
    digitalWriteFast(4, HIGH);
    int ch = txBuffer_.read_char();
    digitalWriteFast(4, LOW);
    if (ch == -1) break;
    txUSBBuf_[buffer_index] = ch;
  }
  //txMut_.unlock();
  usb_tx_queued_ = true;

  // Now queue the write.
  USB_TYPE ret;
  digitalWriteFast(5, LOW);
  digitalWriteFast(5, HIGH);
  if (where_called != 2) printf("submit_async_bulk_write(%u): %p %u\n", where_called, txUSBBuf_, buffer_index);
  if ((ret = host->bulkWrite(dev, bulk_out, (uint8_t *)txUSBBuf_, buffer_index, false)) != USB_TYPE_PROCESSING) {
    printf("Async bulkwrite(%p, %u) failed %u\n\r", txUSBBuf_, buffer_index, ret);
  }
  digitalWriteFast(5, LOW);

}


void USBHostSerialDevice::txHandler() {
  //printf("USBHostSerialDevice::txHandler() called ");
  if (bulk_out && buffer_writes_) {

    // Maybe should check for errors and the like?
   USB_TYPE state = bulk_out->getState();
   if (state == USB_TYPE_IDLE) {
      int tx_avail = txBuffer_.available(); 

      //printf("txHandler %u %u %d - %d %p\n\r", in_tx_write_, in_tx_flush_, tx_avail,
      //  bulk_out->getLengthTransferred(), bulk_out->getBufStart());
      usb_tx_queued_ = false; // USB Completed... 
      if (!in_tx_write_ && tx_avail &&
        (in_tx_flush_ || ((uint32_t)tx_avail >= size_bulk_out_))) {
        // if we are not in the write function and there is enough data in the
        // tx queue to fill the buffer output it now.
        submit_async_bulk_write(1);
      }
    } else {
      //printf("txhandler - state: %u\n\r", state);
    }
  }
}

#if 0
// Quick and dirty forwarders
void USBHostSerialDevice::txTimerCB0() {
  //printf("USBHostSerialDevice::txTimerCB0() called\n\r");
  device_list[0]->processTXTimerCB();
}

void USBHostSerialDevice::txTimerCB1() {
  //printf("USBHostSerialDevice::txTimerCB1() called\n\r");
  device_list[1]->processTXTimerCB();
}
#endif

// Handle the timer interrupt
void USBHostSerialDevice::processTXTimerCB() {
  USBHostSerialDevice::message_t * serobj_msg = mail_serobj_event.try_alloc();
  if (serobj_msg) {
    serobj_msg->event_id = LATENCY_TIMEOUT_MSG;
    mail_serobj_event.put(serobj_msg);    
  } else {
    startWriteTimeout();
  }
}

#define WAIT_FOR_U32_FOREVER 0xFFFFFFFF
void USBHostSerialDevice::tx_timeout_thread_proc() {
  while(1) {
    osEvent evt = mail_serobj_event.get();
    if (evt.status == osEventMail) {
      USBHostSerialDevice::message_t * serobj_msg = (message_t*)evt.value.p;
      switch (serobj_msg->event_id) {
        case LATENCY_TIMEOUT_MSG: 
          {

            if (!usb_tx_queued_ && !in_tx_write_) {
              stopWriteTimeout(); // stop the timer.
              submit_async_bulk_write(2); // submit the request for the rest of the data.
            } else {
              // maybe we need to reschedule
              startWriteTimeout();
            }
          }
          break;
      }
      mail_serobj_event.free(serobj_msg);
    }
  }
}


/*virtual */ void USBHostSerialDevice::flush(void) {
  if (buffer_writes_) {
    stopWriteTimeout();
    // only need to do something if we have a timer running...
    in_tx_flush_ = true; 
    if (!usb_tx_queued_ && txBuffer_.available()) {
      submit_async_bulk_write(3);
    }

    // now wait until they all complete...
    while (usb_tx_queued_) {}
    in_tx_flush_ = false; 
  }
  // Do we need to ask the serial adapter if they are done or not?
  
}


void USBHostSerialDevice::begin(uint32_t baud, uint32_t format) 
{
  baudrate_ = baud;
  format_ = format;

  if (buffer_writes_) {
    #if 1
    if (writeTOThread_ == nullptr) {
      writeTOThread_ = new rtos::Thread(osPriorityNormal2, 3 * 1024);
      if (writeTOThread_) {
        writeTOThread_->start(mbed::callback(this, &USBHostSerialDevice::tx_timeout_thread_proc));
      }
    }
    #else
    // See which slot we have...
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
      if (device_list[i] == this) break; // already in list
      if (device_list[i] == nullptr) {
        device_list[i] = this;
        timerCB_index_ = i;
      } 
    }
    #endif
  }
  if (buffer_writes_) printf ("USBHostSerialDevice::begin - buffered writes\n\r");
  else printf ("USBHostSerialDevice::begin - writes are unbuffered\n\r");

  switch (sertype_) {
    default:
    case CDCACM:
      {
        initCDCACM(false);
      }
      break;
    case FTDI:
      {
      }
      break;
    case PL2303:
      {
        initPL2303(false); 
      }
      break;  // set more stuff...

    case CH341: 
      {
       initCH341(false);  
      }
      break;
    case CP210X: 
      initCP210X();
      break;
  }
}


void USBHostSerialDevice::end() {
  switch (sertype_) {
    default:
    case PL2303:
    case CDCACM:
      host->controlWrite(dev, 0x21, 0x22, 0, 0, nullptr, 0);
      break;
    case FTDI: 
      host->controlWrite(dev, 0x40, 1, 0x0100, 0, nullptr, 0);
      break;  // clear DTR
    case CH341:
      host->controlWrite(dev, 0x40, 0xa4, 0xffff, 0, nullptr, 0);
      break;
  }

  dtr_rts_ = 0;
}


bool USBHostSerialDevice::manufacturer(uint8_t *buffer, size_t len) {
  cacheStringIndexes();
  return getStringDesc(iManufacturer_, buffer, len);
}

bool USBHostSerialDevice::product(uint8_t *buffer, size_t len){
  cacheStringIndexes();
  return getStringDesc(iProduct_, buffer, len);
}

bool USBHostSerialDevice::serialNumber(uint8_t *buffer, size_t len){
  cacheStringIndexes();
  return getStringDesc(iSerialNumber_, buffer, len);
}


#define STRING_DESCRIPTOR  (3)

bool USBHostSerialDevice::cacheStringIndexes() {
  if (iManufacturer_ != 0xff) return true; // already done

  //printf(">>>>> USBHostSerialDevice::cacheStringIndexes() called <<<<< \n\r");
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


bool USBHostSerialDevice::getStringDesc(uint8_t index, uint8_t *buffer, size_t len) {

  //printf(">>>>> USBHostSerialDevice::getStringDesc(%u) called <<<<< \n\r", index);
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

bool USBHostSerialDevice::setDTR(bool fSet)
{
  if (!connected()) return false;
  // NOT sure if we should check pending control and not allow it? OR???
  if (fSet) dtr_rts_ |= 1;
  else dtr_rts_ &= ~1;
  printf("setDTR: %d %d\n\r", fSet, dtr_rts_);

  switch (sertype_) {
    default: 
      return false; // Not sure how to do...
    case PL2303:
    case CDCACM: 
      host->controlWrite(dev, 0x21, 0x22, dtr_rts_, 0, nullptr, 0);
      break;
    case FTDI: 
      println("  >>FTDI");
      // The high 8 is mask and low 8 is setting. 
      host->controlWrite(dev, 0x40, 1, fSet? 0x0101 : 0x0100, 0, nullptr, 0);
      break;
    // not sure yet on these  
    //case CH341: 
    case CP210X: 
      // DTR(1) RTS(2)
      host->controlWrite(dev, 0x41, 7, fSet? 0x0101 : 0x0100, 0, nullptr, 0);
      break;
  }

  return true;
}


// Lets split this up from setting both
bool USBHostSerialDevice::setRTS(bool fSet)
{
  printf("setRTS: %d\n\r", fSet);
  if (fSet) dtr_rts_ |= 2;
  else dtr_rts_ &= ~2;
  printf("setRTS: %d %d\n\r", fSet, dtr_rts_);

  if (!connected()) return false;
  // NOT sure if we should check pending control and not allow it? OR???

  switch (sertype_) {
    default: 
      return false; // Not sure how to do...
    case PL2303:
    case CDCACM: 
      host->controlWrite(dev, 0x21, 0x22, dtr_rts_, 0, nullptr, 0);
      break;
    case FTDI: 
      println("  >>FTDI");
      // The high 8 is mask and low 8 is setting. 
      host->controlWrite(dev, 0x40, 1, fSet? 0x0202 : 0x0200, 0, nullptr, 0);
      break;
    // not sure yet on these  
    //case CH341: 
    case CP210X: 
      // DTR(1) RTS(2)
      host->controlWrite(dev, 0x41, 7, fSet? 0x0202 : 0x0200, 0, nullptr, 0);
      break;
  }

  return true;
}
