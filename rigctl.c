/* TS-2000 emulation via TCP
 * Copyright (C) 2016 Steve Wilson <wevets@gmail.com>
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * PiHPSDR RigCtl by Steve KA6S Oct 16 2016
 * 
 */
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "toolbar.h"
#include "sliders.h"
#include "rigctl.h"
#include "radio.h"
#include "channel.h"
#include "filter.h"
#include <string.h>
#include "mode.h"
#include "filter.h"
#include "wdsp_init.h"
#include "bandstack.h"
#include "vfo.h"
#include <pthread.h>

// IP stuff below
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr

#define RIGCTL_DEBUG

// the port client will be connecting to
#define PORT 19090  // This is the HAMLIB port
// max number of bytes we can get at once
#define MAXDATASIZE 300

int init_server ();

int rigctlGetFilterLow();
int rigctlGetFilterHigh();
int rigctlSetFilterLow(int val);
int rigctlSetFilterHigh(int val);
int new_level;

typedef struct com_list {
     char  cmd_string[80];
     struct com_list * next_ent; 
 } com_list_t;
com_list_t  cmd_list;
char cmd_save[80];

char  cmd_input[MAXDATASIZE] ;
char * wptr;
com_list_t * work_input; 
com_list_t * work_list; 
com_list_t * follow_list; 
com_list_t * com_head = NULL;
com_list_t * cur_ent;
int cmdlistcnt;
int retcode = 1;
char workvar[100];

char command[80];
char msg[80];
char msg2[80];
char * newf;

FILE * TTYFILE;
FILE * out;
int  output;
FILTER * band_filter;

static pthread_t rigctl_thread_id;
static void * rigctl (void * arg);

// This stuff from the telnet server
int sockfd, numbytes;
char buf[MAXDATASIZE];
struct hostent *he;

// connector’s address information
struct sockaddr_in their_addr;

int socket_desc , client_sock , c , read_size;
struct sockaddr_in server , client;
char client_message[MAXDATASIZE];

int freq_flag;  // Determines if we are in the middle of receiving frequency info

int digl_offset = 0;
int digl_pol = 0;
int digu_offset = 0;
int digu_pol = 0;
double new_vol = 0;

// Now my stuff
//
// Looks up entry INDEX_NUM in the command structure and
// returns the command string
//

void send_resp (char * msg) {
    #ifdef  RIGCTL_DEBUG
        fprintf(stderr,"RIGCTL: RESP=%s\n",msg);
    #endif
    write(client_sock, msg, strlen(msg));
}

char * cmd_lookup (int index_num) {
    com_list_t * lcl_p;
    lcl_p = com_head;
    if(index_num == 0)  {
       return(lcl_p->cmd_string);
    } else {
      while((lcl_p->next_ent != (com_list_t * ) NULL) && (index_num != 0)) {
          lcl_p = lcl_p->next_ent;
          index_num--;
      }
    }
    if(index_num == 0)  {
      return(lcl_p->cmd_string);
    } else {
      return ((char *) NULL);
    } 
}

