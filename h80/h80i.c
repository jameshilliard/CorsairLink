/*
 * USB H80i driver
 *
 * Copyright (C) 2014 Barry Harding (barryha@earthlink.net)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 */

/*
 * This is Linux support for the Corsair H80i and H100i CPU enclosed
 * water coolers. This driver is made to support lm-sensors through
 * the sysfs (/sys) interface. Note that there are no guaranties
 * about this source or the information in this driver or the driver
 * itself. It is supplied as-is. So use use your own risk... But also
 * if it helps you out enjoy it and if you make improvements please
 * pass them on to others... 
 *
 * Note that the Corsair specific information was taken from the
 * Corsair forums web site. It was supplied by two users named
 * "CFSworks" & "Thatualle1970". This was apparently collected
 * by snooping the USB bus. I thank these user's for this information.
 * For reference, this info may be found here:
 *
 * http://forum.corsair.com/v3/showthread.php?t=120092&highlight=linux&page=3
 *
 * I have no first hand (insider) knowledge of the H80i or the internals
 * of CorsairLink. Also I am only a customer who purchased a H80i and
 * CorsairLink commander with a cooling node. That is the only relationship
 * I have with Corsair. I did not get any information from this company
 * and my only source of information is the above mentioned web link.
 * In addition I do not know the other users that posted on the web
 * site and have not made any attempt to contact them.
 *
 * Again I want to thank the other users of the device for which
 * this driver became possible!!
 */

/*
 * Mar 30 2014:
 * 
 * This is the first version of this driver. It was tested and developed 
 * with/on kubuntu 13.10. This was written with, a H80i and Corsair Link
 * Commander with a Cooling Node, attached.
 *
 * There are currently two separate driver modules one for the H80i and H100i
 * and the other for the older CorsairLink interfaces. While they could easily
 * be combined into one driver. It was easier to do it as separate files/modules.
 * I think they should be combined and I will most likely do that soon.
 *
 * The H80i/H100i module is called h80i.c and the one for older interfaces is
 * called clink.c. To develop these I first took the OpenCorsairLink program and
 * converted it from c++ to a c program. That makes it a good user-land test-bed
 * for tests of request/response types. For-which could directly transported into
 * the drivers.
 *
 * Currently the main issue with these drivers is that on boot they do not
 * always see the devices in question. As a work around I added the following
 * lines to the rc.local file (After which the devices are always seen):
 *
 * echo "reloading h80i driver."
 * rmmod h80i
 * /usr/local/bin/OpenCorsairLink -i 1
 * modprobe h80i
 * echo "reloading clink driver."
 * rmmod clink
 * /usr/local/bin/OpenCorsairLink -i 2
 * modprobe clink
 * /etc/init.d/sensord restart
 *
 * Things that I think still need to be done to these drivers are as follows:
 *  1.) Add support for LED node.
 *  2.) Add support to set any parameters that a device may support.
 *  3.) Find and fix the start-up device discovery issue.
 *  4.) Combined these drivers into one driver.
 *  5.) Submit this code to the lm-sensor project.
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon-vid.h>
#include <linux/hid.h>

#define DRIVER_AUTHOR "Barry Harding, barryha@earthlink.net"
#define DRIVER_DESC "USB H80i Driver"

/* 
 * table of USB devices that work with this driver
 *  (Note that when more devices types are
 *   supported we may need to add them here)
 */
static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x1b1c, 0x0c04) },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

/***************************************************************/
/* Definitions of Corsaoi LINK interface                       */
/***************************************************************/

/*
 * Corsair Link register mapping for newer V2 interface:
 *
 * Devices like the H80i and H100i  cooling CPU pumps use this one.
 * Note that this device is simply a set of registers that may be
 * read and some written. This list is the offset of these
 * registers and what they are for.
 */
