/**
* @file ozyio.h
* @brief USB I/O with Ozy
* @author John Melton, G0ORX/N6LYT
* @version 0.1
* @date 2009-10-13
*/

/* Copyright (C)
* 2009 - John Melton, G0ORX/N6LYT
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

/*
* modified by Bob Wisdom VK4YA May 2015 to create ozymetis
* modified further Laurence Barker G8NJJ to add USB functionality to pihpsdr
*/


/*
 * code modified from that in Ozy_Metis_RPI_Gateway
 * Laurence Barker, G8NJJ December 2016
 * this gathers all Ozy functionality in one file (merging code 
 * from ozy.c).
 * Further modified to add a "discover" function
 * 
*/

#if !defined __OZYIO_H__
#define __OZYIO_H__

//
// penelope forward, reverse power and ALC settings
//
extern unsigned short penny_fp, penny_rp, penny_alc;
extern int adc_overflow;

int ozy_open(void);
int ozy_close();
int ozy_get_firmware_string(unsigned char* buffer,int buffer_size);
int ozy_write(int ep,unsigned char* buffer,int buffer_size);
int ozy_read(int ep,unsigned char* buffer,int buffer_size);

void ozy_load_fw();
int ozy_load_fpga(char *rbf_fnamep);
int ozy_set_led(int which, int on);
int ozy_reset_cpu(int reset);
int ozy_load_firmware(char *fnamep);
int ozy_initialise();
int ozy_discover(void);		// returns 1 if a device found on USB
void ozy_i2c_readpwr(int addr); // sets local variables



// Ozy I2C commands for polling firmware versions, power levels, ADC overload.
#define I2C_MERC1_FW  0x10 // Mercury1 firmware version
#define I2C_MERC2_FW  0x11 // Mercury2 firmware version
#define I2C_MERC3_FW  0x12 // Mercury3 firmware version
#define I2C_MERC4_FW  0x13 // Mercury4 firmware version

#define I2C_MERC1_ADC_OFS 0x10 // adc1 overflow status
#define I2C_MERC2_ADC_OFS 0x11 // adc2 overflow status
#define I2C_MERC3_ADC_OFS 0x12 // adc3 overflow status
#define I2C_MERC4_ADC_OFS 0x13 // adc4 overflow status

#define I2C_PENNY_FW  0x15 // Penny firmware version
#define I2C_PENNY_ALC 0x16 // Penny forward power
#define I2C_PENNY_FWD 0x17 // Penny forward power from Alex
#define I2C_PENNY_REV 0x18 // Penny reverse power from Alex
#define I2C_ADC_OFS (0x10)	// ADC overload status
#define	VRQ_I2C_READ	0x81	// i2c address; length; how much to read

#endif
