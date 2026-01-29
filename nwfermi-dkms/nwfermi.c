/*
 * NextWindow Fermi USB Touchscreen Driver v2.1.0
 * 
 * Fixed Y-axis inversion direction
 * Works directly with Wayland/GNOME - no daemon needed
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/input.h>
#include <linux/input/mt.h>

#define DRIVER_VERSION "2.1.0"
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

/* Coordinate extraction - discovered from usbmon data */
#define COORD_X_OFFSET_LSB     24    /* X coordinate: bytes 24-25 (little-endian) */
#define COORD_X_OFFSET_MSB     25
#define COORD_Y_OFFSET_LSB     28    /* Y coordinate: bytes 28-29 (little-endian) */
#define COORD_Y_OFFSET_MSB     29

/* Raw coordinate ranges (from device) */
#define RAW_X_MIN              250    /* Actual observed minimum */
#define RAW_X_MAX              8500   /* Actual observed maximum */
#define RAW_Y_MIN              0      /* Actual observed minimum */
#define RAW_Y_MAX              5400   /* Actual observed maximum (expanded) */

/* Coordinate validation - STRICT bounds checking
 * Any coordinate outside these ranges is garbage data
 * Observed valid touches: X(250-8500), Y(0-5400)
 * Allow minimal margin while blocking 60000+ garbage values
 */
#define RAW_X_VALID_MIN        250    /* Tightened - very low X values are garbage */
#define RAW_X_VALID_MAX        9000   /* Allow some margin above max */
#define RAW_Y_VALID_MIN        0
#define RAW_Y_VALID_MAX        6000   /* Expanded to capture full Y range */

/* Screen resolution (adjust for your display) */
#define SCREEN_WIDTH           1366
#define SCREEN_HEIGHT          768

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

/* Parse touch packet using length-based detection */
static void fermi_parse_touch_packet_by_length(struct fermi_dev *dev, const u8 *data, int len)
{
	int slot = 0;  /* For now, assume single touch */
	u16 raw_x, raw_y;
	int screen_x, screen_y;
	
	/* Validate packet length */
	if (len < 32) {
		dev_err(&dev->interface->dev, 
			"Touch packet too short: len=%d\n", len);
		return;
	}
	
	/* Extract coordinates (little-endian 16-bit values)
	 * - Bytes 24-25: X coordinate (LSB first)
	 * - Bytes 28-29: Y coordinate (LSB first)
	 */
	raw_x = data[COORD_X_OFFSET_LSB] | (data[COORD_X_OFFSET_MSB] << 8);
	raw_y = data[COORD_Y_OFFSET_LSB] | (data[COORD_Y_OFFSET_MSB] << 8);
	
	/* STRICT coordinate validation - reject garbage packets
	 * The device sends many packets with corrupt coordinate data.
	 * Valid touches have X in range 200-8600, Y in range 0-4600.
	 * Anything outside these bounds is garbage (e.g., 60000-65000).
	 * 
	 * This is the CRITICAL filter - without it, garbage packets
	 * create phantom touches all over the screen.
	 */
	if (raw_x < RAW_X_VALID_MIN || raw_x > RAW_X_VALID_MAX ||
	    raw_y < RAW_Y_VALID_MIN || raw_y > RAW_Y_VALID_MAX) {
		dev_dbg(&dev->interface->dev,
			"Filtered invalid coordinates: raw(%u,%u)\n", raw_x, raw_y);
		return;
	}
	
	/* Clamp to valid ranges */
	if (raw_x < RAW_X_MIN) raw_x = RAW_X_MIN;
	if (raw_x > RAW_X_MAX) raw_x = RAW_X_MAX;
	if (raw_y < RAW_Y_MIN) raw_y = RAW_Y_MIN;
	if (raw_y > RAW_Y_MAX) raw_y = RAW_Y_MAX;
	
	/* Scale to screen resolution
	 * X: INVERTED (250 → 1366, 8500 → 0) - screen is backwards
	 * Y: NORMAL (0 → 0, 5400 → 768) - testing showed inversion was wrong
	 */
	screen_x = SCREEN_WIDTH - ((raw_x - RAW_X_MIN) * SCREEN_WIDTH) / (RAW_X_MAX - RAW_X_MIN);
	screen_y = ((raw_y - RAW_Y_MIN) * SCREEN_HEIGHT) / (RAW_Y_MAX - RAW_Y_MIN);
	
	/* Clamp to screen bounds */
	if (screen_x < 0) screen_x = 0;
	if (screen_x >= SCREEN_WIDTH) screen_x = SCREEN_WIDTH - 1;
	if (screen_y < 0) screen_y = 0;
	if (screen_y >= SCREEN_HEIGHT) screen_y = SCREEN_HEIGHT - 1;
	
	/* Scale to input device range (0-32767) */
	dev->abs_x[slot] = (screen_x * FERMI_MAX_X) / SCREEN_WIDTH;
	dev->abs_y[slot] = (screen_y * FERMI_MAX_Y) / SCREEN_HEIGHT;
	
	/* Report touch event */
	if (!dev->slot_active[slot]) {
		dev->tracking_id[slot] = dev->next_tracking_id++;
		dev->slot_active[slot] = true;
		
		input_mt_slot(dev->input, slot);
		input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, true);
		input_report_abs(dev->input, ABS_MT_TRACKING_ID, dev->tracking_id[slot]);
	}
	
	/* Always update position */
	input_mt_slot(dev->input, slot);
	input_report_abs(dev->input, ABS_MT_POSITION_X, dev->abs_x[slot]);
	input_report_abs(dev->input, ABS_MT_POSITION_Y, dev->abs_y[slot]);
	input_sync(dev->input);
	
	dev_dbg(&dev->interface->dev,
		"Touch: raw(%u,%u) → screen(%d,%d) → abs(%d,%d)\n",
		raw_x, raw_y, screen_x, screen_y, dev->abs_x[slot], dev->abs_y[slot]);
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
			
			dev_dbg(&dev->interface->dev, "Touch UP slot %d\n", i);
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
		
		/* Parse and report the touch */
		fermi_parse_touch_packet_by_length(dev, data, len);
		dev->last_packet_len = len;
		
	} else if (len >= PACKET_LEN_IDLE_MIN && len <= PACKET_LEN_IDLE_MAX) {
		dev->packet_count_idle++;
		
		/* Release any active touches when returning to idle */
		fermi_release_all_touches(dev);
		
		/* Log stats occasionally */
		if (dev->packet_count_idle % 5000 == 0) {
			dev_info(&dev->interface->dev,
				"Stats: total=%lu idle=%lu touch=%lu\n",
				dev->packet_count_total,
				dev->packet_count_idle,
				dev->packet_count_touch);
		}
		
		dev->last_packet_len = len;
	} else {
		/* Unexpected packet length */
		dev_dbg(&dev->interface->dev,
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
	
	__set_bit(INPUT_PROP_DIRECT, input->propbit);
	
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
