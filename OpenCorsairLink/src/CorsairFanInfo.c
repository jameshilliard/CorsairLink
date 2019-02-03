

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "CorsairFanInfo.h"

struct ModeToString {
	int	mode;
	char	*name;
};

struct ModeToString ModeToString[] = {
	{FixedPWM, "Fixed PWM"},
	{FixedRPM, "Fixed RPM"},
	{Default, "Default"},
	{Quiet, "Quiet"},
	{Balanced, "Balanced"},
	{Performance, "Performance"},
	{Custom, "Custom"},
	{0xff, "N/A"}
};

void CorsairFanInfo_init(CorsairFanInfo_t *fi){
	fi->RPM = 0;
	fi->Mode = 0x03;
}

char *GetFanModeString(int mode){
	int i;
	char *modeString = "";
	
	i = 0;
	while (ModeToString[i].mode != 0xff) {
		if (ModeToString[i].mode == mode)
			break;
		i++;
	}
	modeString = ModeToString[i].name;
	return modeString;
}

void PrintInfo(CorsairFanInfo_t *fi){
	printf ("%s: ",fi->Name);
	printf("Mode: %.2x ", fi->Mode);
	printf("RPM: %.5d ", fi->RPM);
	printf("maxRPM: %.5d\n", fi->maxRPM);
}

void PrintTempInfo(unsigned short temp) {
	unsigned char	wtemp;
	unsigned char	frtemp;
	unsigned int	trans = 0;

	wtemp = ((temp >> 8) & 0xff);
	frtemp = temp & 0xff;

	if (frtemp & 0x80) {
		trans += 500;	/* 128/256 */
	}
	if (frtemp & 0x40) {
		trans += 250;	/* 64/256 */
	}
	if (frtemp & 0x20) {
		trans += 125;	/* 32/256 */
	}
	if (frtemp & 0x10) {
		trans += 62;	/* 16/256 */
	}
	if (frtemp & 0x08) {
		trans += 31;	/* 8/256 */
	}
	if (frtemp & 0x04) {
		trans += 16;	/* 4/256 */
	}
	if (frtemp & 0x02) {
		trans += 8;	/* 2/256 */
	}
	if (frtemp & 0x01) {
		trans += 4;	/* 1/256 */
	}

	printf("Temperature %d.%.3d C\n", wtemp, trans); 
}

