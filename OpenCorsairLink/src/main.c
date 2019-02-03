/*
 * Rework of crazzy c++ test program which has no reason to be that
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "hidapi.h"
#include "CorsairFanInfo.h"
#include "CorsairLink.h"


static struct option long_options[] = {
	{"help",  no_argument, 0, 'h'},
	{"fan", required_argument, 0, 'f'},
	{"mode", required_argument, 0, 'm'},
	{"rpm",  required_argument, 0, 'r'},
	{"intf",  required_argument, 0, 'i'},
	{0, 0, 0, 0}
};

int parseArguments(int argc, char **argv, int *, int *, int *, int *);

CorsairLink_t h80_link;

int main(int argc, char **argv) {
	int			interfaceType = H80I;
	int			fanNumber = 0;
	int			fanMode = 0;
	int			fanRPM = 0;
	int			num_tsen;
	unsigned short		temp;
	int			i;
	CorsairLink_t		*cl = &h80_link;
	CorsairFanInfo_t	fanInfo;

	printf("Open Corsair Link ");
	if(parseArguments(argc, argv, &fanNumber, &fanMode, &fanRPM, &interfaceType)) {
		return 1;
	}
	if (interfaceType == CLINK)
		printf("(Cooling Node):\n");
	else
		printf("(H80i/H100i):\n");

	if(!CorsairLink_init(cl, interfaceType)) {
		fprintf(stderr, "Cannot initialize link.\n");
		return 1;
	}

	if(fanNumber != 0) {
		if(fanMode != 0 || fanRPM != 0) {
			if(fanMode == FixedRPM && fanRPM <= 0) {
				fprintf(stderr, "Fan RMP missing for Fixed RPM fan mode.\n");
				return 1;
			} else {
				fanInfo.Name[0] = 0;
				if(fanMode != 0) {
					printf("Setting fan to mode %s\n", GetFanModeString(fanMode));
					fanInfo.Mode = fanMode;
				}
				if(fanRPM != 0) {
					printf("Setting fan RPM to %d\n", fanRPM);
					fanInfo.RPM = fanRPM;
				}
				SetFansInfo(cl, interfaceType, fanNumber - 1, &fanInfo);
			}
		} else {
			fprintf(stderr, "No mode or fan RPM specified for the fan.\n");
			return 1;
		}
	} else if(fanMode != 0 || fanRPM != 0) {
		fprintf(stderr,
			"Cannot set fan to a specific mode or fixed RPM without specifying the fan number\n");
		return 1;
	} else {

		num_tsen = ConnectedTemps(cl, interfaceType);
		if (num_tsen) {
			for (i = 0; i < num_tsen; i++ ) {
				temp = ReadTempInfo(cl, interfaceType, i);
				printf("Sensor %d ", i + 1);
				PrintTempInfo(temp);
			}
		}
		ReadFansInfo(cl, interfaceType);
		//std::cout << "Number of fans: " << (sizeof(cl->fans)/sizeof(*cl->fans)) << endl;
		for(i = 0; i < NUMFANS; i++) {
			PrintInfo(&cl->fans[i]);
		}
	}

	Close(cl);

	return 0;
} 

void printHelp() {
	printf("OpenCorsairLink [options]\n");
	printf("Options:\n");
	printf("\t-i, --intf <interface type> Selects interface type either h80i or clink.\n");
	printf("\t                     1 - H80i (new interface type - default)\n");
	printf("\t                     2 - CorsairLink commander cooling node\n");
	printf("\t-f, --fan <fan number> Selects a fan to setup. Accepted values are 1, 2, 3 or 4.\n");
	printf("\t-m, --mode <mode> Sets the mode for the selected fan\n");
	printf("\t                  Modes:\n");
	printf("\t                     4 - Fixed RPM (requires to specify the RPM)\n");
	printf("\t                     6 - Default\n");
	printf("\t                     8 - Quiet\n");
	printf("\t                    10 - Balanced\n");
	printf("\t                    12 - Performance\n");
	printf("\t-r, --rpm <fan RPM> The desired RPM for the selected fan.\n");
	printf("\t                    NOTE: it works only when fan mode is set to Fixed RPM\n");
	printf("\t-h, --help          Prints this message\n");
	printf("Not specifying any option will display information about the fans and pumpon a H80i\n");
}

int parseArguments(int argc, char **argv, int *fanNumber, int *fanMode, int *fanRPM, int *intf) {
	int c;
	int returnCode = 0;
	int option_index = 0;

	while (1) {
		c = getopt_long (argc, argv, "i:f:m:r:h", long_options, &option_index);
		//std::cout << c;
		if (c == -1 || returnCode != 0)
			break;
		switch (c) {
		case 'i':
			*intf = strtol(optarg, NULL, 10);
			if(*intf != 1 && *intf != 2){
				fprintf(stderr,
					"interface invalid. Accepted are 1 for h80i, 2 for coolNode.\n");
				returnCode = 1;
			}
			break;
		case 'f':
			errno = 0;
			*fanNumber = strtol(optarg, NULL, 10);
			if(*fanNumber < 1 || *fanNumber > 4){
				fprintf(stderr, "Fan number is invalid. Accepted values are 1, 2, 3 or 4.\n");
				returnCode = 1;
			}
			break;

		case 'm':
			errno = 0;
			*fanMode = strtol(optarg, NULL, 10);
			if(*fanMode != Performance && *fanMode != FixedRPM &&
				*fanMode != Default && *fanMode != Balanced && *fanMode != Quiet){
				fprintf(stderr, "Fan mode is not allowed. Accepted values are:\n");
				fprintf(stderr, "\t 4 - Fixed RPM\n");
				fprintf(stderr,	"\t 6 - Default\n");
				fprintf(stderr,	"\t 8 - Quiet\n");
				fprintf(stderr,	"\t10 - Balanced\n");
				fprintf(stderr,	"\t12 - Performance\n");
				returnCode = 1;
			}
			break;

		case 'r':
			errno = 0;
			*fanRPM = strtol(optarg, NULL, 10);
			if(*fanRPM < 0){
				fprintf(stderr, "Fan RPM cannot be a negative value.\n");
				returnCode = 1;
			}
			break;

		case 'h':
			printHelp();
			exit(0);
		default:
			returnCode = 0;
			exit(1);
		}
	}
	return returnCode;
}
