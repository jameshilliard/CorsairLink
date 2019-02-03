
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "hidapi.h"
#include "CorsairFanInfo.h"
#include "CorsairLink.h"

#define CLINK_HUB 1

int Initialize(CorsairLink_t *cl, int interface)
{
	int res;
	int deviceId;
	unsigned char buf[256];

	if(cl->handle == NULL){
		if (hid_init())
			return 0;
			
		// Set up the command buffer.
		memset(buf,0x00,sizeof(buf));
		buf[0] = 0x01;
		buf[1] = 0x81;
		
		// Open the device using the VID, PID,
		// and optionally the Serial number.
		// open Corsair H80i or H100i cooler 
		if (interface == CLINK) {
			cl->handle = hid_open(0x1b1c, 0x0c02, NULL); /* Old Cooling node */
			if (!cl->handle) {
				fprintf(stderr,
					"Error: Unable to open Corsair Cooler Node\n");
				return 0;
			}
		} else {
			cl->handle = hid_open(0x1b1c, 0x0c04, NULL); /* H80i/H100i */
			if (!cl->handle) {
				fprintf(stderr,
					"Error: Unable to open Corsair H80i or H100i CPU Cooler\n");
				return 0;
			}
		}

		hid_set_nonblocking(cl->handle, 1);
		
		// Read Device ID: 0x3b = H80i. 0x3c = H100i
		buf[0] = 0x03; // Length
		buf[1] = cl->CommandId++; // Command ID
		buf[2] = ReadTwoBytes; // Command Opcode
		buf[3] = DeviceID; // Command data...
		
		if (interface == CLINK) {
			res = hid_write(cl->handle, &buf[1], 3);
		} else {
			res = hid_write(cl->handle, buf, 11);
		}
		if (res < 0) {
			fprintf(stderr, "Error: Unable to write() %s\n", hid_error(cl->handle));
		}
		memset(buf,0x00,sizeof(buf));
		res = hid_read_wrapper(cl, cl->handle, buf);
		if (res < 0) {
			fprintf(stderr, "Error: Unable to read() %s\n", hid_error(cl->handle));
		}
		deviceId = buf[2];

		if (interface == CLINK) {
			if (deviceId != 0x38) {
				fprintf(stderr,
					"Device ID: %02hhx mismatch. Not Corsair Cooling Node\n", deviceId);
				Close(cl);
				return 0;
			}
		} else {
			if ((deviceId != 0x3b) && (deviceId != 0x3c)) {
				fprintf(stderr,
					"Device ID: %02hhx mismatch. Not Corsair H80i or H100i CPU Cooler\n", deviceId);
				Close(cl);
				return 0;
			}
		}
	} else {
		fprintf(stderr, "Cannot initialize twice\n");
		return 0;
	}
	return 1;
}

int CorsairLink_init(CorsairLink_t *cl,int interface) {
	cl->handle = NULL;
	cl->CommandId = 0x81;
	cl->max_ms_read_wait = 5000;
	//fans = new CorsairFanInfo[5];
	return Initialize(cl, interface);
}

int ConnectedTemps(CorsairLink_t *cl, int interface) {
	int sensors = 0;
	unsigned char buf[256];
	int res;
	
	if (interface == CLINK) {
		sensors = 4;
	} else {
		memset(buf,0x00,sizeof(buf));
		// Read number of temp sensors
		buf[0] = 0x03; // Length
		buf[1] = cl->CommandId++; // Command ID
		buf[2] = ReadOneByte; // Command Opcode
		buf[3] = TEMP_CountSensors; // Command data...
		
		res = hid_write(cl->handle, buf, 11);
		if (res < 0) {
			fprintf(stderr, "Error: Unable to write() %s\n", hid_error(cl->handle));
		}
		
		res = hid_read_wrapper(cl, cl->handle, buf);
		if (res < 0) {
			fprintf(stderr, "Error: Unable to read() %s\n", hid_error(cl->handle));
		}
		sensors = buf[2];
	}
	return sensors;
}


int TempIndxToPort[] = {
	0x0a,
	0x09,
	0x08,
	0x07
};

