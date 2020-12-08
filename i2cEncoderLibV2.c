//
//    FILE: i2cEncoderLibV2.h
// VERSION: 0.1..
// PURPOSE: Library for I2C Encoder V2 board with Arduino
// LICENSE: GPL v3 (http://www.gnu.org/licenses/gpl.html)
//
// DATASHEET:
//
//     URL:
//
// AUTHOR:
//  by Simone Caron
// Adapted from C++ to C for use on PSOC:
//  by Rudy De Volder

#include "i2cEncoderLibV2.h"
#include "CyLib.h"
#include <project.h>
#include <stdio.h>

const int max_idx = MAX_IDX;   //Memory reserved for max. xx encoders

struct i2cEnc_ 	i2cEnc[MAX_IDX];
uint32_t 		last_status;

static char Buffer[120];	//	UART line buffer

	
uint32_t last_status; // status of last I2C transmission   

// *********************************** Private functions *************************************/

    uint8_t readEncoderByte(uint8_t reg);
	int16_t readEncoderInt(uint8_t reg);
	int32_t readEncoderLong(uint8_t reg);
	float   readEncoderFloat(uint8_t reg);
    
	void    writeEncoderByte(uint8_t reg, uint8_t data);
    
	void    writeEncoderLong(uint8_t reg, int32_t data);
	void    writeEncoderFloat(uint8_t reg, float data);
	void    writeEncoder24bit(uint8_t reg, uint32_t data);

// *********************************** Public functions *************************************/

/* */
	
	uint8_t encoder_last_idx  = 0x00;   //Index of the last added encoder
    uint8_t encoder_activated = 0;   //Index of the current encoder_activated encoder	
	
// Constructor:                                       //
// !!! Call only once to construct the Encoder !!!    //
bool i2cEnc__init(uint8_t add) {    
	if (encoder_last_idx < max_idx) {
		i2cEnc[encoder_last_idx]._add   = add;
		i2cEnc[encoder_last_idx]._stat  = 0x00;
		i2cEnc[encoder_last_idx]._stat2 = 0x00;
		i2cEnc[encoder_last_idx]._gconf = 0x00;
		// set callback pointers to NULL:
        for (int event = 0; event < EV_Range; event++) {
            i2cEnc[encoder_last_idx].call_back[event] = NULL;
        }
		encoder_activated   = encoder_last_idx;
        encoder_last_idx++;
        return true;
	} else return false;
}

// Used for initialize the encoder //
void i2cEnc__begin(uint8_t conf) {

	writeEncoderByte(REG_GCONF, (uint8_t) conf);
	i2cEnc[encoder_activated]._gconf = conf;
}

void i2cEnc__reset(void) {
	writeEncoderByte(REG_GCONF, (uint8_t) 0x80);
    CyDelay(1); // Reset takes 400 microseconds to execute
}

void i2cEnc__SetEvent(int EventType, Callback event) {
	i2cEnc[encoder_activated].call_back[EventType] = *event;
}


void eventCaller(int EventType) {
    Callback *event;
	*event = (Callback) i2cEnc[encoder_activated].call_back[EventType];
	if (*event != NULL) {
		(*event) (NULL) ;
	}	
}


