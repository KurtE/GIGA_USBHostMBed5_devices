// A lot of the HID Parser code came from the USBHost_t35
// Some parts not...
/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "USBHostHIDParser.h"

//#define DEBUG_HID_PARSER
//#define DEBUG_HID_PARSER_VERBOSE

#ifndef DEBUG_HID_PARSER
#undef DEBUG_HID_PARSER_VERBOSE
void inline DBGPrintf(...) {};
#else
#define DBGPrintf printf
#endif

#ifndef DEBUG_HID_PARSER_VERBOSE
void inline VDBGPrintf(...) {};
#else
#define VDBGPrintf printf
#include <MemoryHexDump.h>
#endif



// Lets try to read in the HID Descriptor
bool USBHostHIDParser::init(USBHost *host, USBDeviceConnected *dev, uint8_t index, uint16_t len) {
  host_ = host;
  dev_ = dev;
  index_ = index;
  descriptor_length_ = len;

  DBGPrintf("USBHostHIDParser::init(%p %p %u %u)\n", host, dev, index, len);
  if ((dev == nullptr) || (index == 0xff)) return false;

  if (descriptor_buffer_ != nullptr) {
    free((void *)descriptor_buffer_);
  }

  descriptor_buffer_ = (uint8_t *)malloc(len);
  if (!descriptor_buffer_) return false;

  if (!getHIDDescriptor()) return false;

  // lets do an initial walk through to see if the HID contains report ids...
  const uint8_t *p = descriptor_buffer_;
  const uint8_t *end = p + descriptor_length_;
  uint16_t usage_page = 0;
  uint16_t usage = 0;
  uint8_t collection_level = 0;
  //uint8_t topusage_count = 0;

  use_report_id_ = false;
  while (p < end) {
    uint8_t tag = *p;
    if (tag == 0xFE) {  // Long Item
      p += *p + 3;
      continue;
    }
    uint32_t val = 0;
    switch (tag & 0x03) {  // Short Item data
      case 0:
        val = 0;
        p++;
        break;
      case 1:
        val = p[1];
        p += 2;
        break;
      case 2:
        val = p[1] | (p[2] << 8);
        p += 3;
        break;
      case 3:
        val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
        p += 5;
        break;
    }
    if (p > end) break;

    switch (tag & 0xFC) {
      case 0x84:  // Report ID (global)
        use_report_id_ = true;
        DBGPrintf("  Report ID: %lu\n", val);
        break;
      case 0x04:  // Usage Page (global)
        usage_page = val;
        break;
      case 0x08:  // Usage (local)
        usage = val;
        break;
      case 0xA0:  // Collection
        if (collection_level == 0 /*&& topusage_count < TOPUSAGE_LIST_LEN*/) {
          uint32_t topusage = ((uint32_t)usage_page << 16) | usage;
          DBGPrintf("Found top level collection %lx\n", topusage);
          //topusage_list[topusage_count] = topusage;
          //topusage_drivers[topusage_count] = find_driver(topusage);
          //topusage_count++;
        }
        collection_level++;
        usage = 0;
        break;
      case 0xC0:  // End Collection
        if (collection_level > 0) {
          collection_level--;
        }
      case 0x80:  // Input
      case 0x90:  // Output
      case 0xB0:  // Feature
        usage = 0;
        break;
    }
  }
}

// Extract 1 to 32 bits from the data array, starting at bitindex.
static uint32_t bitfield(const uint8_t *data, uint32_t bitindex, uint32_t numbits)
{
	uint32_t output = 0;
	uint32_t bitcount = 0;
	data += (bitindex >> 3);
	uint32_t offset = bitindex & 7;
	if (offset) {
		output = (*data++) >> offset;
		bitcount = 8 - offset;
	}
	while (bitcount < numbits) {
		output |= (uint32_t)(*data++) << bitcount;
		bitcount += 8;
	}
	if (bitcount > numbits && numbits < 32) {
		output &= ((1 << numbits) - 1);
	}
	return output;
}