unsigned short ReadTempInfo(CorsairLink_t *cl, int interface, int indx)
{
	unsigned short Temp = 0;
	int res = 0;
	unsigned char buf[256];

	memset(buf,0x00,sizeof(buf));

	if (interface == CLINK) {
		// Read fan Temp
		buf[0] = 0x03; // Length
		buf[1] = cl->CommandId++; // Command ID
		buf[2] = ReadTwoBytes; // Command Opcode
		buf[3] = TempIndxToPort[indx]; // Command data...
		res = hid_write(cl->handle, &buf[1], 3);
	} else {
		// Read fan Temp
		buf[0] = 0x07; // Length
		buf[1] = cl->CommandId++; // Command ID
		buf[2] = WriteOneByte; // Command Opcode
		buf[3] = TEMP_SelectActiveSensor; // Command data...
		buf[4] = indx; // Command data...
		buf[5] = cl->CommandId++; // Command ID
		buf[6] = ReadTwoBytes; // Command Opcode
		buf[7] = TEMP_Read; // Command data...
		res = hid_write(cl->handle, buf, 11);
	}
	if (res < 0) {
		fprintf(stderr, "Error: Unable to write for temp %s\n", hid_error(cl->handle));
	}
	
	memset(buf,0x00,sizeof(buf));
	res = hid_read_wrapper(cl, cl->handle, buf);
	if (res < 0) {
		fprintf(stderr, "Error: Unable to read temp %s\n", hid_error(cl->handle));
	}

	//All data is little-endian.
	if (interface == CLINK) {
		Temp = buf[3] << 8;
		Temp += buf[2];
	} else {
		Temp = buf[5] << 8;
		Temp += buf[4];
	}
	return Temp;
}
		
int ConnectedFans(CorsairLink_t *cl, int interface)
{
	int fans = 0;
	
	if (interface == CLINK) {
		fans = 5;
	} else {
		int i = 0;
		int fanMode = 0;
		unsigned char buf[256];
		int res;

		for (i = 0; i < NUMFANS; i++) {
			memset(buf,0x00,sizeof(buf));
			// Read fan Mode
			buf[0] = 0x07; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FAN_Select; // Command data...
			buf[4] = i; // select fan
			buf[5] = cl->CommandId++; // Command ID
			buf[6] = ReadOneByte; // Command Opcode
			buf[7] = FAN_Mode; // Command data...
		
			res = hid_write(cl->handle, buf, 11);
			if (res < 0) {
				fprintf(stderr, "Error: Unable to write() %s\n", hid_error(cl->handle));
			}

			res = hid_read_wrapper(cl, cl->handle, buf);
			if (res < 0) {
				fprintf(stderr, "Error: Unable to read() %s\n", hid_error(cl->handle));
			}
			fanMode = buf[4];
		
			if(fanMode != 0x03){
				fans++;
			}
		}
	}
	return fans;
}

int FanModeIndxToPort[] = {
	0x20,
	0x30,
	0x40,
	0x50,
	0x60
};
int FanRPMIndxToPort[] = {
	0x0b,
	0x0c,
	0x0d,
	0x0e,
	0x0f
};
int FanMaxRPMIndxToPort[] = {
	0x10,
	0x11,
	0x12,
	0x13,
	0x14
};
int FanFixRPMIndxToPort[] = {
	0x22,
	0x32,
	0x42,
	0x52,
	0x62
};