// Return true if the status of the enconder changed, otherwise return false. 
// It's also calls the callback, when attached //
bool i2cEnc__updateStatus(void) {
    
    //sprintf (Buffer, "\n\rKnob %d: activated", encoder_activated);     
    //UART_UartPutString(Buffer); 
    
    i2cEnc[encoder_activated]._stat = readEncoderByte((uint8_t)REG_ESTATUS);
	i2cEnc[encoder_activated]._stat2 = 0;
    //UART_UartPutChar('A');
	if (i2cEnc[encoder_activated]._stat == 0) {
        //UART_UartPutChar('B');
		return false;
	} else {
        //UART_UartPutChar('C');
        sprintf (Buffer, "\n\rDecoder %d: activated", encoder_activated);     
        UART_UartPutString(Buffer); 
        //UART_UartPutChar('*');   
    }
    //UART_UartPutChar('D');
    if (i2cEnc[encoder_activated]._stat & PUSHP) {
		eventCaller (EV_ButtonPush);
	}
	if (i2cEnc[encoder_activated]._stat & PUSHR) {
		eventCaller (EV_ButtonRelease);
	}
	if (i2cEnc[encoder_activated]._stat & PUSHD) {
		eventCaller (EV_ButtonDoublePush);
	}
	if (i2cEnc[encoder_activated]._stat & RINC) {
        
		eventCaller (EV_Increment);
		eventCaller (EV_Change);
	}
	if (i2cEnc[encoder_activated]._stat & RDEC) {
		
        eventCaller (EV_Decrement);
		eventCaller (EV_Change);
	}
	if (i2cEnc[encoder_activated]._stat & RMAX) {
		eventCaller (EV_Max);
		eventCaller (EV_MinMax);
	}
	if (i2cEnc[encoder_activated]._stat & RMIN) {
		eventCaller (EV_Min);
		eventCaller (EV_MinMax);
	}

	if ((i2cEnc[encoder_activated]._stat & INT_2) != 0) {
		i2cEnc[encoder_activated]._stat2 = readEncoderByte(REG_I2STATUS);
		if (i2cEnc[encoder_activated]._stat2 == 0) {
			return true;
		}

		if (i2cEnc[encoder_activated]._stat2 & GP1_POS) {
			eventCaller (EV_GP1Rise);
		}
		if (i2cEnc[encoder_activated]._stat2 & GP1_NEG) {
			eventCaller (EV_GP1Fall);
		}
		if (i2cEnc[encoder_activated]._stat2 & GP2_POS) {
			eventCaller (EV_GP2Rise);
		}
		if (i2cEnc[encoder_activated]._stat2 & GP2_NEG) {
			eventCaller (EV_GP2Fall);
		}
		if (i2cEnc[encoder_activated]._stat2 & GP3_POS) {
			eventCaller (EV_GP3Rise);
		}
		if (i2cEnc[encoder_activated]._stat2 & GP3_NEG) {
			eventCaller (EV_GP3Fall);
		}
		if (i2cEnc[encoder_activated]._stat2 & FADE_INT) {
			eventCaller (EV_FadeProcess);
		}
	}
	
	return true;
}

// ********************************* Read functions *********************************** //

// Return the GP1 Configuration//
uint8_t i2cEnc__readGP1conf(void) {
	return (readEncoderByte(REG_GP1CONF));
}

// Return the GP1 Configuration//
uint8_t i2cEnc__readGP2conf(void) {
	return (readEncoderByte(REG_GP2CONF));
}

// Return the GP1 Configuration//
uint8_t i2cEnc__readGP3conf(void) {
	return (readEncoderByte(REG_GP3CONF));
}

// Return the INT pin configuration//
uint8_t i2cEnc__readInterruptConfig(void) {
	return (readEncoderByte(REG_INTCONF));
}

// Check if a particular status match, return true is match otherwise false. Before require updateStatus() //
bool i2cEnc__readStatus_match(enum Int_Status s) {
	if ((i2cEnc[encoder_activated]._stat & s) != 0) {
		return true;
	}
	return false;
}

// Return the status of the encoder //
uint8_t i2cEnc__readStatus(void) {
	return i2cEnc[encoder_activated]._stat;
}

// Check if a particular status of the Int2 match, return true is match otherwise false. Before require updateStatus() //
bool i2cEnc__readInt2_match(Int2_Status s) {
	if ((i2cEnc[encoder_activated]._stat2 & s) != 0) {
		return true;
	}
	return false;
}

// Return the Int2 status of the encoder. Before require updateStatus()  //
uint8_t i2cEnc__readInt2(void) {
	return i2cEnc[encoder_activated]._stat2;
}

// Return Fade process status  //
uint8_t i2cEnc__readFadeStatus(void) {
	return readEncoderByte(REG_FSTATUS);
}

// Check if a particular status of the Fade process match, return true is match otherwise false. //
bool i2cEnc__readFadeStatus_match(Fade_Status s) {
	if ((readEncoderByte(REG_FSTATUS) & s) == 1)
		return true;

	return false;
}

// Return the PWM LED R value  //
uint8_t i2cEnc__readLEDR(void) {
	return ((uint8_t) readEncoderByte(REG_RLED));
}

// Return the PWM LED G value  //
uint8_t i2cEnc__readLEDG(void) {
	return ((uint8_t) readEncoderByte(REG_GLED));
}

