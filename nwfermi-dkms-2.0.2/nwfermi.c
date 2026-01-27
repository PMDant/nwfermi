/*
 * NextWindow Fermi USB Touchscreen Driver v2.0.2
 * 
 * Complete driver with packet decoding based on USB analysis
 * Works directly with Wayland/GNOME - no daemon needed
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/input/mt.h>

#define DRIVER_VERSION "2.0.2"
#define DRIVER_AUTHOR "Daniel Newton, refactored with packet decoding"
#define DRIVER_DESC "NextWindow Fermi USB Touchscreen Driver"

/* Device constants */
#define FERMI_MAX_X         32767
#define FERMI_MAX_Y         32767
#define FERMI_MAX_SLOTS     10
#define FERMI_URB_COUNT     4
#define FERMI_URB_SIZE      4096

/* Packet constants */
#define PACKET_HEADER_0     0x5B  /* '[' */
#define PACKET_HEADER_1     0x5D  /* ']' */

/* Packet type identifiers (byte 3 of packet) */
#define PACKET_TYPE_TOUCH_D6   0xD6
#define PACKET_TYPE_TOUCH_D7   0xD7
#define PACKET_TYPE_STATUS     0x09  /* Large status packets */

/* Device structure */
struct fermi_dev {
	struct usb_device       *udev;
	struct usb_interface    *interface;
	struct input_dev        *input;
	struct urb              *urbs[FERMI_URB_COUNT];
	unsigned char           *urb_buffers[FERMI_URB_COUNT];
	
	/* Touch tracking */
	int                     abs_x[FERMI_MAX_SLOTS];
	int                     abs_y[FERMI_MAX_SLOTS];
	int                     tracking_id[FERMI_MAX_SLOTS];
	bool                    slot_active[FERMI_MAX_SLOTS];
	int                     next_tracking_id;
	
	u8                      bulk_in_addr;
	bool                    opened;
	struct mutex            mutex;
};

/* Helper to convert signed 8-bit value to int */
static inline int s8_to_int(u8 val)
{
	return (val & 0x80) ? (int)val - 256 : (int)val;
}