void ReadFansInfo(CorsairLink_t *cl, int interface){
	int i = 0;
	int fanMode = 0;
	int res = 0;
	int rpm, maxrpm;
	unsigned char buf[256];

	for (i = 0; i < NUMFANS; i++) {
		memset(&cl->fans[i].Name, 0x00, sizeof(cl->fans[i].Name));
		if (interface == CLINK) {
			snprintf(&cl->fans[i].Name[0], sizeof(cl->fans[i].Name), "Fan %d", i + 1);
		} else {
			if(i < 4){
				snprintf(&cl->fans[i].Name[0], sizeof(cl->fans[i].Name), "Fan %d",
					 i + 1);
			} else {
				snprintf(&cl->fans[i].Name[0], sizeof(cl->fans[i].Name), "Pump");
			}
		}

		memset(buf,0x00,sizeof(buf));
		// Read fan Mode
		if (interface == CLINK) {
			buf[0] = 0x03; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = ReadTwoBytes; // Command Opcode
			buf[3] = FanModeIndxToPort[i]; // Command data...
			res = hid_write(cl->handle, &buf[1], 3);
		} else {
			buf[0] = 0x07; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FAN_Select; // Command data...
			buf[4] = i; // select fan
			buf[5] = cl->CommandId++; // Command ID
			buf[6] = ReadOneByte; // Command Opcode
			buf[7] = FAN_Mode; // Command data...
			res = hid_write(cl->handle, buf, 11);
		}

		if (res < 0) {
			fprintf(stderr, "Error: Unable to write() %s\n", hid_error(cl->handle));
		}

		res = hid_read_wrapper(cl, cl->handle, buf);
		if (res < 0) {
			fprintf(stderr, "Error: Unable to read() %s\n", hid_error(cl->handle));
		}
		if (interface == CLINK) {
			fanMode = buf[2];
		} else {
			fanMode = buf[4];
		}
		memset(buf,0x00,sizeof(buf));
		// Read fan RPM
		if (interface == CLINK) {
			buf[0] = 0x03; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = ReadTwoBytes; // Command Opcode
			buf[3] = FanRPMIndxToPort[i]; // Command data...
			res = hid_write(cl->handle, &buf[1], 3);
		} else {
			buf[0] = 0x07; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FAN_Select; // Command data...
			buf[4] = i; // select fan
			buf[5] = cl->CommandId++; // Command ID
			buf[6] = ReadTwoBytes; // Command Opcode
			buf[7] = FAN_ReadRPM; // Command data...
			res = hid_write(cl->handle, buf, 11);
		}

		if (res < 0) {
			fprintf(stderr, "Error: Unable to write() %s\n", hid_error(cl->handle));
		}

		res = hid_read_wrapper(cl, cl->handle, buf);
		//All data is little-endian.
		if (interface == CLINK) {
			rpm = buf[3] << 8;
			rpm += buf[2];
		} else {
			rpm = buf[5] << 8;
			rpm += buf[4];
		}

		memset(buf,0x00,sizeof(buf));
		// Read fan RPM
		if (interface == CLINK) {
			buf[0] = 0x03; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = ReadTwoBytes; // Command Opcode
			buf[3] = FanMaxRPMIndxToPort[i]; // Command data...
			res = hid_write(cl->handle, &buf[1], 32);
		} else {
			buf[0] = 0x07; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FAN_Select; // Command data...
			buf[4] = i; // select fan
			buf[5] = cl->CommandId++; // Command ID
			buf[6] = ReadTwoBytes; // Command Opcode
			buf[7] = FAN_MaxRecordedRPM; // Command data...
			res = hid_write(cl->handle, buf, 11);
		}

		if (res < 0) {
			fprintf(stderr, "Error: Unable to write() %s\n", hid_error(cl->handle));
		}

		res = hid_read_wrapper(cl, cl->handle, buf);
		//All data is little-endian.
		if (interface == CLINK) {
			maxrpm = buf[3] << 8;
			maxrpm += buf[2];
		} else {
			maxrpm = buf[5] << 8;
			maxrpm += buf[4];
		}

		cl->fans[i].Mode = fanMode;
		cl->fans[i].RPM = rpm;
		cl->fans[i].maxRPM = maxrpm;

	}

}