// Return the PWM LED B value  //
uint8_t i2cEnc__readLEDB(void) {
	return ((uint8_t) readEncoderByte(REG_BLED));
}

// Return the 32 bit value of the encoder counter  //
float i2cEnc__readCounterFloat(void) {
	return (readEncoderFloat(REG_CVALB4));
}

// Return the 32 bit value of the encoder counter  //
int32_t i2cEnc__readCounterLong(void) {
	return ((int32_t) readEncoderLong(REG_CVALB4));
}

// Return the 16 bit value of the encoder counter  //
int16_t i2cEnc__readCounterInt(void) {
	return ((int16_t) readEncoderInt(REG_CVALB2));
}

// Return the 8 bit value of the encoder counter  //
int8_t i2cEnc__readCounterByte(void) {
	return ((int8_t) readEncoderByte(REG_CVALB1));
}

// Return the Maximum threshold of the counter //
int32_t i2cEnc__readMax(void) {
	return ((int32_t) readEncoderLong(REG_CMAXB4));
}

// Return the Minimum threshold of the counter //
int32_t i2cEnc__readMin(void) {
	return ((int32_t) readEncoderLong(REG_CMINB4));
}

// Return the Maximum threshold of the counter //
float i2cEnc__readMaxFloat(void) {
	return (readEncoderFloat(REG_CMAXB4));
}

// Return the Minimum threshold of the counter //
float i2cEnc__readMinFloat(void) {
	return (readEncoderFloat(REG_CMINB4));

}

// Return the Steps increment //
int32_t i2cEnc__readStep(void) {
	return (readEncoderInt(REG_ISTEPB4));
}

// Return the Steps increment, in float variable //
float i2cEnc__readStepFloat(void) {
	return (readEncoderFloat(REG_ISTEPB4));

}

// Read GP1 register value //
uint8_t i2cEnc__readGP1(void) {
	return (readEncoderByte(REG_GP1REG));
}

// Read GP2 register value //
uint8_t i2cEnc__readGP2(void) {
	return (readEncoderByte(REG_GP2REG));
}

// Read GP3 register value //
uint8_t i2cEnc__readGP3(void) {
	return (readEncoderByte(REG_GP3REG));
}

// Read Anti-bouncing period register //
uint8_t i2cEnc__readAntibouncingPeriod(void) {
	return (readEncoderByte(REG_ANTBOUNC));
}

// Read Double push period register //
uint8_t i2cEnc__readDoublePushPeriod(void) {
	return (readEncoderByte(REG_DPPERIOD));
}

// Read the fade period of the RGB LED//
uint8_t i2cEnc__readFadeRGB(void) {
	return (readEncoderByte(REG_FADERGB));
}

// Read the fade period of the GP LED//
uint8_t i2cEnc__readFadeGP(void) {
	return (readEncoderByte(REG_FADEGP));
}

// Read the EEPROM memory//
uint8_t i2cEnc__readEEPROM(uint8_t add) {
	if (add <= 0x7f) {
		if ((i2cEnc[encoder_activated]._gconf & EEPROM_BANK1) != 0) {
			i2cEnc[encoder_activated]._gconf = i2cEnc[encoder_activated]._gconf & 0xBF;
			writeEncoderByte(REG_GCONF, i2cEnc[encoder_activated]._gconf);
		}
		return (readEncoderByte((REG_EEPROMS + add)));
	} else {
		if ((i2cEnc[encoder_activated]._gconf & EEPROM_BANK1) == 0) {
			i2cEnc[encoder_activated]._gconf = i2cEnc[encoder_activated]._gconf | 0x40;
			writeEncoderByte(REG_GCONF, i2cEnc[encoder_activated]._gconf);
		}
		return (readEncoderByte(add));
	}
}

// ********************************* Write functions *********************************** //
// Write the GP1 configuration//
void i2cEnc__writeGP1conf(uint8_t gp1) {
	writeEncoderByte(REG_GP1CONF, (uint8_t) gp1);
}

// Write the GP2 configuration//
void i2cEnc__writeGP2conf(uint8_t gp2) {
	writeEncoderByte(REG_GP2CONF, (uint8_t) gp2);
}

// Write the GP3 configuration//
void i2cEnc__writeGP3conf(uint8_t gp3) {
	writeEncoderByte(REG_GP3CONF, (uint8_t) gp3);
}