enum CorsairLinkCmndsV2 {
	/*
	 * R - 1 byte 
	 * H80           = 0x37 (V1 interface)
	 * Cooling node  = 0x38 (V1 interface)
	 * Lighting node = 0x39 (V1 interface)
	 * H100          = 0x3A (V1 interface)
	 * H80i          = 0x3B (V2 interface)
	 * H100i         = 0x3c  (V2 interface)
	 * (This field and version are common on all C-Link devices; not the rest)
	 */
	DeviceID = 0x00, 
	/*
	 * R - 2 bytes
	 * Firmware Version in BCD (for example 1.0.5 is 0x1005, or 0x05, 0x10 in little
	 * endianess)
	 * */
	FirmwareID = 0x01,
	/*
	 * R - 8 bytes
	 * Product name, zero-terminated - only present on the H80i and H100i
	 * */
	ProductName = 0x02,
	/*
	 * R - 1 byte
	 * Status, 0 okay, 0xFF bad
	 * */
	Status = 0x03,
	/*
	 * RW - 1 byte
	 * Select current LED
	 * */
	LED_SelectCurrent = 0x04,
	/*
	 * R - 1 byte	 
	 * Number of LEDs
	 * */
	LED_Count = 0x05,
	/*
	 * RW - 1 byte
	 * LED mode - 
	 * 0x00 = static color
	 * 0x40 = 2-color cycle
	 * 0x80 = 4-color
	 * 0xC0 = temperature mode
	 * Low nibble defines cycle speed or the temperature channel to use
	 * (0 internal sensor, 7 manual)
	 * */
	LED_Mode = 0x06,
	/*
	 * R - 3 bytes
	 * LED current color, RGB color of the selected LED
	 */
	LED_CurrentColor = 0x07,
	/*
	 * RW - 2 bytes
	 * In temperature controlled mode (0xC0) this defines the colour to use to with
	 * the below gradients
	 */
	LED_TemperatureColor = 0x08,
	/*
	 * RW - 6 bytes
	 * LED temperature-mode temperatures: 3 temperatures; used when cycle mode is
	 * 0xc0
	 */
	LED_TemperatureMode = 0x09,
	/*
	 * RW - 9 bytes
	 * LED temperature-mode colors: RGBx3 colors, corresponding to temperatures in
	 * register above
	 */
	LED_TemperatureModeColors = 0x0A,
	/*
	 * RW - 12 bytes
	 * LED cycle colors: RGBx4 colors (only first color used if cycle mode set to 00,
	 * first two if 4b, ignored if c0)
	 * */
	LED_CycleColors = 0x0B,
	/*
	 * RW - 1 byte
	 * Select active temperature sensor
	 * */
	TEMP_SelectActiveSensor = 0x0C,
	/*
	 * R - 1 byte
	 * Number of temperature sensors
	 * */
	TEMP_CountSensors = 0x0D,
	/*
	 * R - 2 bytes
	 * Temperature as measured by selected sensor	 * */	TEMP_Read = 0x0E,
	/*
	 * RW - 2 bytes
	 * Temperature limit (when the temperature goes over this, status is set to 0xff)
	 * */
	TEMP_Limit = 0x0F,
	/*
	 * RW - 1 byte
	 * Select current fan; for H100i, 0-3 are the fans, 4 is pump
	 * */
	FAN_Select = 0x10,
	/*
	 * R - 1 byte
	 * Number of fans
	 * */
	FAN_Count = 0x11,
	/*
	 * RW - 1 byte
	 *
	 * Bit |7      |6      |5      |4      |3      |2      |1      |0
	 *---------------------------------------------------------------------
	 *   |PRSNT  | Tempurature Channel   |       Fan  mode       | TACH
	 *---------------------------------------------------------------------
	 * PRSNT:      set to one if fan detected
	 * Temp Chnnl: tempurature channel in curve mode; 0 = internal; 7 = manual
	 * Fan Mode:
	 *          0x02 = Fixed PWM    (mode 1)
	 *          0x04 = Fixed RPM    (mode 2)
	 *          0x06 = Default      (mode 3)
	 *          0x08 = Quiet        (mode 4)
	 *          0x0a = Balanced     (mode 5)
	 *          0x0c = Performance  (mode 6)
	 *          0x0e = Custom       (mode 7)
	 * TACH: set to one is fan is 4 pin and has a tach.
	 */
	FAN_Mode = 0x12,
		#define FAN_PRSNT	0x80
		#define FAN_TACH	0x01
	/*
	 * RW - 1 byte
	 * Fan fixed PWM, 0-255, only used if fan mode is 1
	 * */
	FAN_FixedPWM = 0x13,
	/*
	 * RW - 2 bytes
	 * Fan fixed RPM; when fan mode is 2, controller will target this RPM
	 * */
	FAN_FixedRPM = 0x14,
	/*
	 * RW - 2 bytes
	 * Report external temperature to fan controller - used for controlling fans
	 * via external sensors
	 * */
	FAN_ReportExtTemp = 0x15,
	/*
	 * R - 2 bytes
	 * Current fan RPM
	 * */
	FAN_ReadRPM = 0x16,
	/*
	 * R - 2 bytes
	 * Maximum RPM recorded since power-on
	 * */
	FAN_MaxRecordedRPM = 0x17,
	/*
	 * RW - 2 bytes
	 * Fan under speed threshold
	 * */
	FAN_UnderSpeedThreshold = 0x18,
	/*
	 * RW - 10 bytes
	 * Fan RPM table, for custom (0e) mode: array of 5 RPMs
	 * */
	FAN_RPMTable = 0x19,
	/*
	 * RW - 10 bytes
	 * Fan temp table, for custom (0e) mode: array of 5 temperatures
	 * */
	FAN_TempTable = 0x1A
};

/*
 * Since I do not have a Lighting node, I did not implement these
 * and they are here just for documentation.
 */
enum CorsairLinkCmdsLED {
	/*
	 * R - 1 byte 
	 * H80           = 0x37 (V1 interface)
	 * Cooling node  = 0x38 (V1 interface)
	 * Lighting node = 0x39 (V1 interface)
	 * H100          = 0x3A (V1 interface)
	 * H80i          = 0x3B (V2 interface)
	 * H100i         = 0x3c  (V2 interface)
	 * (This field and version are common on all C-Link devices; not the rest)
	 */
	DeviceID_LED = 0x00, 
	/*
	 * R - 2 bytes
	 * Firmware Version in BCD (for example 1.0.5 is 0x1005, or 0x05, 0x10 in little
	 * endianess)
	 */
	FirmwareID_LED = 0x01,
	/*
	 * R - 2 byte
	 * Status, 0 okay, 0xFF bad
	 */
	Status_LED = 0x03,
	/*
	 * RW - 1 byte x 2 (one for each channel)
	 * Mode of LED channel
	 * 0x10 Pulse Mode; 0x01 through 0x0F Built-In Colors; 0x00 C-Link Mode
	 */
	UserMode_LED = 0x04,
	/*
	 * R - 3 bytes x 2 (one for each channel)
	 * LED current color, RGB color of each LED
	 */
	CurrentColor_LED = 0x06,
	/*
	 * RW - 2 bytes x 2 (one for each channel)
	 * input current temps for led temp change modes
	 */
	CurrentTemp_LED = 0x0c,
	/*
	 * RW - 1 bytes (There are 2 channels each at different offset)
	 *
	 * Bit |7      |6      |5      |4      |3      |2      |1      |0
	 *---------------------------------------------------------------------
	 *   |    Mode       |         ???           |    Cycle speed
	 *---------------------------------------------------------------------
	 * Mode:
	 *          0x00 = Static       (mode 0)
	 *          0x40 = Two Cycle    (mode 1)
	 *          0x80 = Four Cycle   (mode 2)
	 *          0xc0 = Temperature  (mode 3)
	 * Cycle Speed: number times 1/8 of a second (I.E. 0, .125 - 1 second)
	 */
	Chan1_Mode_LED = 0x10,
	Chan2_Mode_LED = 0x20,
	/*
	 * RW - 2 bytes x 5 (one for each temp)
	 * Mode 3 Temp table.
	 * Controller will target the Colors below when temp reaches these values.
	 */
	Mode3_Temp_LED = 0x11,
	/*
	 * RW - 3 bytes x 5 (one for each temp above)
	 * Mode 3 color table.
	 * Controller will target these Colors when temp above reaches values.
	 */
	Mode3_Color_LED = 0x31,
	/*
	 * RW - 4 bytes x 4 (one for each cycle mode above)
	 * Note that the colors are 3 bytes with one byte padding
	 * In mode 1 only two are used.
	 * In mode 2 all four are used.
	 *
	 * Controller will cycle through these Colors based on speed time values in mode reg.
	 */
	Mode1_2_Color_LED1 = 0x20,
	Mode1_2_Color_LED2 = 0x40
};