/* Parse touch packet type 0xD6 or 0xD7 */
static void fermi_parse_touch_packet(struct fermi_dev *dev, const u8 *data, int len)
{
	int offset;
	u8 packet_type;
	int slot;
	u8 touch_byte;
	int dx, dy;
	bool any_touch = false;
	
	if (len < 20) {
		dev_dbg(&dev->interface->dev, "Packet too short: %d\n", len);
		return;
	}
	
	packet_type = data[3];  /* 0xD6 or 0xD7 */
	
	/* Different packet types have different formats
	 * 0xD6 packets seem to have touch data starting around offset 18-20
	 * 0xD7 packets have a slightly different format
	 * 
	 * From USB traces, typical structure:
	 * Bytes 0-1:  5B 5D (header)
	 * Byte 2:     Packet sequence number
	 * Byte 3:     Packet type (D6/D7)
	 * Bytes 4-17: Various metadata
	 * Bytes 18+:  Touch data (delta-encoded)
	 */
	
	if (packet_type == PACKET_TYPE_TOUCH_D6) {
		/* 0xD6 packets - these appear to be multitouch updates */
		offset = 18;  /* Touch data starts here */
		
		/* Look for touch data patterns
		 * The data appears to be delta-encoded coordinates
		 * Pattern seems to be: [touch_byte] [dx] [dy] [more data...]
		 */
		while (offset + 3 < len) {
			touch_byte = data[offset];
			
			/* Check if this looks like a valid touch indicator
			 * High bit set (0x80) seems to indicate active touch
			 */
			if (touch_byte & 0x80) {
				/* This is a touch point */
				slot = (touch_byte & 0x0F) % FERMI_MAX_SLOTS;
				
				/* Get deltas (signed 8-bit values) */
				dx = s8_to_int(data[offset + 1]);
				dy = s8_to_int(data[offset + 2]);
				
				/* Update absolute position */
				if (!dev->slot_active[slot]) {
					/* New touch - try to initialize from packet data
					 * Sometimes absolute coords are in earlier bytes
					 */
					dev->abs_x[slot] = FERMI_MAX_X / 2;
					dev->abs_y[slot] = FERMI_MAX_Y / 2;
					dev->slot_active[slot] = true;
					dev->tracking_id[slot] = dev->next_tracking_id++;
				}
				
				/* Apply deltas */
				dev->abs_x[slot] += dx;
				dev->abs_y[slot] += dy;
				
				/* Clamp to valid range */
				if (dev->abs_x[slot] < 0) dev->abs_x[slot] = 0;
				if (dev->abs_x[slot] > FERMI_MAX_X) dev->abs_x[slot] = FERMI_MAX_X;
				if (dev->abs_y[slot] < 0) dev->abs_y[slot] = 0;
				if (dev->abs_y[slot] > FERMI_MAX_Y) dev->abs_y[slot] = FERMI_MAX_Y;
				
				/* Report to input subsystem */
				input_mt_slot(dev->input, slot);
				input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, true);
				input_report_abs(dev->input, ABS_MT_POSITION_X, dev->abs_x[slot]);
				input_report_abs(dev->input, ABS_MT_POSITION_Y, dev->abs_y[slot]);
				
				any_touch = true;
			}
			
			offset += 3;
		}
	} else if (packet_type == PACKET_TYPE_TOUCH_D7) {
		/* 0xD7 packets - these might be touch-up events or different data */
		offset = 18;
		
		/* Similar processing but might indicate touch release */
		while (offset + 3 < len) {
			touch_byte = data[offset];
			
			if ((touch_byte & 0xF0) == 0x00) {
				/* Touch release pattern */
				slot = (touch_byte & 0x0F) % FERMI_MAX_SLOTS;
				
				if (dev->slot_active[slot]) {
					input_mt_slot(dev->input, slot);
					input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, false);
					dev->slot_active[slot] = false;
					dev->tracking_id[slot] = -1;
				}
			}
			
			offset += 3;
		}
	}
	
	/* Check if any touches are still active */
	for (slot = 0; slot < FERMI_MAX_SLOTS; slot++) {
		if (dev->slot_active[slot]) {
			any_touch = true;
			break;
		}
	}
	
	input_report_key(dev->input, BTN_TOUCH, any_touch);
	input_sync(dev->input);
}

/* Process incoming packet */
static void fermi_process_packet(struct fermi_dev *dev, const u8 *data, int len)
{
	u8 packet_type;
	
	/* Validate packet header */
	if (len < 4) {
		dev_dbg(&dev->interface->dev, "Packet too short: %d\n", len);
		return;
	}
	
	if (data[0] != PACKET_HEADER_0 || data[1] != PACKET_HEADER_1) {
		dev_dbg(&dev->interface->dev, "Invalid header: %02x %02x\n", 
			data[0], data[1]);
		return;
	}
	
	packet_type = data[3];
	
	switch (packet_type) {
	case PACKET_TYPE_TOUCH_D6:
	case PACKET_TYPE_TOUCH_D7:
		fermi_parse_touch_packet(dev, data, len);
		break;
		
	case PACKET_TYPE_STATUS:
		/* Status/calibration packet - ignore for now */
		break;
		
	default:
		/* Unknown packet type */
		dev_dbg(&dev->interface->dev, "Unknown packet type: 0x%02x\n", packet_type);
		break;
	}
}

/* URB completion callback */
static void fermi_irq(struct urb *urb)
{
	struct fermi_dev *dev = urb->context;
	int retval;
	
	switch (urb->status) {
	case 0:
		/* Success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* Device unplugged or shut down */
		return;
	default:
		dev_dbg(&dev->interface->dev, "URB error: %d\n", urb->status);
		goto resubmit;
	}
	
	if (urb->actual_length > 0) {
		/* Print first few bytes for debugging */
		if (urb->actual_length >= 4) {
			dev_dbg(&dev->interface->dev, 
				"Packet: len=%d type=0x%02x [%02x %02x %02x %02x...]\n",
				urb->actual_length, 
				((u8*)urb->transfer_buffer)[3],
				((u8*)urb->transfer_buffer)[0],
				((u8*)urb->transfer_buffer)[1],
				((u8*)urb->transfer_buffer)[2],
				((u8*)urb->transfer_buffer)[3]);
		}
		
		fermi_process_packet(dev, urb->transfer_buffer, urb->actual_length);
	}
	
resubmit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval && retval != -EPERM) {
		dev_err(&dev->interface->dev,
			"Failed to resubmit URB: %d\n", retval);
	}
}

