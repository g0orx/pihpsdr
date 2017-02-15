/** 
* @file libusbio.h
* @brief Header file for the USB I/O functions, interface to libusb1.0
* @author 
* @version 0.1
* @date 2009-05-18
*/

#ifndef _LIBUSBIO_H
#define _LIBUSBIO_H

int libusb_open_ozy(void);
void libusb_close_ozy();
int libusb_get_ozy_firmware_string(char* buffer,int buffersize);
int libusb_write_ozy(int ep,void* buffer,int buffersize);
int libusb_read_ozy(int ep,void* buffer,int buffersize);

#endif /* _LIBUSBIO_H */