/*
 * Valid Corsair LINK commands
 */
enum _CorsairLinkOpCodes{
	/*
	 * 06 AA BB - Write BB into one-byte register AA
	 * */
	WriteOneByte = 0x06,
	/*
	 * 07 AA - Read from one-byte register AA
	 * */
	ReadOneByte = 0x07,
	/*
	 * 08 AA BB CC - Write BB CC into two-byte register AA
	 * */
	WriteTwoBytes = 0x08,
	/*
	 * 09 AA - Read from two-byte register AA
	 * */
	ReadTwoBytes = 0x09,

	/* Note that following two do not seem to be supported on "cooling node" */
	/*
	 * 0A AA 03 00 11 22 - Write 3-byte sequence (00 11 22) into 3-byte register AA
	 * */
	WriteThreeBytes = 0x0A,
	/*
	 * 0B AA 03 - Read from 3-byte register AA
	 * */
	ReadThreeBytes = 0x0B
};

/*
 * Valid Led modes that the LED can be set to
 */
enum CorsairLedModes {
	StaticColor = 0x00,
	TwoColorCycle = 0x40,
	FourColorCycle = 0x80,
	TemperatureColor = 0xC0
};

/*
 * Valid modes for which a fan can be set to
 */
enum CorsairFanModes {
        FixedPWM = 0x02,
        FixedRPM = 0x04,
        Default = 0x06,
        Quiet = 0x08,
        Balanced = 0x0A,
        Performance = 0x0C,
        Custom = 0x0E
};


/***************************************************************/
/* Definitions of/for driver state/mode                        */
/***************************************************************/

/*
 * Per Fan/pump state.
 */
struct CorsairFanInfo {
                char		Name[25]; /* Driver derived */
                unsigned int	RPM;	  /* derived from device */
		unsigned int	maxRPM;	  /* Max RPM since powerup */
                unsigned int	Mode;	  /* derived from device */
#define FANPRESENT(__mode) (__mode & 0x80)
};
typedef struct CorsairFanInfo CorsairFanInfo_t;

/*
 * Per temp state.
 */
struct CorsairTempInfo {
                char		Name[25]; /* Driver derived */
                unsigned char	wholDeg;  /* derived from device */
                unsigned char	partDeg;  /* derived from device */
};
typedef struct CorsairTempInfo CorsairTempInfo_t;

/*
 * Driver list of "known" devices based on CorsairLink id support
 */

/*
 * quick and durty 1/256's of a degre to millidegree
 * integer aproxamation. This could be done much better.
 * But do we really care about parts of a degree?
 * This is here just to test that we are getting
 * real data and should most likely just get removed.
 */
struct temp256 {
	unsigned char bits;
	unsigned int value;
};

/*
 * These are used to convert from a bit weight in a byte.
 * A byte which represents 1/256 of a degree.
 * To an int that is a presentation of the fractional
 * part of a degree. This can then be used to show
 * fractions with out floating point math that a kernel
 * module must not do!
 *
 * Note, our results can be between 0 (IE x.000) and
 * aproxamitly 999 (IE x.999).
 */
static struct temp256 temp256[] = {
	{0x80, 500},		/* 128/256 is .500 */
	{0x40, 250},		/* 64/256 is .250 */
	{0x20, 125},		/* 32/256 is .125 */
	{0x10, 62},		/* 16/256 is .0625 */
	{0x08, 31},		/* 8/256 is .03125 */
	{0x04, 16},		/* 4/256 is .016125 */
	{0x02, 8},		/* 2/256 is .0080625 */
	{0x01, 4}		/* 1/256 is .00403125 */
};

/* 
 * There are two versions of CorsairLink interface that are currently
 * known about. The one for the older h80/h100 and the one for the
 * newer h80i/h100i. Unfortanatly there is no self identifying version
 * field of any sort. Also the two interfaces are different enough
 * and NOT backward compatable. So we need to do this funny busness
 * so we can support both of these interfaces.
 */
#define DEVINTF_NONE		0 /* Not supported type */
#define DEVINTF_TYP1		1 /* Older h80/h100 type */
#define DEVINTF_TYP2		2 /* Newer h80i/h100i type */

struct deviceID_spec {
	unsigned char		id; /* Device ID as read from the "DeviceIP reg */
	int			supported; /* Does this driver support it */
	int			intfType;  /* Type of interface it uses */
	int			maxtempcnt; /* Max temp sensors device supports */
	int			maxfancnt;  /* Max fans device supports */
	int			maxpumpcnt; /* Number of pump device has */
	char			*name;	    /* Device name */
};
typedef struct deviceID_spec devID_t;

/*
 * Now the hit list of what we are supporting and how
 */
static struct deviceID_spec CorsairID[] = {
	/*  D  s  i             t  f  p   n   */
	/*  e  u  n             e  a  u   a   */
	/*  v  p  t             m  n  m   m   */
	/*  I  R  f             p     p   e   */
	/*  D  D                              */
	{0x37, 0, DEVINTF_TYP1, 1, 2, 1, "h80"},
	{0x38, 0, DEVINTF_TYP1, 4, 5, 1, "coolNode"},
	{0x39, 0, DEVINTF_TYP1, 0, 0, 0, "lightNode"},
	{0x3a, 0, DEVINTF_TYP1, 1, 4, 1, "h100"},
	{0x3b, 1, DEVINTF_TYP2, 1, 4, 1, "h80i"},
	{0x3c, 1, DEVINTF_TYP2, 1, 4, 1, "h100i"},
	{0x3d, 0, DEVINTF_TYP2, 4, 6, 0, "extNode"},
	{0x00, 0, DEVINTF_NONE, 0, 0, 0, "unknown"}
};


/*
 * Driver State, CorsairLink used to save state of a channel to the device.
 */
#define NUMFANS			6 /* Number of "fans" the device/driver supports */