static void * rigctl (void * arg) {
        
   int len;
   init_server();
   double meter;
   int save_flag = 0; // Used to concatenate two cmd lines together

  for(;;) {
    fprintf(stderr,"RIGCTL - New run\n");
    while((numbytes=recv(client_sock , cmd_input , 1000 , 0)) > 0 ) {
       
        if(cmd_input[numbytes-1] != ';') {  // Got here because the command line is short
           cmd_input[numbytes] = '\0';      // Turn it into a C string
           strcpy(cmd_save,cmd_input);      // And save a copy of it till next time through
           save_flag = 1;
           #ifdef RIGCTL_DEBUG
           fprintf(stderr,"RIGCTL: *** CMD_SAVE=%s\n",cmd_save);
           #endif
        } else if(save_flag == 1) {
           save_flag = 0;
           cmd_input[numbytes] = '\0';      // Turn it into a C string
           strcat(cmd_save,cmd_input);
           strcpy(cmd_input,cmd_save);      // Cat them together and replace cmd_input
           numbytes = strlen(cmd_input);
           #ifdef RIGCTL_DEBUG
           fprintf(stderr,"RIGCTL: *** CMD_INPUT=%s LEN=%d\n",cmd_input,numbytes);
           #endif
        }

        cmd_input[numbytes-1] = '\0';  // Turn it into a C string.
        len = strlen(cmd_input);
        #ifdef RIGCTL_DEBUG
        fprintf(stderr,"RIGCTL: RCVD=%s LENGTH=%d\n",cmd_input,len);
        #endif
        // Parse the cmd_input
        //int space = command.indexOf(' ');
        //char cmd_char = com_head->cmd_string[0]; // Assume the command is first thing!
        char cmd_str[3];
        cmd_str[0] = cmd_input[0];
        cmd_str[1] = cmd_input[1];
        cmd_str[2] = '\0';
        if(strcmp(cmd_str,"AC")==0)       { // Sets or reads the internal antenna tuner status
                                                   // P1 0:RX-AT Thru, 1: RX-AT IN
                                                   // P2 0: TX-AT Thru 1: TX-AT In
                                                   // 
                                             if(len <= 2) {
                                               send_resp("AC000;");
                                             }
                                          }
        else if(strcmp(cmd_str,"AG")==0) {  // Set Audio Gain - 
                                            // AG123; Value of 0-
                                            // AG1 = Sub receiver - what is the value set
                                            // Response - AG<0/1>123; Where 123 is 0-260
                                            if(len>4) { // Set Audio Gain
                                               volume = (double) atoi(&cmd_input[3])/260; 
/*  KA6S - This crashes the system
    BUG
                                               set_af_gain(volume);               
*/
                                               g_idle_add(update_af_gain,NULL);               
                                            } else { // Read Audio Gain
                                              sprintf(msg,"AG0%03d;",2.6 * volume);
                                              send_resp(msg);
                                              #ifdef RIGCTL_DEBUG
                                              fprintf(stderr,":%s\n",msg);
                                              #endif
                                            }
                                          }
        else if(strcmp(cmd_str,"AI")==0)  {  // Allow rebroadcast of set frequency after set - not supported
                                             if(len <=2) {
                                                send_resp("AI0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AL")==0)  {  // Set/Reads the auto Notch level 
                                             if(len <=2) {
                                                send_resp("AL000;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AM")==0)  {  // Sets or reads the Auto Mode
                                             if(len <=2) {
                                                send_resp("AM0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AN")==0)  {  // Selects the antenna connector (ANT1/2)
                                             if(len <=2) {
                                                send_resp("AN0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AR")==0)  {  // Sets or reads the ASC function on/off
                                             if(len <=2) {
                                                send_resp("AR0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AS")==0)  {  // Sets/reads automode function parameters
                                             // AS<P1><2xP2><11P3><P4>;
                                             // AS<P1><2xP2><11P3><P4>;
                                             if(len < 6) {  
                                                sprintf(msg,"AS%1d%02d%011d%01d;",
                                                             0, // P1
                                                             0, // Automode 
                                                             getFrequency(),
                                                             rigctlGetMode());
                                                send_resp(msg);
                               
                                             } 
                                          }
        else if(strcmp(cmd_str,"BC")==0)  {  // Beat Cancellor OFF
                                             if(len <=2) {
                                                send_resp("BC0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BD")==0)  {  // Moves down the frequency band
                                             // No response 
                                          }
        else if(strcmp(cmd_str,"BP")==0)  {  // Reads the manual beat canceller frequency setting
                                             if(len <=2) {
                                                send_resp("BP000;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BS")==0)  {  // Sets or reads Beat Canceller status
                                             if(len <=2) {
                                                send_resp("BS0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BU")==0)  {  // Moves Up the frequency band
                                             // No response 
                                          }
        else if(strcmp(cmd_str,"BY")==0)  {  // Read the busy signal status
                                             if(len <=2) {
                                                send_resp("BY00;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CA")==0)  {  // Sets/reads cw auto tune function status
                                             if(len <=2) {
                                                send_resp("CA0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CG")==0)  {  // Sets/reads the carrier gain status
                                             // 0-100 legal values
                                             if(len <=2) {
                                                send_resp("CG000;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CH")==0)  {  // Sets the current frequency to call Channel
                                             // No response
                                          }
        else if(strcmp(cmd_str,"CI")==0)  {  // While in VFO mode or memory recall mode, sets freq to the call channel
                                             // No response
                                          }
        else if(strcmp(cmd_str,"CM")==0)  {  // Sets or reads the packet cluster tune f(x) on/off
                                             // 0-100 legal values
                                             if(len <=2) {
                                                send_resp("CM0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CN")==0)  {  // Sets and reads CTSS function - 01-38 legal values
                                             if(len <=2) {
                                                send_resp("CN00;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CT")==0)  {  // Sets and reads CTSS function status
                                             if(len <=2) {
                                                send_resp("CT0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"DC")==0)  {  // Sets/Reads TX band status
                                             if(len <=2) {
                                                send_resp("DC00;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"DN")==0)  {  // Emulate Mic down key 
                                          }
        else if(strcmp(cmd_str,"DQ")==0)  {  // ETs/and reads the DCS function status
                                             if(len <=2) {
                                                send_resp("DQ0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"EX")==0)  {  // Set/reads the extension menu
                                             if(len <=12) {
                                                send_resp("EX00000000000000000000000;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"FA")==0) {  // VFO A frequency
                                            // LEN=7 - set frequency
                                            // Next data will be rest of freq
                                            if(len == 13) { //We are receiving freq info
                                               long long new_freqA = atoll(&cmd_input[2]);
                                               setFrequency(new_freqA);
                                               // KA6S - this kills the system pretty quickly - it freezes
                                               // vfo_update(NULL);
                                               // BUG
                                               g_idle_add(vfo_update,NULL);
                                            } else {
                                               if(len==2) {
                                                  sprintf(msg,"FA%011d;",getFrequency());
                                                  send_resp(msg);
                                               }
                                            }
                                         }
        else if(strcmp(cmd_str,"FB")==0) {  // VFO B frequency
                                            if(len==13) { //We are receiving freq info
                                               long long new_freqA = atoll(&cmd_input[2]); 
                                               setFrequency(new_freqA);
                                            } else if(len == 2) {
                                               sprintf(msg,"FB%011d;",getFrequency());
                                               send_resp(msg);
                                            }
                                         }
        else if(strcmp(cmd_str,"FC")==0) {  // Set Sub receiver freq
                                            // LEN=7 - set frequency
                                            // Next data will be rest of freq
                                            // Len<7 - frequency?
                                            if(len>4) { //We are receiving freq info
                                               long long new_freqA = atoll(&cmd_input[2]);              
                                               //setFrequency(new_freqA);
                                            } else {
                                               sprintf(msg,"FC%011d;",getFrequency());
/*
                                               send_resp(msg);
*/
                                            }
                                         }
        else if(strcmp(cmd_str,"FD")==0)  {  // Read the filter display dot pattern
                                              send_resp("FD00000000;");
                                          }
        else if(strcmp(cmd_str,"FR")==0)  {  // Set/reads the extension menu
                                             if(len <2) {
                                                send_resp("FR0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"FS")==0)  {  // Set/reads fine funct status
                                             if(len <2) {
                                                send_resp("FS0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"FT")==0)  { // Sel or reads the transmitters VFO, M, ch or Call comm   
                                             if(len <2) {
                                                send_resp("FT0;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"FW")==0)  { // Sets/Reas DSP receive filter width in hz 0-9999hz 
                                             if(len <2) {
                                                send_resp("FW0000;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"GT")==0)  { // Sets/Reas AGC constant status
                                             if(len <2) {
                                                send_resp("GT000;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"ID")==0)  { // Read ID - Default to TS-2000 which is type 019.
                                            sprintf(msg,"ID019;");
                                            send_resp(msg);
                                          }
        else if(strcmp(cmd_str,"IF")==0) {  // Reads Transceiver status
                                            // IFFFFFFFFFFFF - Frequency 
                                            //   OFFS        - 4 chars Offset in powers of 10
                                            //   RITRIT      - 6 chars RIT/XIT Frequency - Not supported =0
                                            //   R           - 1 char  RIT Status = 0 is off
                                            //   X           - 1 char  XIT Status = 0 is off
                                            //   0           - 1 char  Channel Bank number - not used
                                            //   00          - 2 char  Channel Bank number - not used
                                            //   C           - 1 char  Mox Status 0=off, 1=on
                                            //   M           - 1 char Operating mode - align with MD commands
                                            //   V           - 1 char VFO Split status - not supported  
                                            //   0           - 1 char Scan status - not supported
                                            //   A           - 1 char - same as FT command
                                            //   0           - 1 char - CTCSS tone - not used
                                            //   00          - 2 char - More tone control
                                            //   0           - 1 char - Shift status
                                            sprintf(msg,"IF%011d%04d%06d%1d%1d%1d%02d%01d%01d%01d%01d%01d%01d%02d%01d;",
                                                         getFrequency(),
                                                         0,  // Shift Offset
                                                         0,  // Rit Freq
                                                         0,  // Rit Status
                                                         0,  // XIT Status
                                                         0,  // Channel Bank num
                                                         0,  // Channel Bank num
                                                         0,  // Mox Status
                                                         rigctlGetMode(),  // Mode Status - same as MD Command
                                                         0,  // VFO Status
                                                         0,  // Scan status
                                                         0,  // FT command 
                                                         0,  // CTSS Tone
                                                         0,  // More tone control
                                                         0); // Shift status
                                            send_resp(msg);
                                         }
        else if(strcmp(cmd_str,"IS")==0)  { // Sets/Reas IF shift funct status
                                             if(len <2) {
                                                send_resp("IS00000;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"KS")==0)  { // Sets/Reads keying freq - 0-060 max
                                             if(len <2) {
                                                send_resp("KS000;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"KY")==0)  { // Convert char to morse code
                                             if(len <2) {
                                                send_resp("KY0;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"LK")==0)  { // Set/read key lock function status
                                             if(len <2) {
                                                send_resp("LK00;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"LM")==0)  { // Set/read DRU 3A unit or elect keyer recording status
                                             if(len <2) {
                                                send_resp("LM0;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"LT")==0)  { // Set/read Alt function
                                             if(len <2) {
                                                send_resp("LT0;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"MC")==0) {  // Recalls or reads memory channel
                                             if(len <2) {
                                               send_resp("MC000;"); // Link this to band stack at some point
                                             }
                                         }
        else if(strcmp(cmd_str,"MD")==0) {  // Mode - digit selects mode
                                            // 1 = LSB
                                            // 2 = USB
                                            // 3 = CWU
                                            // 4 = FM
                                            // 5 = AM
                                            // 6 = DIGL
                                            // 7 = CWL
                                            // 9 = DIGU 
                                            int new_mode;
                                            if(len > 2) { // Set Mode
                                               switch(atoi(&cmd_input[2])) {   
                                                 case 1:  
                                                     new_mode = modeLSB;
                                                     break;
                                                 case 2:  
                                                     new_mode = modeUSB;
                                                     break;
                                                 case 3:  
                                                     new_mode = modeCWU;
                                                     break;
                                                 case 4:  
                                                     new_mode = modeFMN;
                                                     break;
                                                 case 5:  
                                                     new_mode = modeAM;
                                                     break;
                                                 case 6:  
                                                     new_mode = modeDIGL;
                                                     break;
                                                 case 7:  
                                                     new_mode = modeCWL;
                                                     break;
                                                 case 9:  
                                                     new_mode = modeDIGU;
                                                     break;
                                                 default:
                                                     break;
                                                     #ifdef RIGCTL_DEBUG
                                                     fprintf(stderr,"MD command Unknown");
                                                     #endif
                                               }
                                            // Other stuff to switch modes goes here..
                                            // since new_mode has the interpreted command in 
                                            // in it now.
                                            BANDSTACK_ENTRY *entry;
                                            entry= (BANDSTACK_ENTRY *) 
                                                  bandstack_entry_get_current();
                                            entry->mode=new_mode;
                                            setMode(entry->mode);
                                            
                                            FILTER* band_filters=filters[entry->mode];
                                            FILTER* band_filter=&band_filters[entry->filter];
                                            setFilter(band_filter->low,band_filter->high);
                                            /* Need a way to update VFO info here..*/
                                            }  else {     // Read Mode
                                               int curr_mode;
                                               switch (mode) {
                                                  case modeLSB: curr_mode = 1; 
                                                          break;   
                                                  case modeUSB: curr_mode = 2;
                                                          break;
                                                  case modeCWL: curr_mode = 7; // CWL
                                                          break;
                                                  case modeCWU: curr_mode = 3; // CWU
                                                          break;
                                                  case modeFMN: curr_mode = 4; // FMN
                                                          break;
                                                  case modeAM: curr_mode = 5; // AM
                                                          break;
                                                  case modeDIGU: curr_mode = 9; // DIGU
                                                          break;
                                                  case modeDIGL: curr_mode = 6; // DIGL
                                                          break;
                                                  default: 
                                                          #ifdef RIGCTL_DEBUG
                                                            fprintf(stderr,
                                                            "RIGCTL: MD command doesn't support %d\n",
                                                            mode);
                                                          #endif
                                                          break;
                                               }
                                               sprintf(msg,"MD%1d;",curr_mode);
                                               send_resp(msg);
                                            }
                                         }
        else if(strcmp(cmd_str,"MF")==0) {  // Set/read menu A/B
                                             if(len <2) {
                                               send_resp("MF0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"MG")==0) {  // Mike Gain - 3 digit value 
                                            if(len <2) {
                                               sprintf(msg,"MG%03d",mic_gain * 100);
                                               send_resp(msg);
                                            } else {
                                               int tval = atoi(&cmd_input[2]);                
                                               new_vol = (double) tval/100; 
                                               set_mic_gain(new_vol); 
                                            }
                                         }
        else if(strcmp(cmd_str,"ML")==0) {  // Set/read the monitor function level
                                             if(len <2) {
                                               send_resp("ML000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"MO")==0) {  // Set Monitor on/off
                                             if(len <2) {
                                               send_resp("MO0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"MR")==0) {  // Read Memory Channel data
                                             sprintf("MR%1d%02d%02d%011d%1d%1d%1d%02d%02d%03d%1d%1d%09d%02d%1d%08d;",
                                                      0, // P1 - Rx Freq - 1 Tx Freq
                                                      0, // P2 Bankd and channel number -- see MC comment
                                                      0, // P3 - see MC comment 
                                                      getFrequency(), // P4 - frequency
                                                      rigctlGetMode(), // P5 - Mode
                                                      0, // P6 - Locked status
                                                      0, // P7 - O-off, 1-tone, 2-CTCSS, 3 =DCS
                                                      0, // P8 - Tone Numbe3r - see page 35
                                                      0, // P9 - CTCSS tone number - see CN command
                                                      0, // P10 - DCS code - see QC command 
                                                      0, // P11 - Reverse status
                                                      0, // P12 - Shift status - see OS command
                                                      0, // P13 - Offset freq - see OS command
                                                      0, // P14 - Step Size - see ST command
                                                      0, // P15 - Memory Group Number (0-9)
                                                      0);  // P16 - Memory Name - 8 char max
                                               send_resp(msg);
                                         }
        else if(strcmp(cmd_str,"MU")==0) {  // Set/Read Memory Group Data
                                             if(len <2) {
                                               send_resp("MU0000000000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"MW")==0) {  // Store Data to Memory Channel
                                         }
        else if(strcmp(cmd_str,"NB")==0) {  // Set/Read Noise Blanker func status (on/off)
                                             if(len <2) {
                                               send_resp("NB0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"NL")==0) {  // Set/Read Noise Reduction  Level
                                             if(len <2) {
                                               send_resp("NL000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"NR")==0) {  // Set/Read Noise Reduction function status
                                             if(len <2) {
                                               send_resp("NR0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"NT")==0) {  // Set/Read autonotch function
                                             if(len <2) {
                                               send_resp("NT0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"OF")==0) {  // Set/Read Offset freq (9 digits - hz)
                                             if(len <2) {
                                               send_resp("OF000000000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"OI")==0) {  // Read Memory Channel Data
                                             if(len <2) {
                                               sprintf(msg,"OI%011d%04d%06d%1d%1d%1d%02d%1d%1d%1d%1d%1d%1d%02d%1d;",
                                                  getFrequency(),
                                                  0, // P2 - Freq Step size
                                                  0, // P3 - Rit/Xit Freq 
                                                  0, // P4 - RIT off/Rit On
                                                  0, // P5 - XIT off/on
                                                  0, // P6 - Channel
                                                  0, // P7 - Bank
                                                  0, // P8 - 0RX, 1TX
                                                  rigctlGetMode(),  // P10 - MD
                                                  0, // P11 - SC command
                                                  0, // P12 Split op - SP command
                                                  0, // P13 Off, 1, 2, 
                                                  0, // P14 Tone freq - See TN command
                                                  0);
                                               send_resp(msg);
                                             }
                                         }
        else if(strcmp(cmd_str,"OS")==0) {  // Set/Read Offset freq (9 digits - hz)
                                             if(len <2) {
                                               send_resp("OS0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PA")==0) {  // Set/Read Preamp function status
                                             if(len <2) {
                                               send_resp("PA00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PB")==0) {  // Set/Read DRU-3A Playback status
                                             if(len <2) {
                                               send_resp("PB0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PC")==0) {  // Set/Read Drive Power output
                                            if(len==2) {
                                              sprintf(msg,"PC%03d;",drive*100);
                                              send_resp(msg); 
                                            } else {
                                               // Power Control - 3 digit number- 0 to 100
                                               //Scales to 0.00-1.00
                                                
                                               double drive_val = 
                                                   (double)(atoi(&cmd_input[2]))/10; 
                                               // setDrive(drive_val);
                                               set_drive(drive_val);
                                            }
                                         }
        else if(strcmp(cmd_str,"PI")==0) {  // STore the programmable memory channel
                                         }
        else if(strcmp(cmd_str,"PK")==0) {  // Reads the packet cluster data
                                               send_resp("PK000000000000000000000000000000000000000000000000;"); 
                                         }
        else if(strcmp(cmd_str,"PL")==0) {  // Set/Read Speech processor level
                                            // P1 000 - min-100 max
                                            // P2 000 - min - 100 max
                                             if(len <2) {
                                               send_resp("PL000000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PM")==0) {  // Recalls the Programmable memory
                                             if(len <2) {
                                               send_resp("PM0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PR")==0) {  // Sets/reads the speech processor function on/off
                                            // 0 - off, 1=on
                                             if(len <2) {
                                               send_resp("PR0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PS")==0) {  // Sets/reads Power on/off state
                                            // 0 - off, 1=on
                                             if(len <2) {
                                               send_resp("PS1;"); // Lets pretend we're powered up ;-) 
                                             }
                                         }
        else if(strcmp(cmd_str,"PS")==0) {  // Sets/reads DCS code
                                            // Codes numbered from 000 to 103.
                                             if(len <2) {
                                               send_resp("QC000;"); // Lets pretend we're powered up ;-) 
                                             }
                                         }
        else if(strcmp(cmd_str,"QI")==0) {  // Store the settings in quick memory
                                         }
        else if(strcmp(cmd_str,"QR")==0) {  // Send the Quick memory channel data
                                            // P1 - Quick mem off, 1 quick mem on
                                            // P2 - Quick mem channel number
                                             if(len <2) {
                                               send_resp("QR00;"); // Lets pretend we're powered up ;-) 
                                             }
                                         }
        else if(strcmp(cmd_str,"RA")==0) {  // Sets/reads Attenuator function status
                                            // 00-off, 1-99 -on - Main and sub receivers reported
                                             if(len <2) {
                                               send_resp("RA0000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RC")==0) {  // Clears the RIT offset freq
                                         }
        else if(strcmp(cmd_str,"RD")==0) {  // Move the RIT offset freq down, slow down the scan speed in scan mode
                                             if(len <2) {
                                               send_resp("RD0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RG")==0) {  // RF Gain - 3 digit number
                                            // RG123; 0-255 value
                                            // Scale from -20 - 120
                                            if(len>4) { // Set Audio Gain
                                               int base_value = atoi(&cmd_input[2]);
                                               float new_gain = ((((float) base_value)/255) * 140) - 20; 
                                               set_agc_gain(new_gain);               
                                            } else { // Read Audio Gain
                                              sprintf(msg,"RG%03d;",(255/140)*(agc_gain+20));
                                              send_resp(msg);
                                            }
                                         }
        else if(strcmp(cmd_str,"RL")==0) {  // Set/read Noise reduction level
                                             if(len <2) {
                                               send_resp("RL00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RM")==0) {  // Set/read Meter function
                                            // P1- 0, unsel, 1 SWR, 2 Comp, 3 ALC
                                            // P2 - 4 dig - Meter value in dots - 000-0030
                                             if(len <2) {
                                               send_resp("RM00000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RT")==0) {  // Set/read the RIT function status
                                             if(len <2) {
                                               send_resp("RT0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RU")==0) {  // Set/move RIT frequency up
                                             if(len <2) {
                                               send_resp("RU0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RX")==0) {  // Unkey Xmitter
                                            setMox(0);
                                            // 0-main, 1=sub
                                             if(len <2) {
                                               send_resp("RX0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SA")==0) {  // Set/reads satellite mode status
                                            // 0-main, 1=sub
                                             if(len <2) {
                                               send_resp("SA000000000000000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SB")==0) {  // Set/read the SUB TF-W status
                                             if(len <2) {
                                               send_resp("SB0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SC")==0) {  // Set/read the Scan function status
                                             if(len <2) {
                                               send_resp("SC0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SD")==0) {  // Set/read CW break-in time delay
                                            // 0000-1000 ms (in steps of 50 ms) 0000 is full break in
                                             if(len <2) {
                                               send_resp("SD0000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SH")==0) {  // Set/read the DSP filter settings
                                             if(len <2) {
                                               send_resp("SH00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SI")==0) {  // Enters the satellite memory name
                                         }
        else if(strcmp(cmd_str,"SL")==0) {  // Set/read the DSP filter settings - this appears twice? See SH
                                             if(len <2) {
                                               send_resp("SL00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SM")==0) {  // Read SMETER
                                            // SMx;  x=0 RX1, x=1 RX2 
                                            // meter is in dbm - value will be 0<260
                                            // Translate to 00-30 for main, 0-15 fo sub
                                            // Resp= SMxAABB; 
                                            // 
                                            double level;
                                            level = GetRXAMeter(CHANNEL_RX0, smeter); 
                                            level = level + (double) get_attenuation();
                                            new_level = (int) level;            
                                            #ifdef RIGCTL_DEBUG
                                            fprintf(stderr,"RIGCTL: SM level=%.21f new_level=%d\n",
                                                        level,new_level);
                                            #endif
                                            sprintf(msg,"SM%1c%04d;",
                                                cmd_input[2],new_level);
                                            send_resp(msg);
                                         }   
        else if(strcmp(cmd_str,"SQ")==0) {  // Set/read the squelch level
                                            // P1 - 0- main, 1=sub
                                            // P2 - 0-255
                                             if(len <2) {
                                               send_resp("SQ0000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SR")==0) {  // Resets the transceiver
                                         }
        else if(strcmp(cmd_str,"SS")==0) {  // Set/read Scan pause freq
                                             if(len <2) {
                                               send_resp("SS0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"ST")==0) {  // Set/read the multi/ch control freq steps
                                             if(len <2) {
                                               send_resp("ST00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SU")==0) {  // Set/read the scan pause freq
                                             if(len <2) {
                                               send_resp("SU00000000000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SV")==0) {  // Execute the memory transfer function
                                         }
        else if(strcmp(cmd_str,"TC")==0) {  // Set/read the internal TNC mode
                                             if(len <2) {
                                               send_resp("TC00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TD")==0) {  // Sends the DTMF memory channel
                                         }
        else if(strcmp(cmd_str,"TI")==0) {  // Reads the TNC LED status
                                             if(len <2) {
                                               send_resp("TI00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TN")==0) {  // Set/Read sub tone freq
                                             if(len <2) {
                                               send_resp("TN00;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TO")==0) {  // Set/Read tone function
                                             if(len <2) {
                                               send_resp("TO0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TS")==0) {  // Set/Read TF Set function status
                                             if(len <2) {
                                               send_resp("TS0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TX")==0) {  // Key Xmitter - P1 - transmit on main/sub freq
                                            setMox(1);
                                             if(len <2) {
                                               send_resp("TS0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TY")==0) {  // Set/Read uP firmware type
                                             if(len <2) {
                                               send_resp("TY000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"UL")==0) {  // Detects the PLL unlock status - this should never occur - xmit only
                                             if(len <2) {
                                               send_resp("UL0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"UP")==0) {  // Emulates the mic up key
                                         }
        else if(strcmp(cmd_str,"VD")==0) {  // Sets/Reads VOX dleay time - 0000-3000ms in steps of 150
                                             if(len <2) {
                                               send_resp("VD0000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"VG")==0) {  // Sets/Reads VOX gain 000-009
                                             if(len <2) {
                                               send_resp("VG000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"VG")==0) {  // Emulates the voide 1/2 key
                                         }
        else if(strcmp(cmd_str,"VX")==0) {  // Sets/reads vox f(x) state
                                             if(len <2) {
                                               send_resp("VX0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"XT")==0) {  // Sets/reads the XIT f(x) state
                                             if(len <2) {
                                               send_resp("XT0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"XX")==0) {  // 
                                            // Format RL01234: First dig 0=neg, 1=pos number
                                            //                 1-4- digital offset in hertz.
                                            if(len > 4) { // It is set instead of a read
                                               digl_pol = (cmd_input[2]=='0') ? -1 : 1;
                                               digl_offset = atoi(&cmd_input[3]); 
                                               #ifdef RIGCTL_DEBUG
                                               fprintf(stderr,"RIGCTL:RL set %d %d",digl_pol,digl_offset); 
                                               #endif
                                            } else {
                                               if(digl_pol==1) { // Nah - its a read
                                                 sprintf(msg,"RL1%04d;");
                                               } else {
                                                 sprintf(msg,"RL0%04d;");
                                               }         
                                               send_resp(msg);
                                               #ifdef RIGCTL_DEBUG
                                               fprintf(stderr,":%s\n",msg);
                                               #endif
                                            }
                                         }
        else if(strcmp(cmd_str,"XY")==0) {  // set/read RTTY DIGL offset frequency - Not available - just store values
                                            // Format RL01234: First dig 0=neg, 1=pos number
                                            //                 1-4- digital offset in hertz.
                                            if(len > 4) { // It is set instead of a read
                                               digl_pol = (cmd_input[2]=='0') ? -1 : 1;
                                               digl_offset = atoi(&cmd_input[3]); 
                                               #ifdef RIGCTL_DEBUG
                                               fprintf(stderr,"RIGCTL:RL set %d %d",digl_pol,digl_offset); 
                                               #endif
                                            } else {
                                               if(digl_pol==1) { // Nah - its a read
                                                 sprintf(msg,"RL1%04d;");
                                               } else {
                                                 sprintf(msg,"RL0%04d;");
                                               }         
                                               send_resp(msg);
                                               #ifdef RIGCTL_DEBUG
                                               fprintf(stderr,":%s\n",msg);
                                               #endif
                                            }
                                         }
        else                             {
                                            fprintf(stderr,"RIGCTL: UNKNOWN\n");
                                         }
   } 
    // Got here because pipe closed 
   if(numbytes == 0) {
         fprintf(stderr,"RIGCTL: Client disconnected\n");
         close(client_sock);
         sleep(1);
         client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
   }
 }
}

char * rigctlGetFilter()
{
   
    char * m = mode_string[mode];

    if (strcmp(m,"CWU") == 0){
       return (char *) (getFilterHigh() + getFilterLow());
    }
    else
    if (strcmp(m,"CWL") == 0){
       return (char *) (getFilterHigh() + getFilterLow());
    }
    else
    return (char *) (getFilterHigh() - getFilterLow());
}


void launch_rigctl () {
   int err;
   fprintf(stderr, "LAUNCHING RIGCTL!!\n");
   // This routine encapsulates the pthread call
   err = pthread_create (&rigctl_thread_id,NULL,rigctl,NULL);
   if(err != 0) {
       fprintf(stderr, "pthread_create failed on rigctl launch\n");
   }  
}

//
// Telnet Server Code below:
//
     // the port client will be connecting to
    #define PORT 19090  // This is the HAMLIB port
    // max number of bytes we can get at once
    #define MAXDATASIZE 300

int init_server () {

    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        fprintf(stderr,"RIGCTL: Could not create socket");
    }
    fprintf(stderr, "RIGCTL: Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( PORT );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        fprintf(stderr,"RIGCLT: bind failed. Error");
        return 1;
    }
    fprintf(stderr,"RIGCTL: bind done");
     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    fprintf(stderr,"RIGCTL: Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
     
    //accept connection from an incoming client
    client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
    if (client_sock < 0)
    {
        fprintf(stderr,"RIGCTL: Accept failed");
        return 1;
    }
    fprintf(stderr,"RIGCTL: Connection accepted\n");
}

int rigctlGetFilterLow() {
   int lookup;
   int rval;

   BANDSTACK_ENTRY *entry;
   entry = (BANDSTACK_ENTRY *) bandstack_entry_get_current();

   FILTER* band_filters=filters[entry->mode];
   FILTER* band_filter=&band_filters[entry->filter];
   rval = 0;
   for(lookup = 0; lookup<=9; lookup++) {
      if(band_filter[lookup].low == -150) {
          rval = lookup;
          break;
      }      
   }
   return rval;
}

int rigctlGetFilterHigh() {
   int lookup;
   int rval;

   BANDSTACK_ENTRY *entry;
   entry = (BANDSTACK_ENTRY *) bandstack_entry_get_current();

   FILTER* band_filters=filters[entry->mode];
   FILTER* band_filter=&band_filters[entry->filter];
   rval = 0;
   for(lookup = 0; lookup<=9; lookup++) {
      if(band_filter[lookup].high == -150) {
          rval = lookup;
          break;
      }      
   }
   return rval;
}
int rigctlGetMode()  {
        BANDSTACK_ENTRY *entry;
        entry= (BANDSTACK_ENTRY *) 
        bandstack_entry_get_current();
        switch(entry->mode) {
           case modeLSB: return(1); // LSB
           case modeUSB: return(2); // USB
           case modeCWL: return(7); // CWL
           case modeCWU: return(3); // CWU
           case modeFMN: return(4); // FMN
           case modeAM:  return(5); // AM
           case modeDIGU: return(9); // DIGU
           case modeDIGL: return(6); // DIGL
           default: return(0);
        }
}

int rigctlSetFilterLow(int val){
};
int rigctlSetFilterHigh(int val){
};
