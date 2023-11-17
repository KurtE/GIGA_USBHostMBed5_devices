#include <MemoryHexDump.h>
#include <LibPrintf.h>
#include "USBHostTablets.h"
#include "elapsedMillis.h"

// lokki *** Device HID1 56a:27 Intuos5 touch M
//lokki *** Device HID1 56a:ba Intuos4 Large
// My Bamboo: *** Device HID2 56a:d8  CTH-661
// Mikes 056A, ProductID = 0302, Wacom Intuos PT S
// My Intuous S** Device HID1 56a:374 Intuos S
enum {
  PENPARTNER = 0,
  INTUOS5S,
  INTUOS5,
  INTUOS5L,
  INTUOS4L,
  BAMBOO_PEN,
  INTUOSHT,
  INTUOSHT2,
  BAMBOO_TOUCH,
  BAMBOO_PT,
  WACOM_24HDT,
  WACOM_27QHDT,
  WACOM_PTS,
  BAMBOO_PAD,
  H640P,
  INTUOS4100,
  MAX_TYPE,
};

#define WACOM_INTUOS_RES 100
#define WACOM_INTUOS3_RES 200

const USBHostTablets::tablet_info_t USBHostTablets::s_tablets_info[] = {
  { 0x056A, 0x27 /*"Wacom Intuos5 touch M"*/, 44704, 27940, 2047, 63, 2, 2, INTUOS5, 7, 4, 8, true, 44704, 27940 },
  { 0x056A, 0xD8 /*"Wacom Bamboo Comic 2FG"*/, 21648, 13700, 1023, 31, 2, 2, BAMBOO_PT, 2, 4, 4, false, 740, 500 },
  { 0x056A, 0x302 /*"Wacom Intuos PT S*/, 4095, 4095, 1023, 31, 2, 2, WACOM_PTS, 7, 3, 4, false, 4095, 4095 },
  { 0x256c, 0x006d /* "Huion HS64 and H640P"*/, 32767 * 2, 32767, 8192, 10, 0, 0, H640P, 0, 3, 6, false, 0, 0 },
  { 0x056A, 0xBA /*"Wacom Intuos4 L"*/, 44704, 27940, 2047, 63, 2, 2, INTUOS4L, 7, 4, 8, true },
  // Added for 4100, data to be verified.
  { 0x056A, 0x374, 15200, 9500, 1023, 31, 0, 0, INTUOS4100, 0, 3, 4, false, 0, 0 }
};

//static const struct wacom_features wacom_features_HID_ANY_ID =
//	{ "Wacom HID", .type = HID_GENERIC, .oVid = HID_ANY_ID, .oPid = HID_ANY_ID };

void USBHostTablets::init() {
  initHelper();
  dev = NULL;
  int_in = NULL;
  dev_connected = false;
  tablet_intf = -1;
}

bool USBHostTablets::connected() {
  return dev_connected;
}


bool USBHostTablets::connect() {

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


      if (tablet_device_found) {
        {
          /* As this is done in a specific thread
                     * this lock is taken to avoid to process the device
                     * disconnect in usb process during the device registering */
          USBHost::Lock Lock(host);

          int_in = dev->getEndpoint(tablet_intf, INTERRUPT_ENDPOINT, IN);

          if (!int_in) {
            break;
          }

          printf("New Tablet device: VID:%04x PID:%04x [dev: %p - intf: %d]\n", dev->getVid(), dev->getPid(), dev, tablet_intf);
          dev->setName("Tablet", tablet_intf);
          host->registerDriver(dev, tablet_intf, this, &USBHostTablets::init);

          int_in->attach(this, &USBHostTablets::rxHandler);
          size_in_ = int_in->getSize();
          printf("Input Size: %u\n", size_in_);

          hidParser.init(host, dev, tablet_intf, hid_descriptor_size_);
          hidParser.attach(this);
        }
        host->interruptRead(dev, int_in, buf_in_, size_in_, false);

        dev_connected = true;
        maybeSendSetupControlPackets();
        return true;
      }
    }
  }
  init();
  return false;
}


/*virtual*/ void USBHostTablets::setVidPid(uint16_t vid, uint16_t pid) {
  // We need to check these for the tablets that we are playing.
  idVendor_ = vid;
  idProduct_ = pid;
}

/*virtual*/ bool USBHostTablets::parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol)  //Must return true if the interface should be parsed
{
  printf("intf_class: %d\n", intf_class);
  printf("intf_subclass: %d\n", intf_subclass);
  printf("intf_protocol: %d\n", intf_protocol);
  tablet_info_index_ = 0xff;
  if ((idVendor_ != 0x056A) && (idVendor_ != 0x256c)) return false;  // Not one we process.
  for (uint8_t i = 0; i < (sizeof(s_tablets_info) / sizeof(s_tablets_info[0])); i++) {
    if ((s_tablets_info[i].idVendor == idVendor_) && (s_tablets_info[i].idProduct == idProduct_)) {
      tablet_info_index_ = i;
    }
  }
  printf("tablet_info_index_ = %u\n", tablet_info_index_);
  if (tablet_info_index_ == 0xff) return false;  // not in our list


  if (tablet_intf == -1) {
    // primary interface
    tablet_intf = intf_nb;
    return true;
  }
  return false;
}

