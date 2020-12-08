//
//    FILE: i2cEncoderLibV2
// VERSION: 0.1..
// PURPOSE: Library for the i2c encoder board with PSOC
// LICENSE: GPL v3 (http://www.gnu.org/licenses/gpl.html)
//
// DATASHEET:
//
//     URL:
//
// AUTHOR:
// Original Arduino-version: Simone Caron
//

#ifndef i2cEncoderLibV2
#define i2cEncoderLibV2
	
#define MAX_IDX 2  // Equals nr of I2C_Encoders -1

/* I2C Status */
#define TRANSFER_CMPLT      (0x00u)
#define TRANSFER_ERROR      (0xFFu)
#define READ_CMPLT          (TRANSFER_CMPLT)    
/* Timeout */
#define I2C_TIMEOUT         (100u)    

#include <stdbool.h>
#include <stdint.h>
    
//#define NULL 0
#define HIGH 1
#define LOW  0

    
/* I2C Slave Address */
#define I2C_SLAVE_ADDR      (0x04u)    
	
typedef unsigned char byte;
typedef void (*Callback) (void (*ptr));
    
/* Global variables */
extern uint8_t encoder_last_idx;   //Index of the last added encoder
extern uint8_t encoder_activated;  //Index of the current encoder_activated encoder

enum EventsType {
	EV_ButtonRelease,
	EV_ButtonPush,
	EV_ButtonDoublePush,
	EV_Increment,
	EV_Decrement,
	EV_Change,
	EV_Max,
	EV_Min,
	EV_MinMax,
	EV_GP1Rise,
	EV_GP1Fall,
	EV_GP2Rise,
	EV_GP2Fall,
	EV_GP3Rise,
	EV_GP3Fall,
	EV_FadeProcess, EV_Range
};

	//Callback onIncrement;
    
/*  
    
// Constructors & Destructors: 
// =========================== 
struct  i2cEnc_* i2cEnc__create();          //Empty constructor
void    i2cEnc__init(struct i2cEnc_ *self); //re-init
void    i2cEnc__destroy();                          //destructor

// Member FUNCTIONS: 
// ================= 
	 //  RGB LED Functions  //
	void i2cEnc__writeLEDR(struct i2cEnc_ *self, uint8_t rled);
	void i2cEnc__writeLEDG(struct i2cEnc_ *self, uint8_t gled);
	void i2cEnc__writeLEDB(struct i2cEnc_ *self, uint8_t bled);
	void i2cEnc__writeRGBCode(struct i2cEnc_ *self, uint32_t rgb);
*/