// Write the interrupt configuration //
void i2cEnc__writeInterruptConfig(uint8_t interrupt) {
	writeEncoderByte(REG_INTCONF, (uint8_t) interrupt);
}


// Check if there is some attached callback and enable the corresponding interrupt //
void i2cEnc__autoconfigInterrupt(void) {
	uint8_t reg = 0;

	if (i2cEnc[encoder_activated].call_back[EV_ButtonRelease] != NULL)
		reg |= PUSHR;

	if (i2cEnc[encoder_activated].call_back[EV_ButtonPush] != NULL)
		reg |= PUSHP;

	if (i2cEnc[encoder_activated].call_back[EV_ButtonDoublePush] != NULL)
		reg |= PUSHD;

	if (i2cEnc[encoder_activated].call_back[EV_Increment] != NULL)
		reg |= RINC;

	if (i2cEnc[encoder_activated].call_back[EV_Decrement] != NULL)
		reg |= RDEC;

	if (i2cEnc[encoder_activated].call_back[EV_Change] != NULL) {
		reg |= RINC;
		reg |= RDEC;
	}

	if (i2cEnc[encoder_activated].call_back[EV_Max] != NULL)
		reg |= RMAX;

	if (i2cEnc[encoder_activated].call_back[EV_Min] != NULL)
		reg |= RMIN;

	if (i2cEnc[encoder_activated].call_back[EV_MinMax] != NULL) {
		reg |= RMAX;
		reg |= RMIN;
	}

	if (i2cEnc[encoder_activated].call_back[EV_GP1Rise] != NULL)
		reg |= INT_2;

	if (i2cEnc[encoder_activated].call_back[EV_GP1Fall] != NULL)
		reg |= INT_2;

	if (i2cEnc[encoder_activated].call_back[EV_GP2Rise] != NULL)
		reg |= INT_2;

	if (i2cEnc[encoder_activated].call_back[EV_GP2Fall] != NULL)
		reg |= INT_2;

	if (i2cEnc[encoder_activated].call_back[EV_GP3Rise] != NULL)
		reg |= INT_2;

	if (i2cEnc[encoder_activated].call_back[EV_GP3Fall] != NULL)
		reg |= INT_2;

	if (i2cEnc[encoder_activated].call_back[EV_FadeProcess] != NULL)
		reg |= INT_2;

	writeEncoderByte(REG_INTCONF, (uint8_t) reg);

}


// Write the counter value //
void i2cEnc__writeCounter_i(int32_t value) {
	writeEncoderLong(REG_CVALB4, value);
}

// Write the counter value //
void i2cEnc__writeCounter_f(float value) {
	writeEncoderFloat(REG_CVALB4, value);
}

// Write the maximum threshold value //
void i2cEnc__writeMax_i(int32_t max) {
	writeEncoderLong(REG_CMAXB4, max);
}

// Write the maximum threshold value //
void i2cEnc__writeMax_f(float max) {
	writeEncoderFloat(REG_CMAXB4, max);
}

// Write the minimum threshold value //
void i2cEnc__writeMin_i(int32_t min) {
	writeEncoderLong(REG_CMINB4, min);
}

// Write the minimum threshold value //
void i2cEnc__writeMin_f(float min) {
	writeEncoderFloat(REG_CMINB4, min);
}

// Write the Step increment value //
void i2cEnc__writeStep_i(int32_t step) {
	writeEncoderLong(REG_ISTEPB4, step);
}

// Write the Step increment value //
void i2cEnc__writeStep_f(float step) {
	writeEncoderFloat(REG_ISTEPB4, step);
}

// Write the PWM value of the RGB LED red //
void i2cEnc__writeLEDR(uint8_t rled) {
	writeEncoderByte(REG_RLED, rled);
}

// Write the PWM value of the RGB LED green //
void i2cEnc__writeLEDG(uint8_t gled) {
	writeEncoderByte(REG_GLED, gled);
}

// Write the PWM value of the RGB LED blue //
void i2cEnc__writeLEDB(uint8_t bled) {
	writeEncoderByte(REG_BLED, bled);
}

// Write 24bit color code //
void i2cEnc__writeRGBCode(uint32_t rgb) {
	writeEncoder24bit(REG_RLED, rgb);
}

