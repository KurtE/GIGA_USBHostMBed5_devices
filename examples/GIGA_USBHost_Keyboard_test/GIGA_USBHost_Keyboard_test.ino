
/*
  USBHost Keyboard test


  The circuit:
   - Arduino GIGA

  This example code is in the public domain.
*/

#include <Arduino_USBHostMbed5.h>
#include "USBHostKeyboardEx.h"
REDIRECT_STDOUT_TO(Serial)

USBHostKeyboardEx kbd;

// If you are using a Portenta Machine Control uncomment the following line
// mbed::DigitalOut otg(PB_14, 0);

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 5000) {}

    Serial.println("Starting Keyboard test...");

    // Enable the USBHost 
    pinMode(PA_15, OUTPUT);
    digitalWrite(PA_15, HIGH);

    // if you are using a Max Carrier uncomment the following line
    // start_hub();

    while (!kbd.connect()) {
      Serial.println("No keyboard connected");        
        delay(5000);
    }
  
  printf("\nKeyboard (%x:%x): connected\n", kbd.idVendor(), kbd.idProduct());

  uint8_t string_buffer[80];
  if (kbd.manufacturer(string_buffer, sizeof(string_buffer))) {
    Serial.print("Manufacturer: ");
    Serial.println((char*)string_buffer);
  }

  if (kbd.product(string_buffer, sizeof(string_buffer))) {
    Serial.print("Product: ");
    Serial.println((char*)string_buffer);
  }
  if (kbd.serialNumber(string_buffer, sizeof(string_buffer))) {
    Serial.print("Serial Number: ");
    Serial.println((char*)string_buffer);
  }

    kbd.attachPress(&kbd_key_cb);
    kbd.attachRawPress(&kbd_keycode_cb);
    kbd.attachRelease(&kbd_key_release_cb);
    kbd.attachHIDPress(&kbd_hid_key_press_cb);
    kbd.attachHIDRelease(&kbd_hid_key_release_cb);
    Serial.println("End of Setup");
}

void loop()
{
    delay(1000);
}

void kbd_key_cb(uint8_t key) {
  Serial.print("Key pressed: ");
  Serial.print(key, HEX);
  Serial.print("(");
  if ((key >= ' ') && (key <= '~')) Serial.write(key);
  Serial.println(")");
}

void kbd_key_release_cb(uint8_t key) {
  Serial.print("Key released: ");
  Serial.print(key, HEX);
  Serial.print("(");
  if ((key >= ' ') && (key <= '~')) Serial.write(key);
  Serial.println(")");
}


void kbd_keycode_cb(uint8_t keycode, uint8_t mod) {
  Serial.print("Keycode: ");
  Serial.print(keycode, HEX);
  Serial.print(" mod: ");
  Serial.println(mod, HEX);
}

void kbd_hid_key_press_cb(uint32_t top, uint16_t code) {
  Serial.print("Press: HID Key: Page:0x");
  print_hid_data(top, code);
}  

void kbd_hid_key_release_cb(uint32_t top, uint16_t code) {
  Serial.print("Release: HID Key: Page:0x");
  print_hid_data(top, code);
}  


