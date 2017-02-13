
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
 * With a kindly assist from Jae, K5JAE who has helped
 * greatly with hamlib integration!
 */
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <unistd.h>
//#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "toolbar.h"
#include "sliders.h"
#include "rigctl.h"
#include "radio.h"
#include "receiver.h"
#include "channel.h"
#include "filter.h"
#include <string.h>
#include "mode.h"
#include "filter.h"
#include "band.h"
#include "bandstack.h"
#include "vfo.h"
#include "sliders.h"
#include <pthread.h>
#include "agc.h"
#include <wdsp.h>
#include "store.h"

// IP stuff below
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr

//#undef RIGCTL_DEBUG
#define RIGCTL_DEBUG

// the port client will be connecting to
// 2-26-17 K5JAE - Changed the defines to const ints to allow use via pointers.
static const int TelnetPortA = 19090;
static const int TelnetPortB = 19091;
static const int TelnetPortC = 19092;

// max number of bytes we can get at once
#define MAXDATASIZE 300

int init_server ();
void parse_cmd ();

int rigctlGetFilterLow();
int rigctlGetFilterHigh();
int rigctlSetFilterLow(int val);
int rigctlSetFilterHigh(int val);
int new_level;
int active_transmitter = 0;

int cat_control;

extern int enable_tx_equalizer;

/*
pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;
*/
typedef struct {GMutex m; } GT_MUTEX;
GT_MUTEX * mutex_a;
GT_MUTEX * mutex_b;

FILE * out;
int  output;
FILTER * band_filter;

//static pthread_t rigctl_a_thread_id, rigctl_b_thread_id, rigctl_c_thread_id;
static GThread *rigctl_a_thread_id, *rigctl_b_thread_id, *rigctl_c_thread_id;

int squelch=100; //local sim of squelch level


int read_size;

int freq_flag;  // Determines if we are in the middle of receiving frequency info

int digl_offset = 0;
int digl_pol = 0;
int digu_offset = 0;
int digu_pol = 0;
double new_vol = 0;
int  lcl_cmd=0;
long long new_freqA = 0;
long long new_freqB = 0;
long long orig_freqA = 0;
long long orig_freqB = 0;
int  lcl_split = 0;
int  mox_state = 0;
// Radio functions - 
// Memory channel stuff and things that aren't 
// implemented - but here for a more complete emulation
int ctcss_tone;  // Numbers 01-38 are legal values - set by CN command, read by CT command
int ctcss_mode;  // Numbers 0/1 - on off.

int vfo_sm=0;   // VFO State Machine - this keeps track of

// Now my stuff
//

// This looks up the frequency of the Active receiver with 
// protection for 1 versus 2 receivers
long long rigctl_getFrequency() {
   if(receivers == 1) {
      return vfo[VFO_A].frequency;
   } else {
      return vfo[active_receiver->id].frequency;
   } 
}
// Looks up entry INDEX_NUM in the command structure and
// returns the command string
//
void send_resp (int client_sock,char * msg) {
    #ifdef  RIGCTL_DEBUG
        fprintf(stderr,"RIGCTL: RESP=%s\n",msg);
    #endif
    write(client_sock, msg, strlen(msg));
}

//
// 2-25-17 - K5JAE - removed duplicate rigctl
//

//static void * rigctl_a (void * arg) {
static gpointer rigctl (gpointer data) {
   int len;
   int c;
   int socket_desc; 
   struct sockaddr_in client;
   struct sockaddr_in server;

   // user data void* is really an int* of the port
   int port = *((int*) data);
   int client_sock = init_server(&socket_desc, &client, &server, port);

   g_mutex_lock(&mutex_a->m);
   cat_control++;
//#ifdef RIGCTL_DEBUG
fprintf(stderr,"RIGCTL: CTLA INC cat_contro=%d\n",cat_control);
//#endif
   g_mutex_unlock(&mutex_a->m);
   g_idle_add(vfo_update,NULL);

   int save_flag = 0; // Used to concatenate two cmd lines together
   int semi_number = 0;
   int i;
   char * work_ptr;
   char work_buf[MAXDATASIZE];
   int numbytes;
   char  cmd_input[MAXDATASIZE] ;
   char cmd_save[80];

  for(;;) {
    fprintf(stderr,"RIGCTL - New run on server port: %d\n", port);
    while((numbytes=recv(client_sock , cmd_input , MAXDATASIZE-2 , 0)) > 0 ) {
         for(i=0;i<numbytes;i++)  { work_buf[i] = cmd_input[i]; }
         work_buf[i+1] = '\0';
         #ifdef RIGCTL_DEBUG
         fprintf(stderr,"RIGCTL: RCVD=%s<-\n",work_buf);
         #endif

        // Need to handle two cases
        // 1. Command is short, i.e. no semicolon - that will set save_flag=1 and
        //    read another line..
        // 2. 1 to N commands per line. Turns out N1MM sends multiple commands per line
         
        if(save_flag == 0) { // Count the number of semicolons if we aren't already in mode 1.
          semi_number = 0;
          for(i=0;i<numbytes;i++) {
             if(cmd_input[i] == ';') { semi_number++;};
          } 
        }
        if((save_flag == 0) && (semi_number == 0)) {
           cmd_input[numbytes] = '\0';      // Turn it into a C string
           strcpy(cmd_save,cmd_input);      // And save a copy of it till next time through
           save_flag = 1;
        } else if(save_flag == 1) {
           save_flag = 0;
           cmd_input[numbytes] = '\0';      // Turn it into a C string
           strcat(cmd_save,cmd_input);
           strcpy(cmd_input,cmd_save);      // Cat them together and replace cmd_input
           numbytes = strlen(cmd_input);
        } 
        if(save_flag != 1) {
           work_ptr = strtok(cmd_input,";");
           while(work_ptr != NULL) {
               /*
               pthread_mutex_lock (& mutex_b);
               */
               g_mutex_lock(&mutex_b->m);
               parse_cmd(work_ptr,strlen(work_ptr),client_sock);
               /*
               pthread_mutex_unlock (& mutex_b);
               */
               g_mutex_unlock(&mutex_b->m);
               work_ptr = strtok(NULL,";");
           }
           for(i=0;i<MAXDATASIZE;i++){
                cmd_input[i] = '\0'; 
                work_buf[i]  = '\0';  // Clear the input buffer
           }
        }
       // Got here because socket closed 
    }
    if(numbytes == 0) {
         fprintf(stderr,"RIGCTL: Client disconnected\n");
         close(client_sock);

         // Decrement CAT_CONTROL
         g_mutex_lock(&mutex_a->m);
         cat_control--;
#ifdef RIGCTL_DEBUG
fprintf(stderr,"RIGCTL: CTLA DEC - cat_control=%d\n",cat_control);
#endif
         g_mutex_unlock(&mutex_a->m);
         g_idle_add(vfo_update,NULL);

         //sleep(1);
         c = sizeof(struct sockaddr_in);
         client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);

         // Increment CAT_CONTROL
         g_mutex_lock(&mutex_a->m);
         cat_control++;
#ifdef RIGCTL_DEBUG
fprintf(stderr,"RIGCTL: CTLA INC - cat_control=%d\n",cat_control);
#endif
         g_mutex_unlock(&mutex_a->m);
         g_idle_add(vfo_update,NULL);

         //sleep(1);
    }
 }
}