// Write GP1 register, used when GP1 is set to output or PWM //
void i2cEnc__writeGP1(uint8_t gp1) {
	writeEncoderByte(REG_GP1REG, gp1);
}

// Write GP2 register, used when GP2 is set to output or PWM //
void i2cEnc__writeGP2(uint8_t gp2) {
	writeEncoderByte(REG_GP2REG, gp2);
}

// Write GP3 register, used when GP3 is set to output or PWM //
void i2cEnc__writeGP3(uint8_t gp3) {
	writeEncoderByte(REG_GP3REG, gp3);
}

// Write Anti-bouncing period register //
void i2cEnc__writeAntibouncingPeriod(uint8_t bounc) {
	writeEncoderByte(REG_ANTBOUNC, bounc);
}

// Write Anti-bouncing period register //
void i2cEnc__writeDoublePushPeriod(uint8_t dperiod) {
	writeEncoderByte(REG_DPPERIOD, dperiod);
}

// Write Fade timing in ms //
void i2cEnc__writeFadeRGB(uint8_t fade) {
	writeEncoderByte(REG_FADERGB, fade);
}

// Write Fade timing in ms //
void i2cEnc__writeFadeGP(uint8_t fade) {
	writeEncoderByte(REG_FADEGP, fade);
}

// Write the EEPROM memory//
void i2cEnc__writeEEPROM(uint8_t add, uint8_t data) {

	if (add <= 0x7f) {
		if ((i2cEnc[encoder_activated]._gconf & EEPROM_BANK1) != 0) {
			i2cEnc[encoder_activated]._gconf = i2cEnc[encoder_activated]._gconf & 0xBF;
			writeEncoderByte(REG_GCONF, i2cEnc[encoder_activated]._gconf);
		}
		writeEncoderByte((REG_EEPROMS + add), data);
	} else {
		if ((i2cEnc[encoder_activated]._gconf & EEPROM_BANK1) == 0) {
			i2cEnc[encoder_activated]._gconf = i2cEnc[encoder_activated]._gconf | 0x40;
			writeEncoderByte(REG_GCONF, i2cEnc[encoder_activated]._gconf);
		}
		writeEncoderByte(add, data);
	}

	CyDelay(5);
}




// *************************************************************************************** //
// ********************************* Private functions *********************************** //
// *************************************************************************************** //




// *************************** Read function to the encoder ****************************** //
// Read 1 byte from the encoder 
uint8_t readEncoderByte(uint8_t reg) {
    uint8_t data[4];
    uint16_t value = 0;
    
    last_status = TRANSFER_ERROR;
    
    (void) I2C_I2CMasterClearStatus();

    last_status = I2C_I2CMasterSendStart( (uint32_t)i2cEnc[encoder_activated]._add, I2C_I2C_WRITE_XFER_MODE, I2C_TIMEOUT );
    if( I2C_I2C_MSTR_NO_ERROR == last_status )
    {
        last_status = I2C_I2CMasterWriteByte( (uint32_t)reg, I2C_TIMEOUT );
        /* Transfer succeeds send stop and return Success */
        if((I2C_I2C_MSTR_NO_ERROR == last_status)      ||
           (I2C_I2C_MASTER_CMD_M_NACK == last_status))
        {
            if(I2C_I2CMasterSendStop(I2C_TIMEOUT) == I2C_I2C_MSTR_NO_ERROR)
            {
                /* High level I2C: */
                last_status = I2C_I2CMasterReadBuf((uint32_t)i2cEnc[encoder_activated]._add, data, 01u, I2C_I2C_MODE_COMPLETE_XFER);
                if(I2C_I2C_MSTR_NO_ERROR == last_status)
                {
                    // If I2C read started without errors, wait until master complete read transfer
                    while (0u == (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_RD_CMPLT)) { }
                    // Display transfer status
                    if (0u == (I2C_I2C_MSTAT_ERR_XFER & I2C_I2CMasterStatus())) {
                        last_status = TRANSFER_CMPLT;
                    }
                }
            }
        }
        else
            I2C_I2CMasterSendStop(I2C_TIMEOUT);
    }
    else
        I2C_I2CMasterSendStop(I2C_TIMEOUT);
       
	value = data[0];
    
	return value;
}


