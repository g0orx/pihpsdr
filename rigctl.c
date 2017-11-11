
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "toolbar.h"
#include "band_menu.h"
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
#include "filter_menu.h"
#include "vfo.h"
#include "sliders.h"
#include "agc.h"
#include <wdsp.h>
#include "store.h"
#include "ext.h"
#include "rigctl_menu.h"
#include <math.h>

// IP stuff below
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr

//#undef RIGCTL_DEBUG
#define RIGCTL_DEBUG

int rigctl_port_base=19090;
int rigctl_enable=0;

// the port client will be connecting to
// 2-26-17 K5JAE - Changed the defines to const ints to allow use via pointers.
static const int TelnetPortA = 19090;
static const int TelnetPortB = 19091;
static const int TelnetPortC = 19092;

// max number of bytes we can get at once
#define MAXDATASIZE 300

void parse_cmd ();
int cw_busy = 0; // Used to signal that the system is in a busy state for sending CW - assert busy back to the user
int cw_reset = 0; // Signals reset the transceiver
int connect_cnt = 0;

int rigctlGetFilterLow();
int rigctlGetFilterHigh();
int rigctlSetFilterLow(int val);
int rigctlSetFilterHigh(int val);
int new_level;
int active_transmitter = 0;
int rigctl_busy = 0;  // Used to tell rigctl_menu that launch has already occured

int cat_control;

extern int enable_tx_equalizer;

typedef struct {GMutex m; } GT_MUTEX;
GT_MUTEX * mutex_a;
GT_MUTEX * mutex_b;
GT_MUTEX * mutex_c;
GT_MUTEX * mutex_d;

FILE * out;
int  output;
FILTER * band_filter;

#define MAX_CLIENTS 3
static GThread *rigctl_server_thread_id = NULL;
static int server_running;

static int server_socket=-1;
static int server_address_length;
static struct sockaddr_in server_address;

typedef struct _client {
  int socket;
  int address_length;
  struct sockaddr_in address;
  GThread *thread_id;
} CLIENT;

static CLIENT client[MAX_CLIENTS];

int squelch=0; //local sim of squelch level
int fine = 0;     // FINE status for TS-2000 decides whether rit_increment is 1Hz/10Hz.


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

static gpointer rigctl_client (gpointer data);