void print_hid_data(uint32_t top, uint32_t code) {
  Serial.print(top, HEX);
  Serial.print(" Code: 0x");
  Serial.print(code, HEX);
  if (top == 0x0c) {
    switch (code) {
      case  0x20 : Serial.println(" - +10"); return;
      case  0x21 : Serial.println(" - +100"); return;
      case  0x22 : Serial.println(" - AM/PM"); return;
      case  0x23 : Serial.println(" - Home"); return;
      case  0x30 : Serial.println(" - Power"); return;
      case  0x31 : Serial.println(" - Reset"); return;
      case  0x32 : Serial.println(" - Sleep"); return;
      case  0x33 : Serial.println(" - Sleep After"); return;
      case  0x34 : Serial.println(" - Sleep Mode"); return;
      case  0x35 : Serial.println(" - Illumination"); return;
      case  0x36 : Serial.println(" - Function Buttons"); return;
      case  0x40 : Serial.println(" - Menu"); return;
      case  0x41 : Serial.println(" - Menu  Pick"); return;
      case  0x42 : Serial.println(" - Menu Up"); return;
      case  0x43 : Serial.println(" - Menu Down"); return;
      case  0x44 : Serial.println(" - Menu Left"); return;
      case  0x45 : Serial.println(" - Menu Right"); return;
      case  0x46 : Serial.println(" - Menu Escape"); return;
      case  0x47 : Serial.println(" - Menu Value Increase"); return;
      case  0x48 : Serial.println(" - Menu Value Decrease"); return;
      case  0x60 : Serial.println(" - Data On Screen"); return;
      case  0x61 : Serial.println(" - Closed Caption"); return;
      case  0x62 : Serial.println(" - Closed Caption Select"); return;
      case  0x63 : Serial.println(" - VCR/TV"); return;
      case  0x64 : Serial.println(" - Broadcast Mode"); return;
      case  0x65 : Serial.println(" - Snapshot"); return;
      case  0x66 : Serial.println(" - Still"); return;
      case  0x80 : Serial.println(" - Selection"); return;
      case  0x81 : Serial.println(" - Assign Selection"); return;
      case  0x82 : Serial.println(" - Mode Step"); return;
      case  0x83 : Serial.println(" - Recall Last"); return;
      case  0x84 : Serial.println(" - Enter Channel"); return;
      case  0x85 : Serial.println(" - Order Movie"); return;
      case  0x86 : Serial.println(" - Channel"); return;
      case  0x87 : Serial.println(" - Media Selection"); return;
      case  0x88 : Serial.println(" - Media Select Computer"); return;
      case  0x89 : Serial.println(" - Media Select TV"); return;
      case  0x8A : Serial.println(" - Media Select WWW"); return;
      case  0x8B : Serial.println(" - Media Select DVD"); return;
      case  0x8C : Serial.println(" - Media Select Telephone"); return;
      case  0x8D : Serial.println(" - Media Select Program Guide"); return;
      case  0x8E : Serial.println(" - Media Select Video Phone"); return;
      case  0x8F : Serial.println(" - Media Select Games"); return;
      case  0x90 : Serial.println(" - Media Select Messages"); return;
      case  0x91 : Serial.println(" - Media Select CD"); return;
      case  0x92 : Serial.println(" - Media Select VCR"); return;
      case  0x93 : Serial.println(" - Media Select Tuner"); return;
      case  0x94 : Serial.println(" - Quit"); return;
      case  0x95 : Serial.println(" - Help"); return;
      case  0x96 : Serial.println(" - Media Select Tape"); return;
      case  0x97 : Serial.println(" - Media Select Cable"); return;
      case  0x98 : Serial.println(" - Media Select Satellite"); return;
      case  0x99 : Serial.println(" - Media Select Security"); return;
      case  0x9A : Serial.println(" - Media Select Home"); return;
      case  0x9B : Serial.println(" - Media Select Call"); return;
      case  0x9C : Serial.println(" - Channel Increment"); return;
      case  0x9D : Serial.println(" - Channel Decrement"); return;
      case  0x9E : Serial.println(" - Media Select SAP"); return;
      case  0xA0 : Serial.println(" - VCR Plus"); return;
      case  0xA1 : Serial.println(" - Once"); return;
      case  0xA2 : Serial.println(" - Daily"); return;
      case  0xA3 : Serial.println(" - Weekly"); return;
      case  0xA4 : Serial.println(" - Monthly"); return;
      case  0xB0 : Serial.println(" - Play"); return;
      case  0xB1 : Serial.println(" - Pause"); return;
      case  0xB2 : Serial.println(" - Record"); return;
      case  0xB3 : Serial.println(" - Fast Forward"); return;
      case  0xB4 : Serial.println(" - Rewind"); return;
      case  0xB5 : Serial.println(" - Scan Next Track"); return;
      case  0xB6 : Serial.println(" - Scan Previous Track"); return;
      case  0xB7 : Serial.println(" - Stop"); return;
      case  0xB8 : Serial.println(" - Eject"); return;
      case  0xB9 : Serial.println(" - Random Play"); return;
      case  0xBA : Serial.println(" - Select DisC"); return;
      case  0xBB : Serial.println(" - Enter Disc"); return;
      case  0xBC : Serial.println(" - Repeat"); return;
      case  0xBD : Serial.println(" - Tracking"); return;
      case  0xBE : Serial.println(" - Track Normal"); return;
      case  0xBF : Serial.println(" - Slow Tracking"); return;
      case  0xC0 : Serial.println(" - Frame Forward"); return;
      case  0xC1 : Serial.println(" - Frame Back"); return;
      case  0xC2 : Serial.println(" - Mark"); return;
      case  0xC3 : Serial.println(" - Clear Mark"); return;
      case  0xC4 : Serial.println(" - Repeat From Mark"); return;
      case  0xC5 : Serial.println(" - Return To Mark"); return;
      case  0xC6 : Serial.println(" - Search Mark Forward"); return;
      case  0xC7 : Serial.println(" - Search Mark Backwards"); return;
      case  0xC8 : Serial.println(" - Counter Reset"); return;
      case  0xC9 : Serial.println(" - Show Counter"); return;
      case  0xCA : Serial.println(" - Tracking Increment"); return;
      case  0xCB : Serial.println(" - Tracking Decrement"); return;
      case  0xCD : Serial.println(" - Pause/Continue"); return;
      case  0xE0 : Serial.println(" - Volume"); return;
      case  0xE1 : Serial.println(" - Balance"); return;
      case  0xE2 : Serial.println(" - Mute"); return;
      case  0xE3 : Serial.println(" - Bass"); return;
      case  0xE4 : Serial.println(" - Treble"); return;
      case  0xE5 : Serial.println(" - Bass Boost"); return;
      case  0xE6 : Serial.println(" - Surround Mode"); return;
      case  0xE7 : Serial.println(" - Loudness"); return;
      case  0xE8 : Serial.println(" - MPX"); return;
      case  0xE9 : Serial.println(" - Volume Up"); return;
      case  0xEA : Serial.println(" - Volume Down"); return;
      case  0xF0 : Serial.println(" - Speed Select"); return;
      case  0xF1 : Serial.println(" - Playback Speed"); return;
      case  0xF2 : Serial.println(" - Standard Play"); return;
      case  0xF3 : Serial.println(" - Long Play"); return;
      case  0xF4 : Serial.println(" - Extended Play"); return;
      case  0xF5 : Serial.println(" - Slow"); return;
      case  0x100: Serial.println(" - Fan Enable"); return;
      case  0x101: Serial.println(" - Fan Speed"); return;
      case  0x102: Serial.println(" - Light"); return;
      case  0x103: Serial.println(" - Light Illumination Level"); return;
      case  0x104: Serial.println(" - Climate Control Enable"); return;
      case  0x105: Serial.println(" - Room Temperature"); return;
      case  0x106: Serial.println(" - Security Enable"); return;
      case  0x107: Serial.println(" - Fire Alarm"); return;
      case  0x108: Serial.println(" - Police Alarm"); return;
      case  0x150: Serial.println(" - Balance Right"); return;
      case  0x151: Serial.println(" - Balance Left"); return;
      case  0x152: Serial.println(" - Bass Increment"); return;
      case  0x153: Serial.println(" - Bass Decrement"); return;
      case  0x154: Serial.println(" - Treble Increment"); return;
      case  0x155: Serial.println(" - Treble Decrement"); return;
      case  0x160: Serial.println(" - Speaker System"); return;
      case  0x161: Serial.println(" - Channel Left"); return;
      case  0x162: Serial.println(" - Channel Right"); return;
      case  0x163: Serial.println(" - Channel Center"); return;
      case  0x164: Serial.println(" - Channel Front"); return;
      case  0x165: Serial.println(" - Channel Center Front"); return;
      case  0x166: Serial.println(" - Channel Side"); return;
      case  0x167: Serial.println(" - Channel Surround"); return;
      case  0x168: Serial.println(" - Channel Low Frequency Enhancement"); return;
      case  0x169: Serial.println(" - Channel Top"); return;
      case  0x16A: Serial.println(" - Channel Unknown"); return;
      case  0x170: Serial.println(" - Sub-channel"); return;
      case  0x171: Serial.println(" - Sub-channel Increment"); return;
      case  0x172: Serial.println(" - Sub-channel Decrement"); return;
      case  0x173: Serial.println(" - Alternate Audio Increment"); return;
      case  0x174: Serial.println(" - Alternate Audio Decrement"); return;
      case  0x180: Serial.println(" - Application Launch Buttons"); return;
      case  0x181: Serial.println(" - AL Launch Button Configuration Tool"); return;
      case  0x182: Serial.println(" - AL Programmable Button Configuration"); return;
      case  0x183: Serial.println(" - AL Consumer Control Configuration"); return;
      case  0x184: Serial.println(" - AL Word Processor"); return;
      case  0x185: Serial.println(" - AL Text Editor"); return;
      case  0x186: Serial.println(" - AL Spreadsheet"); return;
      case  0x187: Serial.println(" - AL Graphics Editor"); return;
      case  0x188: Serial.println(" - AL Presentation App"); return;
      case  0x189: Serial.println(" - AL Database App"); return;
      case  0x18A: Serial.println(" - AL Email Reader"); return;
      case  0x18B: Serial.println(" - AL Newsreader"); return;
      case  0x18C: Serial.println(" - AL Voicemail"); return;
      case  0x18D: Serial.println(" - AL Contacts/Address Book"); return;
      case  0x18E: Serial.println(" - AL Calendar/Schedule"); return;
      case  0x18F: Serial.println(" - AL Task/Project Manager"); return;
      case  0x190: Serial.println(" - AL Log/Journal/Timecard"); return;
      case  0x191: Serial.println(" - AL Checkbook/Finance"); return;
      case  0x192: Serial.println(" - AL Calculator"); return;
      case  0x193: Serial.println(" - AL A/V Capture/Playback"); return;
      case  0x194: Serial.println(" - AL Local Machine Browser"); return;
      case  0x195: Serial.println(" - AL LAN/WAN Browser"); return;
      case  0x196: Serial.println(" - AL Internet Browser"); return;
      case  0x197: Serial.println(" - AL Remote Networking/ISP Connect"); return;
      case  0x198: Serial.println(" - AL Network Conference"); return;
      case  0x199: Serial.println(" - AL Network Chat"); return;
      case  0x19A: Serial.println(" - AL Telephony/Dialer"); return;
      case  0x19B: Serial.println(" - AL Logon"); return;
      case  0x19C: Serial.println(" - AL Logoff"); return;
      case  0x19D: Serial.println(" - AL Logon/Logoff"); return;
      case  0x19E: Serial.println(" - AL Terminal Lock/Screensaver"); return;
      case  0x19F: Serial.println(" - AL Control Panel"); return;
      case  0x1A0: Serial.println(" - AL Command Line Processor/Run"); return;
      case  0x1A1: Serial.println(" - AL Process/Task Manager"); return;
      case  0x1A2: Serial.println(" - AL Select Tast/Application"); return;
      case  0x1A3: Serial.println(" - AL Next Task/Application"); return;
      case  0x1A4: Serial.println(" - AL Previous Task/Application"); return;
      case  0x1A5: Serial.println(" - AL Preemptive Halt Task/Application"); return;
      case  0x200: Serial.println(" - Generic GUI Application Controls"); return;
      case  0x201: Serial.println(" - AC New"); return;
      case  0x202: Serial.println(" - AC Open"); return;
      case  0x203: Serial.println(" - AC Close"); return;
      case  0x204: Serial.println(" - AC Exit"); return;
      case  0x205: Serial.println(" - AC Maximize"); return;
      case  0x206: Serial.println(" - AC Minimize"); return;
      case  0x207: Serial.println(" - AC Save"); return;
      case  0x208: Serial.println(" - AC Print"); return;
      case  0x209: Serial.println(" - AC Properties"); return;
      case  0x21A: Serial.println(" - AC Undo"); return;
      case  0x21B: Serial.println(" - AC Copy"); return;
      case  0x21C: Serial.println(" - AC Cut"); return;
      case  0x21D: Serial.println(" - AC Paste"); return;
      case  0x21E: Serial.println(" - AC Select All"); return;
      case  0x21F: Serial.println(" - AC Find"); return;
      case  0x220: Serial.println(" - AC Find and Replace"); return;
      case  0x221: Serial.println(" - AC Search"); return;
      case  0x222: Serial.println(" - AC Go To"); return;
      case  0x223: Serial.println(" - AC Home"); return;
      case  0x224: Serial.println(" - AC Back"); return;
      case  0x225: Serial.println(" - AC Forward"); return;
      case  0x226: Serial.println(" - AC Stop"); return;
      case  0x227: Serial.println(" - AC Refresh"); return;
      case  0x228: Serial.println(" - AC Previous Link"); return;
      case  0x229: Serial.println(" - AC Next Link"); return;
      case  0x22A: Serial.println(" - AC Bookmarks"); return;
      case  0x22B: Serial.println(" - AC History"); return;
      case  0x22C: Serial.println(" - AC Subscriptions"); return;
      case  0x22D: Serial.println(" - AC Zoom In"); return;
      case  0x22E: Serial.println(" - AC Zoom Out"); return;
      case  0x22F: Serial.println(" - AC Zoom"); return;
      case  0x230: Serial.println(" - AC Full Screen View"); return;
      case  0x231: Serial.println(" - AC Normal View"); return;
      case  0x232: Serial.println(" - AC View Toggle"); return;
      case  0x233: Serial.println(" - AC Scroll Up"); return;
      case  0x234: Serial.println(" - AC Scroll Down"); return;
      case  0x235: Serial.println(" - AC Scroll"); return;
      case  0x236: Serial.println(" - AC Pan Left"); return;
      case  0x237: Serial.println(" - AC Pan Right"); return;
      case  0x238: Serial.println(" - AC Pan"); return;
      case  0x239: Serial.println(" - AC New Window"); return;
      case  0x23A: Serial.println(" - AC Tile Horizontally"); return;
      case  0x23B: Serial.println(" - AC Tile Vertically"); return;
      case  0x23C: Serial.println(" - AC Format"); return;
    }
  }
  Serial.println();
}
