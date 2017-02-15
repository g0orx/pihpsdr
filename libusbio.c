/* 
 * File:   libusbio.c
 * Author: jm57878
 *
 * Created on 18 February 2009, 21:16
 */


#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#include "libusbio.h"

/*
 *  interface to libusb1.0
 */

#define OZY_PID (0x0007)
#define OZY_VID (0xfffe)

#define VRQ_SDR1K_CTL 0x0d
#define SDR1KCTRL_READ_VERSION  0x7
#define VRT_VENDOR_IN 0xC0

#define OZY_IO_TIMEOUT 500

static int init=0;
static libusb_device_handle* ozy_handle;
static libusb_context* context;

int libusb_open_ozy(void) {
    int rc;

fprintf(stderr,"libusb_open_ozy\n");

    if(init==0) {
        rc=libusb_init(NULL);
        if(rc<0) {
            fprintf(stderr,"libusb_init failed: %d\n",rc);
            return rc;
        }
        init=1;
    }

    ozy_handle=libusb_open_device_with_vid_pid(NULL, OZY_VID, OZY_PID);
    if(ozy_handle==NULL) {
        fprintf(stderr,"libusbio: cannot find ozy device\n");
        return -1;
    }

    rc=libusb_claim_interface(ozy_handle,0);
    if(rc<0) {
        fprintf(stderr,"libusb_claim_interface failed: %d\n",rc);
        return rc;
    }

fprintf(stderr,"libusb_open_ozy: SUCCESS\n");
    return 0;

}

void libusb_close_ozy() {
    libusb_close(ozy_handle);
}

int libusb_get_ozy_firmware_string(char* buffer,int buffersize) {
    int rc;
    rc=libusb_control_transfer(ozy_handle, VRT_VENDOR_IN, VRQ_SDR1K_CTL, SDR1KCTRL_READ_VERSION, 0, buffer, buffersize, OZY_IO_TIMEOUT);
    if(rc<0) {
        fprintf(stderr,"libusb_get_ozy_firmware failed: %d\n",rc);
        return rc;
    }
    buffer[rc]=0x00;
    return 0;
}

int libusb_write_ozy(int ep,void* buffer,int buffersize)
{
	int rc;
        int bytes;
	rc = libusb_bulk_transfer(ozy_handle, (unsigned char)ep, (unsigned char *)buffer, buffersize,&bytes, OZY_IO_TIMEOUT);
        if(rc==0) {
            rc=bytes;
        }
        return rc;
}


int  libusb_read_ozy(int ep,void* buffer,int buffersize)
{
	int rc;
        int bytes;
	rc = libusb_bulk_transfer(ozy_handle, (unsigned char)ep, (unsigned char *)buffer, buffersize,&bytes, OZY_IO_TIMEOUT);
        if(rc==0) {
            rc=bytes;
        }
	return rc;
}