/*virtual*/ bool USBHostTablets::useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir)  //Must return true if the endpoint will be used
{
  printf("intf_nb: %d\n", intf_nb);
  printf(" ??? HID Report size: %u\n", host->getLengthReportDescr());
  if (intf_nb == tablet_intf) {
    if (type == INTERRUPT_ENDPOINT && dir == IN) {
      tablet_device_found = true;
      hid_descriptor_size_ = host->getLengthReportDescr();
      return true;
    }
  }
  return false;
}

USB_TYPE USBHostTablets::sendControlWrite(uint32_t bmRequestType, uint32_t bRequest,
                                          uint32_t wValue, uint32_t wIndex, uint32_t wLength, void *buf) {
  printf(">>> sendControlWrite(%lx, %lx, %lx %lx, %lx, %p)\n", bmRequestType, bRequest, wValue, wIndex, wLength, buf);
  if (wLength) MemoryHexDump(Serial, (uint8_t *)buf, wLength, false);
  USB_TYPE res = host->controlWrite(dev, bmRequestType, bRequest, wValue, wIndex, (uint8_t *)buf, wLength);
  if (res != USB_TYPE_OK) printf("\t !! return status: %u = %s\n", res, int_in->getStateString());
  return res;
}

USB_TYPE USBHostTablets::sendControlRead(uint32_t bmRequestType, uint32_t bRequest,
                                         uint32_t wValue, uint32_t wIndex, uint32_t wLength, void *buf) {
  printf(">>> sendControlRead(%lx, %lx, %lx %lx, %lx, %p)\n", bmRequestType, bRequest, wValue, wIndex, wLength, buf);
  USB_TYPE res = host->controlRead(dev, bmRequestType, bRequest, wValue, wIndex, (uint8_t *)buf, wLength);

  if (wLength) MemoryHexDump(Serial, (uint8_t *)buf, wLength, false);
  if (res != USB_TYPE_OK) printf("\t !! return status: %u = %s\n", res, int_in->getStateString());
  return res;
}


void USBHostTablets::forceResendSetupControlPackets() {
  sendSetupPacket_ = true;
  maybeSendSetupControlPackets();
}

// Some tablets need us to send a control message to them at the end of setup
// for example some want us to set the input to specific report.
// So do that here.
void USBHostTablets::maybeSendSetupControlPackets() {

  if (sendSetupPacket_) {
    sendSetupPacket_ = false;
    // This one is needed for my bamboo tablet
    //bmRequestType=0x21 Data direction=Host to device, Type=Class, Recipient=Interface
    //bRequest=0x09 SET_REPORT (HID class)
    //wValue=0x0302 Report type=Feature, Report ID=0x02
    //wIndex=0x0000 Interface=0x00
    //wLength=0x0002
    //byte=0x02
    //byte=0x02
    printf("maybeSendSetupControlPackets: %u\n", s_tablets_info[tablet_info_index_].report_id);
    if (s_tablets_info[tablet_info_index_].report_id != 0) {
      if (debugPrint_) printf("$$ Setup tablet report ID: %x to %x\n", s_tablets_info[tablet_info_index_].report_id,
                              s_tablets_info[tablet_info_index_].report_value);
      static const uint8_t set_report_data[2] = { s_tablets_info[tablet_info_index_].report_id, s_tablets_info[tablet_info_index_].report_value };
      control_packet_pending_state_ = 0xff;
      control_packet_sent_time = millis();  // remember when we sent it
      sendControlWrite(0x21, 9, 0x0302, 0, 2, (void *)set_report_data);
    }

    // required for Huion tablets
    if (s_tablets_info[tablet_info_index_].idVendor == 0x256c) {
      if (debugPrint_) printf("$$$ Setup tablet Index: %d\n", tablet_info_index_);

      // Ask for firmware version.
      control_packet_pending_state_ = 1;
      ignore_count_ = 2;  // hack
      uint8_t desc_len = 0;
      control_packet_sent_time = millis();  // remember when we sent it
      sendControlRead(0x80, 6, (3 << 8) + 201, 0x0409, sizeof(buffer_), buffer_);
      desc_len = convertBufferToAscii();
      Serial.print("Firmware version: ");
      Serial.println((char *)buffer_);

      //Internal Manufacture:
      control_packet_sent_time = millis();  // remember when
      sendControlRead(0x80, 6, (3 << 8) + 202, 0x0409, sizeof(buffer_), buffer_);

      desc_len = convertBufferToAscii();
      Serial.print("Internal Manufacture: ");
      Serial.println((char *)buffer_);

      //Mandatory to report ID 0x08, calls parameters
      //try 100 for older tablets first - if 0 len then try 200
      control_packet_sent_time = millis();  // remember when
      sendControlRead(0x80, 6, (3 << 8) + 200, 0x0409, sizeof(buffer_), buffer_);

      Serial.println("Special report: ");
      tablet_width_ = __get_unaligned_le16(&buffer_[2]);
      tablet_height_ = __get_unaligned_le16(&buffer_[5]);
      uint16_t PH_PRESSURE_LM = __get_unaligned_le16(&buffer_[8]);
      uint16_t resolution = __get_unaligned_le16(&buffer_[10]);
      cnt_frame_buttons_ = buffer_[13];

      printf("Max X: %d\n", tablet_width_);
      printf("Max Y: %d\n", tablet_height_);
      printf("Max pressure: %d\n", PH_PRESSURE_LM);
      printf("Resolution: %d\n\n", resolution);
      printf("Frame Buttons: %d\n\n", cnt_frame_buttons_);

      if (desc_len == 0) Serial.println("Maybe send other version?");
    }
  }
}


