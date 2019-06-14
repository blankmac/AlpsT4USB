# AlpsT4USB

This is a satellite kext which uses VoodooI2C's multitouch engine to bring native multitouch to the Alps
T4 USB touchpad found on the Elite X2 1012 G1 and G2 devices.

# Installation Considerations

Currently this kext only works when installed to /Library/Extensions.  VoodooI2C and VoodooI2CHID must 
also be installed to /Library/Extensions.

# Experimental

The latest release of this kext *should* also work for I2C Alps touchpad devices with the following device IDs - 0x1209, 0x120b, 0x120c but it is currently untested.  Since it only subclasses the Event Service, you will always need to install VoodooI2C / VoodooI2CHID to instantiate the hid device.  If you have an I2C Alps touchpad and try the kext, please report in the VoodooI2C gitter chat whether or not it works for you.

# Credits
This code is derived and adapted from VoodooI2CHID's Multitouch Event Driver and Precision
Touchpad Event Driver (https://github.com/alexandred/VoodooI2C) and the Linux kernel driver
for the alps t4 touchpad (https://github.com/torvalds/linux/blob/master/drivers/hid/hid-alps.c)