#define FAN0			0
#define FAN1			1
#define FAN2			2
#define FAN3			3
#define FAN4			4
#define PUMP			FAN4 /* This fan is really the pump on h80i/h100i */
#define FAN5			5    /* Only on a "Cooling Node" and not pumps */

#define NUMTEMPS		4    /* Only on a "cooling node", pumps have one */

/*
 * States of pending command related to interrupts
 */
#define CMD_IDLE		0 /* No command pending */
#define CMD_SEND		1 /* The cmd is/was sent and is in play */
#define CMD_AWAIT		2 /* We are waiting for a response */
#define CMD_DONE		3 /* We got a response (interrupt), command done */

struct CorsairLink {
	struct usb_device	*udev;		/* Linux USB device handle */
	struct device		*hwmon_dev;	/* sysfs hwmon support */
	/* Interrupt support */
	unsigned char		pend_cmdID;	/* Pending ID (message #) sent to device */
	unsigned char		pend_cmd;   	/* Pending command sent to device */
	struct urb		*irq;	    	/* URB to control interrupt in pipe */
	/*
	 * Note the following field gets dma'd into by devices.
	 * So we must minimize access to it to the interrupt routine
	 * Any other accesses may be unsafe. So we make a copy during
	 * the interrupt routine (to new_dat) and then only used that
	 * at other times.
	 */
	unsigned char		irq_buf[16]; 	/* USB/device uses to save recv'd data */
	unsigned char		new_dat[16];	/* irq copy of current data */
	atomic_t		irqcmd_state; 	/* Driver "interrupt" state */
	wait_queue_head_t	irq_wait;     	/* Interrupt sleep/wakeup control */
	struct mutex		irq_lock;	/* Lock to protect structure accesses */
	/* CorsairLink Device state */
	devID_t			*devid; 	/* What the device is */
	unsigned int		FirmwareID;
	CorsairFanInfo_t	fans[NUMFANS]; 	/* Fans and pump current state */
	CorsairTempInfo_t	temps[NUMTEMPS];/* Temp current state */
	unsigned int		CommandId; 	/* Current message number */
	int			rw_ms_timeo;	/* Timeout amount in MS */
};
typedef struct CorsairLink CorsairLink_t;


/***************************************************************/
/* Basic Low level Target interface routines                   */
/***************************************************************/

/*
 * Startup interrupts polling since we are about to do IO
 */
static int h80i_enableIRQ(CorsairLink_t *cl)
{
	if (usb_submit_urb(cl->irq, GFP_KERNEL)) {
                return -EIO;
	}
	return 0;
}

/*
 * We must be done with IO so stop polling for interrupts
 * (just incase its still running)
 */
static void h80i_disableIRQ(CorsairLink_t *cl)
{
	int retval;

	/*
	 * We do the following to prevent an extra status 
	 * of ENOENT getting delivered to our irq handler.
	 * I.E. There is no need to kill it if it is not running...
	 */
	retval = atomic_read(&cl->irqcmd_state);
#ifdef ENABLE_PICKY	
	if (retval == CMD_SEND || retval == CMD_AWAIT)
#endif
	{
		usb_kill_urb(cl->irq);
		atomic_set(&cl->irqcmd_state, CMD_IDLE);
	}
}

/*
 * Send a CorairLink command to the device
 */
static int h80i_sendcmd(CorsairLink_t *cl, unsigned char *buf, unsigned short size)
{
	return usb_control_msg(cl->udev, 		     /* dev */
			       usb_sndctrlpipe(cl->udev, 0), /* pipe */
			       HID_REQ_SET_REPORT, 	     /* 0x09 - request */
			       USB_TYPE_CLASS|USB_RECIP_INTERFACE|USB_DIR_OUT,
			                                     /* 0x21 - requesttype */
			       0x200|buf[0], 		     /* value */
			       0,     			     /* index */
			       buf,   			     /* payload data */
			       size,    		     /* payload size */
			       cl->rw_ms_timeo); 	     /* timeout */
}

/*
 * Send a command to the CorsairLink device and wait for an interrupt 
 * completion response and payload data if any. Note that this routine
 * can and most likely will sleep, awaiting an USB interrupt from the device.
 */
static int h80i_sendwait(struct usb_interface *interface, unsigned char *buf,
			 unsigned short size)
{
	CorsairLink_t *cl = usb_get_intfdata(interface);
	int retval;

	memset (cl->irq_buf, 0, 16);
	memset (cl->new_dat, 0, 16);

	/*
	 * So next message is not badly formed, this can not be 0.
	 * But also just in case of an error, we pick a range that will
	 * not look like a valid operation or register address.
	 */
	if (cl->CommandId == 0xff)
		cl->CommandId = 0x81;

	/* 
	 * The following two are part of requests to the CorsairLink device
	 * and are returned on a respnse to varify that the response matches
	 * the current awaiting request. Note this is needed since the nature
	 * of USB might allow stale replies to get sent to us multiple times.
	 */
	cl->pend_cmdID = buf[1]; /* Current CorsairLink message number */
	cl->pend_cmd = buf[2];	 /* Current CorsairLink comment */

	if (h80i_enableIRQ(cl)) {
		return -EIO;
	}
	retval = atomic_read(&cl->irqcmd_state);
	if (retval != CMD_IDLE)
		dev_err(&interface->dev, "send: bad initial cmd state %d\n",retval);

	atomic_set(&cl->irqcmd_state, CMD_SEND);
	retval = h80i_sendcmd(cl, buf, size);
	atomic_set(&cl->irqcmd_state, CMD_AWAIT);
	if (retval < 0 || retval != size) {
		dev_err(&interface->dev, "send: Failed to send cmd %d 0x%x\n",
			retval,retval);
		retval = -EIO;
		goto error;
	}

	retval = wait_event_timeout(cl->irq_wait,
				    atomic_read(&cl->irqcmd_state) == CMD_DONE,
				    cl->rw_ms_timeo/HZ);
	if (!retval) {
		dev_err(&interface->dev, "Wait: Timed out\n");
		retval = -EIO;
	}

	retval = size;
error:
	h80i_disableIRQ(cl);
	atomic_set(&cl->irqcmd_state, CMD_IDLE);
	return retval;
}


