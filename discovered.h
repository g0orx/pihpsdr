#include <netinet/in.h>

#define MAX_DEVICES 16

#define DEVICE_METIS 0
#define DEVICE_HERMES 1
#define DEVICE_GRIFFIN 2
#define DEVICE_ANGELIA 4
#define DEVICE_ORION 5
#define DEVICE_HERMES_LITE 6

#define NEW_DEVICE_ATLAS 0
#define NEW_DEVICE_HERMES 1
#define NEW_DEVICE_HERMES2 2
#define NEW_DEVICE_ANGELIA 3
#define NEW_DEVICE_ORION 4
#define NEW_DEVICE_ANAN_10E 5
#define NEW_DEVICE_HERMES_LITE 6

#define STATE_AVAILABLE 2
#define STATE_SENDING 3

#define ORIGINAL_PROTOCOL 0
#define NEW_PROTOCOL 1

struct _DISCOVERED {
    int protocol;
    int device;
    char name[16];
    int software_version;
    unsigned char mac_address[6];
    int status;
    int address_length;
    struct sockaddr_in address;
    int interface_length;
    struct sockaddr_in interface_address;
    char interface_name[64];
};

typedef struct _DISCOVERED DISCOVERED;

extern int selected_device;
extern int devices;
extern DISCOVERED discovered[MAX_DEVICES];