uint8_t USBHostTablets::convertBufferToAscii() {
  //uint8_t buffer[WACOM_STRING_BUF_SIZE];
  uint8_t buf_index = 0;

  // Try to verify - The first byte should be length and the 2nd byte should be 0x3
  uint8_t count_bytes_returned = buffer_[0];
  if ((buffer_[1] == 0x3) && (count_bytes_returned <= sizeof(buffer_))) {
    for (uint8_t i = 2; i < count_bytes_returned; i += 2) {
      buffer_[buf_index++] = buffer_[i];
    }
  }
  buffer_[buf_index] = 0;  // null terminate.

  return buf_index;
}




void USBHostTablets::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) {
  // TODO: check if absolute coordinates
  if (debugPrint_) printf("$HIB:%lx type:%u min:%ld max:%d\n", topusage, type, lgmin, lgmax);
  hid_input_begin_count_++;
  maybeSendSetupControlPackets();
}

#define WACOM_HID_SP_PAD 0x00040000
#define WACOM_HID_SP_BUTTON 0x00090000
#define WACOM_HID_SP_DIGITIZER 0x000d0000
#define WACOM_HID_SP_DIGITIZERINFO 0x00100000

uint32_t USBHostTablets::wacom_equivalent_usage(uint32_t usage) {
  if ((usage & 0xFFFF0000) == 0xFF0D0000) {
    uint32_t subpage = (usage & 0xFF00) << 8;
    uint32_t subusage = (usage & 0xFF);

    switch (subpage) {
      case WACOM_HID_SP_PAD:
      case 0x00010000:  // Mouse stuff I think
      case WACOM_HID_SP_BUTTON:
      case WACOM_HID_SP_DIGITIZER:
      case WACOM_HID_SP_DIGITIZERINFO:
        if (debugPrint_) Serial.print("########");
        return subpage | subusage;
    }
    if (debugPrint_) Serial.print("********");
    // remove the the leading FF
    return usage & 0x00FFFFFF;
  }
  return usage;
}

void USBHostTablets::rxHandler() {
  uint16_t len = int_in->getLengthTransferred();

  if (len) {
    //Serial.println("$HID RX $$$");
    //MemoryHexDump(Serial, buf_in_, len, true, nullptr, -1, 0);
    //hidParser.parse(buf_in_, len);

    const uint8_t *buffer = (const uint8_t *)buf_in_;
    if (debugPrint_) {
      const uint8_t *p = buffer;
      printf("HPID(%u):", len);
      //  if (len > 32) len = 32;
      while (len--) printf(" %02X", *p++);
      Serial.println();
    }
    // see if we wish to process buffer
    // Only proess if we have a known tablet
    bool succeeded = false;
    switch (s_tablets_info[tablet_info_index_].type) {
      case INTUOS5:
        succeeded = decodeIntuos5(buffer, len);
        break;

      case BAMBOO_PT:
        succeeded = decodeBamboo_PT(buffer, len);
        break;
      case WACOM_PTS:
        succeeded = decodeWacomPTS(buffer, len);
        break;
      case INTUOSHT:
        succeeded = decodeIntuosHT(buffer, len);
        break;
      case H640P:
        succeeded = decodeH640P(buffer, len);
        break;
      case INTUOS4100:
        succeeded = decodeIntuos4100(buffer, len);
        break;
      case INTUOS4L:
        succeeded = decodeIntuos4(buffer, len);
        break;
      default:
        succeeded = false;
    }
  }
  host->interruptRead(dev, int_in, buf_in_, size_in_, false);
}