/***************************************************************/
/* High level device objects interface routines                */
/***************************************************************/


/*
 * Read the RPM of a selcted fan - sysfs interfacde routine
 */
static ssize_t fan_in(struct device *dev, struct device_attribute *devattr, char *buffer)
{
	struct usb_interface *interface = to_usb_interface(dev);
        struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	CorsairLink_t *cl = usb_get_intfdata(interface);
	unsigned char *buf;
	int indx = attr->index;
	ssize_t retval;

	buf = kmalloc(32, GFP_KERNEL);
	if (!buf) {
		dev_err(&interface->dev, "out of memory\n");
		return (-ENOMEM);
	}
	memset(buf, 0, 32);
	/* This request pkt contains two Corsair requests */
	/* First one selects a fan */
	buf[0] = 0x07;		  /* length */
	buf[1] = cl->CommandId++; /* Command Number */
	buf[2] = WriteOneByte;	  /* Corsair Operation */
	buf[3] = FAN_Select;	  /* address of operation */
	buf[4] = indx;		  /* Data to write - Fan number */
	/* 2nd command reads the actual RPM of selected fan */
	buf[5] = cl->CommandId++; /* Command Number */
	buf[6] = ReadTwoBytes;	  /* Data to read - measured fan RPM */
	buf[7] = FAN_ReadRPM;	  /* address of operation */

	mutex_lock(&cl->irq_lock); /* Only one request at atime */
	retval = h80i_sendwait(interface, buf, 11);
	if (retval < 0 || retval != 11) {
		dev_err(&interface->dev, "FanIn: failed\n");
		retval = sprintf(buffer, "ERROR\n");
	} else {
		cl->fans[indx].RPM = cl->new_dat[5] << 8 | cl->new_dat[4];
		retval = sprintf(buffer, "%u\n", cl->fans[indx].RPM);
	}
	mutex_unlock(&cl->irq_lock);
	kfree(buf);

        return retval;
}
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, fan_in, NULL, FAN0);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, fan_in, NULL, FAN1);
static SENSOR_DEVICE_ATTR(fan3_input, S_IRUGO, fan_in, NULL, FAN2);
static SENSOR_DEVICE_ATTR(fan4_input, S_IRUGO, fan_in, NULL, FAN3);
static SENSOR_DEVICE_ATTR(fan5_input, S_IRUGO, fan_in, NULL, PUMP); /* fan4 is the pump */
static SENSOR_DEVICE_ATTR(fan6_input, S_IRUGO, fan_in, NULL, FAN5);

/*
 * Read the RPM of a selcted fan - sysfs interfacde routine
 */
static ssize_t fan_max(struct device *dev, struct device_attribute *devattr, char *buffer)
{
	struct usb_interface *interface = to_usb_interface(dev);
        struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	CorsairLink_t *cl = usb_get_intfdata(interface);
	unsigned char *buf;
	int indx = attr->index;
	ssize_t retval;

	buf = kmalloc(32, GFP_KERNEL);
	if (!buf) {
		dev_err(&interface->dev, "out of memory\n");
		return (-ENOMEM);
	}
	memset(buf, 0, 32);
	/* This request pkt contains two Corsair requests */
	/* First one selects a fan */
	buf[0] = 0x07;		  /* length */
	buf[1] = cl->CommandId++; /* Command Number */
	buf[2] = WriteOneByte;	  /* Corsair Operation */
	buf[3] = FAN_Select;	  /* address of operation */
	buf[4] = indx;		  /* Data to write - Fan number */
	/* 2nd command reads the actual RPM of selected fan */
	buf[5] = cl->CommandId++; /* Command Number */
	buf[6] = ReadTwoBytes;	  /* Data to read - measured fan RPM */
	buf[7] = FAN_MaxRecordedRPM;	  /* address of operation */

	mutex_lock(&cl->irq_lock); /* Only one request at atime */
	retval = h80i_sendwait(interface, buf, 11);
	if (retval < 0 || retval != 11) {
		dev_err(&interface->dev, "FanIn: failed\n");
		retval = sprintf(buffer, "ERROR\n");
	} else {
		cl->fans[indx].maxRPM = cl->new_dat[5] << 8 | cl->new_dat[4];
		retval = sprintf(buffer, "%u\n", cl->fans[indx].maxRPM);
	}
	mutex_unlock(&cl->irq_lock);
	kfree(buf);

        return retval;
}
static SENSOR_DEVICE_ATTR(fan1_max, S_IRUGO, fan_max, NULL, FAN0);
static SENSOR_DEVICE_ATTR(fan2_max, S_IRUGO, fan_max, NULL, FAN1);
static SENSOR_DEVICE_ATTR(fan3_max, S_IRUGO, fan_max, NULL, FAN2);
static SENSOR_DEVICE_ATTR(fan4_max, S_IRUGO, fan_max, NULL, FAN3);
static SENSOR_DEVICE_ATTR(fan5_max, S_IRUGO, fan_max, NULL, PUMP); /* fan4 is the pump */
static SENSOR_DEVICE_ATTR(fan6_max, S_IRUGO, fan_max, NULL, FAN5);

static unsigned int convFraqTemp(unsigned int read_temp)
{
	unsigned int Temp;
	int indx;

	for (indx = 0, Temp = 0; indx < 8; indx++) {
		if (read_temp & temp256[indx].bits)
			Temp += temp256[indx].value;
	}

	return Temp;
}

/*
 * Read the Temp of a selcted sensor - sysfs interfacde routine
 */