/* Original: class i2cEnc_ {
public: */
//struct i2cEnc_; //C: Struct instead of class     

    enum I2C_Register {
		REG_GCONF    = 0x00,
		REG_GP1CONF  = 0x01,
		REG_GP2CONF  = 0x02,
		REG_GP3CONF  = 0x03,
		REG_INTCONF  = 0x04,
		REG_ESTATUS  = 0x05,
		REG_I2STATUS = 0x06,
		REG_FSTATUS  = 0x07,
		REG_CVALB4   = 0x08,
		REG_CVALB3   = 0x09,
		REG_CVALB2   = 0x0A,
		REG_CVALB1   = 0x0B,
		REG_CMAXB4   = 0x0C,
		REG_CMAXB3   = 0x0D,
		REG_CMAXB2   = 0x0E,
		REG_CMAXB1   = 0x0F,
		REG_CMINB4   = 0x10,
		REG_CMINB3   = 0x11,
		REG_CMINB2   = 0x12,
		REG_CMINB1   = 0x13,
		REG_ISTEPB4  = 0x14,
		REG_ISTEPB3  = 0x15,
		REG_ISTEPB2  = 0x16,
		REG_ISTEPB1  = 0x17,
		REG_RLED     = 0x18,
		REG_GLED     = 0x19,
		REG_BLED     = 0x1A,
		REG_GP1REG   = 0x1B,
		REG_GP2REG   = 0x1C,
		REG_GP3REG   = 0x1D,
		REG_ANTBOUNC = 0x1E,
		REG_DPPERIOD = 0x1F,
		REG_FADERGB  = 0x20,
		REG_FADEGP   = 0x21,
		REG_EEPROMS  = 0x80,
	};
    
	 //  Encoder configuration bit. Use with GCONF   // 
	enum GCONF_PARAMETER {
		FLOAT_DATA 	 = 0x01,
		INT_DATA 	 = 0x00,
		WRAP_ENABLE  = 0x02,
		WRAP_DISABLE = 0x00,
		DIRE_LEFT 	 = 0x04,
		DIRE_RIGHT 	 = 0x00,
		IPUP_DISABLE = 0x08,
		IPUP_ENABLE  = 0x00,
		RMOD_X2 	 = 0x10,
		RMOD_X1 	 = 0x00,
		RGB_ENCODER  = 0x20,
		STD_ENCODER  = 0x00,
		EEPROM_BANK1 = 0x40,
		EEPROM_BANK2 = 0x00,
		RESET 		 = 0x80,
	};

	 //  Encoder status bits and setting. Use with: INTCONF for set and with ESTATUS for read the bits    // 
	enum Int_Status {
		PUSHR = 0x01,
		PUSHP = 0x02,
		PUSHD = 0x04,
		RINC = 0x08,
		RDEC = 0x10,
		RMAX = 0x20,
		RMIN = 0x40,
		INT_2 = 0x80,

	};

	 //  Encoder Int2 bits. Use to read the bits of I2STATUS    // 
	typedef enum {
		GP1_POS = 0x01,
		GP1_NEG = 0x02,
		GP2_POS = 0x04,
		GP2_NEG = 0x08,
		GP3_POS = 0x10,
		GP3_NEG = 0x20,
		FADE_INT = 0x40,
	} Int2_Status;

	 //  Encoder Fade status bits. Use to read the bits of FSTATUS    // 
	typedef enum {
		FADE_R = 0x01,
		FADE_G = 0x02,
		FADE_B = 0x04,
		FADES_GP1 = 0x08,
		FADES_GP2 = 0x10,
		FADES_GP3 = 0x20,
	} Fade_Status;

	 //  GPIO Configuration. Use with GP1CONF,GP2CONF,GP3CONF   // 
	enum GP_PARAMETER {
		GP_PWM = 0x00,
		GP_OUT = 0x01,
		GP_AN = 0x02,
		GP_IN = 0x03,
		GP_PULL_EN = 0x04,
		GP_PULL_DI = 0x00,
		GP_INT_DI = 0x00,
		GP_INT_PE = 0x08,
		GP_INT_NE = 0x10,
		GP_INT_BE = 0x18,
	};
    

	union Data_v {
		float fval;
		int32_t val;
		uint8_t bval[4];
	};


    /* Struct definition: */
    struct i2cEnc_{	// Encoder register definition //
    	//enum I2C_Register I2C_Reg;
    	uint8_t _add;   //Address
    	uint8_t _stat;
    	uint8_t _stat2;
    	uint8_t _gconf;
    	union Data_v _tem_data;
	// Event callback pointers stored in an array //
		Callback call_back[EV_Range];
    };
    
      
	 //  Configuration methods  //
    
    bool i2cEnc__init(uint8_t add);    // Init + address of the Encoder => false when not enough memory in the array
    void i2cEnc__begin(uint8_t conf);  // Used for initialize the encoder
    void i2cEnc__reset(void);
	void i2cEnc__SetEvent(int EventType, Callback event);

    void i2cEnc__autoconfigInterrupt();
    
	 //     Read functions    //
	uint8_t i2cEnc__readGP1conf(void);
	uint8_t i2cEnc__readGP2conf(void);
	uint8_t i2cEnc__readGP3conf(void);
	uint8_t i2cEnc__readInterruptConfig(void);
    
    
	 //  Status function  //
	bool    i2cEnc__updateStatus(void);
    
	bool    i2cEnc__readStatus_match(enum Int_Status s); //C: must use enum
	uint8_t i2cEnc__readStatus(void);

	bool    i2cEnc__readInt2_match(Int2_Status s);
	uint8_t i2cEnc__readInt2(void);

	bool    i2cEnc__readFadeStatus_match(Fade_Status s);
	uint8_t i2cEnc__readFadeStatus(void);
    
    
	 //  Encoder functions  //
	float   i2cEnc__readCounterFloat(void);
	int32_t i2cEnc__readCounterLong(void);
	int16_t i2cEnc__readCounterInt(void);
	int8_t  i2cEnc__readCounterByte(void);

	int32_t i2cEnc__readMax(void);
	float   i2cEnc__readMaxFloat(void);

	int32_t i2cEnc__readMin(void);
	float   i2cEnc__readMinFloat(void);

	int32_t i2cEnc__readStep(void);
	float   i2cEnc__readStepFloat(void);

	 //  RGB LED Functions  //
	uint8_t i2cEnc__readLEDR(void);
	uint8_t i2cEnc__readLEDG(void);
	uint8_t i2cEnc__readLEDB(void);

	 //  GP LED Functions  //
	uint8_t i2cEnc__readGP1(void);
	uint8_t i2cEnc__readGP2(void);
	uint8_t i2cEnc__readGP3(void);

	 //  Timing registers  //
	uint8_t i2cEnc__readAntibouncingPeriod(void);
	uint8_t i2cEnc__readDoublePushPeriod(void);
	uint8_t i2cEnc__readFadeRGB(void);
	uint8_t i2cEnc__readFadeGP(void);

	 //  EEPROM register  //
	uint8_t i2cEnc__readEEPROM(uint8_t add);

	 // ****    Write functions   ****** //
	void i2cEnc__writeGP1conf(uint8_t gp1);
	void i2cEnc__writeGP2conf(uint8_t gp2);
	void i2cEnc__writeGP3conf(uint8_t gp3);
	void i2cEnc__writeInterruptConfig(uint8_t interrupt);

    
	 //  Encoder functions  //
	void i2cEnc__writeCounter_i(int32_t counter);
	void i2cEnc__writeCounter_f(float counter);

	void i2cEnc__writeMax_i(int32_t max);
	void i2cEnc__writeMax_f(float max);

	void i2cEnc__writeMin_i(int32_t min);
	void i2cEnc__writeMin_f(float min);

	void i2cEnc__writeStep_i(int32_t step);
	void i2cEnc__writeStep_f(float step);
    

	 //  RGB LED Functions  //
	void i2cEnc__writeLEDR(uint8_t rled);
	void i2cEnc__writeLEDG(uint8_t gled);
	void i2cEnc__writeLEDB(uint8_t bled);
	void i2cEnc__writeRGBCode(uint32_t rgb);

	 //  GP LED Functions  //
	void i2cEnc__writeGP1(uint8_t gp1);
	void i2cEnc__writeGP2(uint8_t gp2);
	void i2cEnc__writeGP3(uint8_t gp3);

	 //  Timing registers  //
	void i2cEnc__writeAntibouncingPeriod(uint8_t bounc);
	void i2cEnc__writeDoublePushPeriod(uint8_t dperiod);
	void i2cEnc__writeFadeRGB(uint8_t fade);
	void i2cEnc__writeFadeGP(uint8_t fade);

	 //  EEPROM register  //
	void i2cEnc__writeEEPROM(uint8_t add, uint8_t data);

//private:
    
    
    /*
	void eventCaller(Callback *event);
	*/
    /*
    uint8_t readEncoderByte(uint8_t reg);
	int16_t readEncoderInt(uint8_t reg);
	int32_t readEncoderLong(uint8_t reg);
	float readEncoderFloat(uint8_t reg);
	void writeEncoderByte(uint8_t reg, uint8_t data);
	void writeEncoderInt(uint8_t reg, int32_t data);
	void writeEncoderFloat(uint8_t reg, float data);
	void writeEncoder24bit(uint8_t reg, uint32_t data);
    */
    /*
}; endof class i2cEnc_ */ 

	
	
#endif