void USBHostTablets::hid_input_data(uint32_t usage, int32_t value) {
#if 0
  //  if (debugPrint_) printf("Digitizer: usage=%X, value=%d(%02x)\n", usage, value, value);

  uint32_t usage_in = usage;
  usage = wacom_equivalent_usage(usage);

  uint32_t usage_page = usage >> 16;
  usage &= 0xFFFF;
  if (debugPrint_) printf("Digitizer: &usage=%X(in:%X), usage_page=%x, value=%d(%x)\n", usage, usage_in, usage_page, value, value);

  if (usage_page == 0x1) {
    // This is main desktop page:
    switch (usage) {
      case 0x30:
        touch_x_[0] = value;
        break;
      case 0x31:
        touch_y_[0] = value;
        break;
      case 0x32:  // Apple uses this for horizontal scroll
        wheelH = value;
        break;
      case 0x38:
        wheel = value;
        break;
      default:
        printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
        break;
    }
    //    digiAxes[usage & 0xf] = value;

  } else if (usage_page == 0x9) {
    // Button Page.
    if (usage >= 0x1 && usage <= 0x32) {
      if (value == 0) {
        pen_buttons_ &= ~(1 << (usage - 1));
      } else {
        pen_buttons_ |= (1 << (usage - 1));
      }
    } else
      printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
  } else if (usage_page == 0xFF00) {
    // going to ignore index for now and just store them out.
    //if ((usage >= 0x100) && (usage <= 0x10F )) digiAxes[usage & 0xf] = value;
    if (digiAxes_index_ < (sizeof(digiAxes) / sizeof(digiAxes[0]))) digiAxes[digiAxes_index_++] = value;
    else printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);

  } else if (usage_page == 0xD) {
    switch (usage) {
      case 0x30: digiAxes[0] = value; break;
      case 0x32: digiAxes[1] = value; break;
      case 0x36: digiAxes[2] = value; break;
      case 0x42: digiAxes[3] = value; break;
      case 0x44: digiAxes[4] = value; break;
      case 0x5A: digiAxes[5] = value; break;
      case 0x5B: digiAxes[6] = value; break;
      case 0x5C: digiAxes[7] = value; break;
      case 0x77: digiAxes[8] = value; break;
      default:
        printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
        break;
    }
  } else {
    printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
  }
#endif
}

void USBHostTablets::hid_input_end() {
#if 0
  if (debugPrint_) printf("$HIE\n");
  digitizerEvent = true;
  hid_input_begin_count_ = 0;
  digiAxes_index_ = 0;
#endif
}


