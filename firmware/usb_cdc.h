#ifndef USB_CDC_H
#define USB_CDC_H

#include "ring.h"

/** Init & start USB */
void fwusb_init(void);

/** FIFO of data to host */
extern struct ring output_ring;
#endif
