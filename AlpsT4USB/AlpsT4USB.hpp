
/*This code is derived and adapted from VoodooI2CHID's Multitouch Event Driver and Precision
Touchpad Event Driver (https://github.com/alexandred/VoodooI2C) and the Linux kernel driver
for the alps t4 touchpad (https://github.com/torvalds/linux/blob/master/drivers/hid/hid-alps.c)*/




#ifndef AlpsT4USBEventDriver_hpp
#define AlpsT4USBEventDriver_hpp

#define _IOKIT_HID_IOHIDEVENTDRIVER_H


#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/hid/IOHIDInterface.h>
#include <kern/clock.h>


#include "IOKit/hid/IOHIDEvent.h"
#include <IOKit/hidevent/IOHIDEventService.h>
#include "IOKit/hid/IOHIDEventTypes.h"
#include <IOKit/hidsystem/IOHIDTypes.h>
#include "IOKit/hid/IOHIDPrivateKeys.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDDevice.h>


#include "VoodooI2CMultitouchInterface.hpp"
#include "MultitouchHelpers.hpp"

#include "helpers.hpp"


#define HID_PRODUCT_ID_T4_USB       0X1216


#define T4_INPUT_REPORT_LEN         sizeof(struct t4_input_report)
#define T4_FEATURE_REPORT_LEN       T4_INPUT_REPORT_LEN
#define T4_FEATURE_REPORT_ID        0x07
#define T4_CMD_REGISTER_READ        0x08
#define T4_CMD_REGISTER_WRITE       0x07
#define T4_INPUT_REPORT_ID          0x09

#define T4_PRM_FEED_CONFIG_1        0xC2C4

#define T4_I2C_ABS                  0x78

#define T4_COUNT_PER_ELECTRODE      256
#define MAX_TOUCHES                 5


// Message types defined by ApplePS2Keyboard
enum
{
    // from keyboard to mouse/touchpad
    kKeyboardSetTouchStatus = iokit_vendor_specific_msg(100),   // set disable/enable touchpad (data is bool*)
    kKeyboardGetTouchStatus = iokit_vendor_specific_msg(101),   // get disable/enable touchpad (data is bool*)
    kKeyboardKeyPressTime = iokit_vendor_specific_msg(110)      // notify of timestamp a non-modifier key was pressed (data is uint64_t*)
};

struct __attribute__((__packed__)) t4_contact_data {
    UInt8  palm;
    UInt8    x_lo;
    UInt8    x_hi;
    UInt8    y_lo;
    UInt8    y_hi;
};

struct __attribute__((__packed__)) t4_input_report {
    UInt8  reportID;
    UInt8  numContacts;
    struct t4_contact_data contact[5];
    UInt8  button;
    UInt8  track[5];
    UInt8  zx[5], zy[5];
    UInt8  palmTime[5];
    UInt8  kilroy;
    UInt16 timeStamp;
};

class AlpsT4USBEventDriver : public IOHIDEventService {
    OSDeclareDefaultStructors(AlpsT4USBEventDriver);
    
public:
    /* @inherit */
    
    void handleInterruptReport(AbsoluteTime timestamp, IOMemoryDescriptor *report, IOHIDReportType report_type, UInt32 report_id);
    
    /* @inherit */
    bool start(IOService* provider) override;
    void stop(IOService* provider) override;
    bool init(OSDictionary *properties) override;
    AlpsT4USBEventDriver* probe(IOService* provider, SInt32* score) override;

    
    bool handleStart(IOService* provider) override;
    void handleStop(IOService* provider) override;
    
    virtual IOReturn message(UInt32 type, IOService* provider, void* argument) override;
    bool didTerminate(IOService* provider, IOOptionBits options, bool* defer) override;

    /* @inherit */
    IOReturn setPowerState(unsigned long whichState, IOService* whatDevice) override;
    
    UInt16 t4_calc_check_sum(UInt8 *buffer,
                             unsigned long offset, unsigned long length);
    
    void put_unaligned_le32(uint32_t val, void *p);
    void __put_unaligned_le16(uint16_t val, uint8_t *p);
    void __put_unaligned_le32(uint32_t val, uint8_t *p);
    IOReturn publishMultitouchInterface();
    const char* getProductName();
    
    
protected:
    const char* name;
    IOHIDInterface* hid_interface;
    IOHIDDevice* hid_device;
    VoodooI2CMultitouchInterface* mt_interface;
    bool should_have_interface = true;
    
private:
    bool ready = false;
    uint64_t max_after_typing = 500000000;
    uint64_t key_time = 0;
    bool awake;
    IOWorkLoop* work_loop;
    IOCommandGate* command_gate;
    
    OSArray* transducers;
    
    /* Sends a report to the device to instruct it to enter Touchpad mode */
    void enterPrecisionTouchpadMode();
};


#endif /* AlpsT4USBEventDriver_hpp */