// 
// FT command intepret vfo_sm state - used by IF command
//
int ft_read() {
   return(active_transmitter);
}
// 
// Determines RIT state - used by IF command
//
int rit_on () {
  if(receivers == 1) { // Worry about 1 versus 2 radios
      if(vfo[VFO_A].rit != 0) {
         return 1;
      } else {
         return 0;
      }  
  } else { // Well - we have two so use active_reciever->id
      if(vfo[active_receiver->id].rit != 0) {
          return 1 ;
      } else {
          return 0;
      }
  }
}
void parse_cmd ( char * cmd_input,int len,int client_sock) {
        int work_int;     
        int new_low, new_high;
        double meter;
        char msg[80];
        char buf[80];
        BANDSTACK_ENTRY *entry;
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
                                               send_resp(client_sock,"AC000;");
                                             }
                                          }
        else if(strcmp(cmd_str,"AG")==0) {  // Set Audio Gain - 
                                            // AG123; Value of 0-
                                            // AG1 = Sub receiver - what is the value set
                                            // Response - AG<0/1>123; Where 123 is 0-260
                                            int lcl_receiver;
                                            if(receivers == 1) {
                                                lcl_receiver = 0;
                                            } else {
                                                lcl_receiver = active_receiver->id;
                                            }
                                            if(len>4) { // Set Audio Gain
                                               active_receiver->volume = (double) atoi(&cmd_input[3])/260; 
                                               g_idle_add(update_af_gain,NULL);               
                                            } else { // Read Audio Gain
                                              sprintf(msg,"AG%1d%03d;",lcl_receiver,(int) (2.6 * active_receiver->volume));
                                              send_resp(client_sock,msg);
                                            }
                                          }
        else if(strcmp(cmd_str,"AI")==0)  {  // Allow rebroadcast of set frequency after set - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"AI0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AL")==0)  {  // Set/Reads the auto Notch level 
                                             if(len <=2) {
                                                //send_resp(client_sock,"AL000;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AM")==0)  {  // Sets or reads the Auto Mode
                                             if(len <=2) {
                                                //send_resp(client_sock,"AM0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AN")==0)  {  // Selects the antenna connector (ANT1/2)
                                             if(len <=2) {
                                                //send_resp(client_sock,"AN0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AR")==0)  {  // Sets or reads the ASC function on/off
                                             if(len <=2) {
                                                //send_resp(client_sock,"AR0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"AS")==0)  {  // Sets/reads automode function parameters
                                             // AS<P1><2xP2><11P3><P4>;
                                             // AS<P1><2xP2><11P3><P4>;
                                             if(len < 6) {  
                                             /*   sprintf(msg,"AS%1d%02d%011lld%01d;",
                                                             0, // P1
                                                             0, // Automode 
                                                             getFrequency(),
                                                             rigctlGetMode());
                                                send_resp(client_sock,msg);*/
                                                send_resp(client_sock,"?;");
                               
                                             } 
                                          }
        else if(strcmp(cmd_str,"BC")==0)  {  // Beat Cancellor OFF
                                             if(len <=2) {
                                                //send_resp(client_sock,"BC0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BD")==0)  {  // Moves down the frequency band
                                             // No response 
                                          }
        else if(strcmp(cmd_str,"BP")==0)  {  // Reads the manual beat canceller frequency setting
                                             if(len <=2) {
                                                //send_resp(client_sock,"BP000;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BS")==0)  {  // Sets or reads Beat Canceller status
                                             if(len <=2) {
                                                //send_resp(client_sock,"BS0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BU")==0)  {  // Moves Up the frequency band
                                             // No response 
                                          }
        else if(strcmp(cmd_str,"BY")==0)  {  // Read the busy signal status
                                             if(len <=2) {
                                                //send_resp(client_sock,"BY00;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CA")==0)  {  // Sets/reads cw auto tune function status
                                             if(len <=2) {
                                                //send_resp(client_sock,"CA0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CG")==0)  {  // Sets/reads the carrier gain status
                                             // 0-100 legal values
                                             if(len <=2) {
                                                //send_resp(client_sock,"CG000;");
                                                send_resp(client_sock,"?;");
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
                                                //send_resp(client_sock,"CM0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"CN")==0)  {  // Sets and reads CTSS function - 01-38 legal values
                                             // Stored locally in rigctl - not used.
                                             if(len <=2) {
                                                sprintf(msg,"CN%02d;",ctcss_tone);
                                                send_resp(client_sock,msg);
                                                send_resp(client_sock,"?;");
                                             } else {
                                                ctcss_tone = atoi(&cmd_input[2]);
                                             }
                                          }
        else if(strcmp(cmd_str,"CT")==0)  {  // Sets and reads CTSS function status
                                             // Stored locally in rigctl - not used.
                                             if(len <=2) {
                                                sprintf(msg,"CT%01d;",ctcss_mode);
                                                send_resp(client_sock,msg);
                                             } else {
                                                ctcss_mode = atoi(&cmd_input[2]);
                                             }
                                          }
        else if(strcmp(cmd_str,"DC")==0)  {  // Sets/Reads TX band status
                                             if(len <=2) {
                                                //send_resp(client_sock,"DC00;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"DN")==0)  {  // Emulate Mic down key 
                                          }
        else if(strcmp(cmd_str,"DQ")==0)  {  // ETs/and reads the DCS function status
                                             if(len <=2) {
                                                //send_resp(client_sock,"DQ0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"EX")==0)  {  // Set/reads the extension menu
                                             // This routine only can look up specific information;
                                             // And only performs reads at this point..
                                             // EX P1 P1 P1 P2 P2 P3 P4 ; - Read command                                      
                                             int p5=0;
                                             strncpy(buf,cmd_input,9); // Get the front of the response
                                             if(len == 10) {  // Read command                                                
                                                 // CW Sidetone frequendcy
                                                 if(strncmp(&cmd_input[2],"031",3) == 0) {
                                                   if(cw_keyer_sidetone_frequency <=400) {
                                                     p5 = 0; 
                                                   } else if (cw_keyer_sidetone_frequency <=450) {
                                                     p5 = 1; 
                                                   } else if (cw_keyer_sidetone_frequency <=500) {
                                                     p5 = 2; 
                                                   } else if (cw_keyer_sidetone_frequency <=550) {
                                                     p5 = 3; 
                                                   } else if (cw_keyer_sidetone_frequency <=600) {
                                                     p5 = 4; 
                                                   } else if (cw_keyer_sidetone_frequency <=650) {
                                                     p5 = 5; 
                                                   } else if (cw_keyer_sidetone_frequency <=700) {
                                                     p5 = 6; 
                                                   } else if (cw_keyer_sidetone_frequency <=750) {
                                                     p5 = 7; 
                                                   } else if (cw_keyer_sidetone_frequency <=800) {
                                                     p5 = 8; 
                                                   } else if (cw_keyer_sidetone_frequency <=850) {
                                                     p5 = 9; 
                                                   }     
                                                   sprintf(msg,"%s%01d;",buf,p5); 
                                                   send_resp(client_sock,msg);
                                                  // SPLIT
                                                 } else if(strncmp(&cmd_input[2],"06A",3) == 0) {
                                                   sprintf(msg,"%s%01d;",buf,split); 
                                                   send_resp(client_sock,msg);
                                                } else  { 
                                                   send_resp(client_sock,"?;");
                                                }
                                            }
                                          }
        else if(strcmp(cmd_str,"FA")==0) {  // VFO A frequency
                                            // LEN=7 - set frequency
                                            // Next data will be rest of freq
                                            if(len == 13) { //We are receiving freq info
                                               new_freqA = atoll(&cmd_input[2]);
                                               set_band(new_freqA,(int)-1);
                                               //setFrequency(new_freqA);
                                               //g_idle_add(vfo_update,NULL);
                                            } else {
                                               if(len==2) {
                                                  //sprintf(msg,"FA%011lld;",getFrequency());
                                                  sprintf(msg,"FA%011lld;",vfo[VFO_A].frequency);
                                                  send_resp(client_sock,msg);
                                               }
                                            }
                                         }
        else if(strcmp(cmd_str,"FB")==0) {  // VFO B frequency
                                            if(len==13) { //We are receiving freq info
                                               new_freqB = atoll(&cmd_input[2]); 
                                               set_freqB(new_freqB);
                                               //setFrequency(new_freqA);
                                               //g_idle_add(vfo_update,NULL);
                                            } else if(len == 2) {
                                               sprintf(msg,"FB%011lld;",vfo[VFO_B].frequency);
                                               send_resp(client_sock,msg);
                                            }
                                         }
        /* Not supported */
        else if(strcmp(cmd_str,"FC")==0) {  // Set Sub receiver freq
                                            // LEN=7 - set frequency
                                            // Next data will be rest of freq
                                            // Len<7 - frequency?
                                            if(len>4) { //We are receiving freq info
                                               long long new_freqA = atoll(&cmd_input[2]);              
                                               //setFrequency(new_freqA);
                                            } else {
                                               sprintf(msg,"FC%011lld;",rigctl_getFrequency());
                                               send_resp(client_sock,"?;");
                                            }
                                         }
        else if(strcmp(cmd_str,"FD")==0)  {  // Read the filter display dot pattern
                                              send_resp(client_sock,"FD00000000;");
                                          }
        else if(strcmp(cmd_str,"FR")==0)  {  // Set/reads the extension menu
                                             if(len <=2) {
                                                if(receivers == 1) {
                                                   sprintf(msg,"FR0;");
                                                } else {
                                                   sprintf(msg,"FR%1d;",active_receiver->id);
                                                }
                                                send_resp(client_sock,msg); 
                                             }  else if (receivers != 1)  {
                                                 lcl_cmd = atoi(&cmd_input[2]);                
                                                 if(active_transmitter != lcl_cmd) {
                                                     split = 1;
                                                 } 
                                                 if(active_receiver->id != lcl_cmd) {
                                                    //active_receiver->id = lcl_cmd; 
                                                    active_receiver = receiver[lcl_cmd];
                                                    g_idle_add(vfo_update,NULL);
                                                    g_idle_add(active_receiver_changed,NULL);
                                                 } 
                                             }
                                          }
        else if(strcmp(cmd_str,"FS")==0)  {  // Set/reads fine funct status
                                             if(len <=2) {
                                                send_resp(client_sock,"FS0;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"FT")==0)  { // Sel or reads the transmitters VFO, M, ch or Call comm   
                                             if(len <=2) {
                                                   sprintf(msg,"FT%1d",active_transmitter);
                                                   send_resp(client_sock,msg); 
                                             } else {
                                                 lcl_cmd = atoi(&cmd_input[2]);                
                                                 if(lcl_cmd != active_receiver->id) {
                                                     split = 1;
                                                 } else {
                                                     split = 0;
                                                 }
                                                 active_transmitter = lcl_cmd;
                                                 g_idle_add(vfo_update,NULL);
                                             } 
                                          }  
        else if(strcmp(cmd_str,"FW")==0)  { // Sets/Reas DSP receive filter width in hz 0-9999hz 
                                             // CW - legal values  50  80  100 150 200 300 400 500 600 1000 2000
                                             //      PiHPSDR map   50  100 100 100 250 250 400 500 600 1000
                                             //                    25                                   750
                                             entry= (BANDSTACK_ENTRY *) 
                                                         bandstack_entry_get_current();
                                             FILTER* band_filters=filters[entry->mode];
                                             FILTER* band_filter=&band_filters[entry->filter];
                                             if(len <=2) {
                                                // CW filter high and low are always the same and the filter value is 2*filter val
                                                int filter_val = band_filter->high * 2;
                                                switch(filter_val) {
                                                 case 25:  
                                                 case 50:
                                                        work_int = 50;
                                                        break;
                                                 case 100:
                                                        work_int = 100; 
                                                        break;
                                                 case 250:
                                                        work_int = 300; 
                                                        break;
                                                 case 400:
                                                        work_int = 400; 
                                                        break;
                                                 case 500:
                                                        work_int = 500; 
                                                        break;
                                                 case 600:
                                                        work_int = 600; 
                                                        break;
                                                 case 750:
                                                        work_int = 1000; 
                                                        break;
                                                 case 800:
                                                        work_int = 1000; 
                                                        break;
                                                 case 1000:
                                                        work_int = 1000; 
                                                        break;
                                                 default: work_int = 500; 
                                                        break;
                                                } 
                                                sprintf(msg,"FW%04d;",work_int);
                                                #ifdef  RIGCTL_DEBUG
                                                    fprintf(stderr,"RIGCTL: FW Filter Act=%d work_int=%d\n",band_filter->high,work_int);
                                                #endif
                                                send_resp(client_sock,msg);
                                             } else {
                                               // Try to set filters here!
                                               // CW - legal values  50  80  100 150 200 300 400 500 600 1000 2000
                                               //      PiHPSDR map   50  100 100 100 250 250 400 500 600 1000
                                               //                    25                                   750
                                               work_int = atoi(&cmd_input[2]);
                                               switch (work_int) {

                                                    case 50:  new_low = 25; new_high = 25; break;
                                                    case 80:  new_low = 50; new_high = 50; break;
                                                    case 100:  new_low = 50; new_high = 50; break;
                                                    case 150:  new_low = 50; new_high = 50; break;
                                                    case 200:  new_low = 125; new_high = 125; break;
                                                    case 300:  new_low = 125; new_high = 125; break;
                                                    case 400:  new_low = 200; new_high = 200; break;
                                                    case 500:  new_low = 250; new_high = 250; break;
                                                    case 600:  new_low = 300; new_high = 300; break;
                                                    case 1000:  new_low = 500; new_high = 500; break;
                                                    case 2000:  new_low = 500; new_high = 500; break;
                                                    default: new_low  = band_filter->low;
                                                             new_high = band_filter->high;

                                               }
                                               #ifdef  RIGCTL_DEBUG
                                                   fprintf(stderr,"RIGCTL: FW Filter new_low=%d new_high=%d\n",new_low,new_high);
                                               #endif
                                               // entry->filter = new_low * 2 ;
                                               //setFilter(new_low,new_high);
                                               set_filter(active_receiver,new_low,new_high);
                                               g_idle_add(vfo_update,NULL);
                                             }
                                          }  
        else if(strcmp(cmd_str,"GT")==0)  { // Sets/Read AGC constant status 000-020
                                            // Map 000 - Off, 001-4 - Fast, 4-9 - Medium 10-14 Slow 15-20 Long
                                             //fprintf(stderr,"GT command seen\n");
                                             int agc_resp = 0;
                                             if(len <=2) {
                                                
                                                switch(active_receiver->agc) {
                                                   case AGC_OFF :   agc_resp = 0; break;
                                                   case AGC_FAST:   agc_resp = 5; break;
                                                   case AGC_MEDIUM: agc_resp = 10; break;
                                                   case AGC_SLOW:   agc_resp = 15; break;
                                                   case AGC_LONG:   agc_resp = 20; break;
                                                   default: agc_resp = 0;
                                                }
                                               
                                                sprintf(msg,"GT%03d;",agc_resp);
                                                send_resp(client_sock,msg);
                                             } else {
                                                //fprintf(stderr,"GT command Set\n");
                                                agc_resp = atoi(&cmd_input[2]);
                                                
                                                // AGC powers of 84 is broken Hamlib... 
                                                // Hamlib TS-2000 is broken here
                    
                                                if(agc_resp == 0) {
                                                   active_receiver->agc = AGC_OFF;
                                                } else if(agc_resp >0 && agc_resp <= 5 || (agc_resp == 84)) {
                                                   active_receiver->agc = AGC_FAST;
                                                  //  fprintf(stderr,"GT command FAST\n");
                                                } else if(agc_resp >6 && agc_resp <= 10 || (agc_resp == 2*84)) {
                                                   active_receiver->agc = AGC_MEDIUM;
                                                  // fprintf(stderr,"GT command MED\n");
                                                } else if(agc_resp >11 && agc_resp <= 15 || (agc_resp == 3*84)) {
                                                   active_receiver->agc = AGC_SLOW;
                                                   //fprintf(stderr,"GT command SLOW\n");
                                                } else if(agc_resp >16 && agc_resp <= 20 || (agc_resp == 4*84)) {
                                                   active_receiver->agc = AGC_LONG;
                                                   // fprintf(stderr,"GT command LONG\n");
                                                }
                                                g_idle_add(vfo_update,NULL);

                                             }
                                          }  
        else if(strcmp(cmd_str,"ID")==0)  { // Read ID - Default to TS-2000 which is type 019.
                                            sprintf(msg,"ID019;");
                                            send_resp(client_sock,msg);
                                          }
        else if(strcmp(cmd_str,"IF")==0) {  // Reads Transceiver status
                                            // IF
                                            //  P1: FFFFFFFFFFF -11 chars : Frequency in Hz (blanks are "0")
                                            //  P2: OFFS        - 4 chars : Offset in powers of 10
                                            //  P3: RITXIT      - 6 chars : RIT/XIT Frequency, Not supported = "000000"
                                            //  P4: R           - 1 char  : RIT Status "1"= On, "0"= off
                                            //  P5: X           - 1 char  : XIT Status "1"= On, "0"= off
                                            //  P6: 0           - 1 char  : Channel Bank number - not used (see MC command)
                                            //  P7: 00          - 2 chars : Channel Bank number - not used
                                            //  P8: C           - 1 char  : Mox Status "1"= TX, "0"= RX
                                            //  P9: M           - 1 char  : Operating mode (see MD command)
                                            // P10: V           - 1 char  : VFO Split status - not supported
                                            // P11: 0           - 1 char  : Scan status - not supported
                                            // P12: A           - 1 char  : same as FT command
                                            // P13: 0           - 1 char  : CTCSS tone - not used
                                            // P14: 00          - 2 chars : More tone control
                                            // P15: 0           - 1 char  : Shift status

                                            // convert first half of the msg
                                                      // IF     P1    P2  P3  P4  P5  P6
                                            sprintf(msg,"IF%011lld%04lld%06d%01d%01d%01d",
                                                         //getFrequency(),
                                                         rigctl_getFrequency(), // P1
                                                         step,  // P2
                                                         vfo[active_receiver->id].rit,  // P3
                                                         rit_on(), // P4
                                                         rit_on(), // P5
                                                         0  // P6
                                                         );

                                            // convert second half of the msg
                                                         //   P7  P8  P9 P10 P11 P12 P13 P14 P15;
                                            sprintf(msg+26,"%02d%01d%01d%01d%01d%01d%01d%02d%01d;",
                                                         0,  // P7
                                                         mox,  // P8
                                                         rigctlGetMode(),  // P9
                                                         0,  // P10
                                                         0,  // P11
                                                         ft_read(), // P12
                                                         ctcss_tone,  // P13
                                                         0,  // P14
                                                         0); // P15
                                            send_resp(client_sock,msg);
                                         }
        else if(strcmp(cmd_str,"IS")==0)  { // Sets/Reas IF shift funct status
                                             if(len <=2) {
                                                send_resp(client_sock,"IS00000;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"KS")==0)  { // Sets/Reads keying freq - 0-060 max
                                             if(len <=2) {
                                                sprintf(msg,"KS%03d;",cw_keyer_speed);
                                                send_resp(client_sock,msg);
                                             } else {
                                                int key_speed;
                                                key_speed = atoi(&cmd_input[2]);
                                                if(key_speed >1 && key_speed <= 60) {
                                                   cw_keyer_speed=key_speed;
                                                   g_idle_add(vfo_update,NULL);
                                                } 
                                            }
                                          }  
        else if(strcmp(cmd_str,"KY")==0)  { // Convert char to morse code
                                             if(len <=2) {
                                                send_resp(client_sock,"KY0;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"LK")==0)  { // Set/read key lock function status
                                             if(len <=2) {
                                                sprintf(msg,"LK%01d%01d;",locked,locked);
                                                send_resp(client_sock,msg);
                                             }  else {
                                                  if(cmd_input[3] == '0') {
                                                      locked = 0;
                                                  } else if(cmd_input[3] == '1') {
                                                      locked = 1;
                                                  }
                                                  g_idle_add(vfo_update,NULL);
                                             }
                                          }  
        else if(strcmp(cmd_str,"LM")==0)  { // Set/read DRU 3A unit or elect keyer recording status
                                             if(len <=2) {
                                                send_resp(client_sock,"LM0;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"LT")==0)  { // Set/read Alt function
                                             if(len <=2) {
                                                send_resp(client_sock,"LT0;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"MC")==0) {  // Recalls or reads memory channel
                                             if(len <=2) {
                                               send_resp(client_sock,"MC000;"); // Link this to band stack at some point
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
                                                     fprintf(stderr,"MD command Unknown\n");
                                                     #endif
                                               }
                                            // Other stuff to switch modes goes here..
                                            // since new_mode has the interpreted command in 
                                            // in it now.
                                            entry= (BANDSTACK_ENTRY *) 
                                                  bandstack_entry_get_current();
                                            entry->mode=new_mode;
                                            //  BUG - kills the system when there is some
                                            // GPIO activity and Mode sets occur. Used twittling the
 					    // frequency along with setting mode between USB/LSB with
					    // flrig. Tried doing the g_idle_add trick - but don't know the
					    // the magic to get that to compile without warnings 
                                            //setMode(entry->mode);
                                            set_mode(active_receiver,entry->mode);
                                            // Moved the vfo_update down after filter updated. (John G0ORX)
                                            //g_idle_add(vfo_update,NULL);
                                            
                                            FILTER* band_filters=filters[entry->mode];
                                            FILTER* band_filter=&band_filters[entry->filter];
                                            //setFilter(band_filter->low,band_filter->high);
                                            set_filter(active_receiver,band_filter->low,band_filter->high);
                                            /* Need a way to update VFO info here..*/
                                            g_idle_add(vfo_update,NULL);
                                            }  else {     // Read Mode
                                               int curr_mode;
                                               switch (vfo[active_receiver->id].mode) {
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
                                                            vfo[active_receiver->id].mode);
                                                          #endif
                                                          break;
                                               }
                                               sprintf(msg,"MD%1d;",curr_mode);
                                               send_resp(client_sock,msg);
                                            }
                                         }
        else if(strcmp(cmd_str,"MF")==0) {  // Set/read menu A/B
                                             if(len <=2) {
                                               send_resp(client_sock,"MF0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"MG")==0) {  // Mike Gain - 3 digit value 
                                            if(len <=2) {
                                               work_int = (int) ((mic_gain +10.0)* 100.0/60.0);
                                               sprintf(msg,"MG%03d;",work_int);
                                               send_resp(client_sock,msg);
                                            } else {
                                               int tval = atoi(&cmd_input[2]);                
                                               new_vol = (double) (tval * 60/100) - 10; 
                                               //set_mic_gain(new_vol); 
                                               double *p_mic_gain=malloc(sizeof(double));
                                               *p_mic_gain=new_vol;
                                               g_idle_add(update_mic_gain,(void *)p_mic_gain);
                                            }
                                         }
        else if(strcmp(cmd_str,"ML")==0) {  // Set/read the monitor function level
                                             if(len <=2) {
                                               send_resp(client_sock,"ML000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"MO")==0) {  // Set Monitor on/off
                                             if(len <=2) {
                                               send_resp(client_sock,"MO0;"); 
                                             }
                                         }
        /* Not supported */
        else if(strcmp(cmd_str,"MR")==0) {  // Read Memory Channel data
                                             sprintf(msg,"MR%1d%02d%02d%011lld%1d%1d%1d%02d%02d%03d%1d%1d%09d%02d%1d%08d;",
                                                      0, // P1 - Rx Freq - 1 Tx Freq
                                                      0, // P2 Bankd and channel number -- see MC comment
                                                      0, // P3 - see MC comment 
                                                      rigctl_getFrequency(), // P4 - frequency
                                                      rigctlGetMode(), // P5 - Mode
                                                      locked, // P6 - Locked status
                                                      0, // P7 - O-off, 1-tone, 2-CTCSS, 3 =DCS
                                                      0, // P8 - Tone Number - see page 35
                                                      ctcss_tone, // P9 - CTCSS tone number - see CN command
                                                      0, // P10 - DCS code - see QC command 
                                                      0, // P11 - Reverse status
                                                      0, // P12 - Shift status - see OS command
                                                      0, // P13 - Offset freq - see OS command
                                                      0, // P14 - Step Size - see ST command
                                                      0, // P15 - Memory Group Number (0-9)
                                                      0);  // P16 - Memory Name - 8 char max
                                               //send_resp(client_sock,msg);
                                               send_resp(client_sock,"?;");
                                         }
        else if(strcmp(cmd_str,"MU")==0) {  // Set/Read Memory Group Data
                                             if(len <=2) {
                                               send_resp(client_sock,"MU0000000000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"MW")==0) {  // Store Data to Memory Channel
                                         }
        else if(strcmp(cmd_str,"NB")==0) {  // Set/Read Noise Blanker func status (on/off)
                                             if(len <=2) {
                                               sprintf(msg,"NB%1d;",active_receiver->snb);
                                               send_resp(client_sock,msg);
                                             } else {
                                               if(cmd_input[2]=='0') { // Turn off ANF
                                                  active_receiver->snb=0;
                                               } else { // Turn on ANF
                                                  active_receiver->snb=1;
                                               }
                                               // Update filters
                                               SetRXASNBARun(active_receiver->id, active_receiver->snb);
                                               g_idle_add(vfo_update,NULL);
                                             }
                                         }
        else if(strcmp(cmd_str,"NL")==0) {  // Set/Read Noise Reduction  Level
                                             if(len <=2) {
                                               send_resp(client_sock,"NL000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"NR")==0) {  // Set/Read Noise Reduction function status
                                            if(len <=2) {
/*
                                              if(nr_none == 1) {
                                                  send_resp(client_sock,"NR0;"); 
                                              } else if ((nr_none == 0) && (nr==1)) { 
                                                  send_resp(client_sock,"NR1;"); 
                                              } else if (nr2 == 1) { 
                                                  send_resp(client_sock,"NR2;"); 
                                              } else {
                                                  send_resp(client_sock,"NR0;"); 
                                              }
*/

                                              // FIX now allow multiple set
                                              if (active_receiver->nr==1) { 
                                                send_resp(client_sock,"NR1;"); 
                                              } else {
                                                send_resp(client_sock,"NR0;"); 
                                              }
                                            } else {
                                               if(cmd_input[2] == '0') {
                                                  active_receiver->nr = 0;
                                                  active_receiver->nr2 = 0;
                                               } else if(cmd_input[2] == '1') {
                                                  active_receiver->nr = 1;
                                               } else if(cmd_input[2] == '2') {
                                                  active_receiver->nr2 = 1;
                                               }
                                               SetRXAANRRun(active_receiver->id, active_receiver->nr);
                                               SetRXAEMNRRun(active_receiver->id, active_receiver->nr2);
                                               g_idle_add(vfo_update,NULL);
                                            } 
                                         }
        else if(strcmp(cmd_str,"NT")==0) {  // Set/Read autonotch function
                                             if(len <=2) {
                                               sprintf(msg,"NT%1d;",active_receiver->anf);
                                               send_resp(client_sock,msg);
                                             } else {
                                               if(cmd_input[2] == '0') { // Clear ANF
                                                 active_receiver->anf = 0;
                                               } else { // Set ANF
                                                 active_receiver->anf = 1;
                                               }
                                             }
                                             SetRXAANFRun(active_receiver->id, active_receiver->anf);
                                             g_idle_add(vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"OF")==0) {  // Set/Read Offset freq (9 digits - hz)
                                             if(len <=2) {
                                               send_resp(client_sock,"OF000000000;"); 
                                             }
                                         }
        /* Not Supported */
        else if(strcmp(cmd_str,"OI")==0) {  // Read Memory Channel Data
                                             if(len <=2) {
                                               sprintf(msg,"OI%011lld%04d%06d%1d%1d%1d%02d%1d%1d%1d%1d%1d%1d%02d%1d;",
                                                  rigctl_getFrequency(),
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
                                                  0,// P14 Tone freq - See TN command
                                                  0,0);
                                               send_resp(client_sock,"?;");
                                             }
                                         }
        else if(strcmp(cmd_str,"OS")==0) {  // Set/Read Offset freq (9 digits - hz)
                                             if(len <=2) {
                                               //send_resp(client_sock,"OS0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PA")==0) {  // Set/Read Preamp function status
                                             if(len <=2) {
                                               sprintf(msg,"PA0%1d;",enable_tx_equalizer);
                                               send_resp(client_sock,msg);
                                             } else {
                                                if(cmd_input[2] =='0') {
                                                  enable_tx_equalizer=0;
                                                  SetTXAEQRun(transmitter->id, enable_tx_equalizer);
                                                } else {
                                                  enable_tx_equalizer=1;
                                                  SetTXAEQRun(transmitter->id, enable_tx_equalizer);
                                                }
                                             }
                                         }
        else if(strcmp(cmd_str,"PB")==0) {  // Set/Read DRU-3A Playback status
                                             if(len <=2) {
                                               send_resp(client_sock,"PB0;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PC")==0) {  // Set/Read Drive Power output
                                            if(len<=2) {
                                              sprintf(msg,"PC%03d;",(int) drive);
                                              send_resp(client_sock,msg); 
                                            } else {
                                               // Power Control - 3 digit number- 0 to 100
                                               //Scales to 0.00-1.00
                                                
                                               double drive_val = 
                                                   (double)(atoi(&cmd_input[2])); 
                                               // setDrive(drive_val);
                                               double *p_drive=malloc(sizeof(double));
                                               *p_drive=drive_val;
                                               g_idle_add(update_drive,(gpointer)p_drive);
                                               //set_drive(drive_val);
                                            }
                                         }
        else if(strcmp(cmd_str,"PI")==0) {  // STore the programmable memory channel
                                         }
        else if(strcmp(cmd_str,"PK")==0) {  // Reads the packet cluster data
                                             //  send_resp(client_sock,"PK000000000000000000000000000000000000000000000000;"); 
                                             send_resp(client_sock,"?;"); 
                                            
                                         }
        else if(strcmp(cmd_str,"PL")==0) {  // Set/Read Speech processor level
                                            // P1 000 - min-100 max
                                            // P2 000 - min - 100 max
                                             if(len <=2) {
                                             //  send_resp(client_sock,"PL000000;"); 
                                                send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PM")==0) {  // Recalls the Programmable memory
                                             if(len <=2) {
                                               //send_resp(client_sock,"PM0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PR")==0) {  // Sets/reads the speech processor function on/off
                                            // 0 - off, 1=on
                                             if(len <=2) {
                                               //send_resp(client_sock,"PR0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PS")==0) {  // Sets/reads Power on/off state
                                            // 0 - off, 1=on
                                             if(len <=2) {
                                               send_resp(client_sock,"PS1;"); // Lets pretend we're powered up ;-) 
                                             }
                                         }
        else if(strcmp(cmd_str,"QC")==0) {  // Sets/reads DCS code
                                            // Codes numbered from 000 to 103.
                                             if(len <=2) {
                                               //send_resp(client_sock,"QC000;"); // Lets pretend we're powered up ;-) 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"QI")==0) {  // Store the settings in quick memory
                                         }
        else if(strcmp(cmd_str,"QR")==0) {  // Send the Quick memory channel data
                                            // P1 - Quick mem off, 1 quick mem on
                                            // P2 - Quick mem channel number
                                             if(len <=2) {
                                               //send_resp(client_sock,"QR00;"); // Lets pretend we're powered up ;-) 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RA")==0) {  // Sets/reads Attenuator function status
                                            // 00-off, 1-99 -on - Main and sub receivers reported
                                             if(len <=2) {
                                               //send_resp(client_sock,"RA0000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RC")==0) {  // Clears the RIT offset freq
                                         }
        else if(strcmp(cmd_str,"RD")==0) {  // Move the RIT offset freq down, slow down the scan speed in scan mode
                                             if(len <=2) {
                                               vfo[active_receiver->id].rit = vfo[active_receiver->id].rit-rit_increment;
                                               sprintf(msg,"RD%1d;",rit_on());
                                               send_resp(client_sock,msg);
                                             } else {
                                                if(len > 3) {
                                                   // Non scan mode when RDxxxxx; - 
                                                   vfo[active_receiver->id].rit = vfo[active_receiver->id].rit-rit_increment;
                                                } 
                                             }
                                             g_idle_add(vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"RG")==0) {  // RF Gain - 3 digit number
                                            // RG123; 0-255 value
                                            // Scale from -20 - 120
                                            if(len>4) { // Set Audio Gain
                                               int base_value = atoi(&cmd_input[2]);
                                               double new_gain = ((((double) base_value)/255) * 140) - 20; 
                                               //set_agc_gain(new_gain);               
                                               double *p_gain=malloc(sizeof(double));
                                               *p_gain=new_gain;
                                               g_idle_add(update_agc_gain,(gpointer)p_gain);
                                            } else { // Read Audio Gain
                                              sprintf(msg,"RG%03d;",((256 * (int) active_receiver->agc_gain)/140)+36);
                                              send_resp(client_sock,msg);
                                            }
                                         }
        else if(strcmp(cmd_str,"RL")==0) {  // Set/read Noise reduction level
                                             if(len <=2) {
                                               //send_resp(client_sock,"RL00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RM")==0) {  // Set/read Meter function
                                            // P1- 0, unsel, 1 SWR, 2 Comp, 3 ALC
                                            // P2 - 4 dig - Meter value in dots - 000-0030
                                             if(len <=2) {
                                               //send_resp(client_sock,"RM00000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RT")==0) {  // Set/read the RIT function status
                                             if(len <=2) {
                                               sprintf(msg,"RT%01d;",rit_on());
                                               send_resp(client_sock,msg); 
                                             } else {
                                                if(atoi(&cmd_input[2]) == 0) {
                                                   vfo[active_receiver->id].rit = 0;
                                                }
                                             }
                                         }
        else if(strcmp(cmd_str,"RU")==0) {  // Set/move RIT frequency up
                                             if(len <=2) {
                                               vfo[active_receiver->id].rit = vfo[active_receiver->id].rit+rit_increment;
                                               sprintf(msg,"RU%01d;",rit_on());
                                               send_resp(client_sock,msg);
                                             } else {
                                                if(len > 3) {
                                                   vfo[active_receiver->id].rit = vfo[active_receiver->id].rit+rit_increment;
                                                } 
                                             }
                                             g_idle_add(vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"RX")==0) {  // Unkey Xmitter
                                            //setMox(0);
                                            mox_state=0;
                                            g_idle_add(mox_update,(gpointer)mox_state); 
                                            g_idle_add(vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"SA")==0) {  // Set/reads satellite mode status
                                            // 0-main, 1=sub
                                             if(len <=2) {
                                               //send_resp(client_sock,"SA000000000000000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SB")==0) {  // Set/read the SUB TF-W status
                                             if(len <=2) {
                                               //send_resp(client_sock,"SB0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SC")==0) {  // Set/read the Scan function status
                                             if(len <=2) {
                                               //send_resp(client_sock,"SC0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SD")==0) {  // Set/read CW break-in time delay
                                            // 0000-1000 ms (in steps of 50 ms) 0000 is full break in
                                             int local_cw_breakin=cw_keyer_hang_time;
                                             // Limit report value to 1000 for TS-2000
                                             if(local_cw_breakin > 1000) { local_cw_breakin = 1000; }
                                             if(len <=2) {
                                               sprintf(msg,"SD%04d;",local_cw_breakin);
                                               send_resp(client_sock,msg);
                                             } else {
                                               local_cw_breakin = atoi(&cmd_input[2]); 
                                               if(cw_keyer_hang_time <= 1000) {
                                                  if(local_cw_breakin==0) {
                                                      cw_breakin = 1;
                                                  }  else {
                                                      cw_breakin = 0;
                                                  } 
                                                  cw_keyer_hang_time = local_cw_breakin; 
                                               }
                                             }
                                         }
        else if(strcmp(cmd_str,"SH")==0) {  // Set/read the DSP filter settings
                                             if(len <=2) {
                                              // send_resp(client_sock,"SH00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SI")==0) {  // Enters the satellite memory name
                                         }
        else if(strcmp(cmd_str,"SL")==0) {  // Set/read the DSP filter settings - this appears twice? See SH
                                             if(len <=2) {
                                               //send_resp(client_sock,"SL00;");
                                               send_resp(client_sock,"?;"); 
                                             } 
                                         }
        else if(strcmp(cmd_str,"SM")==0) {  // Read SMETER
                                            // SMx;  x=0 RX1, x=1 RX2 
                                            // meter is in dbm - value will be 0<260
                                            // Translate to 00-30 for main, 0-15 fo sub
                                            // Resp= SMxAABB; 
                                            //  Received range from the SMeter can be -127 for S0, S9 is -73, and S9+60=-13.;
                                            //  PowerSDR returns S9=0015 code. 
                                            //  Let's make S9 half scale or a value of 70.  
                                            double level=0.0;

                                            if(cmd_input[2] == '0') { 
                                              level = GetRXAMeter(receiver[0]->id, smeter); 
                                            } else if(cmd_input[2] == '1') { 
                                              if(receivers==2) {
                                                level = GetRXAMeter(receiver[1]->id, smeter); 
                                              }
                                            }


                                            level = GetRXAMeter(active_receiver->id, smeter); 
                                            // Determine how high above 127 we are..making a range of 114 from S0 to S9+60db
                                            // 5 is a fugdge factor that shouldn't be there - but seems to get us to S9=SM015
                                            level =  abs(127+(level + (double) get_attenuation()))+5;
                                         
                                            // Clip the value just in case
                                            if(cmd_input[2] == '0') { 
                                               new_level = (int) ((level * 30.0)/114.0);
                                               // Do saturation check
                                               if(new_level < 0) { new_level = 0; }
                                               if(new_level > 30) { new_level = 30;}
                                            } else { //Assume we are using sub receiver where range is 0-15
                                               new_level = (int) ((level * 15.0)/114.0);
                                               // Do saturation check
                                               if(new_level < 0) { new_level = 0; }
                                               if(new_level > 15) { new_level = 15;}
                                            }
                                            sprintf(msg,"SM%1c%04d;",
                                                cmd_input[2],new_level);
                                            send_resp(client_sock,msg);
                                         }   
        else if(strcmp(cmd_str,"SQ")==0) {  // Set/read the squelch level
                                            // P1 - 0- main, 1=sub
                                            // P2 - 0-255
                                             if(len <=3) {
                                               sprintf(msg,"SQ%04d;",squelch);
                                               send_resp(client_sock,msg);
                                             } else {
                                                squelch = atoi(&cmd_input[2]);
                                             }
                                         }
        else if(strcmp(cmd_str,"SR")==0) {  // Resets the transceiver
                                         }
        else if(strcmp(cmd_str,"SS")==0) {  // Set/read Scan pause freq
                                             if(len <=2) {
                                               //send_resp(client_sock,"SS0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"ST")==0) {  // Set/read the multi/ch control freq steps
                                            // SSB/CW/FSK - values 00-03
                                            // 00: 1KHz, 01: 2.5Khz 02:5KHz 03: 10Khz
                                            // AM/FM mode 00-09
                                            // 00: 5KHz, 
                                            // 01: 6.25KHz, 
                                            // 02:10Khz, 
                                            // 03: 12.5Khz,
                                            // 04: 15Khz, 
                                            // 05: 20Khz, 
                                            // 06: 25KHz
                                            // 07: 30Khz
                                            // 08: 50Khz
                                            // 09: 100Khz 
                                             int coded_step_val;
                                             entry= (BANDSTACK_ENTRY *) 
                                                bandstack_entry_get_current();
                                             if(len <=2) {
                                                 switch(entry->mode) {
                                                     case modeLSB: 
                                                     case modeUSB: 
                                                     case modeCWL: 
                                                     case modeCWU: 
                                                     case modeDIGU: 
                                                     case modeDIGL: 
                                                        if(step >0 && step <= 1000)  {
                                                          coded_step_val = 0; 
                                                        } else if (step >1000 && step <=2500) {
                                                          coded_step_val = 1; 
                                                        } else if (step >2500 && step <=5000) {
                                                          coded_step_val = 2; 
                                                        } else {
                                                          coded_step_val = 3; 
                                                        }
                                                        break;
                                                     case modeFMN: 
                                                     case modeAM:  
                                                        if(step >0 && step <= 5000)  {
                                                          coded_step_val = 0; 
                                                        } else if (step >5000 && step <=6250) {
                                                          coded_step_val = 1; 
                                                        } else if (step >6250 && step <=10000) {
                                                          coded_step_val = 2; 
                                                        } else if (step >10000 && step <=12500) {
                                                          coded_step_val = 3; 
                                                        } else if (step >12500 && step <=15000) {
                                                          coded_step_val = 4; 
                                                        } else if (step >15000 && step <=20000) {
                                                          coded_step_val = 5; 
                                                        } else if (step >20000 && step <=25000) {
                                                          coded_step_val = 6; 
                                                        } else if (step >25000 && step <=30000) {
                                                          coded_step_val = 7; 
                                                        } else if (step >30000 && step <=50000) {
                                                          coded_step_val = 8; 
                                                        } else if (step >50000 && step <=100000) {
                                                          coded_step_val = 9; 
                                                        } else {
                                                          coded_step_val = 0; 
                                                        }
                                                        break;
                                                 } 
                                                 sprintf(msg,"ST%02d;",coded_step_val);
                                                 send_resp(client_sock,msg); 
                                             } else {
                                                 coded_step_val = atoi(&cmd_input[2]);   
                                                 switch(entry->mode) {
                                                     case modeLSB: 
                                                     case modeUSB: 
                                                     case modeCWL: 
                                                     case modeCWU: 
                                                     case modeDIGU: 
                                                     case modeDIGL: 
                                                        if(coded_step_val==0) { step = 1000;}
                                                        if(coded_step_val==1) { step = 2500;}
                                                        if(coded_step_val==2) { step = 5000;}
                                                        if(coded_step_val==3) { step = 10000;}
                                                        break; 
                                                     case modeFMN: 
                                                     case modeAM:  
                                                         switch(coded_step_val) {
                                                            case 0: step = 5000; break;
                                                            case 1: step = 6250; break;
                                                            case 2: step = 10000; break;
                                                            case 3: step = 12500; break;
                                                            case 4: step = 15000; break;
                                                            case 5: step = 20000; break;
                                                            case 6: step = 25000; break;
                                                            case 7: step = 30000; break;
                                                            case 8: step = 50000; break;
                                                            case 9: step = 100000; break;
                                                            default: break; // No change if not a valid number
                                                         } 
                                                     default: break; // No change if not a valid number
                                                 } 
                                                 g_idle_add(vfo_update,NULL);
                                             }
                                         }
        else if(strcmp(cmd_str,"SU")==0) {  // Set/read the scan pause freq
                                             if(len <=2) {
                                               //send_resp(client_sock,"SU00000000000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SV")==0) {  // Execute the memory transfer function
                                         }
        else if(strcmp(cmd_str,"TC")==0) {  // Set/read the internal TNC mode
                                             if(len <=2) {
                                               //send_resp(client_sock,"TC00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TD")==0) {  // Sends the DTMF memory channel
                                         }
        else if(strcmp(cmd_str,"TI")==0) {  // Reads the TNC LED status
                                             if(len <=2) {
                                               //send_resp(client_sock,"TI00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TN")==0) {  // Set/Read sub tone freq
                                             if(len <=2) {
                                               //send_resp(client_sock,"TN00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TO")==0) {  // Set/Read tone function
                                             if(len <=2) {
                                               //send_resp(client_sock,"TO0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TS")==0) {  // Set/Read TF Set function status
                                             if(len <=2) {
                                               //send_resp(client_sock,"TS0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TX")==0) {  // Key Xmitter - P1 - transmit on main/sub freq

                                             /*if(len >=3) { 
                                               // K5JAE: The TS-2000 real hardware does not respond 
                                               // to this command, thus hamlib is not expecting response.
                                               //send_resp(client_sock,"TX0;"); 
                                             }*/
                                            mox_state=1;
                                            g_idle_add(mox_update,(gpointer)mox_state); 
                                            g_idle_add(vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"TY")==0) {  // Set/Read uP firmware type
                                             if(len <=2) {
                                               send_resp(client_sock,"TY000;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"UL")==0) {  // Detects the PLL unlock status - this should never occur - xmit only
                                             if(len <=2) {
                                               //send_resp(client_sock,"UL0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"UP")==0) {  // Emulates the mic up key
                                         }
        else if(strcmp(cmd_str,"VD")==0) {  // Sets/Reads VOX dleay time - 0000-3000ms in steps of 150
                                            // We want vox_hang variable in PiHPSDR
                                            // Max value in variable in ms... so 250 = 250ms
                                             if(len <=2) {
                                               work_int = (int) vox_hang;
                                               sprintf(msg,"VD%04d;",work_int); 
                                               send_resp(client_sock,msg);
                                             } else {
                                                  work_int = atoi(&cmd_input[2]);
                                                  // Bounds check for legal values for PiHPSDR
                                                  if(work_int > 1000) { work_int = 1000; }
                                                  if(work_int < 0) { work_int = 0; }
                                                  vox_hang = (double) work_int;
                                                  #ifdef RIGCTL_DEBUG
                                                  fprintf(stderr,"RIGCTL: Vox hang=%0.20f\n",vox_hang);
                                                  #endif
                                             }
                                         }
        else if(strcmp(cmd_str,"VG")==0) {  // Sets/Reads VOX gain 000-009
                                            // We want vox_threshold variable in PiHPSDR
                                            // Max value in variable 0.1 
                                            // 3 char 000-009 valid ranges
                                             if(len <=2) {
                                               work_int = (int) ((vox_threshold) * 100.0);
                                               sprintf(msg,"VG00%1d;",work_int);
                                               send_resp(client_sock,msg);
                                             } else {
                                                  // Set the threshold here
                                                  work_int = atoi(&cmd_input[2]);
                                                  vox_threshold = ((double) work_int)/100.0;
                                                  #ifdef RIGCTL_DEBUG
                                                  fprintf(stderr,"RIGCTL: Vox thresh=%0.20f\n",vox_threshold);
                                                  #endif
                                             }
                                         }
        else if(strcmp(cmd_str,"VR")==0) {  // Emulates the voice 1/2 key
                                         }
        else if(strcmp(cmd_str,"VX")==0) {  // Sets/reads vox f(x) state
                                             if(len <=2) {
                                               sprintf(msg,"VX%1d;",vox_enabled); 
                                               send_resp(client_sock,msg);
                                             } else {
                                               work_int = atoi(&cmd_input[2]);
                                               if(work_int>1) { vox_enabled = 1; vox= 1;}
                                               if(work_int<1) { vox_enabled = 0; vox=0; }
                                             }
                                         }
        else if(strcmp(cmd_str,"XT")==0) {  // Sets/reads the XIT f(x) state
                                             if(len <=2) {
                                               //send_resp(client_sock,"XT0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"XX")==0) {  // 
                                            // Format RL01234: First dig 0=neg, 1=pos number
                                            //                 1-4- digital offset in hertz.
                                            if(len > 4) { // It is set instead of a read
                                               digl_pol = (cmd_input[2]=='0') ? -1 : 1;
                                               digl_offset = atoi(&cmd_input[3]); 
                                               #ifdef RIGCTL_DEBUG
                                               fprintf(stderr,"RIGCTL:RL set %d %d\n",digl_pol,digl_offset); 
                                               #endif
                                            } else {
                                               if(digl_pol==1) { // Nah - its a read
                                                 sprintf(msg,"RL1%04d;",0);
                                               } else {
                                                 sprintf(msg,"RL0%04d;",0);
                                               }         
                                               send_resp(client_sock,msg);
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
                                               fprintf(stderr,"RIGCTL:RL set %d %d\n",digl_pol,digl_offset); 
                                               #endif
                                            } else {
                                               if(digl_pol==1) { // Nah - its a read
                                                 sprintf(msg,"RL1%04d;",0);
                                               } else {
                                                 sprintf(msg,"RL0%04d;",0);
                                               }         
                                               send_resp(client_sock,msg);
                                               #ifdef RIGCTL_DEBUG
                                               fprintf(stderr,":%s\n",msg);
                                               #endif
                                            }
                                         }
        else                             {
                                            fprintf(stderr,"RIGCTL: UNKNOWN=%s\n",cmd_str);
                                         }
}
//
// End of Parser
// 


//
// 2-25-17 - K5JAE - create each thread with the pointer to the port number  
//                   (Port numbers now const ints instead of defines..) 
//
void launch_rigctl () {
   int err;
   fprintf(stderr, "LAUNCHING RIGCTL!!\n");

   mutex_a = g_new(GT_MUTEX,1);
   g_mutex_init(&mutex_a->m);

   mutex_b = g_new(GT_MUTEX,1);
   g_mutex_init(&mutex_b->m);
   
   // This routine encapsulates the pthread call
/*
   err = pthread_create (&rigctl_a_thread_id,NULL,rigctl_a,NULL);
   if(err != 0) {
       fprintf(stderr, "pthread_create failed on rigctl_a launch\n");
   }  
*/

   rigctl_a_thread_id = g_thread_new( "rigctl a", rigctl, (gpointer)&TelnetPortA);
   if( ! rigctl_a_thread_id )
   {
     fprintf(stderr,"g_thread_new failed on rigctl_a\n");
   }


/*
   err = pthread_create (&rigctl_b_thread_id,NULL,rigctl_b,NULL);
   if(err != 0) {
       fprintf(stderr, "pthread_create failed on rigctl_b launch\n");
   }  
*/

   rigctl_b_thread_id = g_thread_new( "rigctl b", rigctl, (gpointer)&TelnetPortB);
   if( ! rigctl_b_thread_id )
   {
     fprintf(stderr,"g_thread_new failed on rigctl_b\n");
   }

/*
   err = pthread_create (&rigctl_c_thread_id,NULL,rigctl_c,NULL);
   if(err != 0) {
       fprintf(stderr, "pthread_create failed on rigctl_c launch\n");
   }  
*/

   rigctl_c_thread_id = g_thread_new( "rigctl c", rigctl, (gpointer)&TelnetPortC);
   if( ! rigctl_c_thread_id )
   {
     fprintf(stderr,"g_thread_new failed on rigctl_c\n");
   }

}


// Telnet Server launch code below
// max number of bytes we can get at once
#define MAXDATASIZE 300

//
// 2-25-17 - K5JAE - removed duplicate init servers.
//


int init_server ( int * socket_desc_ptr, struct sockaddr_in * client, struct sockaddr_in * server, int telnet_port) {
    int client_sock;
    int c;
    //Create socket
    *socket_desc_ptr = socket(AF_INET , SOCK_STREAM , 0);

    if (*socket_desc_ptr == -1)
    {
        fprintf(stderr,"RIGCTL: Could not create socket");
    }
    fprintf(stderr, "RIGCTL: Socket created\n");
     
    //Prepare the sockaddr_in structure
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = INADDR_ANY;
    server->sin_port = htons( telnet_port );
     
    //Bind
    if( bind(*socket_desc_ptr,(struct sockaddr *) server , sizeof(struct sockaddr_in)) < 0)
    {
        //print the error message
        fprintf(stderr,"RIGCTL: bind failed. Error\n");
        return 1;
    }
    fprintf(stderr,"RIGCTL: Bind done on port %d\n",telnet_port);
     
    //Listen
    listen(*socket_desc_ptr, 3);
     
    //Accept and incoming connection
    fprintf(stderr,"RIGCTL: Waiting for incoming connections...\n");
    c = sizeof(struct sockaddr_in);
     
    //accept connection from an incoming client
    client_sock = accept(*socket_desc_ptr,(struct sockaddr *)client, (socklen_t*)&c);
    if (client_sock < 0)
    {
        fprintf(stderr,"RIGCTL: Accept failed\n");
    }
    fprintf(stderr,"RIGCTL: Connection accepted\n");

    return client_sock;
}

int rigctlGetMode()  {
        switch(vfo[active_receiver->id].mode) {
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

void set_band(long long new_freqA, int ctl) {      

   fprintf(stderr,"set_band: f=%11lld ctl=%d\n",new_freqA,ctl );
   BANDSTACK_ENTRY *entry;
   BAND *band;
   FILTER* band_filters;
   FILTER* band_filter;

   int b = get_band_from_frequency(new_freqA);

   if(b == -1) { // Out of ham bands...
      fprintf(stderr,"set_band: out of ham bands\n");
      setFrequency(new_freqA);
      g_idle_add(vfo_update,NULL);
      return;
   }   

   band=band_set_current(b);

   entry=bandstack_entry_next();
   entry->mode = vfo[VFO_A].mode;
   set_mode(active_receiver,entry->mode); // From current band setting
   

   band_filters=filters[entry->mode];
   band_filter=&band_filters[entry->filter];

  set_filter(active_receiver,band_filter->low,band_filter->high);
  setFrequency(new_freqA);

  // BAND *band=band_get_current_band();
  band=band_set_current(b);
  if(active_receiver->id==0) {
    set_alex_rx_antenna(band->alexRxAntenna);
    set_alex_tx_antenna(band->alexTxAntenna);
    set_alex_attenuation(band->alexAttenuation);
  }

  //setFrequency(new_freqA);
  g_idle_add(vfo_update,NULL);

  calcDriveLevel();
  calcTuneDriveLevel();
}

void set_freqB(long long new_freqB) {      

   //BANDSTACK_ENTRY *bandstack = bandstack_entry_get_current();
   //bandstack->frequencyB = new_freqB;
   //frequencyB = new_freqB;
   vfo[VFO_B].frequency = new_freqB;   
   g_idle_add(vfo_update,NULL);
}