// Read a 16-bit register
int16_t readEncoderInt(uint8_t reg) { //uint16_t Encoder_readReg16Bit(uint8_t reg) {
    uint8_t data[4];
    uint16_t value = 0;
    
    last_status = TRANSFER_ERROR;
    
    (void) I2C_I2CMasterClearStatus();
    

    last_status = I2C_I2CMasterSendStart( (uint32_t)i2cEnc[encoder_activated]._add, I2C_I2C_WRITE_XFER_MODE, I2C_TIMEOUT );
    if( I2C_I2C_MSTR_NO_ERROR == last_status )
    {
        last_status = I2C_I2CMasterWriteByte( (uint32_t)reg, I2C_TIMEOUT );
        /* Transfer succeeds send stop and return Success */
        if((I2C_I2C_MSTR_NO_ERROR == last_status)      ||
           (I2C_I2C_MASTER_CMD_M_NACK == last_status))
        {
            if(I2C_I2CMasterSendStop(I2C_TIMEOUT) == I2C_I2C_MSTR_NO_ERROR)
            {
                /* High level I2C: */
                last_status = I2C_I2CMasterReadBuf((uint32_t)i2cEnc[encoder_activated]._add, data, 02u, I2C_I2C_MODE_COMPLETE_XFER);
                if(I2C_I2C_MSTR_NO_ERROR == last_status)
                {
                    // If I2C read started without errors, wait until master complete read transfer
                    while (0u == (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_RD_CMPLT)) { }
                    // Display transfer status
                    if (0u == (I2C_I2C_MSTAT_ERR_XFER & I2C_I2CMasterStatus())) {
                        last_status = TRANSFER_CMPLT;
                    }
                }
            }
        }
        else
            I2C_I2CMasterSendStop(I2C_TIMEOUT);
    }
    else
        I2C_I2CMasterSendStop(I2C_TIMEOUT);
       

    value  = ((uint16_t)data[0]&0xff)<<8;    // data high byte
	value |= ((uint16_t)data[1]&0xff);       // data low byte
			
	return value;
}


// Read 4 bytes from the encoder //
int32_t readEncoderLong(uint8_t reg) { //uint32_t Encoder_readReg32Bit(uint8_t reg) {
    uint8_t data[4];
    //uint32_t value = 0;
    
    last_status = TRANSFER_ERROR;
    
    (void) I2C_I2CMasterClearStatus();
    

    last_status = I2C_I2CMasterSendStart( (uint32_t)i2cEnc[encoder_activated]._add, I2C_I2C_WRITE_XFER_MODE, I2C_TIMEOUT );
    if( I2C_I2C_MSTR_NO_ERROR == last_status )
    {
        last_status = I2C_I2CMasterWriteByte( (uint32_t)reg, I2C_TIMEOUT );
        /* Transfer succeeds send stop and return Success */
        if((I2C_I2C_MSTR_NO_ERROR == last_status)      ||
           (I2C_I2C_MASTER_CMD_M_NACK == last_status))
        {
            if(I2C_I2CMasterSendStop(I2C_TIMEOUT) == I2C_I2C_MSTR_NO_ERROR)
            {
                /* High level I2C: */
                last_status = I2C_I2CMasterReadBuf((uint32_t)i2cEnc[encoder_activated]._add, data, 04u, I2C_I2C_MODE_COMPLETE_XFER);
                if(I2C_I2C_MSTR_NO_ERROR == last_status)
                {
                    // If I2C read started without errors, wait until master complete read transfer
                    while (0u == (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_RD_CMPLT)) { }
                    // Display transfer status
                    if (0u == (I2C_I2C_MSTAT_ERR_XFER & I2C_I2CMasterStatus())) {
                        last_status = TRANSFER_CMPLT;
                    }
                }
            }
        }
        else
            I2C_I2CMasterSendStop(I2C_TIMEOUT);
    }
    else
        I2C_I2CMasterSendStop(I2C_TIMEOUT);
        
    	i2cEnc[encoder_activated]._tem_data.bval[3] = data[0];
		i2cEnc[encoder_activated]._tem_data.bval[2] = data[1];
		i2cEnc[encoder_activated]._tem_data.bval[1] = data[2];
		i2cEnc[encoder_activated]._tem_data.bval[0] = data[3];
	return ((int32_t) i2cEnc[encoder_activated]._tem_data.val);    
}

