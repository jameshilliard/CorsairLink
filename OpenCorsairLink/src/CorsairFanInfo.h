

enum CorsairFanModes {
	FixedPWM = 0x02,
	FixedRPM = 0x04,
	Default = 0x06,
	Quiet = 0x08,
	Balanced = 0x0A,
	Performance = 0x0C,
	Custom = 0x0E
};

struct CorsairFanInfo {
		char Name[25];
		int RPM;
		int maxRPM;
		int Mode;
};

typedef struct CorsairFanInfo CorsairFanInfo_t;

void CorsairFanInfo_init(CorsairFanInfo_t *);
void PrintInfo(CorsairFanInfo_t *);
char *GetFanModeString(int mode);
void PrintTempInfo(unsigned short temp);