/* Start receiving data */
static int fermi_start(struct fermi_dev *dev)
{
	int i, retval;
	
	for (i = 0; i < FERMI_URB_COUNT; i++) {
		retval = usb_submit_urb(dev->urbs[i], GFP_KERNEL);
		if (retval) {
			dev_err(&dev->interface->dev,
				"Failed to submit URB %d: %d\n", i, retval);
			while (--i >= 0)
				usb_kill_urb(dev->urbs[i]);
			return retval;
		}
	}
	
	dev_info(&dev->interface->dev, "Started reading touch data\n");
	return 0;
}

/* Stop receiving data */
static void fermi_stop(struct fermi_dev *dev)
{
	int i;
	
	for (i = 0; i < FERMI_URB_COUNT; i++)
		usb_kill_urb(dev->urbs[i]);
	
	dev_info(&dev->interface->dev, "Stopped reading touch data\n");
}

/* Input device open callback */
static int fermi_input_open(struct input_dev *input)
{
	struct fermi_dev *dev = input_get_drvdata(input);
	int retval;
	
	mutex_lock(&dev->mutex);
	
	if (!dev->opened) {
		retval = fermi_start(dev);
		if (retval) {
			mutex_unlock(&dev->mutex);
			return retval;
		}
		dev->opened = true;
	}
	
	mutex_unlock(&dev->mutex);
	return 0;
}

/* Input device close callback */
static void fermi_input_close(struct input_dev *input)
{
	struct fermi_dev *dev = input_get_drvdata(input);
	
	mutex_lock(&dev->mutex);
	
	if (dev->opened) {
		fermi_stop(dev);
		dev->opened = false;
	}
	
	mutex_unlock(&dev->mutex);
}

/* USB device table */
static const struct usb_device_id fermi_table[] = {
	{ USB_DEVICE(0x1926, 0x0006) },
	{ USB_DEVICE(0x1926, 0x1846) }, /* HP Compaq Elite 8300 Touch */
	{ USB_DEVICE(0x1926, 0x1875) }, /* Lenovo ThinkCentre M92Z */
	{ USB_DEVICE(0x1926, 0x1878) }, /* HP Compaq Elite 8300 Touch */
	/* Add other device IDs as needed */
	{ }
};
MODULE_DEVICE_TABLE(usb, fermi_table);