void close_rigctl_ports() {
  int i;
  struct linger linger = { 0 };
  linger.l_onoff = 1;
  linger.l_linger = 0;

fprintf(stderr,"close_rigctl_ports: server_socket=%d\n",server_socket);
  server_running=0;
  for(i=0;i<MAX_CLIENTS;i++) {
      if(client[i].socket!=-1) {
fprintf(stderr,"setting SO_LINGER to 0 for client_socket: %d\n",client[i].socket);
        if(setsockopt(client[i].socket,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
          perror("setsockopt(...,SO_LINGER,...) failed for client");
        }
fprintf(stderr,"closing client socket: %d\n",client[i].socket);
        close(client[i].socket);
        client[i].socket=-1;
      }
  }

  if(server_socket>=0) {
fprintf(stderr,"setting SO_LINGER to 0 for server_socket: %d\n",server_socket);
    if(setsockopt(server_socket,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
      perror("setsockopt(...,SO_LINGER,...) failed for server");
    }
fprintf(stderr,"closing server_socket: %d\n",server_socket);
    close(server_socket);
    server_socket=-1;
  }
}
//
// Used to convert transmitter->ctcss_frequency into 1-39 value for TS2000.
// This COULD be done with a simple table lookup - but I've already written the code
// so at this point...
//
int convert_ctcss() {
        int local_tone = 1; 
        if(transmitter->ctcss_frequency == (double) 67.0) {
           local_tone = 1; 
        } else if (transmitter->ctcss_frequency == (double) 71.9) {
           local_tone = 2; 
        } else if (transmitter->ctcss_frequency == (double) 74.4) {
           local_tone = 3; 
        } else if (transmitter->ctcss_frequency == (double) 77.0) {
           local_tone = 4; 
        } else if (transmitter->ctcss_frequency == (double) 79.7) {
           local_tone = 5; 
        } else if (transmitter->ctcss_frequency == (double) 82.5) {
           local_tone = 6; 
        } else if (transmitter->ctcss_frequency == (double) 85.4) {
           local_tone = 7; 
        } else if (transmitter->ctcss_frequency == (double) 88.5) {
           local_tone = 8; 
        } else if (transmitter->ctcss_frequency == (double) 91.5) {
           local_tone = 9; 
        } else if (transmitter->ctcss_frequency == (double) 94.8) {
           local_tone = 10; 
        } else if (transmitter->ctcss_frequency == (double) 97.4) {
           local_tone = 11; 
        } else if (transmitter->ctcss_frequency == (double) 100.0) {
           local_tone = 12; 
        } else if (transmitter->ctcss_frequency == (double) 103.5) {
           local_tone = 13; 
        } else if (transmitter->ctcss_frequency == (double) 107.2) {
           local_tone = 14; 
        } else if (transmitter->ctcss_frequency == (double) 110.9) {
           local_tone = 15; 
        } else if (transmitter->ctcss_frequency == (double) 114.8) {
           local_tone = 16; 
        } else if (transmitter->ctcss_frequency == (double) 118.8) {
           local_tone = 17; 
        } else if (transmitter->ctcss_frequency == (double) 123.0) {
           local_tone = 18; 
        } else if (transmitter->ctcss_frequency == (double) 127.3) {
           local_tone = 19; 
        } else if (transmitter->ctcss_frequency == (double) 131.8) {
           local_tone = 20; 
        } else if (transmitter->ctcss_frequency == (double) 136.5) {
           local_tone = 21; 
        } else if (transmitter->ctcss_frequency == (double) 141.3) {
           local_tone = 22; 
        } else if (transmitter->ctcss_frequency == (double) 146.2) {
           local_tone = 23; 
        } else if (transmitter->ctcss_frequency == (double) 151.4) {
           local_tone = 24; 
        } else if (transmitter->ctcss_frequency == (double) 156.7) {
           local_tone = 25; 
        } else if (transmitter->ctcss_frequency == (double) 162.2) {
           local_tone = 26; 
        } else if (transmitter->ctcss_frequency == (double) 167.9) {
           local_tone = 27; 
        } else if (transmitter->ctcss_frequency == (double) 173.8) {
           local_tone = 28; 
        } else if (transmitter->ctcss_frequency == (double) 179.9) {
           local_tone = 29; 
        } else if (transmitter->ctcss_frequency == (double) 186.2) {
           local_tone = 30; 
        } else if (transmitter->ctcss_frequency == (double) 192.8) {
           local_tone = 31; 
        } else if (transmitter->ctcss_frequency == (double) 203.5) {
           local_tone = 32; 
        } else if (transmitter->ctcss_frequency == (double) 210.7) {
           local_tone = 33; 
        } else if (transmitter->ctcss_frequency == (double) 218.1) {
           local_tone = 34; 
        } else if (transmitter->ctcss_frequency == (double) 225.7) {
           local_tone = 35; 
        } else if (transmitter->ctcss_frequency == (double) 233.6) {
           local_tone = 36; 
        } else if (transmitter->ctcss_frequency == (double) 241.8) {
           local_tone = 37; 
        } else if (transmitter->ctcss_frequency == (double) 250.3) {
           local_tone = 38; 
        } else if (transmitter->ctcss_frequency == (double) 1750.0) {
           local_tone = 39; 
        }
        return(local_tone);
}
int vfo_sm=0;   // VFO State Machine - this keeps track of

// Now my stuff
//
void send_dash() {
       //long delay = (1200000L * ((long)cw_keyer_weight/10L))/(long)cw_keyer_speed ;
       int dot_delay = 1200/cw_keyer_speed;
       int delay = (dot_delay * 3 * cw_keyer_weight)/50;
       g_idle_add(ext_cw_key, (gpointer)(long)1);
       usleep((long)delay*3000L);
       g_idle_add(ext_cw_key, (gpointer)(long)0);
       usleep((long)delay * 1000L);
       //fprintf(stderr,"_%d",mox);
       
}
void send_dot() {
       int dot_delay = 1200/cw_keyer_speed;
       g_idle_add(ext_cw_key, (gpointer)(long)1);
       usleep((long)dot_delay * 1000L);
       g_idle_add(ext_cw_key, (gpointer)(long)0);
       usleep((long)dot_delay* 1000L);
       //fprintf(stderr,".%d",mox);
}
void send_space() {
       int dot_delay = 1200/cw_keyer_speed;
       usleep((long)dot_delay* 7000L);
       //fprintf(stderr," %d",mox);
}

void rigctl_send_cw_char(char cw_char) {
    char pattern[9],*ptr;
    strcpy(pattern,"");
    ptr = &pattern[0];
    switch (cw_char) {
       case 'a':
       case 'A': strcpy(pattern,".-"); break;
       case 'b': 
       case 'B': strcpy(pattern,"-..."); break;
       case 'c': 
       case 'C': strcpy(pattern,"-.-."); break;
       case 'd': 
       case 'D': strcpy(pattern,"-.."); break;
       case 'e': 
       case 'E': strcpy(pattern,"."); break;
       case 'f': 
       case 'F': strcpy(pattern,"..-."); break;
       case 'g': 
       case 'G': strcpy(pattern,"--."); break;
       case 'h': 
       case 'H': strcpy(pattern,"...."); break;
       case 'i': 
       case 'I': strcpy(pattern,".."); break;
       case 'j': 
       case 'J': strcpy(pattern,".---"); break;
       case 'k': 
       case 'K': strcpy(pattern,"-.-"); break;
       case 'l': 
       case 'L': strcpy(pattern,".-.."); break;
       case 'm': 
       case 'M': strcpy(pattern,"--"); break;
       case 'n': 
       case 'N': strcpy(pattern,"-."); break;
       case 'o': 
       case 'O': strcpy(pattern,"---"); break;
       case 'p': 
       case 'P': strcpy(pattern,".--."); break;
       case 'q': 
       case 'Q': strcpy(pattern,"-.--"); break;
       case 'r': 
       case 'R': strcpy(pattern,".-."); break;
       case 's': 
       case 'S': strcpy(pattern,"..."); break;
       case 't': 
       case 'T': strcpy(pattern,"-"); break;
       case 'u': 
       case 'U': strcpy(pattern,"..-"); break;
       case 'v': 
       case 'V': strcpy(pattern,"...-"); break;
       case 'w': 
       case 'W': strcpy(pattern,".--"); break;
       case 'x': 
       case 'X': strcpy(pattern,"-..-"); break;
       case 'z': 
       case 'Z': strcpy(pattern,"--.."); break;
       case '0': strcpy(pattern,"-----"); break;
       case '1': strcpy(pattern,".----"); break;
       case '2': strcpy(pattern,"..---"); break;
       case '3': strcpy(pattern,"...--"); break;
       case '4': strcpy(pattern,"....-"); break;
       case '5': strcpy(pattern,".....");break;
       case '6': strcpy(pattern,"-....");break;
       case '7': strcpy(pattern,"--...");break;
       case '8': strcpy(pattern,"---..");break;
       case '9': strcpy(pattern,"----.");break;
       case '.': strcpy(pattern,".-.-.-");break;
       case '/': strcpy(pattern,"-..-.");break;
       case ',': strcpy(pattern,"--..--");break;
       case '!': strcpy(pattern,"-.-.--");break;
       case ')': strcpy(pattern,"-.--.-");break;
       case '(': strcpy(pattern,"-.--.-");break;
       case '&': strcpy(pattern,".-...");break;
       case ':': strcpy(pattern,"---..");break;
       case '+': strcpy(pattern,".-.-.");break;
       case '-': strcpy(pattern,"-....-");break;
       case '_': strcpy(pattern,".--.-.");break;
       case '@': strcpy(pattern,"..--.-");break;
       case ' ': strcpy(pattern," ");break;
       default:  strcpy(pattern," ");
    }
    //fprintf(stderr,"Sending %c:",cw_char);
    g_idle_add(ext_cw_key, (gpointer)(long)0);
     
    while(*ptr != '\0') {
       if(*ptr == '-') {
          send_dash();
       }
       if(*ptr == '.') {
          send_dot();
       }
       if(*ptr == ' ') {
          send_space();
       }
       ptr++;
    }
    // Need a delay HERE between characters
    long delay = (1200000L * ((long)cw_keyer_weight/10L))/(long)cw_keyer_speed ;
    usleep(delay*3L);
    //fprintf(stderr,"\n");
}
void gui_vfo_move_to(gpointer data) {
   long long freq = *(long long *) data;
   fprintf(stderr,"GUI: %11lld\n",freq);
   return;
   //vfo_move_to(freq);
}

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
    int bytes=write(client_sock, msg, strlen(msg));
}

//
// 2-25-17 - K5JAE - removed duplicate rigctl
//

static gpointer rigctl_server(gpointer data) {
  int port=(int)data;
  int rc;
  int on=1;
  int i;

fprintf(stderr,"rigctl_server: starting server on port %d\n",port);

  server_socket=socket(AF_INET,SOCK_STREAM,0);
  if(server_socket<0) {
    perror("rigctl_server: listen socket failed");
    return NULL;
  }

  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

  // bind to listening port
  memset(&server_address,0,sizeof(server_address));
  server_address.sin_family=AF_INET;
  server_address.sin_addr.s_addr=INADDR_ANY;
  server_address.sin_port=htons(port);
  if(bind(server_socket,(struct sockaddr*)&server_address,sizeof(server_address))<0) {
    perror("rigctl_server: listen socket bind failed");
    close(server_socket);
    return NULL;
  }

  for(i=0;i<MAX_CLIENTS;i++) {
    client[i].socket=-1;
  }
  server_running=1;
  while(server_running) {
    // listen with a max queue of 3
    if(listen(server_socket,3)<0) {
      perror("rigctl_server: listen failed");
      close(server_socket);
      return NULL;
    }

    // find a spare thread
    for(i=0;i<MAX_CLIENTS;i++) {
      if(client[i].socket==-1) {

        int client_socket;
        struct sockaddr_in client_address;
        int client_address_length;
fprintf(stderr,"Using client: %d\n",i);

        client[i].socket=accept(server_socket,(struct sockaddr*)&client[i].address,&client[i].address_length);
        if(client[i].socket<0) {
          perror("rigctl_server: client accept failed");
          continue;
        }

        client[i].thread_id = g_thread_new("rigctl client", rigctl_client, (gpointer)&client[i]);
        if(client[i].thread_id==NULL) {
fprintf(stderr,"g_thread_new failed (n rigctl_client\n");
fprintf(stderr,"setting SO_LINGER to 0 for client_socket: %d\n",client[i].socket);
          struct linger linger = { 0 };
          linger.l_onoff = 1;
          linger.l_linger = 0;
          if(setsockopt(client[i].socket,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
            perror("setsockopt(...,SO_LINGER,...) failed for client");
          }
          close(client[i].socket);
        }
      }
    }
  }

  close(server_socket);
  return NULL;
}

static gpointer rigctl_client (gpointer data) {
   int len;
   int c;
   
   CLIENT *client=(CLIENT *)data;

fprintf(stderr,"rigctl_client: starting rigctl_client: socket=%d\n",client->socket);

   g_mutex_lock(&mutex_a->m);
   cat_control++;
//#ifdef RIGCTL_DEBUG
fprintf(stderr,"RIGCTL: CTLA INC cat_contro=%d\n",cat_control);
//#endif
   g_mutex_unlock(&mutex_a->m);
   g_idle_add(ext_vfo_update,NULL);

   int save_flag = 0; // Used to concatenate two cmd lines together
   int semi_number = 0;
   int i;
   char * work_ptr;
   char work_buf[MAXDATASIZE];
   int numbytes;
   char  cmd_input[MAXDATASIZE] ;
   char cmd_save[80];
   char cw_check_buf[4];

    while(server_running && (numbytes=recv(client->socket , cmd_input , MAXDATASIZE-2 , 0)) > 0 ) {
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
               if(cw_busy == 1) {
                  if(strlen(work_ptr)>2) {
                     cw_check_buf[0] = work_ptr[0];
                     cw_check_buf[1] = work_ptr[1];
                     cw_check_buf[2] = '\0';
                     if(strcmp("ZZ",cw_check_buf)==0) {
                          if(strlen(work_ptr)>4) {
                             cw_check_buf[0] = work_ptr[2];
                             cw_check_buf[1] = work_ptr[3];
                             cw_check_buf[2] = '\0';
                          } else {
                             send_resp(client->socket,"?;");
                          }
                     } 
                   } else {
                       // Illegal Command
                       send_resp(client->socket,"?;");
                   } 
                   // Got here because we have a legal command in cw_check_buf
                   // Look for RESET and BUSY which re respond to else - send ?;
                   if(strcmp("BY",cw_check_buf)==0) {
                      send_resp(client->socket,"BY11;"); // Indicate that we are BUSY
                   } else if (strcmp("SR",cw_check_buf) == 0) {
                      // Reset the transceiver
                      g_mutex_lock(&mutex_c->m);
                      cw_reset = 1;
                      g_mutex_unlock(&mutex_c->m);
                      // Wait till BUSY clears
                      while(cw_busy);
                      g_mutex_lock(&mutex_c->m);
                      cw_reset = 0;
                      g_mutex_unlock(&mutex_c->m);
                   }

               }
               // Lock so only one user goes into this at a time
               g_mutex_lock(&mutex_b->m);
               parse_cmd(work_ptr,strlen(work_ptr),client->socket);
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
fprintf(stderr,"RIGCTL: Leaving rigctl_client thread");
  if(client->socket!=-1) {
fprintf(stderr,"setting SO_LINGER to 0 for client_socket: %d\n",client->socket);
    struct linger linger = { 0 };
    linger.l_onoff = 1;
    linger.l_linger = 0;
    if(setsockopt(client->socket,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
      perror("setsockopt(...,SO_LINGER,...) failed for client");
    }
    close(client->socket);
    client->socket=-1;
    // Decrement CAT_CONTROL
    g_mutex_lock(&mutex_a->m);
    cat_control--;
#ifdef RIGCTL_DEBUG
fprintf(stderr,"RIGCTL: CTLA DEC - cat_control=%d\n",cat_control);
#endif
    g_mutex_unlock(&mutex_a->m);
    g_idle_add(ext_vfo_update,NULL);
  }
  return NULL; 
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
//
// TS-2000 parser
//   -- Now extended with zzid_flag to indicate PSDR extended command set
// 
void parse_cmd ( char * cmd_input,int len,int client_sock) {
        int work_int;     
        int new_low, new_high;
        int zzid_flag;
        double meter;
        char msg[200];
        char buf[200];
        BANDSTACK_ENTRY *entry;
        // Parse the cmd_input
        //int space = command.indexOf(' ');
        //char cmd_char = com_head->cmd_string[0]; // Assume the command is first thing!
        char cmd_str[3];

        cmd_str[0] = cmd_input[0];
        cmd_str[1] = cmd_input[1];
        cmd_str[2] = '\0';

        // Added support for PSDR extended command set - they all start
        // with ZZ so:
        //
        // Check to see if first part of string is ZZ - if so
        //    strip the ZZ out and set the zzid_flag = 1;
 
       #ifdef  RIGCTL_DEBUG
       fprintf(stderr,"RIGCTL: CMD=%s\n",cmd_str);
       #endif
        zzid_flag = 0;  // Set to indicate we haven't seen ZZ
        char * zzid_ptr;
        char temp[80];
        int cnt;
        if(strcmp(cmd_str,"ZZ")==0) {
           
           #ifdef  RIGCTL_DEBUG
           fprintf(stderr,"RIGCTL: Init=%s\n",cmd_str);
           #endif

           // Adjust the Parse input like there hadn't been a ZZ in front - but 
           // set the ZZ flag to indicate we saw it.
           zzid_ptr = &temp[0];

           // It is 4AM and this was the only safe way for me to get a strcpy to work
           // so - there ya go...
           for(cnt=2; cnt<=len;cnt++) { 
               //fprintf(stderr,"[%d]=%c:",cnt,cmd_input[cnt]);
               *zzid_ptr++= cmd_input[cnt]; 
           }
           temp[len+1] = '\0';

           strcpy(cmd_input,temp);
           fprintf(stderr,"RIGCTL: Cmd_input=%s\n",cmd_input);
           // 
           cmd_str[0] = cmd_input[0];
           cmd_str[1] = cmd_input[1];
           cmd_str[2] = '\0';
           zzid_flag = 1;
           len = strlen (temp);
        }
        fprintf(stderr,"RIGCTL: Cmd_Str=%s\n",cmd_str);
        if(strcmp(cmd_str,"AC")==0)       {  
                                            // TS-2000 - AC - STEP Command 
                                            // PiHPSDR - ZZAC - Step Command 
                                             if(zzid_flag ==1) { // Set or Read the Step Size (replaces ZZST)
                                                   if(len <= 2) {
                                                       switch(step) {
                                                          case  1: work_int = 0; break;  
                                                          case  10: work_int = 1; break;  
                                                          case  25: work_int = 2; break;  
                                                          case  50: work_int = 3; break;  
                                                          case  100: work_int = 4; break;  
                                                          case  250: work_int = 5; break;  
                                                          case  500: work_int = 6; break;  
                                                          case  1000: work_int = 7; break;  
                                                          case  2500: work_int = 8; break;  
                                                          case  5000: work_int = 9; break;  
                                                          case  6250: work_int = 10; break;  
                                                          case  9000: work_int = 11; break;  
                                                          case  10000: work_int = 12; break;  
                                                          case  12500: work_int = 13; break;  
                                                          case  15000: work_int = 14; break;  
                                                          case  20000: work_int = 15; break;  
                                                          case  25000: work_int = 16; break;  
                                                          case  30000: work_int = 17; break;  
                                                          case  50000: work_int = 18; break;  
                                                          case  100000: work_int = 19; break;  
                                                          default: 
                                                              work_int = 0;
                                                              fprintf(stderr,"RIGCTL: ERROR  step out of range\n");
                                                              send_resp(client_sock,"?;");
                                                              break;
                                                          }
                                                          fprintf(stderr,"RESP: ZZAC%02d;",work_int);
                                                          sprintf(msg,"ZZAC%02d;",work_int);
                                                          send_resp(client_sock,msg);
                                              
                                                    } else { // Legal vals between 00 and 22.
                                                       switch(atoi(&cmd_input[2])) {
                                                          case  0: step = 1; break;  
                                                          case  1: step = 10; break;  
                                                          case  2: step = 25; break;  
                                                          case  3: step = 50; break;  
                                                          case  4: step = 100; break;  
                                                          case  5: step = 250; break;  
                                                          case  6: step = 500; break;  
                                                          case  7: step = 1000; break;  
                                                          case  8: step = 2500; break;  
                                                          case  9: step = 5000; break;  
                                                          case  10: step = 6250; break;  
                                                          case  11: step = 9000; break;  
                                                          case  12: step = 10000; break;  
                                                          case  13: step = 12500; break;  
                                                          case  14: step = 15000; break;  
                                                          case  15: step = 20000; break;  
                                                          case  16: step = 25000; break;  
                                                          case  17: step = 30000; break;  
                                                          case  18: step = 50000; break;  
                                                          case  19: step = 100000; break;  
                                                          default: 
                                                              fprintf(stderr,"RIGCTL: ERROR - ZZAC out of range\n");
                                                              send_resp(client_sock,"?;");
                                                          }
                                                       }
                                                 } else { // Sets or reads the internal antenna tuner status
                                                      // P1 0:RX-AT Thru, 1: RX-AT IN
                                                      // P2 0: TX-AT Thru 1: TX-AT In
                                                      // 
                                                        if(len <= 2) {
                                                          send_resp(client_sock,"AC000;");
                                                        }
                                              }
                                          }
        else if((strcmp(cmd_str,"AD")==0) && (zzid_flag==1)) { 
                                            // PiHPSDR - ZZAD - Move VFO A Down by step
                                              vfo_step(-1);
                                              g_idle_add(ext_vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"AG")==0) {  
                                            if(zzid_flag == 1) { 
                                            // PiHPSDR - ZZAG - Set/read Audio Gain
                                            // Values are 000-100 
                                                if(len<=2) { // Send ZZAGxxx; - active_receiver->volume 0.00-1.00
                                                     sprintf(msg,"ZZAG%03d;",(int) (roundf(active_receiver->volume *100.0)));
                                                     send_resp(client_sock,msg);
                                                } else { // Set Audio Gain
                                                    work_int = atoi(&cmd_input[2]);
                                                    active_receiver->volume = ((double) work_int)/100.0;
                                                    g_idle_add(ext_update_af_gain,NULL);               
                                                }
                                             } else { 
                                            // TS-2000 - AG - Set Audio Gain 
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
                                                   if((atoi(&cmd_input[3]) >=0) && (atoi(&cmd_input[3])<=260)) {
                                                      active_receiver->volume = ((double) atoi(&cmd_input[3]))/260; 
                                                      #ifdef  RIGCTL_DEBUG
                                                      fprintf(stderr,"RIGCTL: Set to: %d AG volume=%5.3f\n",atoi(&cmd_input[3])/260,active_receiver->volume);
                                                      #endif
                                                      g_idle_add(ext_update_af_gain,NULL);               
                                                   } else {
                                                     send_resp(client_sock,"?;");
                                                   }
                                                } else { // Read Audio Gain
                                                  #ifdef  RIGCTL_DEBUG
                                                  fprintf(stderr,"RIGCTL: AG volume=%4.2f\n",active_receiver->volume);
                                                  #endif
                                                  sprintf(msg,"AG%1d%03d;",lcl_receiver,(int) (260 * active_receiver->volume));
                                                  send_resp(client_sock,msg);
                                                }
                                             }
                                          }
        else if((strcmp(cmd_str,"AI")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - AI- Allow rebroadcast of set frequency after set - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"AI0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"AL")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - AL - Set/Reads the auto Notch level - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"AL000;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"AM")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - AM - Sets or reads the Auto Mode - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"AM0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"AN")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - AN - Selects the antenna connector (ANT1/2) - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"AN0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"AP")==0) && (zzid_flag == 1)) { 
                                            // PiHPSDR - ZZAP - Set/Read Power Amp Settings.
                                            //                  format - P1P1P1 P2P2.P2 
                                            //                  P1 - Band P2 - 3.1 float WITH decimal point required!
                                               fprintf(stderr,"RIGCTL: ZZAP len=%d\n",len);
                                               if(len<=5) { // Read Command
                                                 work_int = lookup_band(atoi(&cmd_input[2]));
                                                 fprintf(stderr,"RIGCTL ZZAP work_int=%d\n",work_int);
                                                 BAND *band = band_get_band(work_int);
                                                 sprintf(msg,"ZZAP%03d%3.1f;",atoi(&cmd_input[2]),band->pa_calibration);
                                                 send_resp(client_sock,msg);
                                              } else {
                                                 /* isolate the command band from the value */ 
                                                 char lcl_char = cmd_input[5];;
                                                 cmd_input[5]='\0'; // Make a temp string
                                                 work_int = atoi(&cmd_input[2]);
                                                 cmd_input[5] = lcl_char; // Restore the orig string..
                                                 double lcl_float = atof(&cmd_input[5]);
                                                 fprintf(stderr,"RIGCTL ZZAP - band=%d setting=%3.1f\n",work_int, lcl_float); 
                                                 work_int = lookup_band(work_int);

                                                 // Sequence below lifted from pa_menu
                                                 BAND *band = band_get_band(work_int);
                                                 band->pa_calibration = lcl_float;
                                                 work_int = VFO_A;
                                                 if(split) work_int = VFO_B;
                                                 int b = vfo[work_int].band;
                                                 BAND *current = band_get_band(b);
                                                 if(band == current) {
                                                     g_idle_add( ext_calc_drive_level,(gpointer) NULL);
                                                     //g_idle_add( ext_calc_tune_drive_level,(gpointer) NULL);
                                                 } 
                                              } 
                                          }
        else if(strcmp(cmd_str,"AR")==0)  { 
                                            if(zzid_flag ==1) { 
                                            // PiHPSDR - ZZAR - Set or reads the RX1 AGC Threshold Control
                                                 if(len <=2) { // Read Response
                                                    // X is +/- sign
                                                    if(active_receiver->agc_gain >=0) {
                                                       sprintf(msg,"ZZAR+%03d;",(int)active_receiver->agc_gain);
                                                    } else {
                                                       sprintf(msg,"ZZAR-%03d;",abs((int)active_receiver->agc_gain));
                                                    }
                                                    send_resp(client_sock,msg);
                                                 } else {
                                                    double new_gain = (double) atoi(&cmd_input[2]);
                                                    double *p_gain=malloc(sizeof(double));
                                                    *p_gain=new_gain;
                                                    g_idle_add(ext_update_agc_gain,(gpointer)p_gain);
                                                 } 
                                            } else {     
                                            // TS-2000 - AR - Sets or reads the ASC function on/off - not supported
                                                 if(len <=2) {
                                                    //send_resp(client_sock,"AR0;");
                                                    send_resp(client_sock,"?;");
                                                 } 
                                            }
                                          }
        else if((strcmp(cmd_str,"AS")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - AS - Sets/reads automode function parameters
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
        else if((strcmp(cmd_str,"AU")==0) && (zzid_flag==1)) { 
                                            // PiHPSDR - ZZAU - Move VFO A Up by step
                                              vfo_step(1);
                                              g_idle_add(ext_vfo_update,NULL);
                                         }
        else if((strcmp(cmd_str,"BC")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - BC - Beat Cancellor OFF - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"BC0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BD")==0)  {  
                                            // PiHPSDR - ZZBD - Moves down the frequency band selector
                                            // TS-2000 - BD - Moves down the frequency band selector
                                            // No response 
                                             // This reacts to both BD and ZZBD - only works for active receiver
                                             int cur_band = vfo[active_receiver->id].band;
                                             fprintf(stderr,"RIGCTL: BD - current band=%d\n",cur_band);
                                             if(cur_band == 0) {
                                                #ifdef LIMESDR
                                                    cur_band = band472;
                                                #else
                                                    cur_band = band6;
                                                #endif
                                             } else {
                                                --cur_band;
                                             } 
                                             fprintf(stderr,"RIGCTL: BD - new band=%d\n",cur_band);
                                             g_idle_add(ext_vfo_band_changed,(gpointer)(long)cur_band);
                                          }
        else if((strcmp(cmd_str,"BP")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - BP - Reads the manual beat canceller freq - not supported 
                                             if(len <=2) {
                                                //send_resp(client_sock,"BP000;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if(strcmp(cmd_str,"BS")==0)  {  
                                            if(zzid_flag == 1) {
                                            // PiHPSDR - ZZBS - Read the RX1 Band switch
                                                 int cur_band = vfo[active_receiver->id].band;
                                                 fprintf(stderr,"RIGCTL: BS - band=%d\n",cur_band);
    
                                                 switch(cur_band) {
                                                    case 0:  work_int = 160;  break;
                                                    case 1:  work_int = 80;  break;
                                                    case 2:  work_int = 60;  break;
                                                    case 3:  work_int = 40;  break;
                                                    case 4:  work_int = 30;  break;
                                                    case 5:  work_int = 20;  break;
                                                    case 6:  work_int = 17;  break;
                                                    case 7:  work_int = 15;  break;
                                                    case 8:  work_int = 12;  break;
                                                    case 9:  work_int = 10;  break;
                                                    case 10:  work_int = 6;  break;
                                                    case 11: work_int = 888; break;
                                                    case 12: work_int = 999; break;
                                                    case 13: work_int = 136; break;
                                                    case 14: work_int = 472; break;
                                                    }
                                                 sprintf(msg,"ZZBS%03d;",work_int);
                                                 send_resp(client_sock,msg);

                                             } else {
                                                 // Sets or reads Beat Canceller status
                                                if(len <=2) {
                                                   //send_resp(client_sock,"BS0;");
                                                   send_resp(client_sock,"?;");
                                                } 
                                             }
                                          }
        else if(strcmp(cmd_str,"BU")==0)  {  
                                            // PiHPSDR - ZZBU - Moves Up the frequency band
                                            // TS-2000 - BU - Moves Up the frequency band
                                             // No response 
                                             int cur_band = vfo[active_receiver->id].band;
                                             fprintf(stderr,"RIGCTL: BU - Max Band = %d current band=%d\n",BANDS,cur_band);
                                             if(cur_band >= 10) {
                                                    cur_band = band160;
                                             } else {
                                                ++cur_band;
                                             } 
                                             fprintf(stderr,"RIGCTL: BU - new band=%d\n",cur_band);
                                             g_idle_add(ext_vfo_band_changed,(gpointer)(long)cur_band);
                                          }
        else if((strcmp(cmd_str,"BY")==0) && (zzid_flag == 0))  {
                                            // TS-2000 - BY - Read the busy signal status
                                             if(len <=2) {
                                                //send_resp(client_sock,"BY00;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"CA")==0) && (zzid_flag == 0))  {
                                            // TS-2000 - CA - Sets/reads cw auto tune f(x) status - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"CA0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"CB")==0) && (zzid_flag==1)) {
                                            // PIHPSDR - ZZCB - Sets/reads CW Break In checkbox status
                                            // P1 = 0 for disabled, 1 for enabled
                                            if(len <= 2) {
                                                 sprintf(msg,"ZZCB%01d;",cw_breakin);
                                                 send_resp(client_sock,msg);
                                            } else {
                                                cw_breakin = atoi(&cmd_input[2]);
                                            }
                                          }
        else if((strcmp(cmd_str,"CG")==0) && (zzid_flag == 0))  { 
                                            // TS-2000 - CD - Sets/Reads the carrier gain status - not supported
                                            // 0-100 legal values
                                            if(len <=2) {
                                                //send_resp(client_sock,"CG000;");
                                                send_resp(client_sock,"?;");
                                            } 
                                          }
        else if((strcmp(cmd_str,"CH")==0) && (zzid_flag == 0)) { 
                                            // TS-2000 - CH - Sets the current frequency to call Channel -no response 
                                          }
        else if((strcmp(cmd_str,"CI")==0) && (zzid_flag == 0)) { 
                                            // TS-2000 - CI - In VFO md or Mem recall md, sets freq 2 call channel - no response
                                          }
        else if((strcmp(cmd_str,"CL")==0) && (zzid_flag == 1)) {  
                                            // PiHPSDR - ZZCL - CW Pitch set
                                            // 0200 - 1200
                                             if(len <=2) {
                                                 sprintf(msg,"ZZCL%04d;",cw_keyer_sidetone_frequency);
                                                 send_resp(client_sock,msg);
                                             } else {
                                                int local_freq = atoi(&cmd_input[2]);
                                                if((local_freq >= 200) && (local_freq <=1200)) {
                                                   cw_keyer_sidetone_frequency = atoi(&cmd_input[2]);
                                                   g_idle_add(ext_vfo_update,NULL);
                                                } else {
                                                   fprintf(stderr,"RIGCTL: ZZCL illegal freq=%d\n",local_freq);
                                                }
                                             }
                                          }
        else if((strcmp(cmd_str,"CM")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - CM - Sets or reads the packet cluster tune f(x) on/off -not supported
                                            // 0-100 legal values
                                             if(len <=2) {
                                                //send_resp(client_sock,"CM0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"CN")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - CN - Sets and reads CTSS function - 01-38 legal values
                                            // Stored locally in rigctl - not used.
                                            if(len >2) {
                                                ctcss_tone = atoi(&cmd_input[2]);
                                                if((ctcss_tone > 0) && (ctcss_tone <= 39)) {
                                                    switch(ctcss_tone) {
                                                      case  1: transmitter->ctcss_frequency = (double) 67.0;break;
                                                      case  2: transmitter->ctcss_frequency = (double) 71.9;break;
                                                      case  3: transmitter->ctcss_frequency = (double) 74.4;break;
                                                      case  4: transmitter->ctcss_frequency = (double) 77.0;break;
                                                      case  5: transmitter->ctcss_frequency = (double) 79.7;break;
                                                      case  6: transmitter->ctcss_frequency = (double) 82.5;break;
                                                      case  7: transmitter->ctcss_frequency = (double) 85.4;break;
                                                      case  8: transmitter->ctcss_frequency = (double) 88.5;break;
                                                      case  9: transmitter->ctcss_frequency = (double) 91.5;break;
                                                      case 10: transmitter->ctcss_frequency = (double) 94.8;break;
                                                      case 11: transmitter->ctcss_frequency = (double) 97.4;break;
                                                      case 12: transmitter->ctcss_frequency = (double) 100.0;break;
                                                      case 13: transmitter->ctcss_frequency = (double) 103.5;break;
                                                      case 14: transmitter->ctcss_frequency = (double) 107.2;break;
                                                      case 15: transmitter->ctcss_frequency = (double) 110.9;break;
                                                      case 16: transmitter->ctcss_frequency = (double) 114.8;break;
                                                      case 17: transmitter->ctcss_frequency = (double) 118.8;break;
                                                      case 18: transmitter->ctcss_frequency = (double) 123.0;break;
                                                      case 19: transmitter->ctcss_frequency = (double) 127.3;break;
                                                      case 20: transmitter->ctcss_frequency = (double) 131.8;break;
                                                      case 21: transmitter->ctcss_frequency = (double) 136.5;break;
                                                      case 22: transmitter->ctcss_frequency = (double) 141.3;break;
                                                      case 23: transmitter->ctcss_frequency = (double) 146.2;break;
                                                      case 24: transmitter->ctcss_frequency = (double) 151.4;break;
                                                      case 25: transmitter->ctcss_frequency = (double) 156.7;break;
                                                      case 26: transmitter->ctcss_frequency = (double) 162.2;break;
                                                      case 27: transmitter->ctcss_frequency = (double) 167.9;break;
                                                      case 28: transmitter->ctcss_frequency = (double) 173.8;break;
                                                      case 29: transmitter->ctcss_frequency = (double) 179.9;break;
                                                      case 30: transmitter->ctcss_frequency = (double) 186.2;break;
                                                      case 31: transmitter->ctcss_frequency = (double) 192.8;break;
                                                      case 32: transmitter->ctcss_frequency = (double) 203.5;break;
                                                      case 33: transmitter->ctcss_frequency = (double) 210.7;break;
                                                      case 34: transmitter->ctcss_frequency = (double) 218.1;break;
                                                      case 35: transmitter->ctcss_frequency = (double) 225.7;break;
                                                      case 36: transmitter->ctcss_frequency = (double) 233.6;break;
                                                      case 37: transmitter->ctcss_frequency = (double) 241.8;break;
                                                      case 38: transmitter->ctcss_frequency = (double) 250.3;break;
                                                      case 39: transmitter->ctcss_frequency = (double) 1750.0;break;
                                                    }
                                                    transmitter_set_ctcss(transmitter,transmitter->ctcss,transmitter->ctcss_frequency);
                                                  }
                                               } else {
                                                    ctcss_tone = -1; 
                                                    if(transmitter->ctcss_frequency == (double) 67.0) {
                                                       ctcss_tone = 1; 
                                                    } else if (transmitter->ctcss_frequency == (double) 71.9) {
                                                       ctcss_tone = 2; 
                                                    } else if (transmitter->ctcss_frequency == (double) 74.4) {
                                                       ctcss_tone = 3; 
                                                    } else if (transmitter->ctcss_frequency == (double) 77.0) {
                                                       ctcss_tone = 4; 
                                                    } else if (transmitter->ctcss_frequency == (double) 79.7) {
                                                       ctcss_tone = 5; 
                                                    } else if (transmitter->ctcss_frequency == (double) 82.5) {
                                                       ctcss_tone = 6; 
                                                    } else if (transmitter->ctcss_frequency == (double) 85.4) {
                                                       ctcss_tone = 7; 
                                                    } else if (transmitter->ctcss_frequency == (double) 88.5) {
                                                       ctcss_tone = 8; 
                                                    } else if (transmitter->ctcss_frequency == (double) 91.5) {
                                                       ctcss_tone = 9; 
                                                    } else if (transmitter->ctcss_frequency == (double) 94.8) {
                                                       ctcss_tone = 10; 
                                                    } else if (transmitter->ctcss_frequency == (double) 97.4) {
                                                       ctcss_tone = 11; 
                                                    } else if (transmitter->ctcss_frequency == (double) 100.0) {
                                                       ctcss_tone = 12; 
                                                    } else if (transmitter->ctcss_frequency == (double) 103.5) {
                                                       ctcss_tone = 13; 
                                                    } else if (transmitter->ctcss_frequency == (double) 107.2) {
                                                       ctcss_tone = 14; 
                                                    } else if (transmitter->ctcss_frequency == (double) 110.9) {
                                                       ctcss_tone = 15; 
                                                    } else if (transmitter->ctcss_frequency == (double) 114.8) {
                                                       ctcss_tone = 16; 
                                                    } else if (transmitter->ctcss_frequency == (double) 118.8) {
                                                       ctcss_tone = 17; 
                                                    } else if (transmitter->ctcss_frequency == (double) 123.0) {
                                                       ctcss_tone = 18; 
                                                    } else if (transmitter->ctcss_frequency == (double) 127.3) {
                                                       ctcss_tone = 19; 
                                                    } else if (transmitter->ctcss_frequency == (double) 131.8) {
                                                       ctcss_tone = 20; 
                                                    } else if (transmitter->ctcss_frequency == (double) 136.5) {
                                                       ctcss_tone = 21; 
                                                    } else if (transmitter->ctcss_frequency == (double) 141.3) {
                                                       ctcss_tone = 22; 
                                                    } else if (transmitter->ctcss_frequency == (double) 146.2) {
                                                       ctcss_tone = 23; 
                                                    } else if (transmitter->ctcss_frequency == (double) 151.4) {
                                                       ctcss_tone = 24; 
                                                    } else if (transmitter->ctcss_frequency == (double) 156.7) {
                                                       ctcss_tone = 25; 
                                                    } else if (transmitter->ctcss_frequency == (double) 162.2) {
                                                       ctcss_tone = 26; 
                                                    } else if (transmitter->ctcss_frequency == (double) 167.9) {
                                                       ctcss_tone = 27; 
                                                    } else if (transmitter->ctcss_frequency == (double) 173.8) {
                                                       ctcss_tone = 28; 
                                                    } else if (transmitter->ctcss_frequency == (double) 179.9) {
                                                       ctcss_tone = 29; 
                                                    } else if (transmitter->ctcss_frequency == (double) 186.2) {
                                                       ctcss_tone = 30; 
                                                    } else if (transmitter->ctcss_frequency == (double) 192.8) {
                                                       ctcss_tone = 31; 
                                                    } else if (transmitter->ctcss_frequency == (double) 203.5) {
                                                       ctcss_tone = 32; 
                                                    } else if (transmitter->ctcss_frequency == (double) 210.7) {
                                                       ctcss_tone = 33; 
                                                    } else if (transmitter->ctcss_frequency == (double) 218.1) {
                                                       ctcss_tone = 34; 
                                                    } else if (transmitter->ctcss_frequency == (double) 225.7) {
                                                       ctcss_tone = 35; 
                                                    } else if (transmitter->ctcss_frequency == (double) 233.6) {
                                                       ctcss_tone = 36; 
                                                    } else if (transmitter->ctcss_frequency == (double) 241.8) {
                                                       ctcss_tone = 37; 
                                                    } else if (transmitter->ctcss_frequency == (double) 250.3) {
                                                       ctcss_tone = 38; 
                                                    } else if (transmitter->ctcss_frequency == (double) 1750.0) {
                                                       ctcss_tone = 39; 
                                                    } else {
                                                       send_resp(client_sock,"?;");
                                                    }
                                                    if(ctcss_tone != -1) {
                                                       sprintf(msg,"CN%02d;",ctcss_tone);
                                                       send_resp(client_sock,msg);
                                                    }
                                                }
                                          }
        // ZZCS - Keyer Speed implemented within "KS" command
        else if((strcmp(cmd_str,"CT")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - CS - Sets and reads CTSS function status 
                                            // Stored locally in rigctl - not used.
                                            if(len <=2) {
                                                sprintf(msg,"CT%01d;",transmitter->ctcss);
                                                send_resp(client_sock,msg);
                                            } else {
                                                if((atoi(&cmd_input[2]) == 0) ||(atoi(&cmd_input[2]) == 1)) {
                                                   transmitter->ctcss = atoi(&cmd_input[2]);
                                                   transmitter_set_ctcss(transmitter,transmitter->ctcss,transmitter->ctcss_frequency);
                                                } else {
                                                   send_resp(client_sock,"?;");
                                                }
                                            }
                                          }
        else if((strcmp(cmd_str,"DC")==0) && (zzid_flag == 0))  { 
                                            // TS-2000 - DC - Sets/Reads TX band status
                                            if(len <=2) {
                                                //send_resp(client_sock,"DC00;");
                                                send_resp(client_sock,"?;");
                                            } 
                                          }
        else if(strcmp(cmd_str,"DN")==0)  {  
                                            // TS-2000 - DN - Emulate Mic down key  - Not emulated
                                            // PiHPSDR - ZZDN - Set/Read Waterfall Lo Limit 
                                             if(zzid_flag == 1) { // ZZDN
                                               if(len<=2) {
                                                  if(active_receiver->waterfall_low <0) {
                                                     sprintf(msg,"ZZDN-%03d;",abs(active_receiver->waterfall_low));
                                                  } else {
                                                     sprintf(msg,"ZZDN+%03d;",active_receiver->waterfall_low);
                                                  }
                                                  send_resp(client_sock,msg);
                                               } else {
                                                  int lcl_waterfall_low = atoi(&cmd_input[3]);
                                                  if((lcl_waterfall_low >= -200) && (lcl_waterfall_low <=200)) {
                                                     if(cmd_input[2]=='-') {
                                                         //waterfall_low = lcl_waterfall_low;
                                                         active_receiver->waterfall_low = -lcl_waterfall_low;
                                                     } else {
                                                         active_receiver->waterfall_low =  lcl_waterfall_low;
                                                     }
                                                  } else {
                                                    fprintf(stderr,"RIGCTL: ZZDN illegal val=%d\n",lcl_waterfall_low);
                                                  }
                                               }
                                             }
                                          }
        else if((strcmp(cmd_str,"DO")==0) && zzid_flag == 1)  {  
                                            // PiHPSDR - ZZDO - Set/Read Waterfall Hi Limit 
                                             if(zzid_flag == 1) { // ZZDO
                                               if(len<=2) {
                                                  if(active_receiver->waterfall_high <0) {
                                                     sprintf(msg,"ZZDO-%03d;",abs(active_receiver->waterfall_high));
                                                  } else {
                                                     sprintf(msg,"ZZDO+%03d;",active_receiver->waterfall_high);
                                                  }
                                                  send_resp(client_sock,msg);
                                               } else {
                                                  int lcl_waterfall_high = atoi(&cmd_input[3]);
                                                  if((lcl_waterfall_high >= -200) && (lcl_waterfall_high <=200)) {
                                                     if(cmd_input[2]=='-') {
                                                         active_receiver->waterfall_high = -lcl_waterfall_high;
                                                     } else {
                                                         active_receiver->waterfall_high =  lcl_waterfall_high;
                                                     }
                                                  } else {
                                                    fprintf(stderr,"RIGCTL: ZZDO illegal val=%d\n",lcl_waterfall_high);
                                                  }
                                               }
                                             }
                                          }
        else if((strcmp(cmd_str,"DQ")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - DQ - ETs/and reads the DCS function status - not supported
                                             if(len <=2) {
                                                //send_resp(client_sock,"DQ0;");
                                                send_resp(client_sock,"?;");
                                             } 
                                          }
        else if((strcmp(cmd_str,"DU")==0) && zzid_flag ==1)   {  
                                            // PiHPSDR - ZZDU - Read Status Word - NOT compatible with PSDR!!
                                             int smeter = (int) GetRXAMeter(active_receiver->id, smeter) ;
                                             sprintf(msg,"ZZDU%1d%1d%1d%1d%1d%1d%1d%1d%1d%06d%1d%03d%1c%1d%1d%1d%1c%06lld%1d%011lld%011lld%c%03d%1d%1d%1d%1d;",
                                                              split,  //P1
                                                              tune,   // P2,
                                                              mox,    // P3 
                                                              receivers, // P4
                                                              active_receiver->id, // P5
                                                              active_receiver->alex_antenna, // P6
                                                              rit_on(),  // P7 
                                                              active_receiver->agc, // P8
                                                              vfo[active_receiver->id].mode, //P9
                                                              (int) step,  // P10
                                                              vfo[active_receiver->id].band, // P11 6d
                                                              ((int) transmitter->drive)*100, // P12
                                                              (int)active_receiver->agc_gain >=0 ? '+' : '-', // P13 sign
                                                              (int)active_receiver->agc_gain, // P13 3d
                                                              (int)active_receiver->volume, // P14 3d
                                                              vfo[active_receiver->id].rit_enabled,
                                                              vfo[active_receiver->id].rit >=0 ? '+' : '-', // P15 sign
                                                              vfo[active_receiver->id].rit, // P15 6d
                                                              vfo[active_receiver->id].ctun, // P16 1d
                                                              vfo[active_receiver->id].frequency, // P17 11d
                                                              vfo[active_receiver->id].ctun_frequency, // P18 11d
                                                              smeter>=0 ? '+': '-',
                                                              abs(smeter),
                                                              active_receiver->nr  , active_receiver->nr2  ,
                                                              active_receiver->anf , active_receiver->snb);
                                                  send_resp(client_sock,msg);
                                          }

        else if((strcmp(cmd_str,"EA")==0) && zzid_flag ==1)   {  
                                            // PiHPSDR - ZZEA - sets/reads RX EQ values
                                            if(len<=2) {
                                                    sprintf(msg,"ZZEA%1d%1c%03d%1c%03d%1c%03d%1c%03d;",
                                                             enable_rx_equalizer,
                                                             rx_equalizer[0]>=0? '+' : '-',
                                                             abs((int) rx_equalizer[0]),
                                                             rx_equalizer[1]>=0? '+' : '-',
                                                             abs((int) rx_equalizer[1]),
                                                             rx_equalizer[2]>=0? '+' : '-',
                                                             abs((int) rx_equalizer[2]),
                                                             rx_equalizer[3]>=0? '+' : '-',
                                                             abs((int) rx_equalizer[3]));
                                                     send_resp(client_sock,msg);
                                           
                                            } else {
                                                 if(len !=19) {
                                                     fprintf(stderr,"RIGCTL: ZZEA - incorrect number of arguments\n"); 
                                                     return;
                                                 }
                                                 double work_do;
                                                 enable_rx_equalizer = atoi(&cmd_input[2]); 
                                                 work_do = (double) atoi(&cmd_input[3]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEA - Preamp arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 rx_equalizer[0]  = work_do;

                                                 work_do = (double) atoi(&cmd_input[7]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEA - Low arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 rx_equalizer[1]  = work_do;

                                                 work_do  = (double) atoi(&cmd_input[11]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEA - Mid arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 rx_equalizer[2]  = work_do;

                                                 work_do  = (double) atoi(&cmd_input[15]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEA - Hi arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 rx_equalizer[3]  = work_do;

                                                 SetRXAGrphEQ(active_receiver->id, rx_equalizer);
                                            }
                                          }
        else if((strcmp(cmd_str,"EB")==0) && zzid_flag ==1)   {  
                                            // PiHPSDR - ZZEB - sets/reads TX EQ values
                                             if(len<=2) {
                                                    sprintf(msg,"ZZEB%1d%1c%03d%1c%03d%1c%03d%1c%03d;",
                                                             enable_tx_equalizer,
                                                             tx_equalizer[0]>=0? '+' : '-',
                                                             abs((int) tx_equalizer[0]),
                                                             tx_equalizer[1]>=0? '+' : '-',
                                                             abs((int) tx_equalizer[1]),
                                                             tx_equalizer[2]>=0? '+' : '-',
                                                             abs((int) tx_equalizer[2]),
                                                             tx_equalizer[3]>=0? '+' : '-',
                                                             abs((int) tx_equalizer[3]));
                                                     send_resp(client_sock,msg);
                                           
                                             } else {
                                                 if(len !=19) {
                                                     fprintf(stderr,"RIGCTL: ZZEB - incorrect number of arguments\n"); 
                                                     return;
                                                 }
                                                 double work_do;
                                                 enable_tx_equalizer = atoi(&cmd_input[2]); 

                                                 work_do = (double) atoi(&cmd_input[3]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEB - Preamp arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 tx_equalizer[0]  = work_do;

                                                 work_do = (double) atoi(&cmd_input[7]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEB - Low arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 tx_equalizer[1]  = work_do;

                                                 work_do  = (double) atoi(&cmd_input[11]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEB - Mid arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 tx_equalizer[2]  = work_do;

                                                 work_do  = (double) atoi(&cmd_input[15]); 
                                                 if((work_do < -12) || (work_do > 15)) {
                                                     fprintf(stderr,"RIGCTL: ZZEB - Hi arg out of range=%d \n",(int) work_do); 
                                                     return;
                                                 }
                                                 tx_equalizer[3]  = work_do;

                                                 SetTXAGrphEQ(transmitter->id, tx_equalizer);
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
        else if(strcmp(cmd_str,"FA")==0) { 
                                            // PiHPSDR - ZZFA - VFO A frequency 
                                            // TS2000 - FA - VFO A frequency
                                            // LEN=7 - set frequency
                                            // Next data will be rest of freq
                                            if(len == 13) { //We are receiving freq info
                                               new_freqA = atoll(&cmd_input[2]);
                                               long long *p_int=malloc(sizeof(long long));
                                               *p_int= new_freqA;
                                               g_idle_add(set_band,(gpointer) p_int);
                                               long long * freq_p;
                                               *freq_p=new_freqA;
                                               //setFrequency(new_freqA);
                                               //g_idle_add(gui_vfo_move_to,(gpointer)freq_p);
                                               return;
                                            } else {
                                               if(len==2) {
                                                  //sprintf(msg,"FA%011lld;",getFrequency());
                                                  if(zzid_flag == 0) {
                                                     sprintf(msg,"FA%011lld;",vfo[VFO_A].frequency);
                                                  } else {
                                                     sprintf(msg,"ZZFA%011lld;",vfo[VFO_A].frequency);
                                                  }
                                                  send_resp(client_sock,msg);
                                               }
                                            }
                                         }
        else if(strcmp(cmd_str,"FB")==0) {  
                                            // TS-2000 - FB - VFO B frequency
                                            // PiHPSDR - ZZFB - VFO B frequency 
                                            if(len==13) { //We are receiving freq info
                                               new_freqB = atoll(&cmd_input[2]); 
                                               set_freqB(new_freqB);
                                               //setFrequency(new_freqA);
                                               //g_idle_add(ext_vfo_update,NULL);
                                            } else if(len == 2) {
                                               if(zzid_flag == 0) {
                                                  sprintf(msg,"FB%011lld;",vfo[VFO_B].frequency);
                                               } else {
                                                  sprintf(msg,"ZZFB%011lld;",vfo[VFO_B].frequency);
                                               }
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
        else if((strcmp(cmd_str,"FD")==0) & (zzid_flag==0))  {  
                                            // TS-2000 - FD - Read the filter display dot pattern
                                            send_resp(client_sock,"FD00000000;");
                                          }
        else if((strcmp(cmd_str,"FH")==0) & (zzid_flag==1))  {
                                            // PiHPSDR - ZZFH - Set/Read Selected current DSP Filter Low High
                                            // P1 = (0)XXXX 5 chars --9999-09999
                                            if(len<=2) {
                                               sprintf(msg,"ZZFH%05d;",active_receiver->filter_high);
                                               send_resp(client_sock,msg);
                                            } else {
                                               // Insure that we have a variable filter selected!
                                               if(vfo[active_receiver->id].filter > 9) {
                                                  work_int = atoi(&cmd_input[2]); 
                                                  if((work_int >= -9999) && (work_int <=9999)) {
                                                      active_receiver->filter_high = work_int;
                                                  } else {
                                                     fprintf(stderr,"RIGCTL Warning ZZFH Value=%d out of range\n",work_int);
                                                  }
                                               } else {
                                                  fprintf(stderr,"RIGCTL Warning ZZFH not applied - VAR1/2 not selected\n");
                                               }
                                            } 
                                          }
        else if((strcmp(cmd_str,"FI")==0) & (zzid_flag==1))  {
                                            // PiHPSDR - ZZFI - Set/Read current DSP filter selected filter
                                            if(len<=2) {
                                               sprintf(msg,"ZZFI%02d;",vfo[active_receiver->id].filter);
                                               send_resp(client_sock,msg);
                                            } else {
                                               work_int = atoi(&cmd_input[2]);
                                               if((work_int >=0) && (work_int<=11)) {
                                                  g_idle_add(filter_select,(gpointer)(long)work_int);
                                               } else { 
                                                  fprintf(stderr,"RIGCTL: ERROR ZZFI incorrect filter value=%d",work_int);
                                                  send_resp(client_sock,"?;");
                                               }
                                            } 
                                          }
        else if((strcmp(cmd_str,"FL")==0) & (zzid_flag==1))  {
                                            // PiHPSDR - ZZFL - Set/Read Selected current DSP Filter Low 
                                            // P1 = (0)XXXX 5 chars --9999-09999
                                            if(len<=2) {
                                               sprintf(msg,"ZZFH%05d;",active_receiver->filter_low);
                                               send_resp(client_sock,msg);
                                            } else {
                                               // Insure that we have a variable filter selected!
                                               if(vfo[active_receiver->id].filter > 9) {
                                                  work_int = atoi(&cmd_input[2]); 
                                                  if((work_int >= -9999) && (work_int <=9999)) {
                                                      active_receiver->filter_low = work_int;
                                                  } else {
                                                     fprintf(stderr,"RIGCTL Warning ZZFH Value=%d out of range\n",work_int);
                                                  }
                                               } else {
                                                  fprintf(stderr,"RIGCTL Warning ZZFH not applied - VAR1/2 not selected\n");
                                               }
                                            } 
                                          }
        else if((strcmp(cmd_str,"FR")==0) && (zzid_flag == 0))  { 
                                            // TS-2000 - FR - Set/reads the extension menu
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
                                                    g_idle_add(ext_vfo_update,NULL);
                                                    g_idle_add(sliders_active_receiver_changed,NULL);
                                                 } 
                                             }
                                          }
        else if((strcmp(cmd_str,"FS")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - FS - Set/reads fine funct status -
                                             if(len <=2) {
                                                sprintf(msg,"FS%1d;",fine);
                                                send_resp(client_sock,msg);
                                             } else {
                                                int lcl_fine;
                                                lcl_fine = atoi(&cmd_input[2]); 
                                                if((lcl_fine >=0) && (lcl_fine <=1)) {
                                                    fine = lcl_fine;
                                                } else {
                                                   send_resp(client_sock,"?;");
                                                }
                                             }
                                          }
        else if(strcmp(cmd_str,"FT")==0)  { 
                                            // TS-2000 - FT - Set/Read Active Transmitter
                                            // PiHPSDR - ZZFT - Set/Read Active Transmitter
                                             if(len <=2) {
                                                   sprintf(msg,"FT%1d;",active_transmitter);
                                                   send_resp(client_sock,msg); 
                                             } else {
                                                 lcl_cmd = atoi(&cmd_input[2]);                
                                                 if((lcl_cmd ==0) ||(lcl_cmd == 1)) {
                                                    if(lcl_cmd != active_receiver->id) {
                                                        split = 1;
                                                    } else {
                                                        split = 0;
                                                    }
                                                    active_transmitter = lcl_cmd;
                                                    fprintf(stderr,"RIGCTL: FT New=%d\n",active_transmitter); 
                                                    g_idle_add(ext_vfo_update,NULL);
                                                 } else {
                                                    send_resp(client_sock,"?;");
                                                 }
                                             } 
                                          }  
        else if((strcmp(cmd_str,"FW")==0) & (zzid_flag==0)) {  
                                            // TS-2000 - FW - Sets/Read DSP receive filter width in hz 0-9999hz 
                                            // CW - legal values  50  80  100 150 200 300 400 500 600 1000 2000
                                            //      Pi HPSDR map  50  100 100 100 250 250 400 500 600 1000
                                            //                    25                                   750
                                             /*entry= (BANDSTACK_ENTRY *)
                                                         bandstack_entry_get_current();
                                             FILTER* band_filters=filters[entry->mode];
                                             FILTER* band_filter=&band_filters[entry->filter];*/
                                             BAND *band=band_get_band(vfo[active_receiver->id].band);
                                             BANDSTACK *bandstack=band->bandstack;
                                             BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[active_receiver->id].bandstack];
                                             FILTER* band_filters=filters[vfo[active_receiver->id].mode];
                                             FILTER* band_filter=&band_filters[vfo[active_receiver->id].filter];
                                             int cur_rad_mode= vfo[active_receiver->id].mode;
                                             // SDW1
                                             fprintf(stderr,"RIGCTL: FW - active recv mode =%d\n",cur_rad_mode);
                                             if((cur_rad_mode == modeCWL) || (cur_rad_mode == modeCWU)) {
                                               if(len <=2) {
                                                // CW filter high and low are always the same and the filter value is 2*filter val
                                                int filter_val = abs(band_filter->high * 2);
                                                fprintf(stderr,"RIGCTL: Filter Value=%d\n",filter_val);
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
                                                 //      Pi HPSDR map  50  100 100 100 250 250 400 500 600 1000
                                                 //                    25                                   750
                                                 work_int = atoi(&cmd_input[2]);
                                                 int f=5;
                                                 switch (work_int) {

                                                    case 50:  new_low = 25; new_high = 25; f=8; break;
                                                    case 80:  new_low = 50; new_high = 50; f=7; break;
                                                    case 100:  new_low = 50; new_high = 50; f=7; break;
                                                    case 150:  new_low = 50; new_high = 50; f=7; break;
                                                    case 200:  new_low = 125; new_high = 125; f=6; break;
                                                    case 300:  new_low = 125; new_high = 125; f=6; break;
                                                    case 400:  new_low = 200; new_high = 200; f=5; break;
                                                    case 500:  new_low = 250; new_high = 250; f=4; break;
                                                    case 600:  new_low = 300; new_high = 300; f=3; break;
                                                    case 1000:  new_low = 500; new_high = 500; f=0; break;
                                                    case 2000:  new_low = 500; new_high = 500; f=0; break;
                                                    default: new_low  = band_filter->low;
                                                             new_high = band_filter->high;
                                                             f=10;

                                                 }
                                               #ifdef  RIGCTL_DEBUG
                                                 fprintf(stderr,"RIGCTL: FW Filter new_low=%d new_high=%d f=%d\n",new_low,new_high,f);
                                               #endif
                                               // entry->filter = new_low * 2 ;
                                               //setFilter(new_low,new_high);
                                               //set_filter(active_receiver,new_low,new_high);
                                               //g_idle_add(ext_vfo_update,NULL);
                                               g_idle_add(ext_vfo_filter_changed,(gpointer)(long)f);
                                              }
                                            } else {
                                                /* Not in CW mode */
                                                send_resp(client_sock,"?;");
                                            }
                                          }  
        else if(strcmp(cmd_str,"GT")==0)  { 
                                            // TS-2000 - GT - Sets/Reads AGC Constant
                                            // PiHPSDR - ZZGT - AGC Constant 0-4 are legal values
                                           if(zzid_flag == 0) {
                                            // Sets/Read AGC constant status 000-020
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
                                                // Hamlib TS 2000 is broken here
                    
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
                                                g_idle_add(ext_vfo_update,NULL);

                                             }
                                            } else {
                                                if(len<=2) {
                                                   sprintf(msg,"ZZGT%03d;",active_receiver->agc);
                                                   send_resp(client_sock,msg);
                                                } else {
                                                   int lcl_agc = atoi(&cmd_input[2]);
                                                   if(lcl_agc >= AGC_OFF && lcl_agc<=AGC_FAST) {
                                                      active_receiver->agc;
                                                      g_idle_add(ext_vfo_update,NULL);
                                                   }
                                                }
                                           }
                                          }  
        else if(strcmp(cmd_str,"ID")==0)  { 
                                            // TS-2000 - ID - Read ID - Default to TS-2000 which is type 019.
                                            // PiHPSDR - ZZID - Read ID - 240, i.e. hamlib number
                                              if(zzid_flag == 0) {
                                                   sprintf(msg,"ID019;");
                                              } else {
                                                   sprintf(msg,"ZZID240;");
                                              } 
                                              send_resp(client_sock,msg);
                                          }
        else if((strcmp(cmd_str,"IF")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - IF - Reads Transceiver status
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
                                            sprintf(msg,"IF%011lld%04lld%06lld%01d%01d%01d",
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
                                                         transmitter->ctcss,  // P14
                                                         convert_ctcss(),  // P13
                                                         0); // P15
                                            send_resp(client_sock,msg);
                                         }
        else if(strcmp(cmd_str,"IS")==0)  { // Sets/Reas IF shift funct status
                                             if(len <=2) {
                                                send_resp(client_sock,"IS00000;");
                                             } 
                                          }  
        else if((strcmp(cmd_str,"KS")==0) && (zzid_flag == 0) || 
                (strcmp(cmd_str,"CS")==0) && (zzid_flag==1))  { 
                                            // TS-2000 - KS - Set/Reads keying speed 0-060 max
                                            // PiHPSDR - ZZCS - Sets/Reads Keying speed
                                             if(len <=2) {
                                                if(zzid_flag ==0) {
                                                    sprintf(msg,"KS%03d;",cw_keyer_speed);
                                                } else {
                                                    sprintf(msg,"ZZCS%02d;",cw_keyer_speed);
                                                }
                                                send_resp(client_sock,msg);
                                             } else {
                                                int key_speed;
                                                key_speed = atoi(&cmd_input[2]);
                                                fprintf(stderr,"RIGCTL: Set CW speed=%d",key_speed);
                                                if(key_speed >= 1 && key_speed <= 60) {
                                                   cw_keyer_speed=key_speed;
                                                   g_idle_add(ext_vfo_update,NULL);
                                                } else {
                                                   send_resp(client_sock,"?;");
                                                } 
                                            }
                                          }  
        else if((strcmp(cmd_str,"KY")==0) && (zzid_flag == 0)) { 
                                            // TS-2000 - KY - Convert char to morse code - not supported
                                             int index = 2;
                                             long delay = 1200000L/(long)cw_keyer_speed; // uS
                                             fprintf(stderr,"RIGCTL: KY DELAY=%ld, cw_keyer_speed=%d cw_keyer_weight=%d\n",delay,cw_keyer_speed,cw_keyer_weight); 
                                             if(len <=2) {
                                                send_resp(client_sock,"KY0;");
                                             } else {
                                               // Set CW BUSY flag
                                               g_mutex_lock(&mutex_c->m);
                                               cw_busy = 1;
                                               g_mutex_unlock(&mutex_c->m);
                                               while((cmd_input[index] != '\0') && (!cw_reset)) {
                                                  //fprintf(stderr,"send=%c\n",cmd_input[index]);
                                                  rigctl_send_cw_char(cmd_input[index]);
                                                  //fprintf(stderr,"RIGCTL: 0 mox=%d\n",mox);
                                                  index++;
                                               } 
                                               fprintf(stderr,"RIGCTL: KY - Done sending cw\n");
                                               fprintf(stderr,"RIGCTL: 1 mox=%d\n",mox);
                                             } 
                                             g_mutex_lock(&mutex_c->m);
                                             cw_busy = 0;
                                             g_mutex_unlock(&mutex_c->m);
                                             fprintf(stderr,"RIGCTL: 2 mox=%d\n",mox);
                                          }  
        else if(strcmp(cmd_str,"LK")==0)  { 
                                            // TS-2000 - LK - Set/read key lock function status
                                            // PiHPSDR - ZZLK - Set/read key lock function status
                                             if(len <=2) {
                                                sprintf(msg,"LK%01d%01d;",locked,locked);
                                                send_resp(client_sock,msg);
                                             }  else {
                                                  if((cmd_input[2] == '1') || (cmd_input[3]=='1'))  {
                                                      locked = 1;
                                                  } else {
                                                      locked = 0;
                                                  }
                                                  g_idle_add(ext_vfo_update,NULL);
                                             }
                                          }  
        else if(strcmp(cmd_str,"LM")==0)  { 
                                            // TS-2000 - LM - Set/Read DRU 3A keyer recording status - not supported
                                             if(len <=2) {
                                                send_resp(client_sock,"?;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"LT")==0)  { 
                                            // TS-2000 - LT - Set/read Alt function - not supported
                                             if(len <=2) {
                                                send_resp(client_sock,"?;");
                                             } 
                                          }  
        else if(strcmp(cmd_str,"MC")==0) {  
                                            // TS-2000 - MC - Recalls or reads memory channel - not supported
                                             if(len <=2) {
                                               send_resp(client_sock,"?;"); // Link this to band stack at some point
                                             }
                                         }
        else if((strcmp(cmd_str,"MD")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - MD - Set/Read Mode
                                            // Mode - digit selects mode
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
                                            // BUG - kills the system when there is some
                                            // GPIO activity and Mode sets occur. Used twittling the
 					    // frequency along with setting mode between USB/LSB with
					    // flrig. Tried doing the g_idle_add trick - but don't know the
					    // the magic to get that to compile without warnings 
                                            //setMode(entry->mode);
                                            set_mode(active_receiver,entry->mode);
                                            // Moved the ext_vfo_update down after filter updated. (John G0ORX)
                                            //g_idle_add(ext_vfo_update,NULL);
                                            
                                            FILTER* band_filters=filters[entry->mode];
                                            FILTER* band_filter=&band_filters[entry->filter];
                                            //setFilter(band_filter->low,band_filter->high);
                                            set_filter(active_receiver,band_filter->low,band_filter->high);
                                            /* Need a way to update VFO info here..*/
                                            g_idle_add(ext_vfo_update,NULL);
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
		else if((strcmp(cmd_str,"MD")==0) && (zzid_flag == 1)) {  
						    // PiHPSDR - ZZMD - Set/Read Modulation Mode
						    if(len <= 2) { // Set Mode
						       sprintf(msg,"ZZMD%1d;",vfo[active_receiver->id].mode);
						       send_resp(client_sock,msg);
						    } else {
						       work_int = atoi(&cmd_input[2]);
						       if((work_int >=0) && (work_int <=11)) {
							  // Other stuff to switch modes goes here..
							  // since new_mode has the interpreted command in 
							  // in it now.
							  entry= (BANDSTACK_ENTRY *) 
							     bandstack_entry_get_current();
							  entry->mode=work_int;
							  set_mode(active_receiver,entry->mode);
							  FILTER* band_filters=filters[entry->mode];
							  FILTER* band_filter=&band_filters[entry->filter];
							  //setFilter(band_filter->low,band_filter->high);
							  set_filter(active_receiver,band_filter->low,band_filter->high);
							  /* Need a way to update VFO info here..*/
							  g_idle_add(ext_vfo_update,NULL);
						       } else {
							  fprintf(stderr,"RIGCTL: Error - ZZMD - Illegal cmd received=%d",work_int);
						       }
						    } 
						 }
		else if((strcmp(cmd_str,"MF")==0) && (zzid_flag == 0)) {  
						    // TS-2000 - MF - Set/read menu A/B - not supported
						     if(len <=2) {
						       send_resp(client_sock,"?;"); 
						     }
						 }
		else if(strcmp(cmd_str,"MG")==0) {  
						    // PiHPSDR - ZZMG - Mic Gain P1=+/-P2=2digits (-10-50)
						    // TS-2000 - MG - Mike Gain - 3 digits
						    if(zzid_flag == 0) {
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
						    } else {
						       if(len <=2) {
							  sprintf(msg,"ZZMG%c%02d;",mic_gain>=0 ? '+' : '-',(int)mic_gain);
							  send_resp(client_sock,msg);
						       } else {
							  int new_vol = atoi(&cmd_input[2]);                
							  double *p_mic_gain=malloc(sizeof(double));
							  *p_mic_gain=new_vol;
							  g_idle_add(update_mic_gain,(void *)p_mic_gain);
						       }
						    }
						 }
		else if((strcmp(cmd_str,"ML")==0) && (zzid_flag == 0)) {  
						    // TS-2000 - ML - Set/read the monitor function level - not supported
						     if(len <=2) {
						       send_resp(client_sock,"?;"); 
						     }
						 }
		else if(strcmp(cmd_str,"MO")==0) {  
						    // TS-2000 - MO - Set Monitor on/off - not supported
						     fprintf(stderr,"RIGCTL: Command seen\n"); 

						     if(len <=2) {
						       send_resp(client_sock,"?;"); 
						     }
						 }
		else if((strcmp(cmd_str,"MR")==0) && (zzid_flag == 0)) {  
						     fprintf(stderr,"RIGCTL: Command seen\n"); 
	 
						     // TS-2000 - MR - Read Memory Channel data
						     sprintf(msg,"MR%1d%1d%02d%011lld%1d%1d%1d%02d%02d%03d%1d%1d%09d%02d%1d%08d;",
							      0, // P1 - Rx Freq - 1 Tx Freq
							      0, // P2 Bankd and channel number -- see MC comment
							      0, // P3 - see MC comment 
							      rigctl_getFrequency(), // P4 - frequency
							      rigctlGetMode(), // P5 - Mode
							      locked, // P6 - Locked status
							      transmitter->ctcss, // P7 - O-off, 1-tone, 2-CTCSS, 3 =DCS
							      convert_ctcss(),    // P8 - Tone Number - see page 35
							      convert_ctcss(),    // P9 - CTCSS tone number - see CN command
							      0, // P10 - DCS code - see QC command 
							      0, // P11 - Reverse status
							      0, // P12 - Shift status - see OS command
							      0, // P13 - Offset freq - see OS command
							      0, // P14 - Step Size - see ST command
							      0, // P15 - Memory Group Number (0-9)
							      0);  // P16 - Memory Name - 8 char max
						       send_resp(client_sock,msg);
						       //send_resp(client_sock,"?;");
						 }
		else if((strcmp(cmd_str,"MT")==0) && (zzid_flag == 1)) {  
						    fprintf(stderr,"RIGCTL: Command seen\n");
						    // PiHPSDR - ZZMT - Read TX Meter Mode
						    // Mapped from PSDR
						    // 0= ALC Peak
						    // 1= ALC Average
						    // 2= ALG Gain
						      if(len<=2) { 
							  switch(alc) {
							      case TXA_ALC_PK:  work_int = 0; break;
							      case TXA_ALC_AV:  work_int = 1; break;
							      case TXA_ALC_GAIN: work_int = 2; break;
							      default: work_int = 0;
							  } 
							  sprintf(msg,"ZZMT%01d;",work_int);
							  send_resp(client_sock,msg);
						      } else {
							  work_int = atoi(&cmd_input[2]);
							  switch (work_int)  {
							      case 0: work_int = TXA_ALC_PK;
							      case 1: work_int = TXA_ALC_AV;
							      case 2: work_int = TXA_ALC_GAIN;
							      default: work_int = TXA_ALC_PK;
							      double *p_alc=malloc(sizeof(double));
							      *p_alc=work_int;
							      g_idle_add(set_alc,(gpointer )p_alc);
								
							  }
						      }
						 }
		else if(strcmp(cmd_str,"MU")==0) {   if(zzid_flag == 1) {  
						    fprintf(stderr,"RIGCTL: Command seen\n");
						    // PiHPSDR - ZZMU - Sets or Read the MultiRX status
							 if(len<=2) {
							 fprintf(stderr,"ZZMU =%d\n",receivers);
							 sprintf(msg,"ZZMU%1d;",receivers);
							 send_resp(client_sock,msg);
							 } else {
							   int lcl_rcvr = atoi(&cmd_input[2]);
							   fprintf(stderr,"ZZMU Set=%d\n",lcl_rcvr);
							   /*if (lcl_rcvr <1 || lcl_rcvr >2) {
							      fprintf(stderr,"RIGCTL: ZZMU - illegal recevier number\n");
							      return;
							   }*/
							   g_idle_add(ext_radio_change_sample_rate,(gpointer)(long)lcl_rcvr);
							   g_idle_add(ext_vfo_update,NULL);
							 }
	 
						     } else { 
						    // TS-2000 - MU - Set/Read Memory Group Data - not supported
							 if(len <=2) {
							   send_resp(client_sock,"MU0000000000;"); 
							 }
						     }
						 }
		else if((strcmp(cmd_str,"MW")==0) && (zzid_flag == 0)) {  
						    fprintf(stderr,"RIGCTL: Command seen\n");
						    // TS-2000 - MW - Store Data to Memory Channel -not supported
						 }
		else if(strcmp(cmd_str,"NB")==0) {  
						    // PiHPSDR - ZZNB - Set/Read Noise Blanker func status (on/off)
						    // TS-2000 - NB - Set/Read Noise Blanker func status (on/off)
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
						       g_idle_add(ext_vfo_update,NULL);
						     }
						 }
		else if((strcmp(cmd_str,"NE")==0) && (zzid_flag == 1)) {  
						    // PiHPSDR - ZZNE - NPS AE FIlter - DSP Menu
						    //   P1 - 0=Off
						    //   P1 - 1=On
						       if(len<=2) {
							 sprintf(msg,"ZZNE%1d;",(int) active_receiver->nr2_ae); 
							 send_resp(client_sock,msg);
						       } else {
							 work_int = atoi(&cmd_input[2]);
							 if(work_int >=0 && work_int <=1) {
							   active_receiver->nr2_ae = work_int; 
							   SetRXAEMNRaeRun(active_receiver->id, active_receiver->nr2_ae);
							 } else {
							   fprintf(stderr,"RIGCTL: ZZNE - ERROR illegal value=%d s/b 0 to 2\n",work_int);
							   send_resp(client_sock,"?;");
							 }
						       }
						   }
		else if((strcmp(cmd_str,"NG")==0) && (zzid_flag == 1)) {  
						    // PiHPSDR - ZZNG - NR Pre AGC/Post AGC- DSP Menu
						       if(len<=2) {
							 sprintf(msg,"ZZNG%1d;",(int) active_receiver->nr_agc); 
							 send_resp(client_sock,msg);
						       } else {
							 work_int = atoi(&cmd_input[2]);
							 if(work_int >=0 && work_int <=1) {
							     active_receiver->nr_agc = atoi(&cmd_input[2]);
							     SetRXAEMNRPosition(active_receiver->id, active_receiver->nr_agc);
							 } else { 
							   fprintf(stderr,"RIGCTL: ZZNG - illegal value=%d s/b 0 or 1\n",work_int);
							   send_resp(client_sock,"?;");
							 }
						       }
						   }

		else if((strcmp(cmd_str,"NM")==0) && (zzid_flag == 1)) {  
						    // PiHPSDR - ZZNM - NR2 Gain Method - DSP Menu
						    //   P1 - 0=Linear
						    //   P1 - 1=Log
									//   P1 - 2=Gamma
						       if(len<=2) {
							 sprintf(msg,"ZZNM%1d;",(int) active_receiver->nr2_gain_method); 
							 send_resp(client_sock,msg);
						       } else {
							 work_int = atoi(&cmd_input[2]);
							 if(work_int >=0 && work_int <=2) {
							   active_receiver->nr2_gain_method = work_int; 
							   SetRXAEMNRgainMethod(active_receiver->id, active_receiver->nr2_gain_method);
							 } else {
							   fprintf(stderr,"RIGCTL: ZZNM - illegal value=%d s/b 0 to 2\n",work_int);
							   send_resp(client_sock,"?;");
							 }
						       }
						 }
		else if(strcmp(cmd_str,"AB")==0)  { fprintf(stderr,"RIGCTL: Command seen\n"); }
		else if((strcmp(cmd_str,"NP")==0) && (zzid_flag == 1)) {  
						    // PiHPSDR - ZZNP - NPS Method - DSP Menu
						    //   P1 - 0=OSMS
						    //   P1 - 1=MMSE
						       if(len<=2) {
							 sprintf(msg,"ZZNP%1d;",(int) active_receiver->nr2_npe_method); 
							 send_resp(client_sock,msg);
						       } else {
							 work_int = atoi(&cmd_input[2]);
							 if(work_int >=0 && work_int <=1) {
							   active_receiver->nr2_npe_method = work_int; 
							   SetRXAEMNRnpeMethod(active_receiver->id, active_receiver->nr2_npe_method);
							 } else {
							   fprintf(stderr, "RIGCTL: ZZNP - ERROR illegal value=%d s/b 0 to 2\n",work_int);
							   send_resp(client_sock,"?;");
							 }
						       }
						 }
		else if(strcmp(cmd_str,"NL")==0) {if(zzid_flag == 0) { 
						    // TS-2000 - NL - Set/Read Noise Reduction  Level - Not Supported
						     if(len <=2) {
						       send_resp(client_sock,"?;"); 
						     }
						  } else {
						    // PiHPSDR - ZZNL - AGC Hang Threshold - DSP Menu slider
						     //         P1P1P1 - 0 - 100
						     if(len <=2) {
							sprintf(msg,"ZZNL%03d;",(int) active_receiver->agc_hang_threshold);
							send_resp(client_sock,msg);
						     } else {
							work_int = atoi(&cmd_input[2]);
							if((work_int >= 0) && (work_int<=100)) {
							   active_receiver->agc_hang_threshold = work_int;
							   if(active_receiver->agc==AGC_LONG || active_receiver->agc==AGC_SLOW) {
							       SetRXAAGCHangThreshold(active_receiver->id, (int)active_receiver->agc_hang_threshold);
							   }
							}
						     }
						  }
						 }
		else if(strcmp(cmd_str,"NR")==0) {  
						    // TS-2000 - NR - Set/Read Noise Reduction function status
						    // PiHPSDR - ZZNR - Set/Read Noise Reduction function status
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
                                              
                                              if (active_receiver->nr==1 & active_receiver->nr2==0) { 
                                                send_resp(client_sock,"NR1;"); 
                                              } else if (active_receiver->nr==1 & active_receiver->nr2==1) { 
                                                send_resp(client_sock,"NR2;"); 
                                              } else {
                                                send_resp(client_sock,"NR0;"); 
                                              }
                                            } else {
                                              if((atoi(&cmd_input[2]) >=0) && (atoi(&cmd_input[2]) <=2)) {
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
                                                 g_idle_add(ext_vfo_update,NULL);
                                              } else {
                                                send_resp(client_sock,"?;"); 
                                              }
                                            } 
                                         }
        else if(strcmp(cmd_str,"NT")==0) {  
                                            // TS-2000 - NT - Set/Read autonotch function - 
                                            // PiHPSDR - ZZNT - Sets/reads ANF status
                                             if(len <=2) {
                                               sprintf(msg,"NT%1d;",active_receiver->anf);
                                               send_resp(client_sock,msg);
                                             } else {
                                               if((atoi(&cmd_input[2]) >=0) && (atoi(&cmd_input[2]) <=1)) {
                                                  if(cmd_input[2] == '0') { // Clear ANF
                                                    active_receiver->anf = 0;
                                                  } else { // Set ANF
                                                    active_receiver->anf = 1;
                                                  }
                                               } else {
                                                  send_resp(client_sock,"?;"); 
                                               }
                                             }
                                             SetRXAANFRun(active_receiver->id, active_receiver->anf);
                                             g_idle_add(ext_vfo_update,NULL);
                                         }
        else if((strcmp(cmd_str,"OA")==0) && (zzid_flag ==1)) {  
                                            // PiHPSDR - ZZOA - Set/Read RX Antenna by band
                                            //           P1P1P1 - Band
                                            //           P2 - 1,2,3,EXT1,EXT2, XVTR
                                             int int_band;
                                             int b;
                                             if(len<=5) {
                                                int_band = atoi(&cmd_input[2]);
                                                b = lookup_band(int_band);
                                                BAND *band=band_get_band(b);
                                                work_int = band->alexRxAntenna;
                                                sprintf(msg,"ZZOA%03d%1d;",int_band,work_int);
                                                send_resp(client_sock,msg);
                                             } else {
                                                char lcl_char = cmd_input[5];
                                                cmd_input[5] = '\0';
                                                int_band = atoi(&cmd_input[2]);
                                                b = lookup_band(int_band);

                                                cmd_input[5] = lcl_char;
                                                work_int = atoi(&cmd_input[5]);
                                                if(work_int >=0 && work_int <=5) {
                                                   fprintf(stderr,"RIGCTL ZZOA Band=%d Val=%d\n",int_band,work_int);
                                                   BAND *band=band_get_band(b);
                                                   band->alexRxAntenna = work_int;
                                                } else {
                                                   fprintf(stderr,"RIGCTL ZZOA illegal val Band=%d Val=%d\n",int_band,work_int);
                                                }
                                             }
                                         }
        else if((strcmp(cmd_str,"OB")==0) && (zzid_flag ==1)) {  
                                            // PiHPSDR - ZZOB - Set/Read TX Antenna by band
                                            //           P1P1P1 - Band
                                            //           P2 - 1,2,3
                                             int int_band;
                                             int b ;
                                             if(len<=5) {
                                                int_band = atoi(&cmd_input[2]);
                                                b = lookup_band(int_band);
                                                BAND *band=band_get_band(b);
                                                work_int = band->alexTxAntenna;
                                                sprintf(msg,"ZZOB%03d%1d;",int_band,work_int);
                                                send_resp(client_sock,msg);

                                             } else {
                                                char lcl_char = cmd_input[5];
                                                cmd_input[5] = '\0';
                                                int_band = atoi(&cmd_input[2]);
                                                b = lookup_band(int_band);

                                                cmd_input[5] = lcl_char;
                                                work_int = atoi(&cmd_input[5]);
                                                if(work_int >=0 && work_int <=2) {
                                                   fprintf(stderr,"RIGCTL ZZOB Band=%d Val=%d\n",int_band,work_int);
                                                   BAND *band=band_get_band(b);
                                                   band->alexTxAntenna = work_int;
                                                } else {
                                                   fprintf(stderr,"RIGCTL ZZOB illegal val Band=%d Val=%d\n",int_band,work_int);
                                                }
                                             }
                                         }
        else if((strcmp(cmd_str,"OF")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - OF - Set/Read Offset freq (9 digits - hz) -not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"OF000000000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        /* Not Supported */
        else if((strcmp(cmd_str,"OI")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - OI - Read Memory Channel Data - not supported
                                             /*if(len <=2) {
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
                                              */
                                               send_resp(client_sock,"?;");
                                         }
        else if(strcmp(cmd_str,"OS")==0) {  
                                            // TS-2000 - OS - Set/Read Offset freq (9 digits - hz) -not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"OS0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PA")==0) { 
                                            // TS-2000 - PA - Set/Read Preamp function status
                                            // PiHPSDR - ZZPA - Set/Read Preamp function status
                                             if(len <=2) {
                                               sprintf(msg,"PA%1d%1d;",active_receiver->preamp,active_receiver->preamp);
                                               send_resp(client_sock,msg);
                                             } else {
                                                work_int = atoi(&cmd_input[2]);
                                                if((work_int >= 0) && (work_int <= 1)) {
                                                    active_receiver->preamp = work_int;  
                                                } else {
                                                    send_resp(client_sock,"?;");
                                                }
                                             }
                                         }
        else if(strcmp(cmd_str,"PB")==0) { 
                                            // TS-2000 - PB - DRU-3A Playback status - not supported
                                             if(len <=2) {
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"PC")==0) {  
                                            // TS-2000 - PC - Set/Read Drive Power output
                                            // PiHPSDR - ZZPC - Set/Read Drive Power output
                                            if(len<=2) {
                                              sprintf(msg,"PC%03d;",(int) transmitter->drive);
                                              send_resp(client_sock,msg); 
                                            } else {
                                              int lcl_pc = atoi(&cmd_input[2]); 
                                              fprintf(stderr,"RIGCTL: PC received=%d\n",lcl_pc);
                                              if((lcl_pc >=0) && (lcl_pc<=100)) {
                                                  // Power Control - 3 digit number- 0 to 100
                                                  //Scales to 0.00-1.00
                                                
                                                  double drive_val = 
                                                      (double)(atoi(&cmd_input[2])); 
                                                  // setDrive(drive_val);
                                                  double *p_drive=malloc(sizeof(double));
                                                  *p_drive=drive_val;
                                                  g_idle_add(update_drive,(gpointer)p_drive);
                                                  //set_drive(drive_val);
                                              } else {
                                                  fprintf(stderr,"RIGCTL: PC received=%d - Illegal value\n",lcl_pc);
                                                  send_resp(client_sock,"?;");
                                              }
                                            }
                                         }
        else if((strcmp(cmd_str,"PI")==0) && (zzid_flag == 0)) 
                                         {  
                                            // TS-2000 - PI - Store the programmable memory channel - not supported
                                         }
        else if(strcmp(cmd_str,"PK")==0) {  
                                            // TS-2000 - PK - Reads the packet cluster data - not supported
                                            //  send_resp(client_sock,"PK000000000000000000000000000000000000000000000000;"); 
                                             send_resp(client_sock,"?;"); 
                                            
                                         }
        else if((strcmp(cmd_str,"PL")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - PL - Set/Read Speech processor level 
                                            // PL 000 000 ;
                                            // P1 000 - min-100 max
                                            // P2 000 - min - 100 max
                                             double lcl_comp_level;
                                             if(len <=2) {
                                             //  send_resp(client_sock,"PL000000;"); 
                                                lcl_comp_level = 100 *(transmitter->compressor_level)/20;
                                                sprintf(msg,"PL%03d000;",(int)lcl_comp_level);
                                                send_resp(client_sock,msg); 
                                             } else {
                                                // Isolate 1st command
                                                cmd_input[5] = '\0';
                                                lcl_comp_level = 20.0 *(((double)atoi(&cmd_input[2]))/100.0);
                                                fprintf(stderr,"RIGCTL - PR new level=%f4.1",lcl_comp_level);
                                                transmitter_set_compressor_level(transmitter,lcl_comp_level);
                                                //transmitter->compressor_level = lcl_comp_level;
                                             }
                                         }
        else if((strcmp(cmd_str,"PM")==0) && ( zzid_flag == 0)) {  
                                            // TS-2000 - PM - Recalls the Programmable memory - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"PM0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"PR")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - PR - Sets/reads the speech processor function on/off 
                                            // 0 - off, 1=on
                                             if(len <=2) {
                                               sprintf(msg,"PR%01d;",transmitter->compressor);
                                               send_resp(client_sock,msg);
                                             } else {
                                               transmitter_set_compressor(transmitter,atoi(&cmd_input[2]));
                                               //transmitter->compressor = atoi(&cmd_input[2]);
                                             }
                                         }
        else if(strcmp(cmd_str,"PS")==0) {  
                                            // PiHPSDR - ZZPS - Sets/reads Power on/off state- always on
                                            // TS-2000 - PS - Sets/reads Power on/off state
                                            // 0 - off, 1=on
                                             if(len <=2) {
                                               send_resp(client_sock,"PS1;"); // Lets pretend we're powered up ;-) 
                                             }
                                         }
        else if(strcmp(cmd_str,"PZ")==0) {   if(zzid_flag == 1) {  
                                            // PiHPSDR - ZZPZ - Sets/Reads  Radio Sample Rate
                                            // Valid values are 048000, 096000, 192000, 384000
                                                 fprintf(stderr,"ZZPZ command\n");
                                                 if(len<=2) {
                                                 sprintf(msg,"ZZPZ%06d;",active_receiver->sample_rate);
                                                 send_resp(client_sock,msg);
                                                 } else {
                                                   int lcl_rate = atoi(&cmd_input[2]);
                                                   fprintf(stderr,"ZZPZ Set=%d\n",lcl_rate);
                                                   if (lcl_rate !=48000 && lcl_rate >=96000  &&
                                                       lcl_rate !=192000 && lcl_rate != 384000) {
                                                      fprintf(stderr,"RIGCTL: ZZMU - illegal recevier number\n");
                                                      return;
                                                   }
                                                   
                                                   g_idle_add(ext_radio_change_sample_rate,(gpointer)(long)lcl_rate);
                                                   g_idle_add(ext_vfo_update,NULL);
                                                 }
                                              } 
                                         }
        else if(strcmp(cmd_str,"QC")==0) {  
                                            // TS-2000 - QC - Sets/reads DCS code - not supported
                                            // Codes numbered from 000 to 103.
                                             if(len <=2) {
                                               //send_resp(client_sock,"QC000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"QI")==0) {  
                                            // TS-2000 - QI - Store the settings in quick memory - not supported
                                         }
        else if(strcmp(cmd_str,"QR")==0) {  
                                            // TS-2000 - QR - Send the Quick memory channel data - not supported
                                            // P1 - Quick mem off, 1 quick mem on
                                            // P2 - Quick mem channel number
                                             if(len <=2) {
                                               //send_resp(client_sock,"QR00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"RA")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - RA - Sets/reads Attenuator function status
                                            // 00-off, 1-99 -on - Main and sub receivers reported
                                            int lcl_attenuation;
                                            float calc_atten;
                                             if(len <=2) {
                                               //send_resp(client_sock,"RA0000;"); 
                                               calc_atten = round((float)active_receiver->attenuation*99.0/30.0);
                                               if(calc_atten > 99.0) {
                                                  calc_atten = 99.0;
                                               } 
                                               fprintf(stderr,"RIGCTL: calc_atten=%6.3f\n",calc_atten);
                                               sprintf(msg,"RA%02d%02d;",(int)calc_atten,(int)calc_atten); 
                                               send_resp(client_sock,msg);
                                             } else {
                                                lcl_attenuation = atoi(&cmd_input[2]); 
                                                if((lcl_attenuation >=0) && (lcl_attenuation <= 99)) {
                                                  lcl_attenuation = (int)((float)lcl_attenuation * 30.0/99.0);
                                                  fprintf(stderr,"RIGCTL: lcl_att=%d\n",lcl_attenuation);
                                                  //set_attenuation(lcl_attenuation);
                                                  set_attenuation_value((double) lcl_attenuation);
                                                  g_idle_add(ext_vfo_update,NULL);
                                                } else {
                                                  send_resp(client_sock,"?;"); 
                                                }
                                             }
                                         }
        else if(strcmp(cmd_str,"RC")==0) {  
                                            // TS-2000 - RC - Clears the RIT offset freq
                                            // PiHPSDR - ZZRC - Clears the RIT offset freq
                                            vfo[active_receiver->id].rit = 0;
                                            g_idle_add(ext_vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"RD")==0) {  // 
                                            // TS-2000 - RD - Move the RIT offset freq down - P1=rit_increment! 
                                            //                P1=No Argument - decrement frequency
                                            //                P1=NonZero - load rit offset
                                            //                FS controls fine variable 1 or 0. rit increment 1 hz or 10hz
                                            // PiHPSDR - ZZRD - Move the RIT offset freq down by rit_increment
                                             int lcl_rit;
                                             int lcl_rit_increment = fine ? 1 : 10;
                                             if(len <=2) {
                                               vfo[active_receiver->id].rit = vfo[active_receiver->id].rit-lcl_rit_increment;
                                             } else {
                                                if(len > 3) {
                                                   lcl_rit = atoi(&cmd_input[2]);   
                                                   if((lcl_rit >=0) &&(lcl_rit <=99999)) {
                                                       vfo[active_receiver->id].rit = -lcl_rit;
                                                   } else {
                                                       send_resp(client_sock,"?;");
                                                   }
                                                } 
                                             }
                                             g_idle_add(ext_vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"RG")==0) {  
                                            // TS-2000 - RG - RF Gain Control
                                            // PiHPSDR - ZZRG - AGC Gain Control -20 to 120 legal range
                                            // TS 2000 settings 0-255 value scaled to -20 to 120 range
                                            if(zzid_flag == 0) {
                                              if(len>4) { // Set Audio Gain
                                                 int base_value = atoi(&cmd_input[2]);
                                                 double new_gain = roundf(((((double) base_value+1)/256.0) * 141.0))- 21.0; 
                                                 //set_agc_gain(new_gain);               
                                                 double *p_gain=malloc(sizeof(double));
                                                 *p_gain=new_gain;
                                                 g_idle_add(ext_update_agc_gain,(gpointer)p_gain);
                                              } else { // Read Audio Gain
                                                 double lcl_gain = (active_receiver->agc_gain+21.0)/141.0;
                                                 fprintf(stderr,"RIGCTL: RG agc=%6.3f lcl_gain=%6.3f comp gain=%d\n",
                                                                         active_receiver->agc_gain,lcl_gain,(int)((256.0*lcl_gain)-1));
                                                 sprintf(msg,"RG%03d;",(int)((256.0 * lcl_gain) -1));
                                                 send_resp(client_sock,msg);
                                              }
                                            } else {
                                              // Pi HPSDR  version
                                              if(len <= 2) {
                                                 sprintf(msg,"RG%03d;",(int)active_receiver->agc_gain);
                                                 send_resp(client_sock,msg);
                                                 
                                              } else {
                                                 work_int = atoi(&cmd_input[2]);
                                                 if((work_int >= -20) && (work_int <=120)) {
                                                    double *p_gain=malloc(sizeof(double));
                                                    *p_gain=(double) work_int;
                                                    g_idle_add(ext_update_agc_gain,(gpointer)p_gain);
                                                 } else {
                                                    fprintf(stderr,"RIGCTL: ZZRG ERROR - Illegal AGC Value=%d\n", work_int);
                                                    send_resp(client_sock,"?;");
                                                 }
                                              }
                                            }
                                            g_idle_add(ext_vfo_update,NULL);
                                         }
        else if((strcmp(cmd_str,"RL")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - RL - Set/read Noise reduction level - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"RL00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"RM")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - RM - Set/read Meter function - not supported
                                            // P1- 0, unsel, 1 SWR, 2 Comp, 3 ALC
                                            // P2 - 4 dig - Meter value in dots - 000-0030
                                             if(len <=2) {
                                               //send_resp(client_sock,"RM00000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"RT")==0) {  
                                            // TS-2000 - RT - Set/read the RIT function status
                                            // PiHPSDR - ZZRT - Set/read the RIT function status
                                             int lcl_rit_val;
                                             if(len <=2) {
                                               sprintf(msg,"RT%01d;",rit_on());
                                               send_resp(client_sock,msg); 
                                             } else {
                                                lcl_rit_val = atoi(&cmd_input[2]);
                                                if((lcl_rit_val >=0) && (lcl_rit_val <=1)) {
                                                   vfo[active_receiver->id].rit = lcl_rit_val;
                                                } else {
                                                   send_resp(client_sock,"?;"); 
                                                }
                                             }
                                         }
        else if(strcmp(cmd_str,"RU")==0) {  
                                            // TS-2000 - RU - Set/move RIT frequency up
                                            // PiHPSDR - ZZRU - Set/move RIT frequency up
                                             int lcl_incr;
                                             if(len <=2) {
                                               lcl_incr = fine ? 1 : 10;
                                               vfo[active_receiver->id].rit = vfo[active_receiver->id].rit+lcl_incr;
                                             } else {
                                                if(len > 3) {
                                                   lcl_incr = atoi(&cmd_input[2]);    
                                                   if((lcl_incr >=0) && (lcl_incr <= 99999)) {
                                                      vfo[active_receiver->id].rit = lcl_incr;
                                                   } else {
                                                      send_resp(client_sock,"?;"); 
                                                   }
                                                } 
                                             }
                                             g_idle_add(ext_vfo_update,NULL);
                                         }
        else if(strcmp(cmd_str,"RX")==0) {  
                                            // TS-2000 - RX - Unkey Xmitter
                                            // PiHPSDR - ZZRX - Unkey Xmitter
                                            //setMox(0);
                                            if(!((vfo[active_receiver->id].mode == modeCWL) ||
                                                 (vfo[active_receiver->id].mode == modeCWU))) {
                                               fprintf(stderr, "RIGCTL: Inside RX command\n");
                                               mox_state=0;
                                               g_idle_add(ext_mox_update,(gpointer)(long)mox_state); 
                                               g_idle_add(ext_vfo_update,NULL);
                                            } else {
                                               // Clear External MOX in CW mode
                                               g_idle_add(ext_mox_update,(gpointer)(long)0); // Turn on xmitter (set Mox)
                                            }
                                         }
        else if(strcmp(cmd_str,"SA")==0) {  
                                            // TS-2000 - SA - Set/reads satellite mode status - not supported
                                            // 0-main, 1=sub
                                             if(len <=2) {
                                               //send_resp(client_sock,"SA000000000000000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SB")==0) {  
                                            // TS-2000 - SB - Set/read the SUB TF-W status - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"SB0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SC")==0) {  
                                            // TS-2000 - SC - Set/read the Scan function status - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"SC0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"SD")==0) && (zzid_flag == 0) ||  
                (strcmp(cmd_str,"CD")==0) && (zzid_flag ==1)) {  
                                            // PiHPSDR - ZZCD - Set/Read CW Keyer Hang Time
                                            // TS-2000 - SD - Set/Read Break In Delay
                                            // 
                                            // 0000-1000 ms (in steps of 50 ms) 0000 is full break in
                                                int local_cw_breakin=cw_keyer_hang_time;
                                                // Limit report value to 1000 for TS 2000
                                                if(local_cw_breakin > 1000) { local_cw_breakin = 1000; }
                                                if(len <=2) {
                                                  if(zzid_flag == 0) { // TS 2000
                                                       sprintf(msg,"SD%04d;",local_cw_breakin);
                                                  } else {   // ZZ command response
                                                       sprintf(msg,"ZZCD%04d;",local_cw_breakin);
                                                  }
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
        else if(strcmp(cmd_str,"SH")==0) {  
                                            // TS-2000 - SH - Set/read the DSP filter settings - not supported
                                             if(len <=2) {
                                              // send_resp(client_sock,"SH00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"SI")==0) {  
                                            // TS-2000 - SI - Enters the satellite memory name - not supported
                                         }
        else if(strcmp(cmd_str,"SL")==0) {  
                                            // TS-2000 - SL - Set/read the DSP filter settings - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"SL00;");
                                               send_resp(client_sock,"?;"); 
                                             } 
                                         }
        else if(strcmp(cmd_str,"SM")==0) {  
                                            // PiHPSDR - ZZSM - Read SMeter - same range as TS 2000
                                            // TI-2000 - SM - Read SMETER
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
        else if(strcmp(cmd_str,"SQ")==0) {  
                                            // TS-2000 - SQ - Set/read the squelch level - not supported
                                            // P1 - 0- main, 1=sub
                                            // P2 - 0-255
                                             float float_sq;
                                             if(len <=3) {
                                               float_sq = (float) active_receiver->squelch;
                                               float_sq = roundf((float_sq/100.0)*256.0);
                                               fprintf(stderr,"RIGCTL: Init=%f float_sq=%6.2f\n",
                                                                  active_receiver->squelch,float_sq); 
                                               sprintf(msg,"SQ0%03d;",(int) float_sq);
                                               send_resp(client_sock,msg);
                                             } else {  // Set the value
                                               int lcl_sq = atoi(&cmd_input[3]); // Note we skip the first char!
                                               if((lcl_sq >= 0) && (lcl_sq <=255)) {
                                                 float_sq = roundf(((float)lcl_sq/256.0)*100);
                                                 active_receiver->squelch = (int)float_sq; 
                                                 g_idle_add(ext_update_squelch,NULL);
                                                 g_idle_add(ext_vfo_update,NULL);
                                               } else {
                                                 send_resp(client_sock,"?;");
                                               }
                                             }
                                           }
        else if(strcmp(cmd_str,"SR")==0) {  
                                            // TS-2000 - SR - Resets the transceiver - not supported
                                         }
        else if(strcmp(cmd_str,"SS")==0) {  
                                            // TS-2000 - SS - Set/read Scan pause freq - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"SS0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"ST")==0) {  
                                            // TS-2000 - ST - Set/read the multi/ch control freq steps
                                            // PiHPSDR - ZZST - Set/read the multi/ch control freq steps
                                            // SSB/CW/FSK - values 00-03
                                            // 00: 1KHz, 01: 2.5Khz 02:5KHz 03: 10Khz
                                            // AM/FM mode 00-09
                                            // 00: 5KHz, 
                                            // 01: 6.25KHz, 
                                            // 02: 10Khz, 
                                            // 03: 12.5Khz,
                                            // 04: 15Khz, 
                                            // 05: 20Khz, 
                                            // 06: 25KHz
                                            // 07: 30Khz
                                            // 08: 50Khz
                                            // 09: 100Khz 
                                            if(zzid_flag == 0) {
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
                                             }
                                            } else {
                                                // Pi HPSDR handling
				                if(len <= 2) {
                                                   sprintf(msg,"ZZST%06lld;", step);
                                                   send_resp(client_sock,msg);
                                                }  else {
                                                   int okay= 0;
                                                   work_int = atoi(&cmd_input[2]);
                                                   switch(work_int) {
                                                         case 100000: okay = 1; break;
                                                         case  50000: okay = 1; break;
                                                         case  30000: okay = 1; break;
                                                         case  25000: okay = 1; break;
                                                         case  20000: okay = 1; break;
                                                         case  15000: okay = 1; break;
                                                         case  12500: okay = 1; break;
                                                         case  10000: okay = 1; break;
                                                         case   9000: okay = 1; break;
                                                         case   6250: okay = 1; break;
                                                         case   5000: okay = 1; break;
                                                         case   2500: okay = 1; break;
                                                         case   1000: okay = 1; break;
                                                         case    500: okay = 1; break;
                                                         case    250: okay = 1; break;
                                                         case    100: okay = 1; break;
                                                         case     50: okay = 1; break;
                                                         case     10: okay = 1; break;
                                                         case      1: okay = 1; break;
                                                         default:     okay = 0; break;
                                                   }
                                                   if(okay == 0) {
                                                         fprintf(stderr,"RIGCTL: ZZST ERROR - illegal  step val=%d\n",work_int);
                                                         send_resp(client_sock,"?;");
                                                   } else {
                                                      step = work_int; 
                                                   }
                                                }
                                            }
                                            g_idle_add(ext_vfo_update,NULL);
                                         }
        else if((strcmp(cmd_str,"SU")==0) && (zzid_flag == 0)) {
                                            // TS-2000 - SU - Set/read the scan pause freq - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"SU00000000000;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"SV")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - SV - Execute the memory transfer function - not supported
                                         }
        else if((strcmp(cmd_str,"TC")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - TC - Set/read the internal TNC mode - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"TC00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"TD")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - TD - Sends the DTMF memory channel - not supported
                                         }
        else if((strcmp(cmd_str,"TI")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - TI - Reads the TNC LED status - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"TI00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"TN")==0) && (zzid_flag == 0))  {  
                                            // TS-2000 - TN - Set/Read sub tone freq - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"TN00;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"TO")==0) && (zzid_flag == 0)) {  
                                            // TI-2000 - TO - Set/Read tone function - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"TO0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"TS")==0) && (zzid_flag == 0)) {  
                                            // TI-2000 - TS - Set/Read TF Set function status
                                             if(len <=2) {
                                               //send_resp(client_sock,"TS0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if(strcmp(cmd_str,"TX")==0)  { 
                                            // TS-2000 - TX - Key Xmitter - P1 - transmit on main/sub freq
                                            // PiHPSDR - ZZTX - Key Xmitter - P1 - transmit on main/sub freq
                                            if(!((vfo[active_receiver->id].mode == modeCWL) ||
                                                 (vfo[active_receiver->id].mode == modeCWU))) {
                                                 //*if(len >=3) { 
                                                 // K5JAE: The TS 2000 real hardware does not respond 
                                                 // to this command, thus hamlib is not expecting response.
                                                 //send_resp(client_sock,"TX0;");  */
                                               mox_state = 1;
                                               g_idle_add(ext_mox_update,(gpointer)(long)mox_state); 
                                               g_idle_add(ext_vfo_update,NULL);
                                            } else {
                                              g_idle_add(ext_cw_setup,NULL);    // Initialize for external transmit
                                              g_idle_add(ext_mox_update,(gpointer)(long)1); // Turn on External MOX
                                            }
                                         }
        else if(strcmp(cmd_str,"TY")==0) {  
                                            // TI-2000 - TY -- Set/Read uP firmware type
                                             if(len <=2) {
                                               send_resp(client_sock,"TY000;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"UA")==0) && (zzid_flag == 1)) {  
                                            // PiHPSDR - ZZUA - Xvrt Set/Read 
                                            //        RW P1 - 1 Char - Entry number 0-7
                                            //         W P1 - Disable PA - 1 char
                                            //         W P2 - 15 Char -  Title - 16th char with last being '\0'
                                            //         W P3 - 11 Chars - Min Freq HZ
                                            //         W P4 - 11 Chars - Max Freq HZ
                                            //         W P5 - 11 Chars - Low Freq HZ
                                            char lcl_buf[] = "                "; 
                                            char tempc;
                                            if(len<=3) {
                                               work_int = atoi(&cmd_input[2]);
                                               if((work_int >=0) && (work_int <=7)) {
                                                  BAND *xvtr=band_get_band(BANDS+work_int);
                                                  BANDSTACK *bandstack=xvtr->bandstack;
                                                  BANDSTACK_ENTRY *entry=bandstack->entry;
                                                  strcpy(lcl_buf,xvtr->title);
                                                  lcl_buf[strlen(lcl_buf)] = ' '; // Replace the last entry with ' ';
                                                  lcl_buf[15] = '\0';
                                                  sprintf(msg,"ZZUA%1d%1d%s%011lld%011lld%011lld;",work_int,xvtr->disablePA,lcl_buf,
                                                                              xvtr->frequencyMin,xvtr->frequencyMax,xvtr->frequencyLO);
                                                  send_resp(client_sock,msg);
                                               }
                                            } else if(len==52) {
                                              
                                               tempc = cmd_input[3];
                                               cmd_input[3] = '\0';
                                               work_int = atoi(&cmd_input[2]);
                                               cmd_input[3] = tempc;
                                               if((work_int >=0) && (work_int <=7)) {

                                                  BAND *xvtr=band_get_band(BANDS+work_int);
                                                  BANDSTACK *bandstack=xvtr->bandstack;
                                                  BANDSTACK_ENTRY *entry=bandstack->entry;
                                                  tempc = cmd_input[4]; 
                                                  cmd_input[4]='\0';
                                                  xvtr->disablePA = atoi(&cmd_input[3]); 
                                                  cmd_input[4]=tempc;
                                                  
                                                  /* Get the title of the XVTR */
                                                  tempc = cmd_input[19];
                                                  cmd_input[19] = '\0';
                                                  strncpy(xvtr->title,&cmd_input[4],16); 
                                                  cmd_input[19] = tempc;

                                                  /* Pull out the Min freq */
                                                  tempc = cmd_input[30];
                                                  cmd_input[30]='\0';
                                                  xvtr->frequencyMin = (long long) atoi(&cmd_input[19]); 
                                                  cmd_input[30] = tempc;

                                                  /* Pull out the Max freq */
                                                  tempc = cmd_input[41];
                                                  cmd_input[41]='\0';
                                                  xvtr->frequencyMax = (long long) atoi(&cmd_input[30]); 
                                                  cmd_input[41] = tempc;
                                                  

                                                  /* Pull out the LO freq */
                                                  xvtr->frequencyLO = (long long) atoi(&cmd_input[41]); 
                                                } else {
                                                  fprintf(stderr,"RIGCTL: ERROR ZZUA - incorrect length command received=%d",len);
                                                  send_resp(client_sock,"?;");
                                                }
                                            } 
                                         }
        else if((strcmp(cmd_str,"UL")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - UL - Detects the PLL unlock status - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"UL0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                         }
        else if((strcmp(cmd_str,"UP")==0) && (zzid_flag == 0))  {
                                            // TS-2000 - UP - Emulates the mic up key
                                         }
        else if(strcmp(cmd_str,"VD")==0) {  
                                            // TS-2000 - VD - Sets/Reads VOX delay time - 0000-3000ms in steps of 150
                                            // PiHPSDR - ZZVD - Sets/Reads VOX Hang time
                                            // We want vox_hang variable in Pi HPSDR
                                            // Max value in variable in ms... so 250 = 250ms
                                             if(len <=2) {
                                               work_int = (int) vox_hang;
                                               sprintf(msg,"VD%04d;",work_int); 
                                               send_resp(client_sock,msg);
                                             } else {
                                                  work_int = atoi(&cmd_input[2]);
                                                  // Bounds check for legal values for Pi HPSDR
                                                  if(work_int > 1000) { work_int = 1000; }
                                                  if(work_int < 0) { work_int = 0; }
                                                  vox_hang = (double) work_int;
                                                  #ifdef RIGCTL_DEBUG
                                                  fprintf(stderr,"RIGCTL: Vox hang=%0.20f\n",vox_hang);
                                                  #endif
                                             }
                                         }
        else if(strcmp(cmd_str,"VG")==0) {  
                                            // TI-2000 - VG - Sets/Reads VOX gain 000-009
                                            // PiHPSDR - ZZVG - Set/Read VOX Threshold - 0-1000
                                            // We want vox_threshold variable in Pi HPSDR
                                            // Max value in variable 0.1 
                                            // 3 char 000-009 valid ranges
                                            if(len <=2) {
                                               if(zzid_flag == 0) {
                                                    work_int = (int) ((vox_threshold) * 100.0);
                                                    sprintf(msg,"VG00%1d;",work_int);
                                               } else {
                                                    work_int = (int) ((vox_threshold) * 1000.0);
                                                    sprintf(msg,"ZZVG%04d;",work_int);
                                               }
                                               send_resp(client_sock,msg);
                                             } else {
                                               work_int = atoi(&cmd_input[2]);
                                               if((work_int >=0) && (work_int<=9)) {
                                                  if(zzid_flag == 0) {
                                                     // Set the threshold here
                                                     vox_threshold = ((double) work_int)/100.0;
                                                     #ifdef RIGCTL_DEBUG
                                                     fprintf(stderr,"RIGCTL: Vox thresh=%0.20f\n",vox_threshold);
                                                     #endif
                                                  } else {
                                                     vox_threshold = ((double) work_int)/1000.0;
                                                     #ifdef RIGCTL_DEBUG
                                                     fprintf(stderr,"RIGCTL: Vox thresh=%0.20f\n",vox_threshold);
                                                     #endif
                                                  }
                                                } else {
                                                     send_resp(client_sock,"?;");
                                                }
                                             }
                                         }
        else if((strcmp(cmd_str,"VR")==0) && (zzid_flag == 0)) {  
                                            // TS-2000 - VR - Emulates the voice 1/2 key - not supported
                                         }
        else if(strcmp(cmd_str,"VX")==0) {  
                                            // TS-2000 - VX - Sets/reads vox f(x) state
                                            // PiHPSDR - ZZVX - Set/Read VOX enabled
                                             if(len <=2) {
                                               if(zzid_flag == 0 ) {
                                                   sprintf(msg,"VX%1d;",vox_enabled); 
                                               } else {
                                                   sprintf(msg,"ZZVX%1d;",vox_enabled); 
                                               } 
                                               send_resp(client_sock,msg);
                                             } else {
                                               work_int = atoi(&cmd_input[2]);
                                               if(work_int==1) { vox_enabled = 1; vox= 1;}
                                               if(work_int!=1) { vox_enabled = 0; vox=0; }
                                             }
                                         }
        else if((strcmp(cmd_str,"XC")==0) && (zzid_flag == 1)) {  
                                            // PiHPSDR - ZZXC  - Turn off External MOX- 
                                            g_idle_add(ext_mox_update,(gpointer)(long)0); // Turn on xmitter (set Mox)
                                         }
        else if((strcmp(cmd_str,"XI")==0) && (zzid_flag == 1)) {  
                                            // PiHPSDR - ZZXI - Initialize the transmitter for external CW
                                            g_idle_add(ext_cw_setup,NULL);    // Initialize for external transmit
                                            g_idle_add(ext_mox_update,(gpointer)(long)1); // Turn on External MOX
                                         }
       else if((strcmp(cmd_str,"XO")==0) && (zzid_flag == 1)) {  
                                            // PiHPSDR - ZZXT - Turn CW Note off when in CW mode
                                            g_idle_add(ext_cw_key, (gpointer)(long)0);
                                         }
        else if(strcmp(cmd_str,"XT")==0) {  
                                           if(zzid_flag == 0 ) {
                                            // TS-2000 - XT - Sets/reads the XIT f(x) state - not supported
                                             if(len <=2) {
                                               //send_resp(client_sock,"XT0;"); 
                                               send_resp(client_sock,"?;"); 
                                             }
                                           } else {
                                            // PiHPSDR - ZZXT - Turn CW Note on when in CW mode
                                            g_idle_add(ext_cw_key, (gpointer)(long)1);
                                           }
                                         }
        else if((strcmp(cmd_str,"XX")==0) && (zzid_flag == 0)) {  // 
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

   rigctl_busy = 1;
   mutex_a = g_new(GT_MUTEX,1);
   g_mutex_init(&mutex_a->m);

   mutex_b = g_new(GT_MUTEX,1);
   g_mutex_init(&mutex_b->m);
   
   mutex_c = g_new(GT_MUTEX,1);
   g_mutex_init(&mutex_c->m);

   // This routine encapsulates the thread call
   rigctl_server_thread_id = g_thread_new( "rigctl server", rigctl_server, (gpointer)(long)rigctl_port_base);
   if( ! rigctl_server_thread_id )
   {
     fprintf(stderr,"g_thread_new failed on rigctl_server\n");
   }
}


#define MAXDATASIZE 300

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

void set_freqB(long long new_freqB) {      

   //BANDSTACK_ENTRY *bandstack = bandstack_entry_get_current();
   //bandstack->frequencyB = new_freqB;
   //frequencyB = new_freqB;
   vfo[VFO_B].frequency = new_freqB;   
   g_idle_add(ext_vfo_update,NULL);
}


int set_band (gpointer data) {
  
  BANDSTACK *bandstack;
  long long new_freq = *(long long *) data;
  free(data);

  fprintf(stderr,"RIGCTL set_band: New freq=%lld\n",new_freq);

  int b = get_band_from_frequency (new_freq);

  if(b == -1) { // Not in the ham bands!
     // We're not going to update the bandstack - but rather just
     // change the frequency and move on  
        vfo[active_receiver->id].frequency=new_freq;
        receiver_vfo_changed(receiver[active_receiver->id]);
        g_idle_add(ext_vfo_update,NULL);
        return 0;
  }

  fprintf(stderr,"RIGCTL set_band: New Band=%d\n",b);
  int id=active_receiver->id;

  //if(id==0) {
  //  fprintf(stderr,"RIGCTL set_band: id=0\n");
  //  vfo_save_bandstack();
  //}
  if(b==vfo[id].band) {
    //fprintf(stderr,"RIGCTL set_band:b=cur_band \n");
    // same band selected - step to the next band stack
    bandstack=bandstack_get_bandstack(b);
    vfo[id].bandstack++;
    if(vfo[id].bandstack>=bandstack->entries) {
      //fprintf(stderr,"VFO_BAND_CHANGED: bandstack set to 0\n");
      vfo[id].bandstack=0;
    }
  } else {
    // new band - get band stack entry
    //fprintf(stderr,"VFO_BAND_CHANGED: new_band\n");
    bandstack=bandstack_get_bandstack(b)      ;
    vfo[id].bandstack=bandstack->current_entry;
    //fprintf(stderr,"VFO_BAND_CHANGED: vfo[id].banstack=%d\n",vfo[id].bandstack);
  }

  BAND *band=band_get_band(b);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[id].bandstack];
  if(vfo[id].band != b) {
     vfo[id].mode=entry->mode;
  }
  vfo[id].band=b;
  //vfo[id].frequency=entry->frequency;
  entry->frequency = new_freq;
  vfo[id].frequency=new_freq;
  //vfo[id].mode=entry->mode;
  vfo[id].filter=entry->filter;
  vfo[id].lo=band->frequencyLO;

  switch(id) {
    case 0:
      bandstack->current_entry=vfo[id].bandstack;
      receiver_vfo_changed(receiver[id]);
      BAND *band=band_get_band(vfo[id].band);
      set_alex_rx_antenna(band->alexRxAntenna);
      set_alex_tx_antenna(band->alexTxAntenna);
      set_alex_attenuation(band->alexAttenuation);
      receiver_vfo_changed(receiver[0]);
      break;
   case 1:
      if(receivers==2) {
        receiver_vfo_changed(receiver[1]);
      }
      break;
  }

  if(split) {
    tx_set_mode(transmitter,vfo[VFO_B].mode);
  } else {
    tx_set_mode(transmitter,vfo[VFO_A].mode);
  }
  calcDriveLevel();
  //calcTuneDriveLevel();
  g_idle_add(ext_vfo_update,NULL);

  return 0;
}
 
int set_alc(gpointer data) {
    alc = (uintptr_t) data;
    return 0;
}

int lookup_band(int val) {
    int work_int;
    switch(val) {
       case 160: work_int = 0; break;
       case  80: work_int = 1; break;
       case  60: work_int = 2; break;
       case  40: work_int = 3; break;
       case  30: work_int = 4; break;
       case  20: work_int = 5; break;
       case  17: work_int = 6; break;
       case  15: work_int = 7; break;
       case  12: work_int = 8; break;
       case  10: work_int = 9; break;
       case   6: work_int = 10; break;
       case 888: work_int = 11; break; // General coverage
       case 999: work_int = 12; break; // WWV
       case 136: work_int = 13; break; // WWV
       case 472: work_int = 14; break; // WWV
       default: work_int = 0;
     }
     return work_int;
}
