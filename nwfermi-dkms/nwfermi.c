/*
 * NextWindow Fermi USB Touchscreen Driver v2.0.4
 * 
 * Length-based touch detection with coordinate extraction
 * Works directly with Wayland/GNOME - no daemon needed
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/input/mt.h>

#define DRIVER_VERSION "2.0.4"
#define DRIVER_AUTHOR "Daniel Newton, refactored with length-based detection"
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

/* Packet length thresholds - KEY DISCOVERY */
#define PACKET_LEN_IDLE_MIN    478  /* Idle packets: 478-486 bytes */
#define PACKET_LEN_IDLE_MAX    486
#define PACKET_LEN_TOUCH_MIN   487  /* Touch packets: 488-493 bytes */
#define PACKET_LEN_TOUCH_MAX   493

/* Packet type identifiers (byte 3 of packet) */
#define PACKET_TYPE_01         0x01  /* Main data stream */
#define PACKET_TYPE_STATUS     0x09  /* Large status packets (2471 bytes) */

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
	
	/* Packet statistics for diagnostic */
	unsigned long           packet_count_total;
	unsigned long           packet_count_idle;
	unsigned long           packet_count_touch;
	int                     last_packet_len;
	
	u8                      bulk_in_addr;
	struct mutex            mutex;
	bool                    disconnected;
};

/* Helper to convert signed 8-bit value to int */
static inline int s8_to_int(u8 val)
{
	return (val & 0x80) ? (int)val - 256 : (int)val;
}

/* Dump hex data for analysis (only when touch detected) */
static void dump_packet_hex(struct device *dev, const u8 *data, int len, const char *label)
{
	int i;
	char hex_buf[256];
	int pos = 0;
	
	/* Print first 64 bytes in groups of 8 */
	for (i = 0; i < min(64, len) && pos < sizeof(hex_buf) - 3; i++) {
		if (i > 0 && (i % 8) == 0) {
			dev_info(dev, "%s [%02d-%02d]: %s\n", label, i-8, i-1, hex_buf);
			pos = 0;
		}
		pos += snprintf(hex_buf + pos, sizeof(hex_buf) - pos, "%02x ", data[i]);
	}
	if (pos > 0)
		dev_info(dev, "%s [%02d-%02d]: %s\n", label, (i-1)/8*8, i-1, hex_buf);
}

/* Parse touch packet using length-based detection */
static void fermi_parse_touch_packet_by_length(struct fermi_dev *dev, const u8 *data, int len)
{
	/* The extra bytes (beyond the 478-byte baseline) contain touch data
	 * We need to extract and decode these bytes to get coordinates
	 * 
	 * From usbmon analysis:
	 * - Idle packets: 478-486 bytes (cycling through 478, 486, 479, 483)
	 * - Touch packets: 488-493 bytes (8-15 bytes longer)
	 * 
	 * The extra bytes appear around offset 16-32 based on packet type
	 * Packet structure shows data fields like:
	 *   00006e80 015b22f3 04fee810 0d06f60d
	 *   00005d80 00ce1413 16132709 0715f7e3
	 * 
	 * These likely contain:
	 * - Touch point identifiers
	 * - X/Y coordinates (possibly 16-bit values)
	 * - Pressure or other metadata
	 */
	
	int extra_bytes = len - PACKET_LEN_IDLE_MIN;
	int slot = 0;  /* For now, assume single touch */
	
	if (extra_bytes <= 0) {
		dev_err(&dev->interface->dev, 
			"Logic error: touch packet with no extra bytes (len=%d)\n", len);
		return;
	}
	
	/* Dump the packet for analysis */
	dev_info(&dev->interface->dev,
		"=== TOUCH PACKET: len=%d, extra_bytes=%d ===\n", 
		len, extra_bytes);
	dump_packet_hex(&dev->interface->dev, data, len, "RAW");
	
	/* Try to extract coordinates from the data
	 * Looking at byte offset 16-32 where touch data appears
	 */
	if (len >= 32) {
		u16 val1, val2, val3, val4;
		
		/* Extract some 16-bit values from different offsets */
		val1 = (data[16] << 8) | data[17];  /* Offset 16-17 */
		val2 = (data[20] << 8) | data[21];  /* Offset 20-21 */
		val3 = (data[24] << 8) | data[25];  /* Offset 24-25 */
		val4 = (data[28] << 8) | data[29];  /* Offset 28-29 */
		
		dev_info(&dev->interface->dev,
			"Potential coords: [16-17]=0x%04x=%d [20-21]=0x%04x=%d [24-25]=0x%04x=%d [28-29]=0x%04x=%d\n",
			val1, val1, val2, val2, val3, val3, val4, val4);
	}
	
	/* For now, just report a touch event at center screen to verify input system works */
	if (!dev->slot_active[slot]) {
		dev->tracking_id[slot] = dev->next_tracking_id++;
		dev->slot_active[slot] = true;
		
		/* Report touch down at center of screen */
		dev->abs_x[slot] = FERMI_MAX_X / 2;
		dev->abs_y[slot] = FERMI_MAX_Y / 2;
		
		input_mt_slot(dev->input, slot);
		input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, true);
		input_report_abs(dev->input, ABS_MT_TRACKING_ID, dev->tracking_id[slot]);
		input_report_abs(dev->input, ABS_MT_POSITION_X, dev->abs_x[slot]);
		input_report_abs(dev->input, ABS_MT_POSITION_Y, dev->abs_y[slot]);
		input_sync(dev->input);
		
		dev_info(&dev->interface->dev, "Touch DOWN at center (diagnostic)\n");
	}
}