int SetFansInfo(CorsairLink_t *cl, int interface, int fanIndex, CorsairFanInfo_t *fanInfo){
	unsigned char buf[256];
	int rpm;
	int res;

	memset(buf,0x00,sizeof(buf));
	if(fanInfo->Mode == FixedPWM || fanInfo->Mode == FixedRPM
		|| fanInfo->Mode == Default || fanInfo->Mode == Quiet
		|| fanInfo->Mode == Balanced || fanInfo->Mode == Performance
		|| fanInfo->Mode == Custom) {

		if (interface == CLINK) {
			buf[0] = 0x04; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FanModeIndxToPort[fanIndex]; // Command data...
			buf[4] = fanInfo->Mode;

			res = hid_write(cl->handle, &buf[1], 4);
		} else {
			buf[0] = 0x0b; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FAN_Select; // Command data...
			buf[4] = fanIndex; // select fan
			buf[5] = cl->CommandId++; // Command ID
			buf[6] = WriteOneByte; // Command Opcode
			buf[7] = FAN_Mode; // Command data...
			buf[8] = fanInfo->Mode;
			buf[9] = cl->CommandId++; // Command ID
			buf[10] = ReadOneByte; // Command Opcode
			buf[11] = FAN_Mode; // Command data...

			res = hid_write(cl->handle, buf, 17);
		}
		if (res < 0) {
			fprintf(stderr, "SetFan: write failed %s\n", hid_error(cl->handle));
			return 1;
		}

		res = hid_read_wrapper(cl, cl->handle, buf);
		if (res < 0) {
			fprintf(stderr, "SetFan: read failed %s\n", hid_error(cl->handle));
			return 1;
		}
		if (interface == H80I) {
			if(fanInfo->Mode != buf[6]){
				fprintf(stderr, "SetFan: Cannot set fan mode.\n");
				return 1;
			}
		}
	} else {
		fprintf(stderr, "Invalid fan mode.\n");
		return 1;
	}
	if(fanInfo->RPM != 0) {
		memset(buf,0x00,sizeof(buf));

		if (interface == CLINK) {
			buf[0] = 0x05; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FanFixRPMIndxToPort[fanIndex]; // Command data...
			buf[4] = fanInfo->RPM & 0x00ff;
			buf[5] = fanInfo->RPM >> 8;

			res = hid_write(cl->handle, &buf[1], 5);
		} else {
			buf[0] = 0x0b; // Length
			buf[1] = cl->CommandId++; // Command ID
			buf[2] = WriteOneByte; // Command Opcode
			buf[3] = FAN_Select; // Command data...
			buf[4] = fanIndex; // select fan
			buf[5] = cl->CommandId++; // Command ID
			buf[6] = WriteTwoBytes; // Command Opcode
			buf[7] = FAN_FixedRPM; // Command data...
			buf[8] = fanInfo->RPM & 0x00FF;
			buf[9] = fanInfo->RPM >> 8;
			buf[10] = cl->CommandId++; // Command ID
			buf[11] = ReadTwoBytes; // Command Opcode
			buf[12] = FAN_ReadRPM; // Command data...

			res = hid_write(cl->handle, buf, 18);
		}
		if (res < 0) {
			fprintf(stderr, "SetFan: write failed %s\n", hid_error(cl->handle));
			return 1;
		}

		res = hid_read_wrapper(cl, cl->handle, buf);
		if (res < 0) {
			fprintf(stderr, "SetFan: read failed %s\n", hid_error(cl->handle));
			return 1;
		}
		//All data is little-endian.
		if (interface != CLINK) {
			rpm = buf[7] << 8;
			rpm += buf[6];
			if(fanInfo->RPM != rpm){
				fprintf(stderr, "SetFan: Cannot set fan mode.\n");
				return 1;
			}
		}
	}

	return 0;
}

void Close(CorsairLink_t *cl) {
	if(cl->handle != NULL){	
		hid_close(cl->handle);
		hid_exit();
		cl->handle = NULL;
	}
}

char *GetManufacturer(CorsairLink_t *cl){
	char *str;
	wchar_t wstr[MAX_STR];
	int res;

	wstr[0] = 0x0000;
	res = hid_get_manufacturer_string(cl->handle, wstr, MAX_STR);
	if (res < 0)
		fprintf(stderr, "Unable to read manufacturer string\n");
	str = malloc((wcslen(wstr)+2) * sizeof(wchar_t));
	memcpy(str, wstr, ((wcslen(wstr)+1) * sizeof(wchar_t)));
	return str;
}

char *GetProduct(CorsairLink_t *cl){
	char *str;
	wchar_t wstr[MAX_STR];
	int res;

	wstr[0] = 0x0000;
	res = hid_get_product_string(cl->handle, wstr, MAX_STR);
	if (res < 0)
		fprintf(stderr, "Unable to read product string\n");
	str = malloc((wcslen(wstr)+2) * sizeof(wchar_t));
	memcpy(str, wstr, ((wcslen(wstr)+1) * sizeof(wchar_t)));
	return str;
}

void Csleep(int ms){
	#ifdef WIN32
	Sleep(ms);
	#else
	usleep(ms*1000);
	#endif
}


int hid_read_wrapper(CorsairLink_t *cl, hid_device *handle, unsigned char *buf)
{
	// Read requested state. hid_read() has been set to be
	// non-blocking by the call to hid_set_nonblocking() above.
	// This loop demonstrates the non-blocking nature of hid_read().
	int res = 0;
	int sleepTotal = 0;
	while (res == 0 && sleepTotal < cl->max_ms_read_wait) {
		res = hid_read(handle, buf, sizeof(buf));
		if (res < 0)
			fprintf(stderr, "Unable to read()n");
		
		Csleep(100);
		sleepTotal += 100;
	}
	if(sleepTotal == cl->max_ms_read_wait) {
		res = 0;
	}
	return 1;
}

