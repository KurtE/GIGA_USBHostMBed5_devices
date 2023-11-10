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

#include "USBHostGamepadDevice.h"
//#include <MemoryHexDump.h>

#define Debug 0

#if USBHOST_GAMEPAD

template<class T>
const T& clamp(const T& x, const T& lower, const T& upper) {
    return min(upper, max(x, lower));
}

bool ack_rvd = false;

USBHostGamepad::product_vendor_mapping_t USBHostGamepad::pid_vid_mapping[] = {
    //{ 0x045e, 0x02dd, XBOXONE,  false },  // Xbox One Controller
    { 0x045e, 0x02ea, XBOXONE,  false },  // Xbox One S Controller - only tested on giga
    //{ 0x045e, 0x0b12, XBOXONE,  false },  // Xbox Core Controller (Series S/X)
    //{ 0x045e, 0x0719, XBOX360,  false },
    //{ 0x045e, 0x028E, SWITCH,   false },  // Switch?
    { 0x057E, 0x2009, SWITCH,   true  },  // Switch Pro controller.  // Let the swtich grab it, but...
    //{ 0x0079, 0x201C, SWITCH,   false },
    { 0x054C, 0x0268, PS3,      true  },
    { 0x054C, 0x042F, PS3,      true  },   // PS3 Navigation controller
    { 0x054C, 0x03D5, PS3_MOTION, true},   // PS3 Motion controller
    { 0x054C, 0x05C4, PS4,      true  },
    { 0x054C, 0x09CC, PS4,      true  },
    { 0x0A5C, 0x21E8, PS4,      true  },
    { 0x046D, 0xC626, SpaceNav, true  },  // 3d Connextion Space Navigator, 0x10008
    { 0x046D, 0xC628, SpaceNav, true  },  // 3d Connextion Space Navigator, 0x10008
    { 0x046d, 0xc215, LogiExtreme3DPro, true}

};

