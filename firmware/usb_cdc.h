#ifndef USB_CDC_H
#define USB_CDC_H

#include "ring.h"

/** Init USB */
void fwusb_init(void);

/** call regularly to service USB requests */
void fwusb_poll(void);

/** FIFO of data to host */
extern struct ring output_ring;
#endif