// Read 4 bytes from the encoder //
float readEncoderFloat(uint8_t reg) {
    uint8_t data[4];
    //int32_t value = 0;
    last_status = TRANSFER_ERROR;
    
    (void) I2C_I2CMasterClearStatus();
    

    last_status = I2C_I2CMasterSendStart( (uint32_t)i2cEnc[encoder_activated]._add, I2C_I2C_WRITE_XFER_MODE, I2C_TIMEOUT );
    if( I2C_I2C_MSTR_NO_ERROR == last_status )
    {
        last_status = I2C_I2CMasterWriteByte( (uint32_t)reg, I2C_TIMEOUT );
        /* Transfer succeeds send stop and return Success */
        if((I2C_I2C_MSTR_NO_ERROR == last_status)      ||
           (I2C_I2C_MASTER_CMD_M_NACK == last_status))
        {
            if(I2C_I2CMasterSendStop(I2C_TIMEOUT) == I2C_I2C_MSTR_NO_ERROR)
            {
                /* High level I2C: */
                last_status = I2C_I2CMasterReadBuf((uint32_t)i2cEnc[encoder_activated]._add, data, 04u, I2C_I2C_MODE_COMPLETE_XFER);
                if(I2C_I2C_MSTR_NO_ERROR == last_status)
                {
                    // If I2C read started without errors, wait until master complete read transfer
                    while (0u == (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_RD_CMPLT)) { }
                    // Display transfer status
                    if (0u == (I2C_I2C_MSTAT_ERR_XFER & I2C_I2CMasterStatus())) {
                        last_status = TRANSFER_CMPLT;
                    }
                }
            }
        }
        else
            I2C_I2CMasterSendStop(I2C_TIMEOUT);
    }
    else
        I2C_I2CMasterSendStop(I2C_TIMEOUT);
        
        i2cEnc[encoder_activated]._tem_data.bval[3] = data[0];
		i2cEnc[encoder_activated]._tem_data.bval[2] = data[1];
		i2cEnc[encoder_activated]._tem_data.bval[1] = data[2];
		i2cEnc[encoder_activated]._tem_data.bval[0] = data[3];
	return ((float) i2cEnc[encoder_activated]._tem_data.fval);   
}

// *************************** Write function to the encoder ****************************** //
// Send to the encoder 1 byte //
void writeEncoderByte(uint8_t reg, uint8_t data) { //void Encoder_writeReg(uint8_t reg, uint8_t data)
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = data;
    I2C_I2CMasterClearStatus();
    I2C_I2CMasterWriteBuf((uint32_t)i2cEnc[encoder_activated]._add, buf, 02u, I2C_I2C_MODE_COMPLETE_XFER);
    for(;;)
    {
        if(0u != (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT))
        {
            /* Transfer complete. Check Master status to make sure that transfer
            completed without errors. */
        break;
        }
    }
}


// Send to the encoder 4 byte 
void writeEncoderLong(uint8_t reg, int32_t data) {  //void Encoder_writeReg32Bit(uint8_t reg, uint32_t data) {
    uint8_t buf[5];
    i2cEnc[encoder_activated]._tem_data.val = data;
    buf[0] = reg;
    /*
    buf[1] = (uint8_t)((data >> 24) & 0xFF);
    buf[2] = (uint8_t)((data >> 16) & 0xFF);
    buf[3] = (uint8_t)((data >> 8) & 0xFF);
    buf[4] = (uint8_t)(data  & 0xFF); */
	buf[1] = i2cEnc[encoder_activated]._tem_data.bval[3];
	buf[2] = i2cEnc[encoder_activated]._tem_data.bval[2];
	buf[3] = i2cEnc[encoder_activated]._tem_data.bval[1];
	buf[4] = i2cEnc[encoder_activated]._tem_data.bval[0];
    I2C_I2CMasterClearStatus();
    I2C_I2CMasterWriteBuf((uint32_t)i2cEnc[encoder_activated]._add, buf, 05u, I2C_I2C_MODE_COMPLETE_XFER);
    for(;;)
    {
        if(0u != (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT))
        {
            /* Transfer complete. Check Master status to make sure that transfer
            completed without errors. */
        break;
        }
    }
}
// Send to the encoder 4 byte for floating number //
void writeEncoderFloat(uint8_t reg, float data) {
    uint8_t buf[5];
    buf[0] = reg;
    i2cEnc[encoder_activated]._tem_data.fval = data;
	buf[1] = i2cEnc[encoder_activated]._tem_data.bval[3];
	buf[2] = i2cEnc[encoder_activated]._tem_data.bval[2];
	buf[3] = i2cEnc[encoder_activated]._tem_data.bval[1];
	buf[4] = i2cEnc[encoder_activated]._tem_data.bval[0];    
    I2C_I2CMasterClearStatus();
    I2C_I2CMasterWriteBuf((uint32_t)i2cEnc[encoder_activated]._add, buf, 05u, I2C_I2C_MODE_COMPLETE_XFER);
    for(;;)
    {
        if(0u != (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT))
        {
            /* Transfer complete. Check Master status to make sure that transfer
            completed without errors. */
        break;
        }
    }
}