static  uint8_t xboxone_start_input[] = {0x05, 0x20, 0x00, 0x01, 0x00};
static  uint8_t xbox360w_inquire_present[] = {0x08, 0x00, 0x0F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//static  uint8_t switch_start_input[] = {0x19, 0x01, 0x03, 0x07, 0x00, 0x00, 0x92, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
static  uint8_t switch_start_input[] = {0x80, 0x02};

//-----------------------------------------------------------------------------
// Switch controller config structure.
//-----------------------------------------------------------------------------
static uint8_t switch_packet_num = 0;
struct SWProIMUCalibration {
	int16_t acc_offset[3];
	int16_t acc_sensitivity[3] = {16384, 16384, 16384};
	int16_t gyro_offset[3];
	int16_t gyro_sensitivity[3] = {15335, 15335, 15335};
}  __attribute__((packed));

struct SWProIMUCalibration SWIMUCal;

struct SWProStickCalibration {
	int16_t rstick_center_x;
	int16_t rstick_center_y;
	int16_t rstick_x_min;
	int16_t rstick_x_max;
	int16_t rstick_y_min;
	int16_t rstick_y_max;
	
	int16_t lstick_center_x;
	int16_t lstick_center_y;
	int16_t lstick_x_min;
	int16_t lstick_x_max;
	int16_t lstick_y_min;
	int16_t lstick_y_max;
	
	int16_t deadzone_left;
	int16_t deadzone_right;
}  __attribute__((packed));

struct SWProStickCalibration SWStickCal;


USBHostGamepad::USBHostGamepad()
{
    host = USBHost::getHostInst();
    init();
}

void USBHostGamepad::init()
{
  dev = NULL;
  int_in = NULL;
  int_out = NULL; 
  onUpdate = NULL;
  report_id_ = 0;
  dev_connected = false;
  gamepad_device_found = false;
  gamepad_intf = -1;
  nb_ep = 0;
  iManufacturer_ = 0xff; // make sure we get them again...
  iProduct_ = 0xff; // make sure we get them again...
  iSerialNumber_ = 0xff; // make sure we get them again...
  
    initialPass_ = true;
    initialPassButton_ = true;
    initialPassBT_ = true;
    buttonOffset_ = 0x00;
    connectedComplete_pending_ = 0;
    sw_last_cmd_sent_ = 0;
    sw_last_cmd_repeat_count = 0;

    // State values to output to Joystick.
    rumble_lValue_ = 0;
    rumble_rValue_ = 0;
    rumble_timeout_ = 0;
    leds_[0] = 0; leds_[1] = 0; leds_[3] = 0;
    buttons = 0;
}

bool USBHostGamepad::connected() {
    return dev_connected;
}


bool USBHostGamepad::connect()
{
    int len_listen;

    if (dev_connected) {
        return true;
    }

    for (uint8_t i = 0; i < MAX_DEVICE_CONNECTED; i++) {

        if (dev = host->getDevice(i)) {
            if(host->enumerate(dev, this)) {
                break;
            }
            if (gamepad_device_found) {
                {
                    /* As this is done in a specific thread
                     * this lock is taken to avoid to process the device
                     * disconnect in usb process during the device registering */
                    //USBHost::Lock  Lock(host);

                    int_in = dev->getEndpoint(gamepad_intf, INTERRUPT_ENDPOINT, IN);
                    USB_INFO("int in:%p", int_in);

                    int_out = dev->getEndpoint(gamepad_intf, INTERRUPT_ENDPOINT, OUT);
                    USB_INFO(" int out:%p\r\n", int_out);

                    printf("\tAfter get end points\n\r");

                    if (!int_in) {
                        break;
                    }

                    USB_INFO("New Gamepad device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, gamepad_intf);
                    printf("New Gamepad device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, gamepad_intf);
                    dev->setName("Gamepad", gamepad_intf);
                    host->registerDriver(dev, gamepad_intf, this, &USBHostGamepad::init);

                    int_in->attach(this, &USBHostGamepad::rxHandler);
                    if(int_out != 0)
                      int_out->attach(this, &USBHostGamepad::txHandler);
                    len_listen = int_in->getSize();
                    if (len_listen > sizeof(report)) {
                        len_listen = sizeof(report);
                    }
                }
                int ret=host->interruptRead(dev, int_in, report, len_listen, false);
                MBED_ASSERT((ret==USB_TYPE_OK) || (ret ==USB_TYPE_PROCESSING) || (ret == USB_TYPE_FREE));
                if ((ret==USB_TYPE_OK) || (ret ==USB_TYPE_PROCESSING)) {
                    dev_connected = true;
                }
                if (ret == USB_TYPE_FREE) {
                    dev_connected = false;
                }

                joytype_t jtype = mapVIDPIDtoGamepadType(dev->getVid(), dev->getPid(), true);
                USB_INFO("\tVID:%x PID:%x Jype:%x\n", dev->getVid(), dev->getPid(), jtype);
                USB_INFO("Jtype=", (uint8_t)jtype, DEC);

                if (jtype == XBOXONE) {
                    printf("XBOXONE Initialization Sent.....");
                    sendMessage(xboxone_start_input, sizeof(xboxone_start_input));
                } else if (jtype == XBOX360) {
                    sendMessage(xbox360w_inquire_present, sizeof(xbox360w_inquire_present));
                } else if (jtype == SWITCH) {
                    printf("Switch Initialization Sent.....");
                    sendMessage(switch_start_input, sizeof(switch_start_input));
                }
                return true;
            }
        }
    }
    init();
    return false;
}

/*
Some extraneous code for fun
                    uint8_t interf_hid = 0;
                    for(uint8_t i = 0; i < 4; i++) {
                      test = dev->getInterface(i);
                      if(test->intf_class == 3) interf_hid = i;
                      printf("->>>>>>>>>>>>>>> Interface: %x, Endpoint: %x, Class: %x\n", i, test->nb_endpoint, test->intf_class, test->intf_subclass);
                    }

                    //dev->addInterface(interf_hid, 3, 0, 0);
                    //gamepad_intf = interf_hid;
*/


void USBHostGamepad::disconnect() {
  USB_INFO(" USBHostSerialDevice::disconnect() called\r\n");
  init();  // clear everything
}


void USBHostGamepad::rxHandler()
{
    int len_listen = int_in->getLengthTransferred();
    if (len_listen !=0) {
        processMessages(report, len_listen);
    }

    /*  set again the maximum value */
    len_listen = int_in->getSize();

    if (len_listen > sizeof(report)) {
        len_listen = sizeof(report);
    }

    if (dev) {
        host->interruptRead(dev, int_in, report, len_listen, false);
        //MemoryHexDump(Serial, report, sizeof(report), false);
    }
}

void USBHostGamepad::txHandler() {
  uint8_t report1[64];
  USB_INFO("USBHostGamepad::txHandler() called");
  if (int_out) {
  }
}

/*virtual*/ void USBHostGamepad::setVidPid(uint16_t vid, uint16_t pid)
{
    // we don't check VID/PID for Gamepad driver
    USB_INFO("VID: %X, PID: %X\n\r", vid, pid);

    // Lets see if we know what type of Gamepad this is. That is, is it a PS3 or PS4 or ...
    gamepadType_ = mapVIDPIDtoGamepadType(dev->getVid(), dev->getPid(), false);
    USB_INFO("GamepadController:: gamepadType_=%d\n", gamepadType_);

}

/*virtual*/ bool USBHostGamepad::parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol) //Must return true if the interface should be parsed
{
  USB_INFO("(parseInterface) NB: %x, Class: 0x%x, Subclass: 0x%x, Protocol: 0x%x\n", intf_nb, intf_class, intf_subclass, intf_protocol);

  if (gamepad_intf == -1) {
    if(gamepadType_ == XBOXONE) {
        if ((intf_class == 0xff) &&
            (intf_subclass == 0x47) &&
            (intf_protocol == 0xd0)) {
          gamepad_intf = intf_nb;
        return true;
        }
    } else {
      if ((intf_class == HID_CLASS) &&
          (intf_subclass == 0x00) &&
          (intf_protocol == 0x00)) {
        gamepad_intf = intf_nb;
        return true;
      }
    }  
  } 
  return false;
}

/*virtual*/ bool USBHostGamepad::useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir) //Must return true if the endpoint will be used
{
  USB_INFO("USE ENDPOINT........\n");
  if (intf_nb == gamepad_intf) {
    if (type == INTERRUPT_ENDPOINT && dir == IN) {
        USB_INFO("Using interrupt endpoint");
        if(gamepadType_ == XBOXONE) {
          nb_ep ++;
          if (nb_ep == 3) {
              gamepad_device_found = true;
          }
        } else {
          gamepad_device_found = true;
        }
        return true;
    }
    if (intf_nb == gamepad_intf) {
      if (type == INTERRUPT_ENDPOINT && dir == OUT) {
            gamepad_device_found = true;
      }
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------------

bool USBHostGamepad::processMessages(uint8_t * buffer, uint16_t length) {
    if (gamepadType_ == SWITCH) {
        if (sw_usb_init(buffer, length, false))
            return true;
        //USB_INFO("Processing Switch Message\n");
        sw_process_HID_data(buffer, length);
    } else {
        //USB_INFO("Processing HID Message\n");
        process_HID_data(buffer, length);
    }
    return true;
}


bool USBHostGamepad::sendMessage(uint8_t * buffer, uint16_t length) 
{
    int ret = host->interruptWrite(dev, int_out, buffer, length, false);
    MBED_ASSERT((ret==USB_TYPE_OK) || (ret ==USB_TYPE_PROCESSING) || (ret == USB_TYPE_FREE));
    if ((ret==USB_TYPE_OK) || (ret ==USB_TYPE_PROCESSING)) {
      //USB_INFO("Message Sent....\n");
      return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
USBHostGamepad::joytype_t USBHostGamepad::mapVIDPIDtoGamepadType(uint16_t idVendor, uint16_t idProduct, bool exclude_hid_devices)
{
    for (uint8_t i = 0; i < (sizeof(pid_vid_mapping) / sizeof(pid_vid_mapping[0])); i++) {
        if ((idVendor == pid_vid_mapping[i].idVendor) && (idProduct == pid_vid_mapping[i].idProduct)) {
            printf("Match PID/VID: \n", i, DEC);
            if (exclude_hid_devices && pid_vid_mapping[i].hidDevice) return UNKNOWN;
            return pid_vid_mapping[i].joyType;
        }
    }
    return UNKNOWN;     // Not in our list
}

//-----------------------------------------------------------------------------
static uint8_t rumble_counter = 0;

bool USBHostGamepad::setRumble(uint8_t lValue, uint8_t rValue, uint8_t timeout)
{
    // Need to know which joystick we are on.  Start off with XBox support - maybe need to add some enum value for the known
    // joystick types.
    rumble_lValue_ = lValue;
    rumble_rValue_ = rValue;
    rumble_timeout_ = timeout;
    int ret;

    switch (gamepadType_) {
    default:
        break;
    case PS3:
        return transmitPS3UserFeedbackMsg();
    case PS3_MOTION:
        return transmitPS3MotionUserFeedbackMsg();
    case PS4:
        return transmitPS4UserFeedbackMsg();
    case XBOXONE:
        // Lets try sending a request to the XBox 1.
        txbuf_[0] = 0x9;
        txbuf_[1] = 0x0;
        txbuf_[2] = 0x0;
        txbuf_[3] = 0x09; // Substructure (what substructure rest of this packet has)
        txbuf_[4] = 0x00; // Mode
        txbuf_[5] = 0x0f; // Rumble mask (what motors are activated) (0000 lT rT L R)
        txbuf_[6] = 0x0; // lT force
        txbuf_[7] = 0x0; // rT force
        txbuf_[8] = lValue; // L force
        txbuf_[9] = rValue; // R force
        txbuf_[10] = 0xff; // Length of pulse
        txbuf_[11] = 0x00; // Period between pulses
        txbuf_[12] = 0x00; // Repeat
        sendMessage(txbuf_, 13);
        break;
    case XBOX360:
        txbuf_[0] = 0x00;
        txbuf_[1] = 0x01;
        txbuf_[2] = 0x0F;
        txbuf_[3] = 0xC0;
        txbuf_[4] = 0x00;
        txbuf_[5] = lValue;
        txbuf_[6] = rValue;
        txbuf_[7] = 0x00;
        txbuf_[8] = 0x00;
        txbuf_[9] = 0x00;
        txbuf_[10] = 0x00;
        txbuf_[11] = 0x00;
        sendMessage(txbuf_, 12); 
        break;
    case SWITCH:
        printf("Set Rumble data (USB): %d, %d\n", lValue, rValue);

        memset(txbuf_, 0, 18);  // make sure it is cleared out
        //txbuf_[0] = 0x80;
        //txbuf_[1] = 0x92;
        //txbuf_[3] = 0x31;
        txbuf_[0] = 0x10;   // Command

        // Now add in subcommand data:
        // Probably do this better soon
		if(switch_packet_num > 0x10) switch_packet_num = 0;
        txbuf_[1 + 0] = switch_packet_num;
        switch_packet_num = (switch_packet_num + 1) & 0x0f; //

        uint8_t rumble_on[8] = {0x28, 0x88, 0x60, 0x61, 0x28, 0x88, 0x60, 0x61};
        uint8_t rumble_off[8] =  {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};
        
        //if ((lValue == 0x00) && (rValue == 0x00)) {
        //	for(uint8_t i = 0; i < 4; i++) packet->rumbleDataR[i] = rumble_off[i];
        //	for(uint8_t i = 4; i < 8; i++) packet->rumbleDataL[i-4] = rumble_off[i];
        //}
        if ((lValue != 0x0) || (rValue != 0x0)) {
          if (lValue != 0 && rValue == 0x00) {
            for(uint8_t i = 0; i < 4; i++) txbuf_[i + 2] = rumble_off[i];
            for(uint8_t i = 4; i < 8; i++) txbuf_[i - 4 + 6] = rumble_on[i];
          } else if (rValue != 0 && lValue == 0x00) {
            for(uint8_t i = 0; i < 4; i++) txbuf_[i + 2] = rumble_on[i];
            for(uint8_t i = 4; i < 8; i++) txbuf_[i - 4 + 6] = rumble_off[i];
          } else if (rValue != 0 && lValue != 0) {
            for(uint8_t i = 0; i < 4; i++) txbuf_[i + 2] = rumble_on[i];
            for(uint8_t i = 4; i < 8; i++) txbuf_[i - 4 + 6] = rumble_on[i];
          }
        }
        txbuf_[11] = 0x00;
        txbuf_[12] = 0x00;
		
        sendMessage(txbuf_, sizeof(txbuf_));  
        break;    

    }
    return false;
}

//-----------------------------------------------------------------------------
bool USBHostGamepad::setLEDs(uint8_t lr, uint8_t lg, uint8_t lb)
{
    int ret;
    // Need to know which joystick we are on.  Start off with XBox support - maybe need to add some enum value for the known
    // joystick types.
    //DBGPrintf("::setLEDS(%x %x %x)\n", lr, lg, lb);
    if ((leds_[0] != lr) || (leds_[1] != lg) || (leds_[2] != lb)) {
        leds_[0] = lr;
        leds_[1] = lg;
        leds_[2] = lb;

        switch (gamepadType_) {
        case PS3:
            return transmitPS3UserFeedbackMsg();
        case PS3_MOTION:
            return transmitPS3MotionUserFeedbackMsg();
        case PS4:
            return transmitPS4UserFeedbackMsg();
        case XBOX360:
            // 0: off, 1: all blink then return to before
            // 2-5(TL, TR, BL, BR) - blink on then stay on
            // 6-9() - On
            // ...
            txbuf_[1] = 0x00;
            txbuf_[2] = 0x08;
            txbuf_[3] = 0x40 + lr;
            txbuf_[4] = 0x00;
            txbuf_[5] = 0x00;
            txbuf_[6] = 0x00;
            txbuf_[7] = 0x00;
            txbuf_[8] = 0x00;
            txbuf_[9] = 0x00;
            txbuf_[10] = 0x00;
            txbuf_[11] = 0x00;
            sendMessage(txbuf_, sizeof(txbuf_));    
            break;
        case SWITCH:
            memset(txbuf_, 0, 20);  // make sure it is cleared out
            txbuf_[0] = 0x01;   // Command
            // Now add in subcommand data:
            // Probably do this better soon
            txbuf_[1 + 0] = rumble_counter++; //
            txbuf_[1 + 1] = 0x00;
            txbuf_[1 + 2] = 0x01;
            txbuf_[1 + 3] = 0x40;
            txbuf_[1 + 4] = 0x40;
            txbuf_[1 + 5] = 0x00;
            txbuf_[1 + 6] = 0x01;
            txbuf_[1 + 7] = 0x40;
            txbuf_[1 + 8] = 0x40;
            txbuf_[1 + 9] = 0x30; // LED Command
            txbuf_[1 + 10] = lr;
            sendMessage(txbuf_, sizeof(txbuf_));
            break;
        case XBOXONE:
        default:
            return false;
        }
    }
    return false;
}

bool USBHostGamepad::transmitPS4UserFeedbackMsg()
{
    uint8_t packet[32];
    memset(packet, 0, sizeof(packet));

    packet[0] = 0x05; // Report ID
    packet[1] = 0xFF;

    packet[4] = rumble_lValue_; // Small Rumble
    packet[5] = rumble_rValue_; // Big rumble
    packet[6] = leds_[0]; // RGB value
    packet[7] = leds_[1];
    packet[8] = leds_[2];
    // 9, 10 flash ON, OFF times in 100ths of second?  2.5 seconds = 255
    return sendMessage(packet, sizeof(packet));    

}

static const uint8_t PS3_USER_FEEDBACK_INIT[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};

bool USBHostGamepad::transmitPS3UserFeedbackMsg() {
    memcpy(txbuf_, PS3_USER_FEEDBACK_INIT, 48);

    txbuf_[1] = rumble_lValue_ ? rumble_timeout_ : 0;
    txbuf_[2] = rumble_lValue_; // Small Rumble
    txbuf_[3] = rumble_rValue_ ? rumble_timeout_ : 0;
    txbuf_[4] = rumble_rValue_; // Big rumble
    txbuf_[9] = leds_[2] << 1; // RGB value     // using third led now...

    //DBGPrintf("\nJoystick update Rumble/LEDs %d %d %d %d %d\n",  txbuf_[1], txbuf_[2],  txbuf_[3],  txbuf_[4],  txbuf_[9]);
    return sendMessage(txbuf_, sizeof(txbuf_));    

}

#define MOVE_REPORT_BUFFER_SIZE 7
#define MOVE_HID_BUFFERSIZE 50 // Size of the buffer for the Playstation Motion Controller

bool USBHostGamepad::transmitPS3MotionUserFeedbackMsg() {
    
    txbuf_[0] = 0x02; // Set report ID, this is needed for Move commands to work
    txbuf_[2] = leds_[0];
    txbuf_[3] = leds_[1];
    txbuf_[4] = leds_[2];
    txbuf_[6] = rumble_lValue_; // Set the rumble value into the write buffer

    return sendMessage(txbuf_, sizeof(txbuf_));    

}

bool USBHostGamepad::process_HID_data(const uint8_t *data, uint16_t length)
{
    joystickEvent = false;
    // Example data from PS4 controller
    //01 7e 7f 82 84 08 00 00 00 00
    //   LX LY RX RY BT BT PS LT RT
    //DBGPrintf("JoystickController::process_bluetooth_HID_data: data[0]=%x\n", data[0]);
    // May have to look at this one with other controllers...
    report_id_ = data[0];

  if(Debug) {
    printf("  Joystick Data: ");
    for (uint16_t i = 0; i < length; i++) printf("%02x ", data[i]);
    printf("\r\n");
  }

    if (data[0] == 0x01) {
        if(gamepadType_ == PS3) {
            // Quick and dirty hack to match PS3 HID data
            uint32_t cur_buttons = data[2] | ((uint16_t)data[3] << 8) | ((uint32_t)data[4] << 16);
            if (cur_buttons != buttons) {
                buttons = cur_buttons;
                joystickEvent = true;   // something changed.
            }

            //uint64_t mask = 0x1;
            //axis_mask_ = 0x27;  // assume bits 0, 1, 2, 5
            for (uint16_t i = 0; i < 4; i++) {
                if (axis[i] != data[i + 6]) {
                    //axis_changed_mask_ |= mask;
                    axis[i] = data[i + 6];
                }
                //mask <<= 1; // shift down the mask.
            }
            
            for(uint16_t i = 0; i < 6; i++) {
                axis[i+4] = data[i];
            }

            // Then rest of data
            //mask = 0x1 << 10;   // setup for other bits
            for (uint16_t i = 10; i < length; i++ ) {
                //axis_mask_ |= mask;
                if (data[i] != axis[i]) {
                    //axis_changed_mask_ |= mask;
                    axis[i] = data[i];
                }
                //mask <<= 1; // shift down the mask.
            }
            
            joystickEvent = true;

        } else 
          if(gamepadType_ == PS4) {
            /*
             * [1] LX, [2] = LY, [3] = RX, [4] = RY
             * [5] combo, tri, cir, x, sqr, D-PAD (4bits, 0-3
             * [6] R3,L3, opt, share, R2, L2, R1, L1
             * [7] Counter (bit7-2), T-PAD, PS
             * [8] Left Trigger, [9] Right Trigger
             * [10-11] Timestamp
             * [12] Battery (0 to 0xff)
             * [13-14] acceleration x
             * [15-16] acceleration y
             * [17-18] acceleration z
             * [19-20] gyro x
             * [21-22] gyro y
             * [23-24] gyro z
             * [25-29] unknown
             * [30] 0x00,phone,mic, usb, battery level (4bits)
             * rest is trackpad?  to do implement?
             */
            //print("  Joystick Data: ");
            // print_hexbytes(data, length);
            if (length > TOTAL_AXIS_COUNT) length = TOTAL_AXIS_COUNT;   // don't overflow arrays...

            
            for(uint16_t i = 0; i < (length-1); i++ )  { axis[i] = data[i+1]; }
            
            //This moves data to be equivalent to what we see for
            //data[0] = 0x01
            uint8_t tmp_data[length - 2];

            for (uint16_t i = 0; i < (length - 2); i++ ) {
                tmp_data[i] = 0;
                tmp_data[i] = data[i];
            }

            //lets try our button logic
            //PS Bit
            tmp_data[7] = (tmp_data[7] >> 0) & 1;
            //set arrow buttons to axis[0]
            tmp_data[10] = tmp_data[5] & ((1 << 4) - 1);
            //set buttons for last 4bits in the axis[5]
            tmp_data[5] = tmp_data[5] >> 4;

            // Lets try mapping the DPAD buttons to high bits
            //                                            up    up/right  right    R DN      DOWN    L DN      Left    LUP
            static const uint32_t dpad_to_buttons[] = {0x10000, 0x30000, 0x20000, 0x60000, 0x40000, 0xC0000, 0x80000, 0x90000};

            // Quick and dirty hack to match PS4 HID data
            uint32_t cur_buttons = ((uint32_t)tmp_data[7] << 12) | (((uint32_t)tmp_data[6] * 0x10)) | ((uint16_t)tmp_data[5] ) ;

            if (tmp_data[10] < 8) cur_buttons |= dpad_to_buttons[tmp_data[10]];

            if (cur_buttons != buttons) {
                buttons = cur_buttons;
            //    joystickEvent = true;   // something changed.
            } else if (data[0] == 0x11) {

            }
            
            joystickEvent = true;
        } 
    }
    
    if(gamepadType_ == XBOXONE) {
 
/*        if(data[0] == 0x07) {       //Got share button
            if(data[4] == 1) {
                cur_buttons = (cur_buttons | (uint32_t) data[1] << 16);
                if (cur_buttons != buttons) {
                    buttons = cur_buttons;
                    joystickEvent = true;   // something changed.
                } 
            }
        } else
            */
          if(data[0] == 0x20) {
            uint8_t tmp_data[length];

            for (uint16_t i = 0; i < (length - 2); i++ ) {
                tmp_data[i] = 0;
                tmp_data[i] = data[i];
            }

            uint32_t cur_buttons = data[4] | ((uint16_t)data[5] << 8);
            if (cur_buttons != buttons) {
                buttons = cur_buttons;
                joystickEvent = true;   // something changed.
            }   

            //analog hats
            axis[0] = (int16_t)(((uint16_t)data[11] << 8) | data[10]);
            axis[1] = (int16_t)(((uint16_t)data[13] << 8) | data[12]);
            axis[2] = (int16_t)(((uint16_t)data[15] << 8) | data[14]);
            axis[3] = (int16_t)(((uint16_t)data[17] << 8) | data[16]);
            
            //buttons
            axis[4] = data[4];  //YBXA, pages, lines?
            axis[5] = data[5];  //DPAD, L1, R1
            
            axis[6] = (uint16_t)(((uint16_t)data[7] << 7) | data[6]);   //ltv
            axis[7] = (uint16_t)(((uint16_t)data[9] << 9) | data[8]);   //rtv
           
            joystickEvent = true;
        }
    } 

    if(gamepadType_ == LogiExtreme3DPro) {
      joystickEvent = false;
      axis[0] = data[0];      //rx
      axis[1] = data[1];      //ry
      axis[2] = data[3];      //rz
      axis[3] = data[2] >> 4; //hat
      axis[4] = data[5];      //slider
      axis[5] = data[4];      //button group A
      axis[6] = data[6];      //button group B
      joystickEvent = true;
    }



}
        


void USBHostGamepad::sw_sendCmdUSB(uint8_t cmd, uint32_t timeout) {
    //USB_INFO("sw_sendCmdUSB: cmd:%x, timeout:%x\n",  cmd, timeout);
	//sub-command
    txbuf_[0] = 0x80;
	  txbuf_[1] = cmd;
    sw_last_cmd_sent_ = cmd; // remember which command we sent
    
    sendMessage(txbuf_, 2);
    
/*
	if(driver_) {
        if (timeout != 0) {
            driver_->startTimer(timeout);
        }
		driver_->sendPacket(txbuf_, 2);
        em_sw_ = 0;
	} else {
		if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 18, this, __LINE__)) {
			println("switch transfer fail");
		}
	}
    */
}

void USBHostGamepad::sw_sendSubCmdUSB(uint8_t sub_cmd, uint8_t *data, uint8_t size, uint32_t timeout) {
        //USB_INFO("sw_sendSubCmdUSB(%x, %p, %u): ",  sub_cmd, size);
        if(Debug){
          for (uint8_t i = 0; i < size; i++) printf(" %02x", data[i]);
          printf("\n");
        }
        memset(txbuf_, 0, 32);  // make sure it is cleared out

		txbuf_[0] = 0x01;
        // Now add in subcommand data:
        // Probably do this better soon
        txbuf_[ 1] = switch_packet_num = (switch_packet_num + 1) & 0x0f; //

        txbuf_[ 2] = 0x00;
        txbuf_[ 3] = 0x01;
        txbuf_[ 4] = 0x40;
        txbuf_[ 5] = 0x40;
        txbuf_[ 6] = 0x00;
        txbuf_[ 7] = 0x01;
        txbuf_[ 8] = 0x40;
        txbuf_[ 9] = 0x40;
		
		txbuf_[ 10] = sub_cmd;
		
		//sub-command
		for(uint16_t i = 0; i < size; i++) {
			txbuf_[i + 11] = data[i];
		}

        sendMessage(txbuf_, 32);
		//println("USB send sub cmd: driver? ", (uint32_t)driver_, HEX);
		//print_hexbytes((uint8_t*)txbuf_, 32);
/*
		if(driver_) {
			driver_->sendPacket(txbuf_, 32);
            if (timeout != 0) {
                driver_->startTimer(timeout);
            }
		} else if (txpipe_) {
			if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 32, this, __LINE__)) {
				println("switch transfer fail");
			}
		}
*/
        em_sw_ = 0;
//		if (!timeout) delay(100);
        delay(100);
}

bool USBHostGamepad::sw_usb_init(uint8_t *buffer, uint16_t cb, bool timer_event)
{
    if (buffer) {
        if ((buffer[0] != 0x81) && (buffer[0] != 0x21))
            return false; // was not an event message
        //driver_->stopTimer();
        uint8_t ack_rpt = buffer[0];
        if (ack_rpt == 0x81) {
            uint8_t ack_81_subrpt = buffer[1];
            USB_INFO("\t(%u)CMD last sent: %x ack cmd: %x ", (uint32_t)em_sw_, sw_last_cmd_sent_, ack_81_subrpt);
            switch(ack_81_subrpt) {
                case 0x02: USB_INFO("Handshake Complete......\n"); break;
                case 0x03: USB_INFO("Baud Rate Change Complete......\n"); break;
                case 0x04: USB_INFO("Disabled USB Timeout Complete......\n"); break;
                default:  USB_INFO("???");
            }

            if (!initialPass_) return true; // don't need to process

            if (sw_last_cmd_sent_ == ack_rpt) { 
                sw_last_cmd_repeat_count = 0;
                connectedComplete_pending_++;
            } else {
                USB_INFO("\tcmd != ack rpt count:%u ", sw_last_cmd_repeat_count);
                if (sw_last_cmd_repeat_count) {
                    USB_INFO("Skip to next\n");
                    sw_last_cmd_repeat_count = 0;
                    connectedComplete_pending_++;
                } else {
                    USB_INFO("Retry\n");
                    sw_last_cmd_repeat_count++;
                }
            }
        } else {
            // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 
            //21 0a 71 00 80 00 01 e8 7f 01 e8 7f 0c 80 40 00 00 00 00 00 00 ...
            uint8_t ack_21_subrpt = buffer[14];
            sw_parseAckMsg(buffer);
            USB_INFO("\t(%u)CMD Submd ack cmd: %x \n", (uint32_t)em_sw_, ack_21_subrpt);
            switch (ack_21_subrpt) {
                case 0x40: USB_INFO("IMU Enabled......\n"); break;
                case 0x48: USB_INFO("Rumbled Enabled......\n"); break;
                case 0x10: USB_INFO("IMU Cal......\n"); break;
                case 0x30: USB_INFO("Std Rpt Enabled......\n"); break;
                default: USB_INFO("Other\n"); break;
            }
            
            if (!initialPass_) return true; // don't need to process
            sw_last_cmd_repeat_count = 0;
            connectedComplete_pending_++;

        }

    } 
    /* else if (timer_event) {
        if (!initialPass_) return true; // don't need to process
        DBGPrintf("\t(%u)Timer event - advance\n", (uint32_t)em_sw_);
        sw_last_cmd_repeat_count = 0;
        connectedComplete_pending_++; 
    }
    */

    // Now lets Send out the next state
    // Note: we don't increment it here, but let the ack or
    // timeout increment it.
    uint8_t packet_[8];
    switch(connectedComplete_pending_) {
        case 0:
            //Change Baud
            USB_INFO("Change Baud\n");
            sw_sendCmdUSB(0x03, SW_CMD_TIMEOUT);
            break;
        case 1:
            USB_INFO("Handshake2\n");
            sw_sendCmdUSB(0x02, SW_CMD_TIMEOUT);
            break;
        case 2:
            USB_INFO("Try to get IMU Cal\n");
            packet_[0] = 0x20;
            packet_[1] = 0x60;
            packet_[2] = 0x00;
            packet_[3] = 0x00;
            packet_[4] = (0x6037 - 0x6020 + 1);
            sw_sendSubCmdUSB(0x10, packet_, 5, SW_CMD_TIMEOUT);   // doesnt work wired
            break;
		case 3:
			USB_INFO("\nTry to Get IMU Horizontal Offset Data\n");
			packet_[0] = 0x80;
			packet_[1] = 0x60;
			packet_[2] = 0x00;
			packet_[3] = 0x00;
			packet_[4] = (0x6085 - 0x6080 + 1);
			sw_sendSubCmdUSB(0x10, packet_, 5, SW_CMD_TIMEOUT);   
			break;
		case 4:
			USB_INFO("\n Read: Factory Analog stick calibration and Controller Colours\n");
			packet_[0] = 0x3D;
			packet_[1] = 0x60;
			packet_[2] = 0x00;
			packet_[3] = 0x00;
			packet_[4] = (0x6055 - 0x603D + 1); 
			sw_sendSubCmdUSB(0x10, packet_, 5, SW_CMD_TIMEOUT);	
            break;
        case 5:
            connectedComplete_pending_++;
            USB_INFO("Enable IMU\n");
            packet_[0] = 0x01;
            sw_sendSubCmdUSB(0x40, packet_, 1, SW_CMD_TIMEOUT);
            break;
        case 6:
            USB_INFO("JC_USB_CMD_NO_TIMEOUT\n");
            sw_sendCmdUSB(0x04, SW_CMD_TIMEOUT);
            break;
        case 7:
            USB_INFO("Enable Rumble\n");
            packet_[0] = 0x01;
            sw_sendSubCmdUSB(0x48, packet_, 1, SW_CMD_TIMEOUT);
            break;
        case 8:
            USB_INFO("Enable Std Rpt\n");
            packet_[0] = 0x30;
            sw_sendSubCmdUSB(0x03, packet_, 1, SW_CMD_TIMEOUT);
            break;
        case 9:
            USB_INFO("JC_USB_CMD_NO_TIMEOUT\n");
            packet_[0] = 0x04;
            sw_sendCmdUSB(0x04, SW_CMD_TIMEOUT);
            break;
        case 10:
            connectedComplete_pending_ = 99;
            initialPass_ = false;
    }
    return true;
}
        
 void USBHostGamepad::sw_parseAckMsg(const uint8_t *buf_) 
{
	int16_t data[6];
	uint8_t offset = 20;
	uint8_t icount = 0;
	//uint8_t packet_[8];
	
	if((buf_[14] == 0x10 && buf_[15] == 0x20 && buf_[16] == 0x60)) {
		//parse IMU calibration
		USB_INFO("===>  IMU Calibration \n");	
		for(uint8_t i = 0; i < 3; i++) {
			SWIMUCal.acc_offset[i] = (int16_t)(buf_[icount+offset] | (buf_[icount+offset+1] << 8));
			SWIMUCal.acc_sensitivity[i] = (int16_t)(buf_[icount+offset+6] | (buf_[icount+offset+1+6] << 8));
			SWIMUCal.gyro_offset[i] = (int16_t)(buf_[icount+offset+12] | (buf_[icount+offset+1+12] << 8));
			SWIMUCal.gyro_sensitivity[i] = (int16_t)(buf_[icount+offset+18] | (buf_[icount+offset+1+18] << 8));
			icount = i * 2;
		}
		for(uint8_t i = 0; i < 3; i++) {
			USB_INFO("\t %d, %d, %d, %d\n", SWIMUCal.acc_offset[i], SWIMUCal.acc_sensitivity[i],
				SWIMUCal.gyro_offset[i], SWIMUCal.gyro_sensitivity[i]);
		} 
	} else if((buf_[14] == 0x10 && buf_[15] == 0x80 && buf_[16] == 0x60)) {
		//parse IMU calibration
		USB_INFO("===>  IMU Calibration Offsets \n");	
		for(uint8_t i = 0; i < 3; i++) {
			SWIMUCal.acc_offset[i] = (int16_t)(buf_[i+offset] | (buf_[i+offset+1] << 8));
		}
		for(uint8_t i = 0; i < 3; i++) {
			USB_INFO("\t %d\n", SWIMUCal.acc_offset[i]);
		}
	} else if((buf_[14] == 0x10 && buf_[15] == 0x3D && buf_[16] == 0x60)){		//left stick
		offset = 20;
		data[0] = ((buf_[1+offset] << 8) & 0xF00) | buf_[0+offset];
		data[1] = (buf_[2+offset] << 4) | (buf_[1+offset] >> 4);
		data[2] = ((buf_[4+offset] << 8) & 0xF00) | buf_[3+offset];
		data[3] = (buf_[5+offset] << 4) | (buf_[4+offset] >> 4);
		data[4] = ((buf_[7+offset] << 8) & 0xF00) | buf_[6+offset];
		data[5] = (buf_[8+offset] << 4) | (buf_[7+offset] >> 4);
		
		SWStickCal.lstick_center_x = data[2];
		SWStickCal.lstick_center_y = data[3];
		SWStickCal.lstick_x_min = SWStickCal.lstick_center_x - data[0];
		SWStickCal.lstick_x_max = SWStickCal.lstick_center_x + data[4];
		SWStickCal.lstick_y_min = SWStickCal.lstick_center_y - data[1];
		SWStickCal.lstick_y_max = SWStickCal.lstick_center_y + data[5];
		
		USB_INFO("Left Stick Calibrataion\n");
		USB_INFO("center: %d, %d\n", SWStickCal.lstick_center_x, SWStickCal.lstick_center_y );
		USB_INFO("min/max x: %d, %d\n", SWStickCal.lstick_x_min, SWStickCal.lstick_x_max);
		USB_INFO("min/max y: %d, %d\n", SWStickCal.lstick_y_min, SWStickCal.lstick_y_max);
		
		//right stick
		offset = 29;
		data[0] = ((buf_[1+offset] << 8) & 0xF00) | buf_[0+offset];
		data[1] = (buf_[2+offset] << 4) | (buf_[1+offset] >> 4);
		data[2] = ((buf_[4+offset] << 8) & 0xF00) | buf_[3+offset];
		data[3] = (buf_[5+offset] << 4) | (buf_[4+offset] >> 4);
		data[4] = ((buf_[7+offset] << 8) & 0xF00) | buf_[6+offset];
		data[5] = (buf_[8+offset] << 4) | (buf_[7+offset] >> 4);
		
		SWStickCal.rstick_center_x = data[0];
		SWStickCal.rstick_center_y = data[1];
		SWStickCal.rstick_x_min = SWStickCal.rstick_center_x - data[2];
		SWStickCal.rstick_x_max = SWStickCal.rstick_center_x + data[4];
		SWStickCal.rstick_y_min = SWStickCal.rstick_center_y - data[3];
		SWStickCal.rstick_y_max = SWStickCal.rstick_center_y + data[5];
		
		USB_INFO("\nRight Stick Calibrataion\n");
		USB_INFO("center: %d, %d\n", SWStickCal.rstick_center_x, SWStickCal.rstick_center_y );
		USB_INFO("min/max x: %d, %d\n", SWStickCal.rstick_x_min, SWStickCal.rstick_x_max);
		USB_INFO("min/max y: %d, %d\n", SWStickCal.rstick_y_min, SWStickCal.rstick_y_max);
	}  else if((buf_[14] == 0x10 && buf_[15] == 0x86 && buf_[16] == 0x60)){			//left stick deadzone_left
		offset = 20;
		SWStickCal.deadzone_left = (((buf_[4 + offset] << 8) & 0xF00) | buf_[3 + offset]);
		USB_INFO("\nLeft Stick Deadzone\n");
		USB_INFO("deadzone: %d\n", SWStickCal.deadzone_left);
	}   else if((buf_[14] == 0x10 && buf_[15] == 0x98 && buf_[16] == 0x60)){			//left stick deadzone_left
		offset = 20;
		SWStickCal.deadzone_left = (((buf_[4 + offset] << 8) & 0xF00) | buf_[3 + offset]);
		USB_INFO("\nRight Stick Deadzone\n");
		USB_INFO("deadzone: %d\n", SWStickCal.deadzone_right);
	} else if((buf_[14] == 0x10 && buf_[15] == 0x10 && buf_[16] == 0x80)){
		USB_INFO("\nUser Calibration Rcvd!\n");
	}
    	
}

bool USBHostGamepad::sw_getIMUCalValues(float *accel, float *gyro) 
{
    // Fail if we don't have actually have those fields. We need axis 8-13 for this
    //if ((axis_mask_ & 0x3f00) != 0x3f00) return false;
	for(uint8_t i = 0; i < 3; i++) {
		accel[i] = (float)(axis[8+i] - SWIMUCal.acc_offset[i]) * (1.0f / (float)SWIMUCal.acc_sensitivity[i]) * 4.0f;
		gyro[i]  = (float)(axis[11+i] - SWIMUCal.gyro_offset[i]) * (816.0f / (float)SWIMUCal.gyro_sensitivity[i]);
	}	
    return true;
}


#define sw_scale 2048
void USBHostGamepad::CalcAnalogStick
(
	float &pOutX,       // out: resulting stick X value
	float &pOutY,       // out: resulting stick Y value
	int16_t x,         // in: initial stick X value
	int16_t y,         // in: initial stick Y value
	bool isLeft			// are we dealing with left or right Joystick
)
{
//		uint16_t x_calc[3],      // calc -X, CenterX, +X
//		uint16_t y_calc[3]       // calc -Y, CenterY, +Y

	int16_t min_x;
	int16_t max_x;
	int16_t center_x;
	int16_t min_y;		// analog joystick calibration
	int16_t max_y;
	int16_t center_y;
	if(isLeft) {
		min_x = SWStickCal.lstick_x_min;
		max_x = SWStickCal.lstick_x_max;
		center_x = SWStickCal.lstick_center_x;
		min_y = SWStickCal.lstick_y_min;
		max_y = SWStickCal.lstick_y_max;
		center_y = SWStickCal.lstick_center_y;
	} else {
		min_x = SWStickCal.rstick_x_min;
		max_x = SWStickCal.rstick_x_max;
		center_x = SWStickCal.rstick_center_x;
		min_y = SWStickCal.rstick_y_min;
		max_y = SWStickCal.rstick_y_max;
		center_y = SWStickCal.rstick_center_y;
	}


	float x_f, y_f;
	// Apply Joy-Con center deadzone. 0xAE translates approx to 15%. Pro controller has a 10% () deadzone
	float deadZoneCenter = 0.15f;
	// Add a small ammount of outer deadzone to avoid edge cases or machine variety.
	float deadZoneOuter = 0.0f;

	// convert to float based on calibration and valid ranges per +/-axis
	x = clamp(x, min_x, max_x);
	y = clamp(y, min_y, max_y);
	if (x >= center_x) {
		x_f = (float)(x - center_x) / (float)(max_x - center_x);
	} else {
		x_f = -((float)(x - center_x) / (float)(min_x - center_x));
	}
	if (y >= center_y) {
		y_f = (float)(y - center_y) / (float)(max_y - center_y);
	} else {
		y_f = -((float)(y - center_y) / (float)(min_y - center_y));
	}

	// Interpolate zone between deadzones
	float mag = sqrtf(x_f*x_f + y_f*y_f);
	if (mag > deadZoneCenter) {
		// scale such that output magnitude is in the range [0.0f, 1.0f]
		float legalRange = 1.0f - deadZoneOuter - deadZoneCenter;
		float normalizedMag = min(1.0f, (mag - deadZoneCenter) / legalRange);
		float scale = normalizedMag / mag;
		pOutX = (x_f * scale * sw_scale);
		pOutY = (y_f * scale * sw_scale);
	} else {
		// stick is in the inner dead zone
		pOutX = 0.0f;
		pOutY = 0.0f;
	}
}


bool USBHostGamepad::sw_process_HID_data(const uint8_t *data, uint16_t length)
{
  if(Debug) {
    printf("  SW Joystick Data: ");
    for (uint16_t i = 0; i < length; i++) printf("%02x ", data[i]);
    printf("\r\n");
  }
    if (data[0] == 0x3f) {
        // Assume switch:
        //<<(02 15 21):48 20 11 00 0D 00 71 00 A1 
        // 16 bits buttons
        // 4 bits hat
        // 4 bits <constant>
        // 16 bits X
        // 16 bits Y
        // 16 bits rx
        // 16 bits ry
        typedef struct __attribute__ ((packed)) {
            uint8_t report_type; // 1
            uint16_t buttons;
            uint8_t hat;
            int16_t axis[4];
            // From online references button order:
            //     sync, dummy, start, back, a, b, x, y
            //     dpad up, down left, right
            //     lb, rb, left stick, right stick
            // Axis:
            //     lt, rt, lx, ly, rx, ry
            //
        } switchbt_t;

        static const uint8_t switch_bt_axis_order_mapping[] = { 0, 1, 2, 3};
        //axis_mask_ = 0x1ff;
        //axis_changed_mask_ = 0; // assume none for now
        
        switchbt_t *sw1d = (switchbt_t *)data;
        // We have a data transfer.  Lets see what is new...
        if (sw1d->buttons != buttons) {
            buttons = sw1d->buttons;
            //anychange = true;
            joystickEvent = true;
            //USB_INFO("  Button Change: ", buttons, HEX);
        }
        // We will put the HAT into axis 9 for now..
        if (sw1d->hat != axis[9]) {
            axis[9] = sw1d->hat;
            //axis_changed_mask_ |= (1 << 9);
            //anychange = true;            
        }
        
        //just a hack for a single joycon.
        if(buttons == 0x8000) { //ZL
            axis[6] = 1;
        } else {
            axis[6] = 0;
        }
        if(buttons == 0x8000) {     //ZR
            axis[7] = 1;
        } else {
            axis[7] = 0;
        }
        
        
        for (uint8_t i = 0; i < sizeof (switch_bt_axis_order_mapping); i++) {
            // The first two values were unsigned.
            int axis_value = (uint16_t)sw1d->axis[i];

            //DBGPrintf(" axis value [ %d ] = %d \n", i, axis_value);
            
            if (axis_value != axis[switch_bt_axis_order_mapping[i]]) {
                axis[switch_bt_axis_order_mapping[i]] = axis_value;
                //axis_changed_mask_ |= (1 << switch_bt_axis_order_mapping[i]);
                //anychange = true;
            }
        }

        joystickEvent = true;

    } else if (data[0] == 0x30) {
        // Assume switch full report
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48
        // 30 E0 80 00 00 00 D9 37 79 19 98 70 00 0D 0B F1 02 F0 0A 41 FE 25 FC 89 00 F8 0A F0 02 F2 0A 41 FE D9 FB 99 00 D4 0A F6 02 FC 0A 3C FE 69 FB B8 00 
        //<<(02 15 21):48 20 11 00 0D 00 71 00 A1 
        static const uint8_t switch_bt_axis_order_mapping[] = { 0, 1, 2, 3};
        //axis_mask_ = 0x7fff;  // have all of the fields. 
        //axis_changed_mask_ = 0; // assume none for now
        // We have a data transfer.  Lets see what is new...
        uint32_t cur_buttons = data[3] | (data[4] << 8) | (data[5] << 16);

        //DBGPrintf("BUTTONS: %x\n", cur_buttons);
        if(initialPassButton_ == true) {
            if(cur_buttons == 0x8000) {
                buttonOffset_ = 0x8000;
            } else {
                buttonOffset_ = 0;
            }
            initialPassButton_ = false;
        }
        
        cur_buttons = cur_buttons - buttonOffset_;
        //Serial.printf("Buttons (3,4,5): %x, %x, %x, %x, %x, %x\n", buttonOffset_, cur_buttons, buttons, data[3], data[4], data[5]);

        if (cur_buttons != buttons) {
            buttons = cur_buttons;
            //anychange = true;
            joystickEvent = true;
            //USB_INFO("  Button Change: ", buttons, HEX);
        }
        // We will put the HAT into axis 9 for now..
        /*
        if (sw1d->hat != axis[9]) {
            axis[9] = sw1d->hat;
            axis_changed_mask_ |= (1 << 9);
            anychange = true;            
        }
        */

        uint16_t new_axis[14];
        //Joystick data
        new_axis[0] = data[6] | ((data[7] & 0xF) << 8);   //xl
        new_axis[1] = (data[7] >> 4) | (data[8] << 4);    //yl
        new_axis[2] = data[9] | ((data[10] & 0xF) << 8);  //xr
        new_axis[3] = (data[10] >> 4) | (data[11] << 4);  //yr

        //Kludge to get trigger buttons tripping
        if(buttons == 0x40) {   //R1
            new_axis[5] = 1;
        } else {
            new_axis[5] = 0;
        }
        if(buttons == 0x400000) {   //L1
            new_axis[4] = 1;
        } else {
            new_axis[4] = 0;
        }
        if(buttons == 0x400040) {
            new_axis[4] = 0xff;
            new_axis[5] = 0xff;
        }
        if(buttons == 0x800000) {   //ZL
            new_axis[6] = 0xff;
        } else {
            new_axis[6] = 0;
        }
        if(buttons == 0x80) {       //ZR
            new_axis[7] = 0xff;
        } else {
            new_axis[7] = 0;
        }
        if(buttons == 0x800080) {
            new_axis[6] = 0xff;
            new_axis[7] = 0xff;
        }
        
        sw_update_axis(8, (int16_t)(data[13]  | (data[14] << 8))); //ax
        sw_update_axis(9, (int16_t)(data[15]  | (data[16] << 8))); //ay
        sw_update_axis(10,  (int16_t)(data[17] | (data[18] << 8))); //az
        sw_update_axis(11,  (int16_t)(data[19] | (data[20] << 8)));  //gx
        sw_update_axis(12,  (int16_t)(data[21] | (data[22] << 8))); //gy
        sw_update_axis(13,  (int16_t)(data[23] | (data[24] << 8))); //gz  
        
        sw_update_axis(14,  data[2] >> 4);  //Battery level, 8=full, 6=medium, 4=low, 2=critical, 0=empty

        //map axes
        for (uint8_t i = 0; i < 8; i++) {
            // The first two values were unsigned.
            if (new_axis[i] != axis[i]) {
                axis[i] = new_axis[i];
                //axis_changed_mask_ |= (1 << i);
                //anychange = true;
            }
        }
        
		//apply stick calibration
		float xout, yout;
		CalcAnalogStick(xout, yout, new_axis[0], new_axis[1], true);
		//Serial.printf("Correctd Left Stick: %f, %f\n", xout , yout);
		axis[0] = int(round(xout));
		axis[1] = int(round(yout));
		
		CalcAnalogStick(xout, yout, new_axis[2], new_axis[3], true);
		axis[2] = int(round(xout));
		axis[3] = int(round(yout));
		
        joystickEvent = true;
        initialPass_ = false;
        
    }
    return false;
}

void USBHostGamepad::sw_update_axis(uint8_t axis_index, int new_value)
{
    if (axis[axis_index] != new_value) {
        axis[axis_index] = new_value;
        //anychange = true;
        //axis_changed_mask_ |= (1 << axis_index);
    }
}


bool USBHostGamepad::manufacturer(uint8_t *buffer, size_t len) {
  cacheStringIndexes();
  return getStringDesc(iManufacturer_, buffer, len);
}

bool USBHostGamepad::product(uint8_t *buffer, size_t len){
  cacheStringIndexes();
  return getStringDesc(iProduct_, buffer, len);
}

bool USBHostGamepad::serialNumber(uint8_t *buffer, size_t len){
  cacheStringIndexes();
  return getStringDesc(iSerialNumber_, buffer, len);
}


#define STRING_DESCRIPTOR  (3)

bool USBHostGamepad::cacheStringIndexes() {
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


bool USBHostGamepad::getStringDesc(uint8_t index, uint8_t *buffer, size_t len) {

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

#endif