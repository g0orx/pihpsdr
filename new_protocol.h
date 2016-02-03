
// port definitions from host
#define GENERAL_REGISTERS_FROM_HOST_PORT 1024
#define PROGRAMMING_FROM_HOST_PORT 1024
#define RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT 1025
#define TRANSMITTER_SPECIFIC_REGISTERS_FROM_HOST_PORT 1026
#define HIGH_PRIORITY_FROM_HOST_PORT 1027
#define AUDIO_FROM_HOST_PORT 1028
#define TX_IQ_FROM_HOST_PORT 1029

// port definitions to host
#define COMMAND_RESPONCE_TO_HOST_PORT 1024
#define HIGH_PRIORITY_TO_HOST_PORT 1025
#define MIC_LINE_TO_HOST_PORT 1026
#define WIDE_BAND_TO_HOST_PORT 1027
#define RX_IQ_TO_HOST_PORT 1035

#define BUFFER_SIZE 1024

#define modeLSB 0
#define modeUSB 1
#define modeDSB 2
#define modeCWL 3
#define modeCWU 4
#define modeFMN 5
#define modeAM 6
#define modeDIGU 7
#define modeSPEC 8
#define modeDIGL 9
#define modeSAM 10
#define modeDRM 11

extern int data_socket;
extern sem_t response_sem;

extern long response_sequence;
extern int response;

extern unsigned int exciter_power;
extern unsigned int alex_forward_power;
extern unsigned int alex_reverse_power;

void new_protocol_init(int rx,int pixels);
void new_protocol_stop();

void filter_board_changed();
void pa_changed();
void tuner_changed();
void cw_changed();

void set_attenuation(int value);
int get_attenuation();

void set_alex_rx_antenna(unsigned long v);
void set_alex_tx_antenna(unsigned long v);
void set_alex_attenuation(unsigned long v);

void setMox(int state);
int getMox();
void setTune(int state);
int getTune();
int isTransmitting();

double getDrive();
void setDrive(double d);
double getTuneDrive();
void setTuneDrive(double d);

void setSampleRate(int rate);
int getSampleRate();
void setFrequency(long long f);
long long getFrequency();
void setMode(int m);
int getMode();
void setFilter(int low,int high);
int getFilterLow();
int getFilterHigh();