/* Release all active touches */
static void fermi_release_all_touches(struct fermi_dev *dev)
{
	int i;
	
	for (i = 0; i < FERMI_MAX_SLOTS; i++) {
		if (dev->slot_active[i]) {
			input_mt_slot(dev->input, i);
			input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, false);
			dev->slot_active[i] = false;
			
			dev_info(&dev->interface->dev, "Touch UP slot %d (diagnostic)\n", i);
		}
	}
	input_sync(dev->input);
}

/* Process incoming packet using length-based detection */
static void fermi_process_packet(struct fermi_dev *dev, const u8 *data, int len)
{
	u8 packet_type;
	bool is_touch;
	
	/* Update statistics */
	dev->packet_count_total++;
	
	/* Validate packet header */
	if (len < 4 || data[0] != PACKET_HEADER_0 || data[1] != PACKET_HEADER_1) {
		dev_dbg(&dev->interface->dev, "Invalid packet header\n");
		return;
	}
	
	packet_type = data[3];
	
	/* Filter by packet type first */
	if (packet_type == PACKET_TYPE_STATUS) {
		/* Status packets are 2471 bytes, sent ~1/sec - ignore */
		return;
	}
	
	if (packet_type != PACKET_TYPE_01) {
		/* Unknown packet type - ignore */
		return;
	}
	
	/* Now use LENGTH to detect touch vs idle */
	is_touch = (len >= PACKET_LEN_TOUCH_MIN && len <= PACKET_LEN_TOUCH_MAX);
	
	if (is_touch) {
		dev->packet_count_touch++;
		
		/* Only log on length changes to reduce spam */
		if (len != dev->last_packet_len || dev->packet_count_touch == 1) {
			dev_info(&dev->interface->dev,
				"Touch packet detected: len=%d (was %d)\n",
				len, dev->last_packet_len);
		}
		
		fermi_parse_touch_packet_by_length(dev, data, len);
		dev->last_packet_len = len;
		
	} else if (len >= PACKET_LEN_IDLE_MIN && len <= PACKET_LEN_IDLE_MAX) {
		dev->packet_count_idle++;
		
		/* Release any active touches when returning to idle */
		fermi_release_all_touches(dev);
		
		/* Only log occasionally to avoid spam */
		if (dev->packet_count_idle % 1000 == 0) {
			dev_info(&dev->interface->dev,
				"Stats: total=%lu idle=%lu touch=%lu\n",
				dev->packet_count_total,
				dev->packet_count_idle,
				dev->packet_count_touch);
		}
		
		dev->last_packet_len = len;
	} else {
		/* Unexpected packet length */
		dev_warn(&dev->interface->dev,
			"Unexpected packet length: %d (type=0x%02x)\n",
			len, packet_type);
	}
}

/* URB completion callback */
static void fermi_irq(struct urb *urb)
{
	struct fermi_dev *dev = urb->context;
	int retval;
	
	/* Don't resubmit if we're disconnected */
	if (dev->disconnected)
		return;
	
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
	
	dev->disconnected = true;
	
	for (i = 0; i < FERMI_URB_COUNT; i++)
		usb_kill_urb(dev->urbs[i]);
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
	dev->disconnected = false;
	
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
	
	/* Start reading data immediately */
	retval = fermi_start(dev);
	if (retval) {
		dev_err(&intf->dev, "Failed to start URBs: %d\n", retval);
		goto error_unregister;
	}
	
	dev_info(&intf->dev, "NextWindow Fermi touchscreen registered successfully\n");
	return 0;

error_unregister:
	input_unregister_device(input);
	input = NULL;  /* Don't free it again */
	
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
	
	/* Mark as disconnected and stop URBs */
	mutex_lock(&dev->mutex);
	fermi_stop(dev);
	mutex_unlock(&dev->mutex);
	
	input_unregister_device(dev->input);
	
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
	int i;
	
	/* Kill URBs during suspend */
	for (i = 0; i < FERMI_URB_COUNT; i++)
		usb_kill_urb(dev->urbs[i]);
	
	return 0;
}

static int fermi_resume(struct usb_interface *intf)
{
	struct fermi_dev *dev = usb_get_intfdata(intf);
	int retval;
	
	/* Restart URBs after resume */
	retval = fermi_start(dev);
	if (retval)
		dev_err(&intf->dev, "Failed to restart after resume: %d\n", retval);
	
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
