#include "AlpsT4USB.hpp"


#define super IOService
OSDefineMetaClassAndStructors(AlpsT4USBEventDriver, IOHIDEventService);

void AlpsT4USBEventDriver::enterPrecisionTouchpadMode() {
    
    UInt16 check_sum;
    UInt8 buffer[T4_FEATURE_REPORT_LEN] = {};
    
    buffer[0] = T4_FEATURE_REPORT_ID;
    buffer[1] = T4_CMD_REGISTER_WRITE;
    buffer[8] = T4_I2C_ABS;
    
    put_unaligned_le32(T4_PRM_FEED_CONFIG_1, buffer + 2);
    
    buffer[6] = 1;
    buffer[7] = 0;
    
    check_sum = t4_calc_check_sum(buffer, 1, 8);
    
    buffer[9] = (UInt8)check_sum;
    buffer[10] = (UInt8)(check_sum >> 8);
    buffer[11] = 0;
    
    
    OSData* buffer_updated = OSData::withBytes(buffer, T4_FEATURE_REPORT_LEN);
    
    
    
    IOBufferMemoryDescriptor* report = IOBufferMemoryDescriptor::withBytes(buffer_updated->getBytesNoCopy(0, T4_FEATURE_REPORT_LEN), buffer_updated->getLength(), kIODirectionInOut);
    
    
    buffer_updated->release();
    
    
    hid_interface->setReport(report, kIOHIDReportTypeFeature, T4_FEATURE_REPORT_ID);
    
    ready = true;
}

void AlpsT4USBEventDriver::handleInterruptReport(AbsoluteTime timestamp, IOMemoryDescriptor *report, IOHIDReportType report_type, UInt32 report_id) {
    
    uint64_t now_abs;
    clock_get_uptime(&now_abs);
    uint64_t now_ns;
    absolutetime_to_nanoseconds(now_abs, &now_ns);
    
    // Ignore touchpad interaction(s) shortly after typing
    if (now_ns - key_time < max_after_typing)
        return;
    
    if (!ready || !report)
        return;
    
    if (report_type != kIOHIDReportTypeInput)
        return;
    
    if (report_id != T4_INPUT_REPORT_ID)
        return;
    
    if (!transducers)
        return;
    
    
    
    t4_input_report reportData;
    
    unsigned int x, y, z;
    
    report->readBytes(0, &reportData, T4_INPUT_REPORT_LEN);
    
    
    int contactCount = 0;
    for (int i = 0; i < MAX_TOUCHES; i++) {
        
        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer,  transducers->getObject(i));
        transducer->type = kDigitiserTransducerFinger;
        transducer->logical_max_x = mt_interface->logical_max_x;
        transducer->logical_max_y = mt_interface->logical_max_y;
        transducer->id = 9;
        if (!transducer) {
            continue;
        }
        
        x = reportData.contact[i].x_hi << 8 | reportData.contact[i].x_lo;
        y = reportData.contact[i].y_hi << 8 | reportData.contact[i].y_lo;
        y = 3060 - y + 255;
        z = (reportData.contact[i].palm < 0x80 &&
             reportData.contact[i].palm > 0) * 62;
        
        bool contactValid = z;
        transducer->is_valid = contactValid;
        if (contactValid) {
            transducer->coordinates.x.update(x, timestamp);
            transducer->coordinates.y.update(y, timestamp);
            transducer->physical_button.update(reportData.button, timestamp);
            transducer->tip_switch.update(1, timestamp);
            transducer->secondary_id = i;
            contactCount += 1;
            
        } else {
            transducer->secondary_id = i;
            transducer->coordinates.x.update(transducer->coordinates.x.last.value, timestamp);
            transducer->coordinates.y.update(transducer->coordinates.y.last.value, timestamp);
            transducer->physical_button.update(0, timestamp);
            transducer->tip_switch.update(0, timestamp);
            x = 0;
            y = 0;
            z = 0;
            
            
        }
        
    }
    
    VoodooI2CMultitouchEvent tp_event;
    tp_event.contact_count = contactCount;
    tp_event.transducers = transducers;
    
    
    mt_interface->handleInterruptReport(tp_event, timestamp);
    
    
    
}

bool AlpsT4USBEventDriver::handleStart(IOService* provider) {
    
     IOLog("%s::%s hid interface \n", getName(), name);

    hid_interface = OSDynamicCast(IOHIDInterface, provider);
    
    
    if (!hid_interface)
        return false;
    
    if (hid_interface->getTransport()->getCStringNoCopy() != kIOHIDTransportUSBValue)
        hid_interface->setProperty("VoodooI2CServices Supported", OSBoolean::withBoolean(true));
    
    if (!hid_interface->open(this, 0, OSMemberFunctionCast(IOHIDInterface::InterruptReportAction, this, &AlpsT4USBEventDriver::handleInterruptReport), NULL))
        return false;
    
     IOLog("%s::%s HID device \n", getName(), name);
    
    hid_device = OSDynamicCast(IOHIDDevice, hid_interface->getParentEntry(gIOServicePlane));
    
    if (!hid_device)
        return false;
    
    name = getProductName();
    
    
    registerPowerDriver(this, VoodooI2CIOPMPowerStates, kVoodooI2CIOPMNumberPowerStates);
    
    IOLog("%s::%s Publishing multitouch interface Alps USB\n", getName(), name);

    publish_multitouch_interface();
    
    if (mt_interface) {
        mt_interface->logical_max_x = 5120;
        mt_interface->logical_max_y = 3072;
        mt_interface->physical_max_x = 1024;
        mt_interface->physical_max_y = 614;
        
    }
    IOLog("%s::%s Putting device into Precision Touchpad Mode\n", getName(), name);
    
    enterPrecisionTouchpadMode();
    
    return true;
}