static ssize_t temp_in(struct device *dev, struct device_attribute *devattr, char *buffer)
{
	struct usb_interface *interface = to_usb_interface(dev);
        struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	CorsairLink_t *cl = usb_get_intfdata(interface);
	unsigned char *buf;
	int sensor = attr->index;
	int retval;
	unsigned int Temp = 0;

	buf = kmalloc(32, GFP_KERNEL);
	if (!buf) {
		dev_err(&interface->dev, "out of memory\n");
		return (-ENOMEM);
	}
	memset(buf, 0, 32);
	/* This request pkt contains two Corsair requests */
	/* First one selects a temp sensor */
	buf[0] = 0x07;		  		/* length */
	buf[1] = cl->CommandId++; 		/* Command message number */
	buf[2] = WriteOneByte;	  		/* Corsair Operation */
	buf[3] = TEMP_SelectActiveSensor; 	/* address of operation */
	buf[4] = sensor;	  		/* Data to write - sensor number */
	/* 2nd command reads the actual tempurature */
	buf[5] = cl->CommandId++; 		/* Command message number */
	buf[6] = ReadTwoBytes;	  		/* Corsair Operation */
	buf[7] = TEMP_Read;	  		/* address of operation */

	mutex_lock(&cl->irq_lock);
	retval = h80i_sendwait(interface, buf, 11);
	if (retval < 0 || retval != 11) {
		dev_err(&interface->dev, "TempIn: failed %d 0x%x\n", retval, retval);
		retval = sprintf(buffer, "ERROR\n");
	} else {
		cl->temps[sensor].wholDeg = cl->new_dat[5];	/* Only whole degree number */
		cl->temps[sensor].partDeg = cl->new_dat[4];	/* 1/256's of degree */
		Temp = convFraqTemp(cl->temps[sensor].partDeg);
		Temp += cl->temps[sensor].wholDeg * 1000;
		retval = sprintf(buffer, "%u\n", Temp);
	}
	mutex_unlock(&cl->irq_lock);
	kfree(buf);

        return retval;
}
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, temp_in, NULL, 0);
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO, temp_in, NULL, 1);
static SENSOR_DEVICE_ATTR(temp3_input, S_IRUGO, temp_in, NULL, 2);
static SENSOR_DEVICE_ATTR(temp4_input, S_IRUGO, temp_in, NULL, 3);


#ifdef BSH_NOTYET
static SENSOR_DEVICE_ATTR(LEDmode, S_IRUGO | S_IWUSR | S_IWGRP, show_led, set_led, 0);
#endif

/*
 * sysfs and lm-sensors require a name entry and this is it
 */
static ssize_t show_name(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct usb_interface *interface = to_usb_interface(dev);
	CorsairLink_t *cl = usb_get_intfdata(interface);

	if (cl && cl->devid) {
		return sprintf(buf, "%s\n", cl->devid->name);
	} else {
		return sprintf(buf, "CorsairLink\n");
	}
}
static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);


static struct attribute *h80i_attributes[] = {
        &dev_attr_name.attr,
        NULL
};

static const struct attribute_group h80i_group = {
        .attrs = h80i_attributes,
};


/***************************************************************/
/* High level driver interface routines                        */
/***************************************************************/

/*
 * Get the CorsairLink device ID so we know what we can and cannot do.
 */
static int devid_in(struct usb_interface *interface)
{
	CorsairLink_t *cl = usb_get_intfdata(interface);
	unsigned char *buf;
	int retval;
	int indx;
	

	buf = kmalloc(32, GFP_KERNEL);
	if (!buf) {
		dev_err(&interface->dev, "out of memory\n");
		return (-ENOMEM);
	}
	memset(buf, 0, 32);
	/* This request pkt contains a single Corsair request */
	buf[0] = 0x03;		  /* length */
	buf[1] = cl->CommandId++; /* Command message Number */
	buf[2] = ReadOneByte;	  /* Corsair Operation */
	buf[3] = DeviceID;	  /* address of operation */

	mutex_lock(&cl->irq_lock);
	retval = h80i_sendwait(interface, buf, 17);
	if (retval < 0 || retval != 17) {
		dev_err(&interface->dev, "devID: failed %d 0x%x\n", retval, retval);
		cl->devid = NULL;
	} else {
		retval = -ENOENT;
		for (indx = 0; CorsairID[indx].id != 0; indx++) {
			if (cl->new_dat[2] == CorsairID[indx].id) {
				retval = 0;
				break;
			}
		}
		cl->devid = &CorsairID[indx];
	}
	if (retval || cl->devid == NULL)
		goto error;
	mutex_unlock(&cl->irq_lock);

	memset(buf, 0, 32);
	/* This request pkt contains a single Corsair request */
	buf[0] = 0x03;		  /* length */
	buf[1] = cl->CommandId++; /* Command message Number */
	buf[2] = ReadTwoBytes;	  /* Corsair Operation */
	buf[3] = FirmwareID;	  /* address of operation */

	mutex_lock(&cl->irq_lock);
	retval = h80i_sendwait(interface, buf, 17);
	if (retval < 0 || retval != 17) {
		dev_err(&interface->dev, "FirmwareID: failed %d 0x%x\n", retval, retval);
		cl->devid = NULL;
	} else {
		retval = 0;
		cl->FirmwareID = cl->new_dat[2] | cl->new_dat[3] << 8;
	}
error:	
	mutex_unlock(&cl->irq_lock);
	kfree(buf);
	return retval;
}

/*
 * Interrupt handler - Used to read data from the device.
 */
static void h80i_irq(struct urb *urb)
{
	CorsairLink_t *cl = urb->context;
	unsigned char *irq_buf = urb->transfer_buffer;
	int retval;

        switch (urb->status) {
        case 0:                 /* success */
	case -EOVERFLOW:	/* we ask for less bytes then whole USB xfer  */
                break;
        case -ECONNRESET:       /* unlink */
        case -ESHUTDOWN:
                return;
        /* -EPIPE:  should clear the halt */
        case -ENOENT:
        default:                /* error */
                goto resubmit;
        }

	/* Does the reply match our current request */
	if (irq_buf[0] == cl->pend_cmdID || irq_buf[1] == cl->pend_cmd) {
		retval = atomic_read(&cl->irqcmd_state);
		/* Only if a request reply is pending */
		if (retval == CMD_SEND || retval == CMD_AWAIT) {
			memcpy(cl->new_dat, cl->irq_buf, 16);
			atomic_set(&cl->irqcmd_state, CMD_DONE);
			wake_up(&cl->irq_wait);
			return;
		}
	}
resubmit:
        usb_submit_urb(urb, GFP_ATOMIC);
}

static void *fanIndxToAttr[NUMFANS + 1] = {
	&sensor_dev_attr_fan1_input.dev_attr,
	&sensor_dev_attr_fan2_input.dev_attr,
	&sensor_dev_attr_fan3_input.dev_attr,
	&sensor_dev_attr_fan4_input.dev_attr,
	&sensor_dev_attr_fan5_input.dev_attr,
	&sensor_dev_attr_fan6_input.dev_attr,
	NULL
};