// convert a number with the specified number of bits from unsigned to signed,
// so the result is a proper 32 bit signed integer.
static int32_t signext(uint32_t num, uint32_t bitcount)
{
	if (bitcount < 32 && bitcount > 0 && (num & (1 << (bitcount-1)))) {
		num |= ~((1 << bitcount) - 1);
	}
	return (int32_t)num;
}

// convert a tag's value to a signed integer.
static int32_t signedval(uint32_t num, uint8_t tag)
{
	tag &= 3;
	if (tag == 1) return (int8_t)num;
	if (tag == 2) return (int16_t)num;
	return (int32_t)num;
}


void USBHostHIDParser::parse( const uint8_t *data, uint16_t len)
{
  const uint8_t *p = descriptor_buffer_;
  const uint8_t *end = p + descriptor_length_;

	uint32_t topusage = 0;
	uint8_t topusage_index = 0;
	uint8_t collection_level = 0;
	uint16_t usage[USAGE_LIST_LEN] = {0, 0};
	uint8_t usage_count = 0;
	uint8_t usage_min_max_count = 0;
	uint8_t usage_min_max_mask = 0;
	uint8_t report_id = 0;
	uint16_t report_size = 0;
	uint16_t report_count = 0;
	uint16_t usage_page = 0;
	uint32_t last_usage = 0;
	int32_t logical_min = 0;
	int32_t logical_max = 0;
	uint32_t bitindex = 0;
  uint8_t buffer_report_id = 0;
  
  if (use_report_id_) {
    buffer_report_id = *data++;
    len--;
  }

  DBGPrintf("parse: %p %u: %u %u\n", data, len, use_report_id_, buffer_report_id);
	while (p < end) {
		uint8_t tag = *p;
		if (tag == 0xFE) { // Long Item (unsupported)
			p += p[1] + 3;
			continue;
		}
		uint32_t val = 0;
		switch (tag & 0x03) { // Short Item data
		  case 0: val = 0;
			p++;
			break;
		  case 1: val = p[1];
			p += 2;
			break;
		  case 2: val = p[1] | (p[2] << 8);
			p += 3;
			break;
		  case 3: val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
			p += 5;
			break;
		}
		if (p > end) break;
		bool reset_local = false;
    VDBGPrintf("  T:%x V:%lu\n", tag, val);
		switch (tag & 0xFC) {
		  case 0x04: // Usage Page (global)
			usage_page = val;
			break;
		  case 0x14: // Logical Minimum (global)
			logical_min = signedval(val, tag);
			break;
		  case 0x24: // Logical Maximum (global)
			logical_max = signedval(val, tag);
			break;
		  case 0x74: // Report Size (global)
			report_size = val;
			break;
		  case 0x94: // Report Count (global)
			report_count = val;
			break;
		  case 0x84: // Report ID (global)
      DBGPrintf("    Set Report ID: %u\n", val);
			report_id = val;
			break;
		  case 0x08: // Usage (local)
			if (usage_count < USAGE_LIST_LEN) {
				// Usages: 0 is reserved 0x1-0x1f is sort of reserved for top level things like
				// 0x1 - Pointer - A collection... So lets try ignoring these
				if (val > 0x1f) {
					usage[usage_count++] = val;
				}
        DBGPrintf("    Usage: %lx cnt:%u\n", val, usage_count);
			}
			break;
		  case 0x18: // Usage Minimum (local)
		  	// Note: Found a report with multiple min/max
		  	if (usage_count != 255) {
				usage_count = 255;
			  	usage_min_max_count = 0;
				usage_min_max_mask = 0;
			}
			usage[usage_min_max_count * 2] = val;
			usage_min_max_mask |= 1;
			if (usage_min_max_mask == 3) {
		  		usage_min_max_count++;
				usage_min_max_mask = 0;					
		  	}
			break;
		  case 0x28: // Usage Maximum (local)
		  	if (usage_count != 255) {
				usage_count = 255;
			  	usage_min_max_count = 0;
				usage_min_max_mask = 0;
			}
			usage[usage_min_max_count * 2 + 1] = val;
			usage_min_max_mask |= 2;
			if (usage_min_max_mask == 3) {
		  		usage_min_max_count++;
				usage_min_max_mask = 0;					
		  	}
			break;
		  case 0xA0: // Collection
			if (collection_level == 0) {
				topusage = ((uint32_t)usage_page << 16) | usage[0];
				//driver = NULL;
				//if (topusage_index < TOPUSAGE_LIST_LEN) {
				//	driver = topusage_drivers[topusage_index++];
				//}
			}
			// discard collection info if not top level, hopefully that's ok?
			collection_level++;
			reset_local = true;
			break;
		  case 0xC0: // End Collection
			if (collection_level > 0) {
				collection_level--;
			  if (hidCB_) hidCB_->hid_input_end();
			}
			reset_local = true;
			break;
		  case 0x80: // Input
      DBGPrintf("    Input: %u %u == %u?\n", use_report_id_, report_id, buffer_report_id);
			if (use_report_id_ && (report_id != buffer_report_id)) {
				// completely ignore and do not advance bitindex
				// for descriptors of other report IDs
				reset_local = true;
				break;
			}
			if ((val & 1)) {
				// skip past constant fields or when no driver is listening
				bitindex += report_count * report_size;
			} else {
				DBGPrintf("begin, usage=%lx\n", topusage);
				DBGPrintf("       type= %lx\n", val);
				DBGPrintf("       min=  %ld\n", logical_min);
				DBGPrintf("       max=  %ld\n", logical_max);
				DBGPrintf("       reportcount=%ld\n", report_count);
				DBGPrintf("       usage count=%ld\n", usage_count);
				DBGPrintf("       usage min max count=%ld\n", usage_min_max_count);

				if (hidCB_) hidCB_->hid_input_begin(topusage, val, logical_min, logical_max);
				DBGPrintf("Input, total bits=%d\n", report_count * report_size);
				if ((val & 2)) {
					// ordinary variable format
					uint32_t uindex = 0;
					uint32_t uindex_max = 0xffff;	// assume no MAX
					bool uminmax = false;
					uint8_t uminmax_index = 0;
					if (usage_count > USAGE_LIST_LEN) {
						// usage numbers by min/max, not from list
						uindex = usage[0];
						uindex_max = usage[1];
						uminmax = true;
					} else if ((report_count > 1) && (usage_count <= 1)) {
						// Special cases:  Either only one or no usages specified and there are more than one 
						// report counts .  
						if (usage_count == 1) {
							uindex = usage[0];
						} else {
							// BUGBUG:: Not sure good place to start?  maybe round up from last usage to next higher group up of 0x100?
							uindex = (last_usage & 0xff00) + 0x100;
						}
						uminmax = true;
					}
					DBGPrintf("TU:%x US:%x %x %d %d: C:%d, %d, MM:%d, %x %x\n", topusage, usage_page, val, logical_min, logical_max, 
								report_count, usage_count, uminmax, usage[0], usage[1]);
					for (uint32_t i=0; i < report_count; i++) {
						uint32_t u;
						if (uminmax) {
							u = uindex;
							if (uindex < uindex_max) uindex++;
							else if (uminmax_index < usage_min_max_count) {
								uminmax_index++;
								uindex = usage[uminmax_index * 2];
								uindex_max = usage[uminmax_index * 2 + 1];
								//USBHDBGSerial.DBGPrintf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
							}
						} else {
							u = usage[uindex++];
							if (uindex >= USAGE_LIST_LEN-1) {
								uindex = USAGE_LIST_LEN-1;
							}
						}
						last_usage = u;	// remember the last one we used... 
						u |= (uint32_t)usage_page << 16;
						DBGPrintf("  usage = %lx", u);

						uint32_t n = bitfield(data, bitindex, report_size);
						if (logical_min >= 0) {
							DBGPrintf("  data = %lu\n", n);
							if (hidCB_) hidCB_->hid_input_data(u, n);
						} else {
							int32_t sn = signext(n, report_size);
							DBGPrintf("  sdata = %ld\n", sn);
							if (hidCB_) hidCB_->hid_input_data(u, sn);
						}
						bitindex += report_size;
					}
				} else {
					// array format, each item is a usage number
					// maybe act like the 2 case...
					if (usage_min_max_count && (report_size == 1)) {
						uint32_t uindex = usage[0];
						uint32_t uindex_max = usage[1];
						uint8_t uminmax_index = 0;
						uint32_t u;

						for (uint32_t i=0; i < report_count; i++) {
							u = uindex;
							if (uindex < uindex_max) uindex++;
							else if (uminmax_index < usage_min_max_count) {
								uminmax_index++;
								uindex = usage[uminmax_index * 2];
								uindex_max = usage[uminmax_index * 2 + 1];
								//USBHDBGSerial.DBGPrintf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
							}

							u |= (uint32_t)usage_page << 16;
							uint32_t n = bitfield(data, bitindex, report_size);
							if (logical_min >= 0) {
								DBGPrintf("  data = %lu\n", n);
								if (hidCB_) hidCB_->hid_input_data(u, n);
							} else {
								int32_t sn = signext(n, report_size);
								DBGPrintf("  sdata = %lu", sn);
								if (hidCB_) hidCB_->hid_input_data(u, sn);
							}

							bitindex += report_size;
						}

					} else {
						for (uint32_t i=0; i < report_count; i++) {
							uint32_t u = bitfield(data, bitindex, report_size);
							int n = u;
							if (n >= logical_min && n <= logical_max) {
								u |= (uint32_t)usage_page << 16;
								DBGPrintf("  usage = %lx", u);
								DBGPrintf("  data = 1\n");
								if (hidCB_) hidCB_->hid_input_data(u, 1);
							} else {
								DBGPrintf ("  usage =%lx", u);
								DBGPrintf(" out of range: %lx", logical_min);
								DBGPrintf(" %lx\n", logical_max);
							}
							bitindex += report_size;
						}
					}
				}
			}
			reset_local = true;
			break;
		  case 0x90: // Output
			// TODO.....
			reset_local = true;
			break;
		  case 0xB0: // Feature
			// TODO.....
			reset_local = true;
			break;

		  case 0x34: // Physical Minimum (global)
		  case 0x44: // Physical Maximum (global)
		  case 0x54: // Unit Exponent (global)
		  case 0x64: // Unit (global)
			break; // Ignore these commonly used tags.  Hopefully not needed?

		  case 0xA4: // Push (yikes! Hope nobody really uses this?!)
		  case 0xB4: // Pop (yikes! Hope nobody really uses this?!)
		  case 0x38: // Designator Index (local)
		  case 0x48: // Designator Minimum (local)
		  case 0x58: // Designator Maximum (local)
		  case 0x78: // String Index (local)
		  case 0x88: // String Minimum (local)
		  case 0x98: // String Maximum (local)
		  case 0xA8: // Delimiter (local)
		  default:
			DBGPrintf("Ruh Roh, unsupported tag, not a good thing Scoob %lx\n", tag);
			break;
		}
		if (reset_local) {
			usage_count = 0;
			usage_min_max_count = 0;
			usage[0] = 0;
			usage[1] = 0;
		}
	}
}

bool USBHostHIDParser::getHIDDescriptor() {

  //DBGPrintf(">>>>> USBDumperDevice::getHIDDesc(%u) called <<<<< \n", index);

  USB_TYPE res = host_->controlRead(dev_, 0x81, 6, 0x2200, index_, descriptor_buffer_, descriptor_length_);  // get report desc

  if (res != USB_TYPE_OK) {
    DBGPrintf("\t Read HID descriptor failed: %u\n",  res);
    return false;
  }
  #ifdef DEBUG_HID_PARSER_VERBOSE
  MemoryHexDump(Serial, descriptor_buffer_, descriptor_length_, true);
  #endif
  return true;
}