IOReturn AlpsT4USBEventDriver::setPowerState(unsigned long whichState, IOService* whatDevice) {
    if (whatDevice != this)
        return kIOReturnInvalid;
    if (!whichState) {
        if (awake)
            awake = false;
    } else {
        if (!awake) {
            IOSleep(10);
            enterPrecisionTouchpadMode();
            
            awake = true;
        }
    }
    return kIOPMAckImplied;
}

UInt16 AlpsT4USBEventDriver::t4_calc_check_sum(UInt8 *buffer, unsigned long offset, unsigned long length)
{
    UInt16 sum1 = 0xFF, sum2 = 0xFF;
    unsigned long i = 0;
    
    if (offset + length >= 50)
        return 0;
    
    while (length > 0) {
        UInt32 tlen = length > 20 ? 20 : length;
        
        length -= tlen;
        
        do {
            sum1 += buffer[offset + i];
            sum2 += sum1;
            i++;
        } while (--tlen > 0);
        
        sum1 = (sum1 & 0xFF) + (sum1 >> 8);
        sum2 = (sum2 & 0xFF) + (sum2 >> 8);
    }
    
    sum1 = (sum1 & 0xFF) + (sum1 >> 8);
    sum2 = (sum2 & 0xFF) + (sum2 >> 8);
    
    return(sum2 << 8 | sum1);
}

void AlpsT4USBEventDriver::put_unaligned_le32(uint32_t val, void *p)
{
    __put_unaligned_le32(val, (uint8_t*)p);
}

void AlpsT4USBEventDriver::__put_unaligned_le16(uint16_t val, uint8_t *p)
{
    *p++ = val;
    *p++ = val >> 8;
}

void AlpsT4USBEventDriver::__put_unaligned_le32(uint32_t val, uint8_t *p)
{
    __put_unaligned_le16(val >> 16, p + 2);
    __put_unaligned_le16(val, p);
}

bool AlpsT4USBEventDriver::publish_multitouch_interface() {
    mt_interface = new VoodooI2CMultitouchInterface();
    if (!mt_interface) {
        IOLog("%s::%s No memory to allocate VoodooI2CMultitouchInterface instance\n", getName(), name);
        goto multitouch_exit;
    }
    if (!mt_interface->init(NULL)) {
        IOLog("%s::%s Failed to init multitouch interface\n", getName(), name);
        goto multitouch_exit;
    }
    if (!mt_interface->attach(this)) {
        IOLog("%s::%s Failed to attach multitouch interface\n", getName(), name);
        goto multitouch_exit;
    }
    if (!mt_interface->start(this)) {
        IOLog("%s::%s Failed to start multitouch interface\n", getName(), name);
        goto multitouch_exit;
    }
    // Assume we are a touchpad
    mt_interface->setProperty(kIOHIDDisplayIntegratedKey, false);
    // 0x04f3 is Elan's Vendor Id
    mt_interface->setProperty(kIOHIDVendorIDKey, 0x44e, 32);
    mt_interface->setProperty(kIOHIDProductIDKey, 0x1216, 32);
    return true;
multitouch_exit:
    if (mt_interface) {
        mt_interface->stop(this);
        // multitouch_interface->release();
        // multitouch_interface = NULL;
    }
    return false;
}


bool AlpsT4USBEventDriver::start(IOService* provider) {
    if (!super::start(provider))
        return false;
    
    work_loop = this->getWorkLoop();
    
    if (!work_loop)
        return false;
    
    command_gate = IOCommandGate::commandGate(this);
    if (!command_gate) {
        return false;
    }
    work_loop->addEventSource(command_gate);
    
    
    // Read QuietTimeAfterTyping configuration value (if available)
    OSNumber* quietTimeAfterTyping = OSDynamicCast(OSNumber, getProperty("QuietTimeAfterTyping"));
    
    if (quietTimeAfterTyping != NULL)
        max_after_typing = quietTimeAfterTyping->unsigned64BitValue() * 1000000;
    
    setProperty("VoodooI2CServices Supported", OSBoolean::withBoolean(true));
    
     IOLog("%s::%s Start Finished --  VoodooI2C \n", getName(), name);
    
    return true;
}

void AlpsT4USBEventDriver::handleStop(IOService* provider) {
    if (mt_interface) {
        mt_interface->stop(this);
    }
    
    work_loop->removeEventSource(command_gate);
    OSSafeReleaseNULL(command_gate);
    
    PMstop();
}


IOReturn AlpsT4USBEventDriver::message(UInt32 type, IOService* provider, void* argument)
{
    switch (type)
    {
        case kKeyboardKeyPressTime:
        {
            //  Remember last time key was pressed
            key_time = *((uint64_t*)argument);
#if DEBUG
            IOLog("%s::keyPressed = %llu\n", getName(), key_time);
#endif
            break;
        }
    }
    
    return kIOReturnSuccess;
}

bool AlpsT4USBEventDriver::init(OSDictionary *properties) {
    if (!super::init(properties)) {
        return false;
    }
    transducers = NULL;
    
    transducers = OSArray::withCapacity(MAX_TOUCHES);
    if (!transducers) {
        return false;
    }
    DigitiserTransducerType type = kDigitiserTransducerFinger;
    for (int i = 0; i < MAX_TOUCHES; i++) {
        VoodooI2CDigitiserTransducer* transducer = VoodooI2CDigitiserTransducer::transducer(type, NULL);
        transducers->setObject(transducer);
    }
    
    IOLog("%s::%s Transducers set--  VoodooI2C \n", getName(), name);

    awake = true;
    
    return true;
}

const char* AlpsT4USBEventDriver::getProductName() {
    
    OSString* name = getProduct();
    
    return name->getCStringNoCopy();
}
