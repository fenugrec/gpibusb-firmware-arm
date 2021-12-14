/*
 * This file is adapted from a libopencm3 example project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

#include "host_comms.h"
#include "ring.h"
#include "stypes.h"
#include "usb_cdc.h"

/** manage some internal state
*/
static struct {
	bool usbwrite_busy;	//set to 1 after writing a packet to the EP, cleared in callback
} usb_stuff = {0};


#define COMM_IN_EP	0x83
#define DATA_IN_EP	0x82
#define DATA_OUT_EP	0x01
#define BULK_EP_MAXSIZE 64

#define USB_VID 0x1d50	//openmoko
#define USB_PID 0x0448	//unused PID so far. 488 as in "ISO 488" !

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_CDC,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = USB_VID,
	.idProduct = USB_PID,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec it's
 * optional, but its absence causes a NULL pointer dereference in the
 * Linux cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = COMM_IN_EP,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
} };

static const struct usb_endpoint_descriptor data_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = DATA_OUT_EP,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = BULK_EP_MAXSIZE,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = DATA_IN_EP,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = BULK_EP_MAXSIZE,
	.bInterval = 1,
} };

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	 }
};

static const struct usb_interface_descriptor comm_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors)
} };

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp,
} };

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = comm_iface,
}, {
	.num_altsetting = 1,
	.altsetting = data_iface,
} };

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 100,

	.interface = ifaces,
};

#define USB_NUM_STRINGS 3
static const char * usb_strings[USB_NUM_STRINGS] = {
	"garbage tech",
	"GPIB-USB",
	"serno",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes cdcacm_control_request(usbd_device *usbd_dev,
	struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
	void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		return USBD_REQ_HANDLED;
		}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding)) {
			return USBD_REQ_NOTSUPP;
		}

		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;

	u8 buf[BULK_EP_MAXSIZE];
	unsigned len = usbd_ep_read_packet(usbd_dev, DATA_OUT_EP, buf, BULK_EP_MAXSIZE);

	unsigned cnt;
	for (cnt = 0; cnt < len; cnt++) {
		host_comms_rx(buf[cnt]);
	}
}

/* should be called after a CTR condition "correct transfer received"
 */
static void cdcacm_data_tx_cb(usbd_device *usbd_dev, uint8_t ep) {
	(void) usbd_dev;
	(void) ep;
	usb_stuff.usbwrite_busy = 0;
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;

	usbd_ep_setup(usbd_dev, DATA_OUT_EP, USB_ENDPOINT_ATTR_BULK, BULK_EP_MAXSIZE,
			cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, DATA_IN_EP, USB_ENDPOINT_ATTR_BULK, BULK_EP_MAXSIZE,
			cdcacm_data_tx_cb);
	usbd_ep_setup(usbd_dev, COMM_IN_EP, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);
}

/**** private */
static usbd_device *usbd_dev;
struct ring output_ring;
static uint8_t output_ring_buffer[HOST_IN_BUFSIZE];

/** called every SOF (1ms)
 *
 * Just registering this callback should enable the interrupt ?
 * check if we have any data to send to host.
*/
static void usbsof_cb(void) {

	unsigned len;
	u8 buf[BULK_EP_MAXSIZE];

	// send any pending data if possible
	if (usb_stuff.usbwrite_busy) {
		return;
	}

	for (len = 0;len < BULK_EP_MAXSIZE; len++) {
		//copy while counting bytes
		if (ring_read_ch(&output_ring, &buf[len]) < 0) {
			//no more
			break;
		}
	}

	if (len == 0) {
		return;
	}

	if (usbd_ep_write_packet(usbd_dev, DATA_IN_EP, buf, len) == len) {;
		usb_stuff.usbwrite_busy = 1;
	} else {
		// ep_write failed for no reason ??
	}

}


void usb_isr (void) {
	usbd_poll(usbd_dev);
}


/**** public funcs */
void fwusb_init(void) {
	// need a fifo to hold data and build packets to host
	ring_init(&output_ring, output_ring_buffer, sizeof(output_ring_buffer));

	usbd_dev = usbd_init(&st_usbfs_v2_usb_driver, &dev, &config,
			usb_strings, USB_NUM_STRINGS,
			usbd_control_buffer, sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);
	usbd_register_sof_callback(usbd_dev, usbsof_cb);

	nvic_enable_irq(NVIC_USB_IRQ);
}