bool USBHostTablets::decodeBamboo_PT(const uint8_t *data, uint16_t len) {
  // only process report 2
  if (data[0] != 2) return false;

  // long format
  //HPID(64): 02 80 58 82 76 01 52 82 75 01 50 00 00 00 00 11 1F 11 21 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  //if (debugPrint_) printf("BAMBOO PT %p, %u\n", data, len);
  uint8_t offset = 0;
  if (len == 64) {
    frame_buttons_ = data[1] & 0xf;
    touch_count_ = 0;
    if (debugPrint_) printf("BAMBOO PT(64): BTNS: %x", pen_buttons_);
    for (uint8_t i = 0; i < 2; i++) {
      bool touch = data[offset + 3] & 0x80;
      if (touch) {
        touch_x_[touch_count_] = ((data[offset + 3] << 8) | (data[offset + 4])) & 0x7ff;
        touch_y_[touch_count_] = ((data[offset + 5] << 8) | (data[offset + 6])) & 0x7ff;
        if (debugPrint_) printf(" %u:(%u, %u)", i, touch_x_[touch_count_], touch_y_[touch_count_]);
        touch_count_++;
        offset += (data[1] & 0x80) ? 8 : 9;
      }
    }
    if (touch_count_) {
      event_type_ = TOUCH;

    } else {
      event_type_ = FRAME;
    }
    if (debugPrint_) Serial.println();
    digitizerEvent = true;
    return true;
  } else if (len == 9) {
    if (debugPrint_) Serial.print("BAMBOO PT(9): ");
    // the pen
    //HPID(9): 02 F0 1A 20 62 1B 00 00 0
    bool range = (data[1] & 0x80) == 0x80;
    bool prox = (data[1] & 0x40) == 0x40;
    bool rdy = (data[1] & 0x20) == 0x20;
    pen_buttons_ = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;
    if (rdy) {
      pen_buttons_ = data[1] & 0xf;
      pen_pressure_ = __get_unaligned_le16(&data[6]);
      if (debugPrint_) printf(" BTNS: %x Pressure: %u", pen_buttons_, pen_pressure_);
    }
    if (prox) {
      touch_x_[0] = __get_unaligned_le16(&data[2]);
      touch_y_[0] = __get_unaligned_le16(&data[4]);
      if (debugPrint_) printf(" (%u, %u)", touch_x_[0], touch_y_[0]);
      touch_count_ = 1;
      digitizerEvent = true;  // only set true if we are close enough...
    }
    if (range) {
      if (data[8] <= s_tablets_info[tablet_info_index_].distance_max) {
        pen_distance_ = s_tablets_info[tablet_info_index_].distance_max - data[8];
        if (debugPrint_) printf(" Distance: %u", pen_distance_);
      }
    }
    event_type_ = PEN;
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
}

bool USBHostTablets::decodeWacomPTS(const uint8_t *data, uint16_t len) {
  // only process report 2
  if (data[0] != 2) return false;
  // long format
  //HPID(64): 02 80 58 82 76 01 52 82 75 01 50 00 00 00 00 11 1F 11 21 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  //if (debugPrint_) printf("BAMBOO PT %p, %u\n", data, len);
  uint8_t offset = 2;
  bool touch_changed = false;
  if (len == 64) {
    if ((data[2] & 0x81) != 0x80) {
      uint8_t count = data[1] & 0x7;
      touch_count_ = 0;
      if (debugPrint_) printf("INTOSH PTS(64):");
      for (uint8_t i = 0; i < count; i++) {
        if ((data[offset] >= 2) && (data[offset] <= 17)) {
          if (data[offset + 1] & 0x80) {
            touch_changed = true;
            touch_x_[touch_count_] = (data[offset + 2] << 4) | (data[offset + 4] >> 4);
            touch_y_[touch_count_] = (data[offset + 3] << 4) | (data[offset + 4] & 0x0f);
            if (debugPrint_) printf(" %u(%u):(%u, %u)", i, touch_count_, touch_x_[touch_count_], touch_y_[touch_count_]);
            touch_count_++;
          }
        }
        // Else we will ignore the slot
        offset += 8;
      }

      if (debugPrint_) Serial.println();
      if (touch_changed) {
        // is there anything to report?
        event_type_ = TOUCH;
        //digitizerEvent = true;
      }
    } else {
      frame_buttons_ = data[3];
      if (debugPrint_) printf("Frame buttons: %d\n", frame_buttons_);
      event_type_ = FRAME;
    }
    digitizerEvent = true;
    return true;

  } else if (len <= 16) {
    if (debugPrint_) printf("INTOSH PTS( %d ): ", len);
    if (data[1] != 0x01) {
      // the pen
      //HPID(9): 02 F0 1A 20 62 1B 00 00 0
      bool range = (data[1] & 0x80) == 0x80;
      bool prox = (data[1] & 0x40) == 0x40;
      bool rdy = (data[1] & 0x20) == 0x20;
      pen_buttons_ = 0;
      touch_count_ = 0;
      pen_pressure_ = 0;
      pen_distance_ = 0;
      if (rdy) {
        pen_buttons_ = data[1] & 0xf;
        pen_pressure_ = __get_unaligned_le16(&data[6]);
        if (debugPrint_) printf(" BTNS: %x Pressure: %u", pen_buttons_, pen_pressure_);
      }
      if (prox) {
        touch_x_[0] = __get_unaligned_le16(&data[2]);
        touch_y_[0] = __get_unaligned_le16(&data[4]);
        if (debugPrint_) printf(" (%u, %u)", touch_x_[0], touch_y_[0]);
        touch_count_ = 1;
        //digitizerEvent = true;  // only set true if we are close enough...
      }
      if (range) {
        if (data[8] <= s_tablets_info[tablet_info_index_].distance_max) {
          pen_distance_ = s_tablets_info[tablet_info_index_].distance_max - data[8];
          if (debugPrint_) printf(" Distance: %u", pen_distance_);
        }
      }
      event_type_ = PEN;
      if (debugPrint_) Serial.println();
    } else {
      frame_buttons_ = data[3];
      if (debugPrint_) printf("Frame buttons: %d\n", frame_buttons_);
      event_type_ = FRAME;
    }
    digitizerEvent = true;
    return true;
  }
  return false;
}

bool USBHostTablets::decodeIntuosHT(const uint8_t *data, uint16_t len) {
  // only process report 2
  if (data[0] != 2) return false;
  // long format
  //HPID(64): 02 80 58 82 76 01 52 82 75 01 50 00 00 00 00 11 1F 11 21 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  //if (debugPrint_) printf("BAMBOO PT %p, %u\n", data, len);
  uint8_t offset = 2;
  bool touch_changed = false;
  if (len == 64) {
    uint8_t count = data[1] & 0x7;
    touch_count_ = 0;
    if (debugPrint_) printf("INTOSH PT(64):");
    for (uint8_t i = 0; i < count; i++) {
      if (data[offset] == 0x80) {
        // button message
        pen_buttons_ = data[offset + 1];
        touch_changed = true;
        if (debugPrint_) printf(" BTNS: %x", pen_buttons_);
      } else if ((data[offset] >= 2) && (data[offset] <= 17)) {
        if (data[offset + 1] & 0x80) {
          touch_changed = true;
          touch_x_[touch_count_] = (data[offset + 2] << 4) | (data[offset + 4] >> 4);
          touch_y_[touch_count_] = (data[offset + 3] << 4) | (data[offset + 4] & 0x0f);
          if (debugPrint_) printf(" %u(%u):(%u, %u)", i, touch_count_, touch_x_[touch_count_], touch_y_[touch_count_]);
          touch_count_++;
        }
      }
      // Else we will ignore the slot
      offset += 8;
    }

    if (debugPrint_) Serial.println();
    if (touch_changed) {
      // is there anything to report?
      event_type_ = TOUCH;
      digitizerEvent = true;
    }
    return true;
  } else if (len <= 16) {
    if (debugPrint_) printf("INTOSH PT( %d ): ", len);
    // the pen
    //HPID(9): 02 F0 1A 20 62 1B 00 00 0
    bool range = (data[1] & 0x80) == 0x80;
    bool prox = (data[1] & 0x40) == 0x40;
    bool rdy = (data[1] & 0x20) == 0x20;
    pen_buttons_ = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;
    if (rdy) {
      pen_buttons_ = data[1] & 0xf;
      pen_pressure_ = __get_unaligned_le16(&data[6]);
      if (debugPrint_) printf(" BTNS: %x Pressure: %u", pen_buttons_, pen_pressure_);
    }
    if (prox) {
      touch_x_[0] = __get_unaligned_le16(&data[2]);
      touch_y_[0] = __get_unaligned_le16(&data[4]);
      if (debugPrint_) printf(" (%u, %u)", touch_x_[0], touch_y_[0]);
      touch_count_ = 1;
      digitizerEvent = true;  // only set true if we are close enough...
    }
    if (range) {
      if (data[8] <= s_tablets_info[tablet_info_index_].distance_max) {
        pen_distance_ = s_tablets_info[tablet_info_index_].distance_max - data[8];
        if (debugPrint_) printf(" Distance: %u", pen_distance_);
      }
    }
    event_type_ = PEN;
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
}

bool USBHostTablets::decodeIntuos5(const uint8_t *data, uint16_t len) {
  // only process reports 2 and 3
  if ((data[0] != 2) && (data[0] != 3)) return false;

  // long format
  // HPID(64): 02 07 01 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 00 00 00 00 00 00
  // HPID(64): 02 07 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 00 00 00 00 00 00
  // HPID(64): 02 80 58 82 76 01 52 82 75 01 50 00 00 00 00 11 1F 11 21 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  uint8_t offset = 2;
  bool touch_changed = false;
  if (len == 64) {
    uint8_t count = data[1] & 0x7;
    touch_count_ = 0;
    if (debugPrint_) printf("INTOSH PT(64):");
    for (uint8_t i = 0; i < count; i++) {
      if (data[offset] == 0x80) {
        // button message
        pen_buttons_ = data[offset + 1];
        touch_changed = true;
        if (debugPrint_) printf(" BTNS: %x", pen_buttons_);
      } else if ((data[offset] >= 2) && (data[offset] <= 17)) {
        if (data[offset + 1] & 0x80) {
          touch_changed = true;
          //touch_x_[touch_count_] = ((data[offset + 3] << 8) | (data[offset + 4]));
          //touch_y_[touch_count_] = ((data[offset + 5] << 8) | (data[offset + 6]));
          touch_x_[touch_count_] = (data[offset + 2] << 4) | (data[offset + 4] >> 4);
          touch_y_[touch_count_] = (data[offset + 3] << 4) | (data[offset + 4] & 0x0f);
          if (debugPrint_) printf(" %u(%u):(%u, %u)", i, touch_count_, touch_x_[touch_count_], touch_y_[touch_count_]);
          touch_count_++;
        }
      }
      // Else we will ignore the slot
      offset += 8;
    }

    if (debugPrint_) Serial.println();
    if (touch_changed) {
      // is there anything to report?
      event_type_ = TOUCH;
      digitizerEvent = true;
    }
    return true;

  } else if (len == 16) {
    if (debugPrint_) Serial.print("INTOSH PT(16): ");
    pen_buttons_ = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;

    if ((data[1] & 0xfc) == 0xc0) {
      // In Prox: HPID(16): 02 C2 80 A2 38 03 29 51 00 00 00 00 00 00 00 00
      uint32_t serial = ((data[3] & 0x0f) << 28) + (data[4] << 20) + (data[5] << 12) + (data[6] << 4) + (data[7] >> 4);

      uint16_t id = (data[2] << 4) | (data[3] >> 4) | ((data[7] & 0x0f) << 16) | ((data[8] & 0xf0) << 8);
      if (debugPrint_) printf(" Prox: Serial:%x id:%x", serial, id);
      // Do we process this one?

    } else if ((data[1] & 0xfe) == 0x20) {
      // we are in range: HPID(16): 02 20 1E 0F 00 00 00 00 00 FE 00 00 00 00 00 00
      pen_distance_ = s_tablets_info[tablet_info_index_].distance_max;
      if (debugPrint_) Serial.print(" Range");
      event_type_ = PEN;
      digitizerEvent = true;

    } else if ((data[1] & 0xfe) == 0x80) {

      //only process buttons and wheel here, not stylus removal
      if (data[0] == 0x03) {
        if (data[2] > 0) side_wheel_ = data[2] - 128;
        side_wheel_button_ = data[3];
        //touch the buttons
        frame_touch_buttons_ = data[5];

        //press the buttons
        frame_buttons_ = data[4];
        if (debugPrint_) {
          Serial.println();
          printf("Wheel: %u ", side_wheel_);
          Serial.println();
          printf("WheelButton: %u ", side_wheel_button_);
          Serial.println();
          printf("Touch:%u Press:%u ", frame_touch_buttons_, frame_buttons_);
          Serial.println();
        }
        event_type_ = FRAME;
        digitizerEvent = true;
      }
      // out of proximite
    } else {
      // Maybe should double check tool type.
      uint8_t type = (data[1] >> 1) & 0x0f;
      if (type < 4) {
        // normal pen message.
        touch_x_[0] = __get_unaligned_be16(&data[2]) | ((data[9] >> 1) & 1);
        touch_y_[0] = __get_unaligned_be16(&data[4]) | (data[9] & 1);
        pen_distance_ = data[9] >> 2;
        pen_pressure_ = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
        pen_tilt_x_ = (((data[7] << 1) & 0x7e) | (data[8] >> 7)) - 64;
        pen_tilt_y_ = (data[8] & 0x7f) - 64;
        pen_buttons_ = data[1] & 0x6;
        if (pen_pressure_ > 10) pen_buttons_ |= 1;
        if (debugPrint_) printf("PEN: (%u, %u) BTNS:%x d:%u p:%u tx:%d ty:%d\n", touch_x_[0], touch_y_[0], pen_buttons_, pen_distance_, pen_pressure_, pen_tilt_x_, pen_tilt_y_);
        event_type_ = PEN;
        digitizerEvent = true;
      } else {
        if (debugPrint_) printf("Unprocess tool type: %x", type);
      }
    }
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
}


bool USBHostTablets::decodeH640P(const uint8_t *data, uint16_t len) {

  //printf("decodeH640P(%p, %u): %02x %02x\n", data, len, data[0], data[1]);
  if (data[0] == 0x03) {
    if (ignore_count_) {
      ignore_count_--;
      return true;
    }
    return false;
  }

  if (data[0] != 0x08) return false;
  if (len < 12) return false;
  if (data[1] != 0xE0) {
    if (debugPrint_) Serial.print("H640P(64): ");
    // DATA INTERPRETATION:
    // source: https://github.com/andresm/digimend-kernel-drivers/commit/b7c8b33c0392e2a5e4e448f901e3dfc206d346a6

    // 00 01 02 03 04 05 06 07 08 09 10 11
    // ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^
    // |  |  |  |  |  |  |  |  |  |  |  |
    // |  |  |  |  |  |  |  |  |  |  |  Y Tilt
    // |  |  |  |  |  |  |  |  |  |  X Tilt
    // |  |  |  |  |  |  |  |  |  Y HH
    // |  |  |  |  |  |  |  |  X HH
    // |  |  |  |  |  |  |  Pressure H
    // |  |  |  |  |  |  Pressure L
    // |  |  |  |  |  Y H
    // |  |  |  |  Y L
    // |  |  |  X H
    // |  |  X L
    // |  Pen buttons
    // Report ID - 0x08

    bool prox = (data[1] & 0x80) == 0x80;
    bool rdy = (data[1]) >= 0x80;
    pen_buttons_ = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;
    if (rdy) {
      pen_buttons_ = data[1] & 0xf;
      pen_pressure_ = __get_unaligned_le16(&data[6]);
      pen_tilt_x_ = data[10];
      pen_tilt_y_ = data[11];
      if (debugPrint_) printf(" BTNS: %x Pressure: %u", pen_buttons_, pen_pressure_);
    }
    if (prox) {
      touch_x_[0] = __get_unaligned_le16(&data[2]);
      touch_y_[0] = __get_unaligned_le16(&data[4]);
      if (data[1] == 0x80) pen_buttons_ = 0;
      if (debugPrint_) printf(" (%u, %u)", touch_x_[0], touch_y_[0]);
      touch_count_ = 1;
      digitizerEvent = true;  // only set true if we are close enough...
    }
    /*
  		if (range) {
  			if (data[8] <= s_tablets_info[tablet_info_index_].distance_max) {
  			  pen_distance_ = s_tablets_info[tablet_info_index_].distance_max - data[8];
  			  if (debugPrint_) printf(" Distance: %u", pen_distance_);
  			}
  		}
  		*/
    pen_distance_ = 0;
    event_type_ = PEN;
    if (debugPrint_) Serial.println();
    return true;
  } else if (data[1] == 0xE0) {
    //only process buttons, not stylus removal
    //press the buttons
    frame_buttons_ = data[4];
    //if (debugPrint_) {
    printf("Press:%u ", frame_buttons_);
    Serial.println();
    //}
    event_type_ = FRAME;
    digitizerEvent = true;
    // out of proximite
  }
  return false;
}

bool USBHostTablets::decodeIntuos4(const uint8_t *data, uint16_t len) {
  // only process reports 2 and 3
  if ((data[0] != 2) && (data[0] != 0x0C)) return false;

  //uint8_t offset = 2;
  //bool touch_changed = false;
  if (len == 10) {
    if (debugPrint_) Serial.print("INTUOS 4(10): ");
    pen_buttons_ = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;
    if (data[0] == 0x02) {
      if ((data[1] & 0xfc) == 0xc0) {
        // In Prox: HPID(16): 02 C2 80 A2 38 03 29 51 00 00 00 00 00 00 00 00
        uint32_t serial = ((data[3] & 0x0f) << 28) + (data[4] << 20) + (data[5] << 12) + (data[6] << 4) + (data[7] >> 4);

        uint16_t id = (data[2] << 4) | (data[3] >> 4) | ((data[7] & 0x0f) << 16) | ((data[8] & 0xf0) << 8);
        if (debugPrint_) printf(" Prox: Serial:%x id:%x", serial, id);
        // Do we process this one?

      } else if ((data[1] & 0xfe) == 0x20) {
        // we are in range: HPID(16): 02 20 1E 0F 00 00 00 00 00 FE 00 00 00 00 00 00
        pen_distance_ = s_tablets_info[tablet_info_index_].distance_max;
        if (debugPrint_) Serial.print(" Range");
        event_type_ = PEN;
        digitizerEvent = true;
      } else if ((data[1] & 0xfe) == 0x80) {
        // out of proximite
        if (debugPrint_) Serial.println("Stylus removed");
      } else {
        // Maybe should double check tool type.
        uint8_t type = (data[1] >> 1) & 0x0f;

        if (type < 4) {

          // normal pen message.
          touch_x_[0] = __get_unaligned_be16(&data[2]) | ((data[9] >> 1) & 1);
          touch_y_[0] = __get_unaligned_be16(&data[4]) | (data[9] & 1);
          pen_distance_ = data[9] >> 2;
          pen_pressure_ = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
          pen_tilt_x_ = (((data[7] << 1) & 0x7e) | (data[8] >> 7)) - 64;
          pen_tilt_y_ = (data[8] & 0x7f) - 64;
          pen_buttons_ = data[1] & 0x6;
          if (pen_pressure_ > 10) pen_buttons_ |= 1;
          if (debugPrint_) printf("PEN: (%u, %u) BTNS:%x d:%u p:%u tx:%d ty:%d\n", touch_x_[0], touch_y_[0], pen_buttons_, pen_distance_, pen_pressure_, pen_tilt_x_, pen_tilt_y_);
          event_type_ = PEN;
          digitizerEvent = true;
        } else {
          if (debugPrint_) printf("Unprocess tool type: %x", type);
        }
      }
    } else if (data[0] == 0x0C) {

      if (data[1] > 0) side_wheel_ = data[1] - 128;
      side_wheel_button_ = data[2];
      //touch the buttons
      //  frame_touch_buttons_ = data[4];

      //press the buttons
      frame_buttons_ = data[3];
      if (debugPrint_) {
        Serial.println();
        printf("Wheel: %u ", side_wheel_);
        Serial.println();
        printf("WheelButton: %u ", side_wheel_button_);
        Serial.println();
        printf("Touch:%u Press:%u ", frame_touch_buttons_, frame_buttons_);
        Serial.println();
      }
      event_type_ = FRAME;
      digitizerEvent = true;
    }
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
}

bool USBHostTablets::decodeIntuos4100(const uint8_t *data, uint16_t len) {
  // only process report 0x10(16)
  switch (data[0]) {
    case 16:
      {
        if (debugPrint_) Serial.print("4100 PEN: ");

        // I think most of this is handled in wacom_intuos_general like the pen messages of Intuous5, so will start from there.
        uint8_t type = (data[1] >> 1) & 0x0f;
        if (type < 4) {
          // normal pen message.
          touch_x_[0] = data[2] | (data[3] << 8) | (data[4] << 16);
          touch_y_[0] = data[5] | (data[6] << 8) | (data[7] << 16);
          pen_pressure_ = __get_unaligned_le16(&data[8]);
          pen_distance_ = data[6];
          pen_buttons_ = data[1] & 0x7;
          if (debugPrint_) printf("PEN: (%u, %u) BTNS:%x d:%u p:%u", touch_x_[0], touch_y_[0], pen_buttons_, pen_distance_, pen_pressure_);
          event_type_ = PEN;
          digitizerEvent = true;
        } else {
          if (debugPrint_) printf("Unprocess tool type: %x", type);
        }
        if (debugPrint_) Serial.println();
        return true;
      }
    case 17:
      {
        frame_buttons_ = data[1];
        if (debugPrint_) printf("Press:%u ", frame_buttons_);
        event_type_ = FRAME;
        touch_count_ = 0;
        digitizerEvent = true;
        return true;
      }
  }
  return false;
}

void USBHostTablets::digitizerDataClear() {
  digitizerEvent = false;
  pen_buttons_ = 0;
  frame_buttons_ = 0;
  frame_touch_buttons_ = 0;
  touch_x_[0] = 0;
  touch_y_[0] = 0;
  touch_count_ = 0;
  event_type_ = NONE;
  pen_pressure_ = 0;
  pen_distance_ = 0;
  pen_tilt_x_ = 0;
  pen_tilt_y_ = 0;
  wheel = 0;
  wheelH = 0;
}
