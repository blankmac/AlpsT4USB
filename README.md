# AlpsT4USB

This is a satellite kext which uses VoodooI2C's multitouch engine to bring native multitouch to the Alps
T4 USB touchpad found on the Elite X2 1012 G1 and G2 devices.

# Installation Considerations

Currently this kext only works when installed to /Library/Extensions.  VoodooI2C and VoodooI2CHID must 
also be installed to /Library/Extensions.

# Credits
This code is derived and adapted from VoodooI2CHID's Multitouch Event Driver and Precision
Touchpad Event Driver (https://github.com/alexandred/VoodooI2C) and the Linux kernel driver
for the alps t4 touchpad (https://github.com/torvalds/linux/blob/master/drivers/hid/hid-alps.c)