// Send to the encoder 3 byte 
void writeEncoder24bit(uint8_t reg, uint32_t data) {
    uint8_t buf[4];
    i2cEnc[encoder_activated]._tem_data.val = data;
    buf[0] = reg;
	buf[1] = i2cEnc[encoder_activated]._tem_data.bval[2];
	buf[2] = i2cEnc[encoder_activated]._tem_data.bval[1];
	buf[3] = i2cEnc[encoder_activated]._tem_data.bval[0];
    I2C_I2CMasterClearStatus();
    I2C_I2CMasterWriteBuf((uint32_t)i2cEnc[encoder_activated]._add, buf, 05u, I2C_I2C_MODE_COMPLETE_XFER);
    for(;;)
    {
        if(0u != (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT))
        {
            /* Transfer complete. Check Master status to make sure that transfer
            completed without errors. */
        break;
        }
    }
}

/* ******************************************************************************************************** */



/*
// Write a 16-bit register
void Encoder_writeReg16Bit(uint8_t reg, uint16_t data) {
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)((data >> 8) & 0xFF);
    buf[2] = (uint8_t)(data  & 0xFF);
    I2C_I2CMasterClearStatus();
    I2C_I2CMasterWriteBuf((uint32_t)_add, buf, 03u, I2C_I2C_MODE_COMPLETE_XFER);
    for(;;)
    {
        if(0u != (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT))
        {
            // Transfer complete. Check Master status to make sure that transfer completed without errors.
        break;
        }
    }
} */



/*


// Write an arbitrary number of bytes from the given array to the sensor,
// starting at the given register
void Encoder_writeMulti(uint8_t reg, uint8_t const * src, uint8_t count) {
	uint8_t buf[64];

	memcpy(&buf[1] , src, count);
	buf[0] = reg;
	
    
    I2C_I2CMasterClearStatus();
    I2C_I2CMasterWriteBuf((uint32)_add, buf, (uint32)(count+1), I2C_I2C_MODE_COMPLETE_XFER);
    for(;;)
    {
        if(0u != (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT))
        {
            // Transfer complete. Check Master status to make sure that transfer completed without errors.
        break;
        }
    }    
    
}

// Read an arbitrary number of bytes from the sensor, starting at the given
// register, into the given array
void Encoder_readMulti(uint8_t reg, uint8_t *dst, uint8_t count)
{
    last_status = I2C_I2CMasterSendStart( _add, I2C_I2C_WRITE_XFER_MODE );
    if( I2C_I2C_MSTR_NO_ERROR == last_status )
    {
        last_status = I2C_I2CMasterWriteByte( (uint32_t)reg );   
        if( I2C_I2C_MSTR_NO_ERROR == last_status )
        {
            I2C_I2CMasterClearStatus();
            last_status = I2C_I2CMasterReadBuf( (uint32_t)_add, dst, (uint32_t)count, I2C_I2C_MODE_REPEAT_START);
            for(;;)
            {
                if(0u != (I2C_I2CMasterStatus() & I2C_I2C_MSTAT_RD_CMPLT))
                {
                    // Transfer complete. Check Master status to make sure that transfer completed without errors.
                break;
                }
            }
        }
        else
            I2C_I2CMasterSendStop();
    }
    else
        I2C_I2CMasterSendStop();    
        
}

*/