static void *fanmaxIndxToAttr[NUMFANS + 1] = {
	&sensor_dev_attr_fan1_max.dev_attr,
	&sensor_dev_attr_fan2_max.dev_attr,
	&sensor_dev_attr_fan3_max.dev_attr,
	&sensor_dev_attr_fan4_max.dev_attr,
	&sensor_dev_attr_fan5_max.dev_attr,
	&sensor_dev_attr_fan6_max.dev_attr,
	NULL
};

static void *tempIndxToAttr[NUMFANS + 1] = {
	&sensor_dev_attr_temp1_input.dev_attr,
	&sensor_dev_attr_temp2_input.dev_attr,
	&sensor_dev_attr_temp3_input.dev_attr,
	&sensor_dev_attr_temp4_input.dev_attr,
	NULL
};

/*
 * Main driver interface that probes and gets everything going.
 */
static int h80i_probe(struct usb_interface *interface,
			      const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_host_interface *hiface;
	struct usb_endpoint_descriptor *endpoint;
	CorsairLink_t *cl = NULL;
	unsigned char *buf;
	int retval = -ENOMEM;
	int indx;
	int pipe;
	int maxp;

	/*
	 * Make sure right device - some extra checks
	 */
	hiface = interface->cur_altsetting;
	/* 
	 * h80i/h100i have 1 endpoint:
	 * 1 is interrupt in 
	 * CLink hub has 2:
	 * 1 is interrupt in
	 * 2 is interrupt out
	 */
        if (hiface->desc.bNumEndpoints != 1 && hiface->desc.bNumEndpoints != 2)
                return -ENODEV;
        endpoint = &hiface->endpoint[0].desc;
        if (!usb_endpoint_is_int_in(endpoint))
                return -ENODEV;

	/* Interrupt endpoint pipe */
        pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);
	/* Largest message size supported */
        maxp = usb_maxpacket(udev, pipe, usb_pipeout(pipe));

	/*
	 * Allocate a per instance state structure
	 */
	cl = kzalloc(sizeof(CorsairLink_t), GFP_KERNEL);
	if (cl == NULL) {
		dev_err(&interface->dev, "cl out of memory\n");
		return -ENOMEM;
	}
	cl->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (cl->irq == NULL) {
		dev_err(&interface->dev, "urb alloc - out of memory\n");
		goto error_mem;
	}

	cl->CommandId = 0x81;	/* Starting command message number */
	cl->rw_ms_timeo = 5000; /* Give the request/response up to 5 seconds */

	cl->udev = usb_get_dev(udev);
	usb_set_intfdata(interface, cl);

	/*
	 * Setup interrupt handler
	 */
	usb_fill_int_urb(cl->irq, udev, pipe,
                         &cl->irq_buf,
			 (maxp > sizeof(cl->irq_buf) ? sizeof(cl->irq_buf) : maxp),
                         h80i_irq, cl, endpoint->bInterval);
	init_waitqueue_head(&cl->irq_wait);

	/* userland access flow control - we are single threaded and so is device */
	mutex_init(&cl->irq_lock);

	/*
	 * Find out the device type found
	 */
	if (devid_in(interface) != 0) {
		dev_err(&interface->dev, "Failed to get device ID - NOT attached\n");
		goto error1;
	}
	if (cl->devid->supported == 0) {
		if (cl->devid->id != 0) {
			dev_info(&interface->dev, "%s device found but not yet supported\n",
				 cl->devid->name);
		} else {
			dev_info(&interface->dev, "device NOT found\n");
		}
		dev_err(&interface->dev, "%s device NOT attached\n", cl->devid->name);
		goto error1;
	}


	/*
	 * Now scan fans to find out which ones are present if any
	 */
	buf = kmalloc(32, GFP_KERNEL);
	if (!buf) {
		dev_err(&interface->dev, "out of memory\n");
		retval = -ENOMEM;
		goto error1;
	}
	memset(buf, 0, 32);

	/* Probe the fans */
	for (indx = 0; indx < cl->devid->maxfancnt + cl->devid->maxpumpcnt; indx++) {
		memset(&cl->fans[indx].Name, 0x00, sizeof(cl->fans[indx].Name));
		if (indx == PUMP && cl->devid->maxpumpcnt)
			snprintf(&cl->fans[indx].Name[0],
				 sizeof(cl->fans[indx].Name), "Pump");
		else
			snprintf(&cl->fans[indx].Name[0], 
				 sizeof(cl->fans[indx].Name), "Fan %d", indx + 1);

		buf[0] = 0x07;	/* length */
		buf[1] = cl->CommandId++; /* Command ID */
		buf[2] = WriteOneByte;
		buf[3] = FAN_Select;
		buf[4] = indx;	/* Fan number */
		buf[5] = cl->CommandId++; /* Command ID */
		buf[6] = ReadOneByte;
		buf[7] = FAN_Mode;

		retval = h80i_sendwait(interface, buf, 11);
		if (retval < 0 || retval != 11)
			dev_err(&interface->dev, "Probe FanMode: fan %d failed %d 0x%x\n",
				indx, retval, retval);
		else
			cl->fans[indx].Mode = cl->new_dat[4];

		buf[0] = 0x07;	/* length */
		buf[1] = cl->CommandId++; /* Command ID */
		buf[2] = WriteOneByte;
		buf[3] = FAN_Select;
		buf[4] = indx;	/* Fan number */
		buf[5] = cl->CommandId++; /* Command ID */
		buf[6] = ReadTwoBytes;
		buf[7] = FAN_ReadRPM;

		retval = h80i_sendwait(interface, buf, 11);
		if (retval < 0 || retval != 11)
			dev_err(&interface->dev, "Probe FanRPM: fan %d failed %d 0x%x\n",
				indx, retval, retval);
		else
			cl->fans[indx].RPM = cl->new_dat[5] << 8 | cl->new_dat[4];

		buf[0] = 0x07;	/* length */
		buf[1] = cl->CommandId++; /* Command ID */
		buf[2] = WriteOneByte;
		buf[3] = FAN_Select;
		buf[4] = indx;	/* Fan number */
		buf[5] = cl->CommandId++; /* Command ID */
		buf[6] = ReadTwoBytes;
		buf[7] = FAN_MaxRecordedRPM;

		retval = h80i_sendwait(interface, buf, 11);
		if (retval < 0 || retval != 11)
			dev_err(&interface->dev, "Probe FanmaxRPM: fan %d failed %d 0x%x\n",
				indx, retval, retval);
		else
			cl->fans[indx].maxRPM = cl->new_dat[5] << 8 | cl->new_dat[4];

		if (cl->fans[indx].Mode & FAN_PRSNT) {
			retval = device_create_file(&interface->dev, fanIndxToAttr[indx]);
			if (retval) {
				kfree(buf);
				goto error;
			}
			retval = device_create_file(&interface->dev, fanmaxIndxToAttr[indx]);
			if (retval) {
				kfree(buf);
				goto error;
			}
			dev_info(&interface->dev, "%s %s Mode %x RPM %d Max %d\n", cl->devid->name,
				 cl->fans[indx].Name, cl->fans[indx].Mode, cl->fans[indx].RPM,
				 cl->fans[indx].maxRPM);
		} else
			dev_info(&interface->dev, "%s %s Mode %x NOT PRESENT\n", 
				 cl->devid->name, cl->fans[indx].Name, cl->fans[indx].Mode);
	}

	memset(buf, 0, 32);

	/* Probe the temp sensors */
	for (indx = 0; indx < cl->devid->maxtempcnt; indx++) {
		memset(&cl->temps[indx].Name, 0x00, sizeof(cl->temps[indx].Name));
		snprintf(&cl->temps[indx].Name[0],  sizeof(cl->temps[indx].Name),
			 "Temp %d", indx + 1);

		buf[0] = 0x07;	/* length */
		buf[1] = cl->CommandId++; /* Command ID */
		buf[2] = WriteOneByte;
		buf[3] = TEMP_SelectActiveSensor;
		buf[4] = indx;	/* Fan number */
		buf[5] = cl->CommandId++; /* Command ID */
		buf[6] = ReadOneByte;
		buf[7] = TEMP_Read;

		retval = h80i_sendwait(interface, buf, 11);
		if (retval < 0 || retval != 11)
			dev_err(&interface->dev, "Probe TempIn: Temp %d failed %d 0x%x\n",
				indx, retval, retval);
		else {
			cl->temps[indx].wholDeg = cl->new_dat[5];	/* whole degree's */
			cl->temps[indx].partDeg = cl->new_dat[4];	/* 1/256's of degree */
		}
		if (cl->temps[indx].wholDeg != 0 && 
		    cl->temps[indx].wholDeg < 120) {
			retval = device_create_file(&interface->dev, tempIndxToAttr[indx]);
			if (retval) {
				kfree(buf);
				goto error;
			}
			dev_info(&interface->dev, "%s %s %d.%d Deg C\n",
				 cl->devid->name,
				 cl->temps[indx].Name,
				 cl->temps[indx].wholDeg,
				 convFraqTemp(cl->temps[indx].partDeg));
		} else {
			dev_info(&interface->dev, "%s %s NOT PRESENT\n", 
				 cl->devid->name, cl->temps[indx].Name);
		}
	}

	kfree(buf);