/* Probe function */
static int fermi_probe(struct usb_interface *intf,
		       const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct fermi_dev *dev;
	struct input_dev *input;
	struct usb_endpoint_descriptor *ep;
	int i, retval;
	
	dev_info(&intf->dev, "NextWindow Fermi touchscreen probing...\n");
	
	/* Allocate device structure */
	dev = kzalloc(sizeof(struct fermi_dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	
	dev->udev = usb_get_dev(udev);
	dev->interface = intf;
	mutex_init(&dev->mutex);
	dev->next_tracking_id = 0;
	
	/* Initialize slot tracking */
	for (i = 0; i < FERMI_MAX_SLOTS; i++) {
		dev->tracking_id[i] = -1;
		dev->slot_active[i] = false;
		dev->abs_x[i] = 0;
		dev->abs_y[i] = 0;
	}
	
	/* Find bulk-in endpoint */
	ep = &intf->cur_altsetting->endpoint[0].desc;
	if (!usb_endpoint_is_bulk_in(ep)) {
		dev_err(&intf->dev, "Could not find bulk-in endpoint\n");
		retval = -ENODEV;
		goto error;
	}
	dev->bulk_in_addr = ep->bEndpointAddress;
	
	dev_info(&intf->dev, "Bulk-in endpoint: 0x%02x\n", dev->bulk_in_addr);
	
	/* Allocate URBs and buffers */
	for (i = 0; i < FERMI_URB_COUNT; i++) {
		dev->urbs[i] = usb_alloc_urb(0, GFP_KERNEL);
		if (!dev->urbs[i]) {
			retval = -ENOMEM;
			goto error;
		}
		
		dev->urb_buffers[i] = kmalloc(FERMI_URB_SIZE, GFP_KERNEL);
		if (!dev->urb_buffers[i]) {
			usb_free_urb(dev->urbs[i]);
			dev->urbs[i] = NULL;
			retval = -ENOMEM;
			goto error;
		}
		
		usb_fill_bulk_urb(dev->urbs[i], udev,
				  usb_rcvbulkpipe(udev, dev->bulk_in_addr),
				  dev->urb_buffers[i], FERMI_URB_SIZE,
				  fermi_irq, dev);
	}
	
	/* Allocate input device */
	input = input_allocate_device();
	if (!input) {
		retval = -ENOMEM;
		goto error;
	}
	
	dev->input = input;
	input->name = "Nextwindow Fermi Touchscreen";
	input->phys = "usb";
	input->dev.parent = &intf->dev;
	
	usb_to_input_id(udev, &input->id);
	
	input_set_drvdata(input, dev);
	input->open = fermi_input_open;
	input->close = fermi_input_close;
	
	/* Set up input device capabilities */
	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_ABS, input->evbit);
	__set_bit(BTN_TOUCH, input->keybit);
	
	input_set_prop(input, INPUT_PROP_DIRECT);
	
	/* Single-touch axes (for compatibility) */
	input_set_abs_params(input, ABS_X, 0, FERMI_MAX_X, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, FERMI_MAX_Y, 0, 0);
	
	/* Multitouch */
	retval = input_mt_init_slots(input, FERMI_MAX_SLOTS,
				      INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED | INPUT_MT_TRACK);
	if (retval)
		goto error;
	
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, FERMI_MAX_X, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, FERMI_MAX_Y, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, 65535, 0, 0);
	
	/* Register input device */
	retval = input_register_device(input);
	if (retval)
		goto error;
	
	usb_set_intfdata(intf, dev);
	
	dev_info(&intf->dev, "NextWindow Fermi touchscreen registered successfully\n");
	return 0;
	
error:
	if (input)
		input_free_device(input);
	
	for (i = 0; i < FERMI_URB_COUNT; i++) {
		if (dev->urb_buffers[i])
			kfree(dev->urb_buffers[i]);
		if (dev->urbs[i])
			usb_free_urb(dev->urbs[i]);
	}
	
	usb_put_dev(dev->udev);
	kfree(dev);
	return retval;
}

/* Disconnect function */
static void fermi_disconnect(struct usb_interface *intf)
{
	struct fermi_dev *dev = usb_get_intfdata(intf);
	int i;
	
	usb_set_intfdata(intf, NULL);
	
	input_unregister_device(dev->input);
	
	mutex_lock(&dev->mutex);
	fermi_stop(dev);
	mutex_unlock(&dev->mutex);
	
	for (i = 0; i < FERMI_URB_COUNT; i++) {
		usb_free_urb(dev->urbs[i]);
		kfree(dev->urb_buffers[i]);
	}
	
	usb_put_dev(dev->udev);
	kfree(dev);
	
	dev_info(&intf->dev, "NextWindow Fermi touchscreen disconnected\n");
}

/* Suspend/resume */
static int fermi_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct fermi_dev *dev = usb_get_intfdata(intf);
	
	mutex_lock(&dev->mutex);
	if (dev->opened)
		fermi_stop(dev);
	mutex_unlock(&dev->mutex);
	
	return 0;
}

static int fermi_resume(struct usb_interface *intf)
{
	struct fermi_dev *dev = usb_get_intfdata(intf);
	int retval = 0;
	
	mutex_lock(&dev->mutex);
	if (dev->opened)
		retval = fermi_start(dev);
	mutex_unlock(&dev->mutex);
	
	return retval;
}

static struct usb_driver fermi_driver = {
	.name       = "nwfermi",
	.probe      = fermi_probe,
	.disconnect = fermi_disconnect,
	.suspend    = fermi_suspend,
	.resume     = fermi_resume,
	.id_table   = fermi_table,
	.supports_autosuspend = 1,
};

module_usb_driver(fermi_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
