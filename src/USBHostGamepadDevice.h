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

#ifndef USBHOSTGAMEPAD_H
#define USBHOSTGAMEPAD_H_H

#include <Arduino_USBHostMbed5.h>
#include "USBHost/USBHost.h"
#include "elapsedMillis.h"

#define USBHOST_GAMEPAD 1

#if USBHOST_GAMEPAD

#include "USBHost/USBHostConf.h"

/**
 * A class to communicate a USB Joystick
 */
class USBHostGamepad : public IUSBEnumerator
{
public:
    /**
    * Constructor
    */
    USBHostGamepad();

    /**
     * Try to connect a Joystick device
     *
     * @return true if connection was successful
     */
    bool connect();
    void disconnect();
    /**
    * Check if a Joystick is connected
    *
    * @returns true if a Joystick is connected
    */
    bool connected();

    /**
     * Attach a callback called when a Joystick event is received
     *
     * @param ptr function pointer
     */
    inline void attachEvent(void (*ptr)(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry))
    {
        if (ptr != NULL) {
            onUpdate = ptr;
        }
    }

//----------------------------------------------------------------------------------------
    uint16_t idVendor() { return (dev != nullptr) ? dev->getVid() : 0; }
    uint16_t idProduct() { return (dev != nullptr) ? dev->getPid() : 0; }
    bool manufacturer(uint8_t *buffer, size_t len);
    bool product(uint8_t *buffer, size_t len);
    bool serialNumber(uint8_t *buffer, size_t len);

    /* ------------------------------------------------------------ */
    // Returns the currently pressed buttons on the joystick
    uint32_t getButtons() { return buttons; }

    // Returns the HID Report ID
    uint8_t getReportID() { return report_id_;}

    bool    available() { return joystickEvent; }
    // Returns the specified axis value
    int     getAxis(uint32_t index) { return (index < (sizeof(axis) / sizeof(axis[0]))) ? axis[index] : 0; }
    // set functions functionality depends on underlying joystick.
    bool setRumble(uint8_t lValue, uint8_t rValue, uint8_t timeout = 0xff);
    // setLEDs on PS4(RGB), PS3 simple LED setting (only uses lb)
    bool setLEDs(uint8_t lr, uint8_t lg, uint8_t lb);  // sets Leds,
	  bool sw_getIMUCalValues(float *accel, float *gyro);

    enum { STANDARD_AXIS_COUNT = 10, ADDITIONAL_AXIS_COUNT = 54, TOTAL_AXIS_COUNT = (STANDARD_AXIS_COUNT + ADDITIONAL_AXIS_COUNT) };
    // Mapping table to say which devices we handle
    typedef enum { UNKNOWN = 0, PS3, PS4, XBOXONE, XBOX360, PS3_MOTION, SpaceNav, SWITCH, NES, LogiExtreme3DPro} joytype_t;
    joytype_t mapVIDPIDtoGamepadType(uint16_t idVendor, uint16_t idProduct, bool exclude_hid_devices);
    joytype_t gamepadType_ = UNKNOWN;
    joytype_t gamepadType() {return gamepadType_;}


protected:
    //From IUSBEnumerator
    virtual void setVidPid(uint16_t vid, uint16_t pid);
    virtual bool parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol); //Must return true if the interface should be parsed
    virtual bool useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir); //Must return true if the endpoint will be used
    
    bool process_HID_data(const uint8_t *data, uint16_t length);
    
private:
    USBHost * host;
    USBDeviceConnected * dev;
    USBEndpoint * int_in;
    USBEndpoint * int_out;
    INTERFACE  * test;

    uint8_t report[256];
    bool dev_connected;
    bool gamepad_device_found;
    int gamepad_intf;
    uint8_t nb_ep;
    void rxHandler();
    void txHandler();
    void (*onUpdate)(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry );
    int report_id_;
    void init();
    
    volatile bool joystickEvent = false;

    bool transmitPS4UserFeedbackMsg();
    bool transmitPS3UserFeedbackMsg();
    bool transmitPS3MotionUserFeedbackMsg();
    bool sendMessage(uint8_t * buffer, uint16_t length); 
    void sw_sendCmdUSB(uint8_t cmd, uint32_t timeout);
    void sw_sendSubCmdUSB(uint8_t sub_cmd, uint8_t *data, uint8_t size, uint32_t timeout = 0);
    void sw_parseAckMsg(const uint8_t *buf_);
    bool sw_usb_init(uint8_t *buffer, uint16_t cb, bool timer_event);
    bool sw_handle_bt_init_of_joystick(const uint8_t *data, uint16_t length, bool timer_event);
    inline void sw_update_axis(uint8_t axis_index, int new_value);
    bool sw_process_HID_data(const uint8_t *data, uint16_t length);
    void CalcAnalogStick(float &pOutX, float &pOutY, int16_t x, int16_t y, bool isLeft);
    bool processMessages(uint8_t * buffer, uint16_t length);
    
	//kludge for switch having different button values
	bool initialPass_ = true;
	bool initialPassButton_ = true;
	bool initialPassBT_ = true;
	uint32_t buttonOffset_ = 0x00;
	uint8_t connectedComplete_pending_ = 0;
	uint8_t sw_last_cmd_sent_ = 0;
	uint8_t sw_last_cmd_repeat_count = 0;
	enum {SW_CMD_TIMEOUT = 250000};
  
	// State values to output to Joystick.
	uint8_t rumble_lValue_ = 0;
	uint8_t rumble_rValue_ = 0;
	uint8_t rumble_timeout_ = 0;
	uint8_t leds_[3] = {0, 0, 0};
	uint32_t buttons = 0;
  
	uint8_t         rxbuf_[64]; // receive circular buffer
	uint8_t         txbuf_[64];     // buffer to use to send commands to joystick

  // String indexes 
  uint8_t iManufacturer_ = 0xff;
  uint8_t iProduct_ = 0xff;
  uint8_t iSerialNumber_ = 0xff;
  uint16_t wLanguageID_ = 0x409; // english US

	elapsedMicros em_sw_;
  
	int axis[TOTAL_AXIS_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  
	typedef struct {
      uint16_t    idVendor;
      uint16_t    idProduct;
      joytype_t   joyType;
      bool        hidDevice;
	} product_vendor_mapping_t;
	static product_vendor_mapping_t pid_vid_mapping[];

  bool getStringDesc(uint8_t index, uint8_t *buffer, size_t len);
  bool cacheStringIndexes();

};

#endif  //end Gamepad define

#endif  //end class define