#ifdef BSH_NOTYET
	retval = device_create_file(&interface->dev, &sensor_dev_attr_LEDmode.dev_attr);
	if (retval)
		goto error;
#endif
	retval = sysfs_create_group(&interface->dev.kobj, &h80i_group);
	if (retval) {
		dev_err(&interface->dev, "h80i sysfs group name crate failed\n");
		goto error;
	}
	cl->hwmon_dev = hwmon_device_register(&interface->dev);
	if (IS_ERR(cl->hwmon_dev)) {
		dev_err(&interface->dev, "hwmon reg failed\n");
		cl->hwmon_dev = NULL;
		goto error;
	}


	dev_info(&interface->dev, "%s cooler device V %x now attached\n",
		 cl->devid->name, cl->FirmwareID);
	return 0;

error:
	if (cl && cl->hwmon_dev) {
		hwmon_device_unregister(cl->hwmon_dev);
		cl->hwmon_dev = NULL;
	}
        sysfs_remove_group(&interface->dev.kobj, &h80i_group);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan1_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan2_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan3_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan4_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan5_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan1_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan2_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan3_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan4_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan5_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_temp1_input.dev_attr);
#ifdef BSH_NOTYET
	device_remove_file(&interface->dev, &sensor_dev_attr_LEDmode.dev_attr);
#endif
error1:
	usb_set_intfdata(interface, NULL);
	usb_put_dev(cl->udev);
error_mem:
	if (cl->irq)
		usb_free_urb(cl->irq);
	kfree(cl);
	return retval;
}

static void h80i_disconnect(struct usb_interface *interface)
{
	CorsairLink_t *cl = usb_get_intfdata(interface);

	if (cl && cl->hwmon_dev) {
		hwmon_device_unregister(cl->hwmon_dev);
		cl->hwmon_dev = NULL;
	}
        sysfs_remove_group(&interface->dev.kobj, &h80i_group);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan1_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan2_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan3_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan4_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan5_input.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan1_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan2_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan3_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan4_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_fan5_max.dev_attr);
	device_remove_file(&interface->dev, &sensor_dev_attr_temp1_input.dev_attr);
#ifdef BSH_NOTYET
	device_remove_file(&interface->dev, &sensor_dev_attr_LEDmode.dev_attr);
#endif
	/* first remove the files, then set the pointer to NULL */
	usb_set_intfdata(interface, NULL);
	if (cl) {
		if (cl->irq)
			usb_free_urb(cl->irq);
		usb_put_dev(cl->udev);
		kfree(cl);
	}
	dev_info(&interface->dev, "H80i/H100i cooler now disconnected\n");
}

static struct usb_driver h80i_driver = {
	.name =		"h80i",
	.probe =	h80i_probe,
	.disconnect =	h80i_disconnect,
	.id_table =	id_table,
};

module_usb_driver(h80i_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
