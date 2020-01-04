//
// hpsdrsim.h, define global data
//
// From the main program, this is included with EXTERN="", while
// other modules include is with "EXTERN=extern".
//
///////////////////////////////////////////////////////////////////////////
//
// The 800-Hz tone and the "man made noise" are for a sample rate of
// 1536 kHz, and must be decimated when using smaller sample rates
//
///////////////////////////////////////////////////////////////////////////

//
// Unfortunately, the code number of the gear
// differes in old and new protocol
//

#define DEVICE_METIS           0
#define DEVICE_HERMES          1
#define DEVICE_GRIFFIN         2
#define DEVICE_ANGELIA         4
#define DEVICE_ORION           5
#define DEVICE_HERMES_LITE     6
#define DEVICE_HERMES_LITE2 1006
#define DEVICE_ORION2         10
#define DEVICE_C25           100

#define NEW_DEVICE_ATLAS        0
#define NEW_DEVICE_HERMES       1
#define NEW_DEVICE_HERMES2      2
#define NEW_DEVICE_ANGELIA      3
#define NEW_DEVICE_ORION        4
#define NEW_DEVICE_ORION2       5
#define NEW_DEVICE_HERMES_LITE  6
#define NEW_DEVICE_HERMES_LITE2 1006

EXTERN int OLDDEVICE;
EXTERN int NEWDEVICE;

//
// A table of (random) noise with about -90 dBm on the whole spectrum
// This is a very long table such that there is no audible "beating"
// pattern even at very high sample rates.
//
#define LENNOISE 1536000
#define NOISEDIV (RAND_MAX / 768000)

EXTERN double noiseItab[LENNOISE];
EXTERN double noiseQtab[LENNOISE];

//
// A table of (man made) noise fed to the I samples of ADC0
// and to the Q samples of ADC1, such that it can be eliminated
// using DIVERSITY
//
EXTERN int diversity;

#define LENDIV 16000
EXTERN double divtab[LENDIV];
//
// An 800-Hz tone with 0 dBm
//
#define LENTONE 15360
EXTERN double toneItab[LENTONE];
EXTERN double toneQtab[LENTONE];

//
// TX fifo (needed for PURESIGNAL)
//

// RTXLEN must be an sixteen-fold multiple of 63
// because we have 63 samples per 512-byte METIS packet,
// and two METIS packets per TCP/UDP packet,
// and two/four/eight-fold up-sampling if the TX sample
// rate is 96000/192000/384000
//
// In the new protocol, TX samples come in bunches of
// 240 samples. So NEWRTXLEN is defined as a multiple of
// 240 not exceeding RTXLEN
//
#define OLDRTXLEN 64512 // must be larger than NEWRTXLEN
#define NEWRTXLEN 64320
EXTERN double  isample[OLDRTXLEN];
EXTERN double  qsample[OLDRTXLEN];

//
// Address where to send packets from the old and new protocol
// to the PC
//
EXTERN struct sockaddr_in addr_new;
EXTERN struct sockaddr_in addr_old;

//
// Constants for conversion of TX power
//
EXTERN double c1,c2;

//
// Forward declarations for the audio functions
//
void audio_get_cards(void);
void audio_open_output();
void audio_write(int16_t, int16_t);

//
// Forward declarations for new protocol stuff
//
void   new_protocol_general_packet(unsigned char *buffer);
int    new_protocol_running(void);

#ifndef __APPLE__
// using clock_nanosleep of librt
extern int clock_nanosleep(clockid_t __clock_id, int __flags,
      __const struct timespec *__req,
      struct timespec *__rem);
#endif

//
// Constants defining the distortion of the TX signal
// These give about -24 dBc at full drive, that is
// about the value a reasonable amp gives.
//
#define IM3a  0.60
#define IM3b  0.20

