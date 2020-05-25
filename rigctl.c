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
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
//#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "receiver.h"
#include "toolbar.h"
#include "band_menu.h"
#include "sliders.h"
#include "rigctl.h"
#include "radio.h"
#include "channel.h"
#include "filter.h"
#include "mode.h"
#include "filter.h"
#include "band.h"
#include "bandstack.h"
#include "filter_menu.h"
#include "vfo.h"
#include "sliders.h"
#include "transmitter.h"
#include "agc.h"
#include <wdsp.h>
#include "store.h"
#include "ext.h"
#include "rigctl_menu.h"
#include "noise_menu.h"
#include "new_protocol.h"
#ifdef LOCALCW
#include "iambic.h"              // declare keyer_update()
#endif
#include <math.h>

#define NEW_PARSER

// IP stuff below
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr

int rigctl_port_base=19090;
int rigctl_enable=0;

// the port client will be connecting to
// 2-26-17 K5JAE - Changed the defines to const ints to allow use via pointers.
static const int TelnetPortA = 19090;
static const int TelnetPortB = 19091;
static const int TelnetPortC = 19092;

#define RIGCTL_THROTTLE_NSEC    15000000L
#define NSEC_PER_SEC          1000000000L

// max number of bytes we can get at once
#define MAXDATASIZE 2000

int parse_cmd (void *data);
int connect_cnt = 0;

int rigctlGetFilterLow();
int rigctlGetFilterHigh();
int new_level;
int active_transmitter = 0;
int rigctl_busy = 0;  // Used to tell rigctl_menu that launch has already occured

int cat_control;

extern int enable_tx_equalizer;

typedef struct {GMutex m; } GT_MUTEX;
GT_MUTEX * mutex_a;
GT_MUTEX * mutex_b;
GT_MUTEX * mutex_c;
GT_MUTEX * mutex_busy;

int mutex_b_exists = 0;


FILE * out;
int  output;
FILTER * band_filter;

#define MAX_CLIENTS 3
static GThread *rigctl_server_thread_id = NULL;
static GThread *rigctl_cw_thread_id = NULL;
static int server_running;

static GThread *serial_server_thread_id = NULL;
static gboolean serial_running=FALSE;

static int server_socket=-1;
static int server_address_length;
static struct sockaddr_in server_address;

static int rigctl_timer = 0;

typedef struct _client {
  int fd;
  socklen_t address_length;
  struct sockaddr_in address;
  GThread *thread_id;
} CLIENT;

typedef struct _command {
  CLIENT *client;
  char *command;
} COMMAND;

int fd;  // Serial port file descriptor

static CLIENT client[MAX_CLIENTS];

int squelch=-160; //local sim of squelch level
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

  g_print("close_rigctl_ports: server_socket=%d\n",server_socket);
  server_running=0;
  for(i=0;i<MAX_CLIENTS;i++) {
      if(client[i].fd!=-1) {
        g_print("setting SO_LINGER to 0 for client_socket: %d\n",client[i].fd);
        if(setsockopt(client[i].fd,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
          perror("setsockopt(...,SO_LINGER,...) failed for client");
        }
        g_print("closing client socket: %d\n",client[i].fd);
        close(client[i].fd);
        client[i].fd=-1;
      }
  }

  if(server_socket>=0) {
     g_print("setting SO_LINGER to 0 for server_socket: %d\n",server_socket);
    if(setsockopt(server_socket,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
      perror("setsockopt(...,SO_LINGER,...) failed for server");
    }
    g_print("closing server_socket: %d\n",server_socket);
    close(server_socket);
    server_socket=-1;
  }
}

int vfo_sm=0;   // VFO State Machine - this keeps track of

// 
//  CW sending stuff
//

static char cw_buf[25];
static int  cw_busy=0;
static int  cat_cw_seen=0;

static int dotlen;
static int dashlen;
static int dotsamples;
static int dashsamples;

//
// send_dash()         send a "key-down" of a dashlen, followed by a "key-up" of a dotlen
// send_dot()          send a "key-down" of a dotlen,  followed by a "key-up" of a dotlen
// send_space(int len) send a "key_down" of zero,      followed by a "key-up" of len*dotlen
//
// The "trick" to get proper timing is, that we really specify  the number of samples
// for the next element (dash/dot/nothing) and the following pause. 30 wpm is no
// problem, and without too much "busy waiting". We just take a nap until 10 msec
// before we have to act, and then wait several times for 1 msec until we can shoot.
//
void send_dash() {
  int TimeToGo;
  for(;;) {
    TimeToGo=cw_key_up+cw_key_down;
    // TimeToGo is invalid if local CW keying has set in
    if (cw_key_hit || cw_not_ready) return;
    if (TimeToGo == 0) break;
    // sleep until 10 msec before ignition
    if (TimeToGo > 500) usleep((long)(TimeToGo-500)*20L);
    // sleep 1 msec
    usleep(1000L);
  }
  // If local CW keying has set in, do not interfere
  if (cw_key_hit || cw_not_ready) return;
  cw_key_down = dashsamples;
  cw_key_up   = dotsamples;
}

void send_dot() {
  int TimeToGo;
  for(;;) {
    TimeToGo=cw_key_up+cw_key_down;
    // TimeToGo is invalid if local CW keying has set in
    if (cw_key_hit || cw_not_ready) return;
    if (TimeToGo == 0) break;
    // sleep until 10 msec before ignition
    if (TimeToGo > 500) usleep((long)(TimeToGo-500)*20L);
    // sleep 1 msec
    usleep(1000L);
  }
  // If local CW keying has set in, do not interfere
  if (cw_key_hit || cw_not_ready) return;
  cw_key_down = dotsamples;
  cw_key_up   = dotsamples;
}

void send_space(int len) {
  int TimeToGo;
    for(;;) {
    TimeToGo=cw_key_up+cw_key_down;
    // TimeToGo is invalid if local CW keying has set in
    if (cw_key_hit || cw_not_ready) return;
    if (TimeToGo == 0) break;
    // sleep until 10 msec before ignition
    if (TimeToGo > 500) usleep((long)(TimeToGo-500)*20L);
    // sleep 1 msec
    usleep(1000L);
  }
  // If local CW keying has set in, do not interfere
  if (cw_key_hit || cw_not_ready) return;
  cw_key_up = len*dotsamples;
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
       case 'Q': strcpy(pattern,"--.-"); break;
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
       case 'y':
       case 'Y': strcpy(pattern,"-.--"); break;
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
//
//     DL1YCF:
//     There were some signs I considered wrong, other
//     signs missing. Therefore I put the signs here
//     from ITU Recommendation M.1677-1 (2009)
//     in the order given there.
//
       case '.':  strcpy(pattern,".-.-.-"); break;
       case ',':  strcpy(pattern,"--..--"); break;
       case ':':  strcpy(pattern,"---..");  break;
       case '?':  strcpy(pattern,"..--.."); break;
       case '\'': strcpy(pattern,".----."); break;
       case '-':  strcpy(pattern,"-....-"); break;
       case '/':  strcpy(pattern,"-..-.");  break;
       case '(':  strcpy(pattern,"-.--.");  break;
       case ')':  strcpy(pattern,"-.--.-"); break;
       case '"':  strcpy(pattern,".-..-."); break;
       case '=':  strcpy(pattern,"-...-");  break;
       case '+':  strcpy(pattern,".-.-.");  break;
       case '@':  strcpy(pattern,".--.-."); break;
//
//     Often used, but not ITU: Ampersand for "wait"
//
       case '&': strcpy(pattern,".-...");break;
       default:  strcpy(pattern,"");
    }
     
    while(*ptr != '\0') {
       if(*ptr == '-') {
          send_dash();
       }
       if(*ptr == '.') {
          send_dot();
       }
       ptr++;
    }

    // The last element (dash or dot) sent already has one dotlen space appended.
    // If the current character is another "printable" sign, we need an additional
    // pause of 2 dotlens to form the inter-character spacing of 3 dotlens.
    // However if the current character is a "space" we must produce an inter-word
    // spacing (7 dotlens) and therefore need 6 additional dotlens
    // We need no longer take care of a sequence of spaces since adjacent spaces
    // are now filtered out while filling the CW character (ring-) buffer.

    if (cw_char == ' ') {
      send_space(6);  // produce inter-word space of 7 dotlens
    } else {
      send_space(2);  // produce inter-character space of 3 dotlens
    }
}

//
// This thread constantly looks whether CW data
// is available, and produces CW in this case.
//
// A ring buffer is maintained such that the contents
// of several KY commands can be buffered. This allows
// sending a large CW text word-by-word (each word in a
// separate KY command).
//
// If the contents of the last KY command do not fit into
// the ring buffer, cw_busy is NOT reset. Eventually, there
// is enough space in the ring buffer, then cw_busy is reset.
//
static gpointer rigctl_cw_thread(gpointer data)
{ 
  int i;
  char c;
  char last_char=0;
  char ring_buf[130];
  char *write_buf=ring_buf;
  char *read_buf =ring_buf;
  char *p;
  int  num_buf=0;
  int  txmode;
  
  while (server_running) {
    // wait for CW data (periodically look every 100 msec)
    if (!cw_busy && num_buf ==0) {
      cw_key_hit=0;
      usleep(100000L);
      continue;
    }

    if (rigctl_debug && cw_busy) {
      g_print("SendCW: -->%s<--\n", cw_buf);
    }
    //
    // if a message arrives and the TX mode is not CW, silently ignore
    //
    txmode=get_tx_mode();
    if (cw_busy && txmode != modeCWU && txmode != modeCWL) {
      cw_busy=0;
      continue;
    }
    // if new data is available and fits into the buffer, copy-in.
    // If there are several adjacent spaces, take only the first one.
    // This also swallows the "tails" of the KY commands which
    // (according to Kenwood) have to be padded with spaces up
    // to the maximum length (24)

    if (cw_busy && num_buf < 100) {
      p=cw_buf;
      while ((c=*p++)) {
        if (last_char == ' ' && c == ' ') continue;
        *write_buf++ = c;
        last_char=c;
        num_buf++;
        if (write_buf - ring_buf == 128) write_buf=ring_buf;  // wrap around
      }
      cw_busy=0; // mark one-line buffer free again
    }
    // This may happen if cw_buf was empty or contained only blanks
    if (num_buf == 0) continue;

    // these values may have changed, so recompute them here
    // This means that we can change the speed (KS command) while
    // the buffer is being sent

    dotlen = 1200000L/(long)cw_keyer_speed;
    dashlen = (dotlen * 3 * cw_keyer_weight) / 50L;
    dotsamples = 57600 / cw_keyer_speed;
    dashsamples = (3456 * cw_keyer_weight) / cw_keyer_speed;
    CAT_cw_is_active=1;
    if (!mox) {
	// activate PTT
        g_idle_add(ext_mox_update ,GINT_TO_POINTER(1));
	// have to wait until it is really there
	// Note that if out-of-band, we would wait
	// forever here, so allow at most 200 msec
	// We also have to wait for cw_not_ready becoming zero
	i=200;
        while ((!mox || cw_not_ready) && i-- > 0) usleep(1000L);
	// still no MOX? --> silently discard CW character and give up
	if (!mox) {
	    CAT_cw_is_active=0;
	    continue;
	}
    }
    // At this point, mox==1 and CAT_cw_active == 1
    if (cw_key_hit || cw_not_ready) {
       //
       // CW transmission has been aborted, either due to manually
       // removing MOX, changing the mode to non-CW, or because a CW key has been hit.
       // Do not remove PTT in the latter case
       CAT_cw_is_active=0;
       // If a CW key has been hit, we continue in TX mode.
       // Otherwise, switch PTT off.
       if (!cw_key_hit && mox) {
         g_idle_add(ext_mox_update ,GINT_TO_POINTER(0));
       }
       // Let the CAT system swallow incoming CW commands by setting cw_busy to -1.
       // Do so until no CAT CW message has arrived for 1 second
       cw_busy=-1;
       for (;;) {
         cat_cw_seen=0;
         usleep(1000000L);
         if (cat_cw_seen) continue;
         cw_busy=0;
         break;
       }
       write_buf=read_buf=ring_buf;
       num_buf=0;
    } else {
      rigctl_send_cw_char(*read_buf++);
      if (read_buf - ring_buf == 128) read_buf=ring_buf; // wrap around
      num_buf--;
      //
      // Character has been sent.
      // If there are more to send, or the next message is pending, continue.
      // Otherwise remove PTT and wait for next CAT CW command.
      if (cw_busy || num_buf > 0) continue;
      CAT_cw_is_active=0;
      if (!cw_key_hit) {
        g_idle_add(ext_mox_update ,GINT_TO_POINTER(0));
        // wait up to 500 msec for MOX having gone
        // otherwise there might be a race condition when sending
        // the next character really soon
        i=10;
        while (mox && (i--) > 0) usleep(50000L);
      }
    }
    // end of while (server_running)
  }
  // We arrive here if the rigctl server shuts down.
  // This very rarely happens. But we should shut down the
  // local CW system gracefully, in case we were in the mid
  // of a transmission
  rigctl_cw_thread_id = NULL;
  cw_busy=0;
  if (CAT_cw_is_active) {
    CAT_cw_is_active=0;
    g_idle_add(ext_mox_update ,GINT_TO_POINTER(0));
  }
  return NULL;
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
void send_resp (int fd,char * msg) {
  if(rigctl_debug) g_print("RIGCTL: RESP=%s\n",msg);
  int length=strlen(msg);
  int written=0;
  
  while(written<length) {
    written+=write(fd,&msg[written],length-written);   
  }
  
}

//
// 2-25-17 - K5JAE - removed duplicate rigctl
//

static gpointer rigctl_server(gpointer data) {
  int port=GPOINTER_TO_INT(data);
  int on=1;
  int i;

  g_print("rigctl_server: starting server on port %d\n",port);

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
    client[i].fd=-1;
  }
  server_running=1;

  // must start the thread here in order NOT to inherit a lock
  if (!rigctl_cw_thread_id) rigctl_cw_thread_id = g_thread_new("RIGCTL cw", rigctl_cw_thread, NULL);

  while(server_running) {
    // listen with a max queue of 3
    if(listen(server_socket,3)<0) {
      perror("rigctl_server: listen failed");
      close(server_socket);
      return NULL;
    }

    // find a spare thread
    for(i=0;i<MAX_CLIENTS;i++) {
      if(client[i].fd==-1) {

        g_print("Using client: %d\n",i);

        client[i].fd=accept(server_socket,(struct sockaddr*)&client[i].address,&client[i].address_length);
        if(client[i].fd<0) {
          perror("rigctl_server: client accept failed");
          continue;
        }

        client[i].thread_id = g_thread_new("rigctl client", rigctl_client, (gpointer)&client[i]);
        if(client[i].thread_id==NULL) {
          g_print("g_thread_new failed (n rigctl_client\n");
          g_print("setting SO_LINGER to 0 for client_socket: %d\n",client[i].fd);
          struct linger linger = { 0 };
          linger.l_onoff = 1;
          linger.l_linger = 0;
          if(setsockopt(client[i].fd,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
            perror("setsockopt(...,SO_LINGER,...) failed for client");
          }
          close(client[i].fd);
        }
      }
    }
  }

  close(server_socket);
  return NULL;
}

static gpointer rigctl_client (gpointer data) {
   
  CLIENT *client=(CLIENT *)data;

  g_print("rigctl_client: starting rigctl_client: socket=%d\n",client->fd);

  g_mutex_lock(&mutex_a->m);
  cat_control++;
  if(rigctl_debug) g_print("RIGCTL: CTLA INC cat_contro=%d\n",cat_control);
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

  char *command=g_new(char,MAXDATASIZE);
  int command_index=0;

   while(server_running && (numbytes=recv(client->fd , cmd_input , MAXDATASIZE-2 , 0)) > 0 ) {
       for(i=0;i<numbytes;i++) {
         command[command_index]=cmd_input[i];
         command_index++;
         if(cmd_input[i]==';') {
           command[command_index]='\0';
           if(rigctl_debug) g_print("RIGCTL: command=%s\n",command);
           COMMAND *info=g_new(COMMAND,1);
           info->client=client;
           info->command=command;
           g_idle_add(parse_cmd,info);
           command=g_new(char,MAXDATASIZE);
           command_index=0;
         }
       }
     }
g_print("RIGCTL: Leaving rigctl_client thread");
  if(client->fd!=-1) {
    g_print("setting SO_LINGER to 0 for client_socket: %d\n",client->fd);
    struct linger linger = { 0 };
    linger.l_onoff = 1;
    linger.l_linger = 0;
    if(setsockopt(client->fd,SOL_SOCKET,SO_LINGER,(const char *)&linger,sizeof(linger))==-1) {
      perror("setsockopt(...,SO_LINGER,...) failed for client");
    }
    close(client->fd);
    client->fd=-1;
    // Decrement CAT_CONTROL
    g_mutex_lock(&mutex_a->m);
    cat_control--;
    if(rigctl_debug) g_print("RIGCTL: CTLA DEC - cat_control=%d\n",cat_control);
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

gboolean parse_extended_cmd (char *command,CLIENT *client) {
  gboolean implemented=TRUE;
  char reply[256];
  reply[0]='\0';
  switch(command[2]) {
    case 'A': //ZZAx
      switch(command[3]) {
        case 'A': //ZZAA
          implemented=FALSE;
          break;
        case 'B': //ZZAB
          implemented=FALSE;
          break;
        case 'C': //ZZAC
          // sets or reads the Step Size
          if(command[4]==';') {
            // read the step size
            int i=0;
            for(i=0;i<=14;i++) {
              if(steps[i]==step) break;
            }
            if(i<=14) {
              // send reply back
              sprintf(reply,"ZZAC%02d;",i);
              send_resp(client->fd,reply) ;
            }
          } else if(command[6]==';') {
            // set the step size
            int i=atoi(&command[4]) ;
            if(i>=0 && i<=14) {
              step=steps[i];
              vfo_update();
            }
          } else {
          }
          break;
        case 'D': //ZZAD
          // move VFO A down by selected step
          if(command[6]==';') {
            int step_index=atoi(&command[4]);
            long long hz=0;
            if(step_index>=0 && step_index<=14) {
              hz=(long long)steps[step_index];
            }
            if(hz!=0LL) {
              vfo_id_move(VFO_A,-hz,FALSE);
            }
          } else {
          }
          break;
        case 'E': //ZZAE
          // move VFO A down nn tune steps
          if(command[6]==';') {
            int steps=atoi(&command[4]);
            vfo_id_step(VFO_A,-steps);
          }
          break;
        case 'F': //ZZAF
          // move VFO A up nn tune steps
          if(command[6]==';') {
            int steps=atoi(&command[4]);
            vfo_id_step(VFO_A,steps);
          }
          break;
        case 'G': //ZZAG
          // read/set audio gain
          if(command[4]==';') {
            // send reply back
            sprintf(reply,"ZZAG%03d;",(int)(active_receiver->volume*100.0));
            send_resp(client->fd,reply) ;
          } else {
            int gain=atoi(&command[4]);
            active_receiver->volume=(double)gain/100.0;
            update_af_gain();
          }
          break;
        case 'I': //ZZAI
          implemented=FALSE;
          break;
        case 'P': //ZZAP
          implemented=FALSE;
          break;
        case 'R': //ZZAR
          // read/set RX0 AGC Threshold
          if(command[4]==';') {
            // send reply back
            sprintf(reply,"ZZAR%+04d;",(int)(receiver[0]->agc_gain));
            send_resp(client->fd,reply) ;
          } else {
            int threshold=atoi(&command[4]);
            set_agc_gain(VFO_A,(double)threshold);
          }
          break;
        case 'S': //ZZAS
          // read/set RX1 AGC Threshold
          if(receivers==2) {
            if(command[4]==';') {
              // send reply back
              sprintf(reply,"ZZAS%+04d;",(int)(receiver[1]->agc_gain));
              send_resp(client->fd,reply) ;
            } else {
              int threshold=atoi(&command[4]);
              set_agc_gain(VFO_B,(double)threshold);
            }
          }
          break;
        case 'T': //ZZAT
          implemented=FALSE;
          break;
        case 'U': //ZZAU
          // move VFO A up by selected step
          if(command[6]==';') {
            int step_index=atoi(&command[4]);
            long long hz=0;
            if(step_index>=0 && step_index<=14) {
              hz=(long long)steps[step_index];
            }
            if(hz!=0LL) {
              vfo_id_move(VFO_A,hz,FALSE);
            }
          } else {
          }
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'B': //ZZBx
      switch(command[3]) {
        case 'A': //ZZBA
          // move RX2 down one band
          if(command[4]==';') {
            if(receivers==2) {
              band_minus(receiver[1]->id);
            }
          }
          break;
        case 'B': //ZZBB
          // move RX2 up one band
          if(command[4]==';') {
            if(receivers==2) {
              band_plus(receiver[1]->id);
            }
          }
          break;
        case 'D': //ZZBD
          // move RX1 down one band
          if(command[4]==';') {
            band_minus(receiver[0]->id);
          }
          break;
        case 'E': //ZZBE
          // move VFO B down nn tune steps
          if(command[6]==';') {
            int steps=atoi(&command[4]);
            vfo_id_step(VFO_B,-steps);
          }

          break;
        case 'F': //ZZBF
          // move VFO B up nn tune steps
          if(command[6]==';') {
            int steps=atoi(&command[4]);
            vfo_id_step(VFO_B,+steps);
          }
          break;
        case 'G': //ZZBG
          implemented=FALSE;
          break;
        case 'I': //ZZBI
          implemented=FALSE;
          break;
        case 'M': //ZZBM
          // move VFO B down by selected step
          if(command[6]==';') {
            int step_index=atoi(&command[4]);
            long long hz=0;
            if(step_index>=0 && step_index<=14) {
              hz=(long long)steps[step_index];
            }
            if(hz!=0LL) {
              vfo_id_move(VFO_B,-hz,FALSE);
            }
          } else {
          }

          break;
        case 'P': //ZZBP
          // move VFO B up by selected step
          if(command[6]==';') {
            int step_index=atoi(&command[4]);
            long long hz=0;
            if(step_index>=0 && step_index<=14) {
              hz=(long long)steps[step_index];
            }
            if(hz!=0LL) {
              vfo_id_move(VFO_B,hz,FALSE);
            }
          } else {
          }
          break;
        case 'R': //ZZBR
          implemented=FALSE;
          break;
        case 'S': //ZZBS
          // set/read RX1 band switch
          if(command[4]==';') {
            int b;
            switch(vfo[VFO_A].band) {
              case band136:
                b=136;
                break;
              case band472:
                b=472;
                break;
              case band160:
                b=160;
                break;
              case band80:
                b=80;
                break;
              case band60:
                b=60;
                break;
              case band40:
                b=40;
                break;
              case band30:
                b=30;
                break;
              case band20:
                b=20;
                break;
              case band17:
                b=17;
                break;
              case band15:
                b=15;
                break;
              case band12:
                b=12;
                break;
              case band10:
                b=10;
                break;
              case band6:
                b=6;
                break;
              case bandGen:
                b=888;
                break;
              case bandWWV:
                b=999;
                break;
              default:
                b=20;
                break;
            }
            sprintf(reply,"ZZBS%03d;",b);
            send_resp(client->fd,reply) ;
          } else if(command[7]==';') {
            int band=band20;
            int b=atoi(&command[4]);
            switch(b) {
              case 136:
                band=band136;
                break;
              case 472:
                band=band472;
                break;
              case 160:
                band=band160;
                break;
              case 80:
                band=band80;
                break;
              case 60:
                band=band60;
                break;
              case 40:
                band=band40;
                break;
              case 30:
                band=band30;
                break;
              case 20:
                band=band20;
                break;
              case 17:
                band=band17;
                break;
              case 15:
                band=band15;
                break;
              case 12:
                band=band12;
                break;
              case 10:
                band=band10;
                break;
              case 6:
                band=band6;
                break;
              case 888:
                band=bandGen;
                break;
              case 999:
                band=bandWWV;
                break;
            }
            vfo_band_changed(VFO_A,band);
          }
          break;
        case 'T': //ZZBT
          // set/read RX2 band switch
          break;
        case 'U': //ZZBU
          // move RX1 up one band
          if(command[4]==';') {
            band_plus(receiver[0]->id);
          }
          break;
        case 'Y': //ZZBY
          // closes console (ignored)
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'C': //ZZCx
      switch(command[3]) {
        case 'B': //ZZCB
          implemented=FALSE;
          break;
        case 'D': //ZZCD
          implemented=FALSE;
          break;
        case 'F': //ZZCF
          implemented=FALSE;
          break;
        case 'I': //ZZCI
          implemented=FALSE;
          break;
        case 'L': //ZZCL
          implemented=FALSE;
          break;
        case 'M': //ZZCM
          implemented=FALSE;
          break;
        case 'N': //ZZCN
          // set/read VFO A CTUN
          if(command[4]==';') {
            // return the CTUN status
            sprintf(reply,"ZZCN%d;",vfo[VFO_A].ctun);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            int state=atoi(&command[4]);
            ctun_update(VFO_A,state);
            vfo_update();
          }
          break;
        case 'O': //ZZCO
          // set/read VFO B CTUN
          if(command[4]==';') {
            // return the CTUN status
            sprintf(reply,"ZZCO%d;",vfo[VFO_B].ctun);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            int state=atoi(&command[4]);
            ctun_update(VFO_B,state);
            vfo_update();
          }
          break;
        case 'P': //ZZCP
          // set/read compander
          if(command[4]==';') {
            sprintf(reply,"ZZCP%d;",0);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            // ignore
          }
          break;
        case 'S': //ZZCS
          implemented=FALSE;
          break;
        case 'T': //ZZCT
          implemented=FALSE;
          break;
        case 'U': //ZZCU
          implemented=FALSE;
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'D': //ZZDx
      switch(command[3]) {
        case 'A': //ZZDA
          break;
        case 'B': //ZZDB
          // set/read RX Reference
          if(command[4]==';') {
            sprintf(reply,"ZZDB%d;",0); // currently always 0
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            // ignore
          }
          break;
        case 'C': //ZZDC
          // set/get diversity gain
          if(command[4]==';') {
            sprintf(reply,"ZZDC%04d;",(int)div_gain);
            send_resp(client->fd,reply) ;
          } else if(command[8]==';') {
            // ignore
          }
          break;
        case 'D': //ZZDD
          // set/get diversity phase
          if(command[4]==';') {
            sprintf(reply,"ZZDD%04d;",(int)div_phase);
            send_resp(client->fd,reply) ;
          } else if(command[8]==';') {
            // ignore
          }
        case 'E': //ZZDE
          implemented=FALSE;
          break;
        case 'F': //ZZDF
          implemented=FALSE;
          break;
        case 'M': //ZZDM
          // set/read Display Mode
          if(command[4]==';') {
            int v=0;
            if(active_receiver->display_waterfall) {
              v=8;
            } else {
              v=2;
            }
            sprintf(reply,"ZZDM%d;",v);
            send_resp(client->fd,reply) ;
          } else {
          }
          break;
        case 'N': //ZZDN
          // set/read waterfall low
          if(command[4]==';') {
            sprintf(reply,"ZZDN%+4d;",active_receiver->waterfall_low);
            send_resp(client->fd,reply) ;
          } else {
          }
          break;
        case 'O': //ZZDO
          // set/read waterfall high
          if(command[4]==';') {
            sprintf(reply,"ZZDO%+4d;",active_receiver->waterfall_high);
            send_resp(client->fd,reply) ;
          } else {
          }
          break;
        case 'P': //ZZDP
          // set/read panadapter high
          if(command[4]==';') {
            sprintf(reply,"ZZDP%+4d;",active_receiver->panadapter_high);
            send_resp(client->fd,reply) ;
          } else {
          }
          break;
        case 'Q': //ZZDQ
          // set/read panadapter low
          if(command[4]==';') {
            sprintf(reply,"ZZDQ%+4d;",active_receiver->panadapter_low);
            send_resp(client->fd,reply) ;
          } else {
          }
          break;
        case 'R': //ZZDR
          // set/read panadapter step
          if(command[4]==';') {
            sprintf(reply,"ZZDR%2d;",active_receiver->panadapter_step);
            send_resp(client->fd,reply) ;
          } else {
          }
          break;
        case 'U': //ZZDU
          implemented=FALSE;
          break;
        case 'X': //ZZDX
          implemented=FALSE;
          break;
        case 'Y': //ZZDY
          implemented=FALSE;
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'E': //ZZEx
      switch(command[3]) {
        case 'A': //ZZEA
          // set/read rx equalizer values
          if(command[4]==';') {
            sprintf(reply,"ZZEA%03d%03d%03d%03d%03d00000000000000000000;",3,rx_equalizer[0],rx_equalizer[1],rx_equalizer[2],rx_equalizer[3]);
            send_resp(client->fd,reply) ;
          } else if(command[37]==';') {
            char temp[4];
            temp[3]='\0';
            strncpy(temp,&command[4],3);
            int bands=atoi(temp);
            if(bands==3) {
              strncpy(temp,&command[7],3);
              rx_equalizer[0]=atoi(temp);
              strncpy(temp,&command[10],3);
              rx_equalizer[1]=atoi(temp);
              strncpy(temp,&command[13],3);
              rx_equalizer[2]=atoi(temp);
            } else {
            }
          } else {
          }
          break;
        case 'B': //ZZEB
          // set/read tx equalizer values
          if(command[4]==';') {
            sprintf(reply,"ZZEB%03d%03d%03d%03d%03d00000000000000000000;",3,tx_equalizer[0],tx_equalizer[1],tx_equalizer[2],tx_equalizer[3]);
            send_resp(client->fd,reply) ;
          } else if(command[37]==';') {
            char temp[4];
            temp[3]='\0';
            strncpy(temp,&command[4],3);
            int bands=atoi(temp);
            if(bands==3) {
              strncpy(temp,&command[7],3);
              tx_equalizer[0]=atoi(temp);
              strncpy(temp,&command[10],3);
              tx_equalizer[1]=atoi(temp);
              strncpy(temp,&command[13],3);
              tx_equalizer[2]=atoi(temp);
            } else {
            }
          } else {
          }
          break;
        case 'M': //ZZEM
          implemented=FALSE;
          break;
        case 'R': //ZZER
          // set/read rx equalizer
          if(command[4]==';') {
            sprintf(reply,"ZZER%d;",enable_rx_equalizer);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            enable_rx_equalizer=atoi(&command[4]);
          } else {
          }
          break;
        case 'T': //ZZET
          // set/read tx equalizer
          if(command[4]==';') {
            sprintf(reply,"ZZET%d;",enable_tx_equalizer);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            enable_tx_equalizer=atoi(&command[4]);
          } else {
          }
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'F': //ZZFx
      switch(command[3]) {
        case 'A': //ZZFA
          // set/read VFO-A frequency
          if(command[4]==';') {
            if(vfo[VFO_A].ctun) {
              sprintf(reply,"ZZFA%011lld;",vfo[VFO_A].ctun_frequency);
            } else {
              sprintf(reply,"ZZFA%011lld;",vfo[VFO_A].frequency);
            }
            send_resp(client->fd,reply) ;
          } else if(command[15]==';') {
            long long f=atoll(&command[4]);
            local_set_frequency(VFO_A,f);
            vfo_update();
          }
          break;
        case 'B': //ZZFB
          // set/read VFO-B frequency
          if(command[4]==';') {
            if(vfo[VFO_B].ctun) {
              sprintf(reply,"ZZFB%011lld;",vfo[VFO_B].ctun_frequency);
            } else {
              sprintf(reply,"ZZFB%011lld;",vfo[VFO_B].frequency);
            }
            send_resp(client->fd,reply) ;
          } else if(command[15]==';') {
            long long f=atoll(&command[4]);
            local_set_frequency(VFO_B,f);
            vfo_update();
          }
          break;
        case 'D': //ZZFD
          // set/read deviation
          if(command[4]==';') {
            sprintf(reply,"ZZFD%d;",active_receiver->deviation==2500?0:1);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            int d=atoi(&command[4]);
            if(d==0) {
              active_receiver->deviation=2500;
            } else if(d==1) {
              active_receiver->deviation=5000;
            } else {
            }
            vfo_update();
          }
          break;
        case 'H': //ZZFH
          // set/read RX1 filter high
          if(command[4]==';') {
            sprintf(reply,"ZZFH%05d;",receiver[0]->filter_high);
            send_resp(client->fd,reply) ;
          } else if(command[9]==';') {
            int fh=atoi(&command[4]);
            fh=fmin(9999,fh);
            fh=fmax(-9999,fh);
            // make sure filter is filterVar1
            if(vfo[VFO_A].filter!=filterVar1) {
              vfo_filter_changed(filterVar1);
            }
            FILTER *mode_filters=filters[vfo[VFO_A].mode];
            FILTER *filter=&mode_filters[filterVar1];
            filter->high=fh;
            vfo_filter_changed(filterVar1);
          }
          break;
        case 'I': //ZZFI
          // set/read RX1 DSP receive filter
          if(command[4]==';') {
            sprintf(reply,"ZZFI%02d;",vfo[VFO_A].filter);
            send_resp(client->fd,reply) ;
          } else if(command[6]==';') {
            int filter=atoi(&command[4]);
            // update RX1 filter
          }
          break;
        case 'J': //ZZFJ
          // set/read RX2 DSP receive filter
          if(command[4]==';') {
            sprintf(reply,"ZZFJ%02d;",vfo[VFO_B].filter);
            send_resp(client->fd,reply) ;
          } else if(command[6]==';') {
            int filter=atoi(&command[4]);
            // update RX2 filter
          }
          break;
        case 'L': //ZZFL
          // set/read RX1 filter low
          if(command[4]==';') {
            sprintf(reply,"ZZFL%05d;",receiver[0]->filter_low);
            send_resp(client->fd,reply) ;
          } else if(command[9]==';') {
            int fl=atoi(&command[4]);
            fl=fmin(9999,fl);
            fl=fmax(-9999,fl);
            // make sure filter is filterVar1
            if(vfo[VFO_A].filter!=filterVar1) {
              vfo_filter_changed(filterVar1);
            }
            FILTER *mode_filters=filters[vfo[VFO_A].mode];
            FILTER *filter=&mode_filters[filterVar1];
            filter->low=fl;
            vfo_filter_changed(filterVar1);
          }
          break;
        case 'M': //ZZFM
          implemented=FALSE;
          break;
        case 'R': //ZZFR
          implemented=FALSE;
          break;
        case 'S': //ZZFS
          implemented=FALSE;
          break;
        case 'V': //ZZFV
          implemented=FALSE;
          break;
        case 'W': //ZZFW
          implemented=FALSE;
          break;
        case 'X': //ZZFX
          implemented=FALSE;
          break;
        case 'Y': //ZZFY
          implemented=FALSE;
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'G': //ZZGx
      switch(command[3]) {
        case 'E': //ZZGE
          implemented=FALSE;
          break;
        case 'L': //ZZGL
          implemented=FALSE;
          break;
        case 'T': //ZZGT
          // set/read RX1 AGC
          if(command[4]==';') {
            sprintf(reply,"ZZGT%d;",receiver[0]->agc);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            int agc=atoi(&command[4]);
            // update RX1 AGC
            receiver[0]->agc=agc;
            vfo_update();
          }
          break;
        case 'U': //ZZGU
          // set/read RX2 AGC
          if(command[4]==';') {
            sprintf(reply,"ZZGU%d;",receiver[1]->agc);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            int agc=atoi(&command[4]);
            // update RX2 AGC
            receiver[1]->agc=agc;
            vfo_update();
          }
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'H': //ZZHx
      switch(command[3]) {
        case 'A': //ZZHA
          implemented=FALSE;
          break;
        case 'R': //ZZHR
          implemented=FALSE;
          break;
        case 'T': //ZZHT
          implemented=FALSE;
          break;
        case 'U': //ZZHU
          implemented=FALSE;
          break;
        case 'V': //ZZHV
          implemented=FALSE;
          break;
        case 'W': //ZZHW
          implemented=FALSE;
          break;
        case 'X': //ZZHX
          implemented=FALSE;
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'I': //ZZIx
      switch(command[3]) {
        case 'D': //ZZID
          strcpy(reply,"ZZID240;");
          send_resp(client->fd,reply) ;
          break;
        case 'F': //ZZIF
          implemented=FALSE;
          break;
        case 'O': //ZZIO
          implemented=FALSE;
          break;
        case 'S': //ZZIS
          implemented=FALSE;
          break;
        case 'T': //ZZIT
          implemented=FALSE;
          break;
        case 'U': //ZZIU
          implemented=FALSE;
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'K': //ZZKx
      switch(command[3]) {
        case 'M': //ZZIM
          implemented=FALSE;
          break;
        case 'O': //ZZIO
          implemented=FALSE;
          break;
        case 'S': //ZZIS
          implemented=FALSE;
          break;
        case 'Y': //ZZIY
          implemented=FALSE;
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'L': //ZZLx
      switch(command[3]) {
        case 'A': //ZZLA
          // read/set RX0 gain
          if(command[4]==';') {
            // send reply back
            sprintf(reply,"ZZLA%03d;",(int)(receiver[0]->volume*100.0));
            send_resp(client->fd,reply) ;
          } else {
            int gain=atoi(&command[4]);
            receiver[0]->volume=(double)gain/100.0;
            update_af_gain();
          }
          break;
        case 'B': //ZZLB
          implemented=FALSE;
          break;
        case 'C': //ZZLC
          // read/set RX1 gain
          if(receivers==2) {
            if(command[4]==';') {
              // send reply back
              sprintf(reply,"ZZLC%03d;",(int)(receiver[1]->volume*100.0));
              send_resp(client->fd,reply) ;
            } else {
              int gain=atoi(&command[4]);
              receiver[1]->volume=(double)gain/100.0;
              update_af_gain();
            }
          }
          break;
        case 'D': //ZZLD
          implemented=FALSE;
          break;
        case 'E': //ZZLE
          implemented=FALSE;
          break;
        case 'F': //ZZLF
          implemented=FALSE;
          break;
        case 'G': //ZZLG
          implemented=FALSE;
          break;
        case 'H': //ZZLH
          implemented=FALSE;
          break;
        case 'I': //ZZLI
          if(transmitter!=NULL) {
            if(command[4]==';') {
              // send reply back
              sprintf(reply,"ZZLI%d;",transmitter->puresignal);
              send_resp(client->fd,reply) ;
            } else {
              int ps=atoi(&command[4]);
              transmitter->puresignal=ps;
            }
            vfo_update();
          }
          break;
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'M': //ZZMx
      switch(command[3]) {
        case 'A': //ZZMA
          implemented=FALSE;
          break;
        case 'B': //ZZMB
          implemented=FALSE;
          break;
        case 'D': //ZZMD
          // set/read RX1 operating mode
          if(command[4]==';') {
            sprintf(reply,"ZZMD%02d;",vfo[VFO_A].mode);
            send_resp(client->fd,reply);
          } else if(command[6]==';') {
            vfo_mode_changed(atoi(&command[4]));
          }
          break;
        case 'E': //ZZME
          // set/read RX2 operating mode
          if(command[4]==';') {
            sprintf(reply,"ZZMD%02d;",vfo[VFO_B].mode);
            send_resp(client->fd,reply);
          } else if(command[6]==';') {
            vfo_mode_changed(atoi(&command[4]));
          }
          break;
        case 'G': //ZZMG
          // set/read mic gain
          if(command[4]==';') {
            sprintf(reply,"ZZMG%03d;",(int)mic_gain);
            send_resp(client->fd,reply);
          } else if(command[7]==';') {
            mic_gain=(double)atoi(&command[4]);
          }
          break;
        case 'L': //ZZML
          // read DSP modes and indexes
          if(command[4]==';') {
            sprintf(reply,"ZZML LSB00: USB01: DSB02: CWL03: CWU04: FMN05:  AM06:DIGU07:SPEC08:DIGL09: SAM10: DRM11;");
            send_resp(client->fd,reply);
          }
          break;
        case 'N': //ZZMN
          // read Filter Names and indexes
          if(command[6]==';') {
            int mode=atoi(&command[4])-1;
            FILTER *f=filters[mode];
            sprintf(reply,"ZZMN");
            char temp[32];
            for(int i=0;i<FILTERS;i++) {
              sprintf(temp,"%5s%5d%5d",f[i].title,f[i].high,f[i].low);
              strcat(reply,temp);
            }
            strcat(reply,";");
            send_resp(client->fd,reply);
          }
          break;
        case 'O': //ZZMO
          // set/read MON status
          if(command[4]==';') {
            sprintf(reply,"ZZMO%d;",0);
            send_resp(client->fd,reply);
          }
          break;
        case 'R': //ZZMR
          // set/read RX Meter mode
          if(command[4]==';') {
            sprintf(reply,"ZZMR%d;",smeter+1);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            smeter=atoi(&command[4])-1;
          }
          break;
        case 'S': //ZZMS
          implemented=FALSE;
          break;
        case 'T': //ZZMT
          if(command[4]==';') {
            sprintf(reply,"ZZMT%02d;",1); // forward power
            send_resp(client->fd,reply);
          } else {
          }
          break;
        case 'U': //ZZMU
          implemented=FALSE;
          break;
        case 'V': //ZZMV
          implemented=FALSE;
          break;
        case 'W': //ZZMW
          implemented=FALSE;
          break;
        case 'X': //ZZMX
          implemented=FALSE;
          break;
        case 'Y': //ZZMY
          implemented=FALSE;
          break;
        case 'Z': //ZZMZ
          implemented=FALSE;
          break;
        default:
           implemented=FALSE;
           break;

      }
      break;
    case 'N': //ZZNx
      switch(command[3]) {
        case 'A': //ZZNA
          // set/read RX1 NB1
          if(command[4]==';') {
            sprintf(reply,"ZZNA%d;",receiver[0]->nb);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[0]->nb=atoi(&command[4]);
            if(receiver[0]->nb) {
              receiver[0]->nb2=0;
            }
            update_noise();
          }
          break;
        case 'B': //ZZNB
          // set/read RX1 NB2
          if(command[4]==';') {
            sprintf(reply,"ZZNB%d;",receiver[0]->nb2);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[0]->nb2=atoi(&command[4]);
            if(receiver[0]->nb2) {
              receiver[0]->nb=0;
            }
            update_noise();
          }
          break;
        case 'C': //ZZNC
          // set/read RX2 NB1
          if(command[4]==';') {
            sprintf(reply,"ZZNC%d;",receiver[1]->nb);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[1]->nb=atoi(&command[4]);
            if(receiver[1]->nb) {
              receiver[1]->nb2=0;
            }
            update_noise();
          }
          break;
        case 'D': //ZZND
          // set/read RX2 NB2
          if(command[4]==';') {
            sprintf(reply,"ZZND%d;",receiver[1]->nb2);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[1]->nb2=atoi(&command[4]);
            if(receiver[1]->nb2) {
              receiver[1]->nb=0;
            }
            update_noise();
          }
          break;
        case 'L': //ZZNL
          // set/read NB1 threshold
          implemented=FALSE;
          break;
        case 'M': //ZZNM
          // set/read NB2 threshold
          implemented=FALSE;
          break;
        case 'N': //ZZNN
          // set/read RX1 SNB status
          if(command[4]==';') {
            sprintf(reply,"ZZNN%d;",receiver[0]->snb);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[0]->snb=atoi(&command[4]);
            update_noise();
          }
          break;
        case 'O': //ZZNO
          // set/read RX2 SNB status
          if(command[4]==';') {
            sprintf(reply,"ZZNO%d;",receiver[1]->snb);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[1]->snb=atoi(&command[4]);
            update_noise();
          }
          break;
        case 'R': //ZZNR
          // set/read RX1 NR
          if(command[4]==';') {
            sprintf(reply,"ZZNR%d;",receiver[0]->nr);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[0]->nr=atoi(&command[4]);
            if(receiver[0]->nr) {
              receiver[0]->nr2=0;
            }
            update_noise();
          }
          break;
        case 'S': //ZZNS
          // set/read RX1 NR2
          if(command[4]==';') {
            sprintf(reply,"ZZNS%d;",receiver[0]->nr2);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[0]->nr2=atoi(&command[4]);
            if(receiver[0]->nr2) {
              receiver[0]->nr=0;
            }
            update_noise();
          }
          break;
        case 'T': //ZZNT
          // set/read RX1 ANF
          if(command[4]==';') {
            sprintf(reply,"ZZNT%d;",receiver[0]->anf);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[0]->anf=atoi(&command[4]);
            update_noise();
          }
          break;
        case 'U': //ZZNU
          // set/read RX2 ANF
          if(command[4]==';') {
            sprintf(reply,"ZZNU%d;",receiver[1]->anf);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[1]->anf=atoi(&command[4]);
            update_noise();
          }
          break;
        case 'V': //ZZNV
          // set/read RX2 NR
          if(command[4]==';') {
            sprintf(reply,"ZZNV%d;",receiver[1]->nr);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[1]->nr=atoi(&command[4]);
            if(receiver[1]->nr) {
              receiver[1]->nr2=0;
            }
            update_noise();
          }
          break;
        case 'W': //ZZNW
          // set/read RX2 NR2
          if(command[4]==';') {
            sprintf(reply,"ZZNW%d;",receiver[1]->nr2);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            receiver[1]->nr2=atoi(&command[4]);
            if(receiver[1]->nr2) {
              receiver[1]->nr=0;
            }
            update_noise();
          }
          break;
        default:
           implemented=FALSE;
           break;
      }
    case 'O': //ZZOx
      switch(command[3]) {
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'P': //ZZPx
      switch(command[3]) {
        case 'A': //ZZPA
          // set/read preamp setting
          if(command[4]==';') {
            int a=adc[receiver[0]->adc].attenuation;
            if(a==0) {
              a=1;
            } else if(a<=-30) {
              a=4;
            } else if(a<=-20) {
              a=0;
            } else if(a<=-10) {
              a=2;
            } else {
              a=3;
            }
            sprintf(reply,"ZZPA%d;",a);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            int a=atoi(&command[4]);
            switch(a) {
              case 0:
                adc[receiver[0]->adc].attenuation=-20;
                break;
              case 1:
                adc[receiver[0]->adc].attenuation=0;
                break;
              case 2:
                adc[receiver[0]->adc].attenuation=-10;
                break;
              case 3:
                adc[receiver[0]->adc].attenuation=-20;
                break;
              case 4:
                adc[receiver[0]->adc].attenuation=-30;
                break;
              default:
                adc[receiver[0]->adc].attenuation=0;
                break;
            }
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'Q': //ZZQx
      switch(command[3]) {
        default:
           implemented=FALSE;
           break;
      }
      break;
    case 'R': //ZZRx
      switch(command[3]) {
        case 'C': //ZZRC
          // clear RIT frequency
          if(command[4]==';') {
            vfo[VFO_A].rit=0;
            vfo_update();
          }
          break;
        case 'D': //ZZRD
          // decrement RIT frequency
          if(command[4]==';') {
            if(vfo[VFO_A].mode==modeCWL || vfo[VFO_A].mode==modeCWU) {
              vfo[VFO_A].rit-=10;
            } else {
              vfo[VFO_A].rit-=50;
            }
            vfo_update();
          } else if(command[9]==';') {
            vfo[VFO_A].rit=atoi(&command[4]);
            vfo_update();
          }
          break;
        case 'F': //ZZRF
          // set/read RIT frequency
          if(command[4]==';') {
            sprintf(reply,"ZZRF%+5lld;",vfo[VFO_A].rit);
            send_resp(client->fd,reply);
          } else if(command[9]==';') {
            vfo[VFO_A].rit=atoi(&command[4]);
            vfo_update();
          }
          break;
        case 'M': //ZZRM
          // read meter value
          if(command[5]==';') {
            int m=atoi(&command[4]);
            sprintf(reply,"ZZRM%d%20d;",smeter,(int)receiver[0]->meter);
            send_resp(client->fd,reply);
          }
          break;
        case 'S': //ZZRS
          // set/read RX2 enable
          if(command[4]==';') {
            sprintf(reply,"ZZRS%d;",receivers==2);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            int state=atoi(&command[4]);
            if(state) {
              radio_change_receivers(2);
            } else {
              radio_change_receivers(1);
            }
          }
          break;
        case 'T': //ZZRT
          // set/read RIT enable
          if(command[4]==';') {
            sprintf(reply,"ZZRT%d;",vfo[VFO_A].rit_enabled);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            vfo[VFO_A].rit_enabled=atoi(&command[4]);
            vfo_update();
          }
          break;
        case 'U': //ZZRU
          // increments RIT Frequency
          if(command[4]==';') {
            if(vfo[VFO_A].mode==modeCWL || vfo[VFO_A].mode==modeCWU) {
              vfo[VFO_A].rit+=10;
            } else {
              vfo[VFO_A].rit+=50;
            }
            vfo_update();
          } else if(command[9]==';') {
            vfo[VFO_A].rit=atoi(&command[4]);
            vfo_update();
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
    case 'S': //ZZSx
      switch(command[3]) {
        case 'A': //ZZSA
          // move VFO A down one step 
          if(command[4]==';') {
            vfo_step(-1);
          }
          break;
        case 'B': //ZZSB
          // move VFO A up one step 
          if(command[4]==';') {
            vfo_step(1);
          }
          break;
        case 'D': //ZZSD
          implemented=FALSE;
          break;
        case 'F': //ZZSF
          implemented=FALSE;
          break;
        case 'G': //ZZSG
          // move VFO B down 1 step
          if(command[4]==';') {
            vfo_id_step(VFO_B,-1);
          }
          break;
        case 'H': //ZZSH
          // move VFO B up 1 step
          if(command[4]==';') {
            vfo_id_step(VFO_B,1);
          }
          break;
        case 'M': //ZZSM
          // reads the S Meter (in dB)
          if(command[5]==';') {
            int v=atoi(&command[4]);
            if(v==VFO_A || v==VFO_B) {
              double m=receiver[v]->meter;
              m=fmax(-140.0,m);
              m=fmin(-10.0,m);
              sprintf(reply,"ZZSM%d%03d;",v,(int)((m+140.0)*2));
              send_resp(client->fd,reply);
            }
          }
          break;
        case 'N': //ZZSN
          implemented=FALSE;
          break;
        case 'P': //ZZSP
          // set/read split
          if(command[4]==';') {
            sprintf(reply,"ZZSP%d;",split);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
	    int val=atoi(&command[4]);
	    ext_set_split(GINT_TO_POINTER(val));
          }
          break;
        case 'R': //ZZSR
          implemented=FALSE;
          break;
        case 'S': //ZZSS
          implemented=FALSE;
          break;
        case 'T': //ZZST
          implemented=FALSE;
          break;
        case 'U': //ZZSU
          implemented=FALSE;
          break;
        case 'V': //ZZSV
          implemented=FALSE;
          break;
        case 'W': //ZZSW
          // set/read split
          if(command[4]==';') {
            sprintf(reply,"ZZSW%d;",split);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            int val=atoi(&command[4]);
            ext_set_split(GINT_TO_POINTER(val));
          }
          break;
        case 'Y': //ZZSY
          implemented=FALSE;
          break;
        case 'Z': //ZZSZ
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'T': //ZZTx
      switch(command[3]) {
        case 'A': //ZZTA
          implemented=FALSE;
          break;
        case 'B': //ZZTB
          implemented=FALSE;
          break;
        case 'F': //ZZTF
          implemented=FALSE;
          break;
        case 'H': //ZZTH
          implemented=FALSE;
          break;
        case 'I': //ZZTI
          implemented=FALSE;
          break;
        case 'L': //ZZTL
          implemented=FALSE;
          break;
        case 'M': //ZZTM
          implemented=FALSE;
          break;
        case 'O': //ZZTO
          implemented=FALSE;
          break;
        case 'P': //ZZTP
          implemented=FALSE;
          break;
        case 'S': //ZZTS
          implemented=FALSE;
          break;
        case 'U': //ZZTU
          // sets or reads TUN status
          if(command[4]==';') {
            sprintf(reply,"ZZTU%d;",tune);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            tune_update(atoi(&command[4]));
          }
          break;
        case 'V': //ZZTV
          implemented=FALSE;
          break;
        case 'X': //ZZTX
          // sets or reads MOX status
          if(command[4]==';') {
            sprintf(reply,"ZZTX%d;",mox);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            mox_update(atoi(&command[4]));
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'U': //ZZUx
      switch(command[3]) {
        case 'A': //ZZUA
          implemented=FALSE;
          break;
        case 'S': //ZZUS
          implemented=FALSE;
          break;
        case 'T': //ZZUT
          implemented=FALSE;
          break;
        case 'X': //ZZUX
          implemented=FALSE;
          break;
        case 'Y': //ZZUY
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'V': //ZZVx
      switch(command[3]) {
        case 'A': //ZZVA
          implemented=FALSE;
          break;
        case 'B': //ZZVB
          implemented=FALSE;
          break;
        case 'C': //ZZVC
          implemented=FALSE;
          break;
        case 'D': //ZZVD
          implemented=FALSE;
          break;
        case 'E': //ZZVE
          implemented=FALSE;
          break;
        case 'F': //ZZVF
          implemented=FALSE;
          break;
        case 'H': //ZZVH
          implemented=FALSE;
          break;
        case 'I': //ZZVI
          implemented=FALSE;
          break;
        case 'J': //ZZVJ
          implemented=FALSE;
          break;
        case 'K': //ZZVK
          implemented=FALSE;
          break;
        case 'L': //ZZVL
          // set/get VFO Lock
          implemented=FALSE;
          break;
        case 'M': //ZZVM
          implemented=FALSE;
          break;
        case 'N': //ZZVN
          implemented=FALSE;
          break;
        case 'O': //ZZVO
          implemented=FALSE;
          break;
        case 'P': //ZZVP
          implemented=FALSE;
          break;
        case 'Q': //ZZVQ
          implemented=FALSE;
          break;
        case 'R': //ZZVR
          implemented=FALSE;
          break;
        case 'S': //ZZVS
          implemented=FALSE;
          break;
        case 'T': //ZZVT
          implemented=FALSE;
          break;
        case 'U': //ZZVU
          implemented=FALSE;
          break;
        case 'V': //ZZVV
          implemented=FALSE;
          break;
        case 'W': //ZZVW
          implemented=FALSE;
          break;
        case 'X': //ZZVX
          implemented=FALSE;
          break;
        case 'Y': //ZZVY
          implemented=FALSE;
          break;
        case 'Z': //ZZVZ
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'W': //ZZWx
      switch(command[3]) {
        case 'A': //ZZWA
          implemented=FALSE;
          break;
        case 'B': //ZZWB
          implemented=FALSE;
          break;
        case 'C': //ZZWC
          implemented=FALSE;
          break;
        case 'D': //ZZWD
          implemented=FALSE;
          break;
        case 'E': //ZZWE
          implemented=FALSE;
          break;
        case 'F': //ZZWF
          implemented=FALSE;
          break;
        case 'G': //ZZWG
          implemented=FALSE;
          break;
        case 'H': //ZZWH
          implemented=FALSE;
          break;
        case 'J': //ZZWJ
          implemented=FALSE;
          break;
        case 'K': //ZZWK
          implemented=FALSE;
          break;
        case 'L': //ZZWL
          implemented=FALSE;
          break;
        case 'M': //ZZWM
          implemented=FALSE;
          break;
        case 'N': //ZZWN
          implemented=FALSE;
          break;
        case 'O': //ZZWO
          implemented=FALSE;
          break;
        case 'P': //ZZWP
          implemented=FALSE;
          break;
        case 'Q': //ZZWQ
          implemented=FALSE;
          break;
        case 'R': //ZZWR
          implemented=FALSE;
          break;
        case 'S': //ZZWS
          implemented=FALSE;
          break;
        case 'T': //ZZWT
          implemented=FALSE;
          break;
        case 'U': //ZZWU
          implemented=FALSE;
          break;
        case 'V': //ZZWV
          implemented=FALSE;
          break;
        case 'W': //ZZWW
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'X': //ZZXx
      switch(command[3]) {
        case 'C': //ZZXC
          // clear transmitter XIT
          if(command[4]==';') {
            transmitter->xit=0;
            vfo_update();
          }
          break;
        case 'F': //ZZXF
          // set/read XIT
          if(command[4]==';') {
            sprintf(reply,"ZZXT%+05lld;",transmitter->xit);
            send_resp(client->fd,reply) ;
          } else if(command[9]==';') {
            transmitter->xit=(long long)atoi(&command[4]);
            vfo_update();
          }
          break;
        case 'H': //ZZXH
          implemented=FALSE;
          break;
        case 'N': //ZZXN
          // read combined RX1 status
          if(command[4]==';') {
            int status=0;
            status=status|((receiver[0]->agc)&0x03);
            int a=adc[receiver[0]->adc].attenuation;
            if(a==0) {
              a=1;
            } else if(a<=-30) {
              a=4;
            } else if(a<=-20) {
              a=0;
            } else if(a<=-10) {
              a=2;
            } else {
              a=3;
            }
            status=status|((a&0x03)<<3);
            status=status|((receiver[0]->squelch_enable&0x01)<<6);
            status=status|((receiver[0]->nb&0x01)<<7);
            status=status|((receiver[0]->nb2&0x01)<<8);
            status=status|((receiver[0]->nr&0x01)<<9);
            status=status|((receiver[0]->nr2&0x01)<<10);
            status=status|((receiver[0]->snb&0x01)<<11);
            status=status|((receiver[0]->anf&0x01)<<12);
            sprintf(reply,"ZZXN%04d;",status);
            send_resp(client->fd,reply);
          }
          break;
        case 'O': //ZZXO
          // read combined RX2 status
          if(receivers==2) {
            if(command[4]==';') {
              int status=0;
              status=status|((receiver[1]->agc)&0x03);
              int a=adc[receiver[1]->adc].attenuation;
              if(a==0) {
                a=1;
              } else if(a<=-30) {
                a=4;
              } else if(a<=-20) {
                a=0;
              } else if(a<=-10) {
                a=2;
              } else {
                a=3;
              }
              status=status|((a&0x03)<<3);
              status=status|((receiver[1]->squelch_enable&0x01)<<6);
              status=status|((receiver[1]->nb&0x01)<<7);
              status=status|((receiver[1]->nb2&0x01)<<8);
              status=status|((receiver[1]->nr&0x01)<<9);
              status=status|((receiver[1]->nr2&0x01)<<10);
              status=status|((receiver[1]->snb&0x01)<<11);
              status=status|((receiver[1]->anf&0x01)<<12);
              sprintf(reply,"ZZXO%04d;",status);
              send_resp(client->fd,reply);
            }
          }
          break;
        case 'S': //ZZXS
          /// set/read XIT enable
          if(command[4]==';') {
            sprintf(reply,"ZZXS%d;",transmitter->xit_enabled);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            transmitter->xit_enabled=atoi(&command[4]);
            vfo_update();
          }
          break;
        case 'T': //ZZXT
          implemented=FALSE;
          break;
        case 'V': //ZZXV
          // read combined VFO status
          if(command[4]==';') {
            int status=0;
            if(vfo[VFO_A].rit_enabled) {
              status=status|0x01;
            }
            if(locked) {
              status=status|0x02;
              status=status|0x04;
            }
            if(split) {
              status=status|0x08;
            }
            if(vfo[VFO_A].ctun) {
              status=status|0x10;
            }
            if(vfo[VFO_B].ctun) {
              status=status|0x20;
            }
            if(mox) {
              status=status|0x40;
            }
            if(tune) {
              status=status|0x80;
            }
            sprintf(reply,"ZZXV%03d;",status);
            send_resp(client->fd,reply);
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'Y': //ZZYx
      switch(command[3]) {
	case 'A': //ZZYA
          implemented=FALSE;
          break;
	case 'B': //ZZYB
          implemented=FALSE;
          break;
	case 'C': //ZZYC
          implemented=FALSE;
          break;
	case 'R': //ZZYR
          // switch receivers
          if(command[5]==';') {
            int v=atoi(&command[4]);
            if(v==0) {
              active_receiver=receiver[0];
            } else if(v==1) {
              if(receivers==2) {
                active_receiver=receiver[1];
              }
            }
            vfo_update();
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'Z': //ZZZx
      switch(command[3]) {
        case 'A': //ZZZA
          implemented=FALSE;
          break;
	case 'B': //ZZZB
          implemented=FALSE;
          break;
	case 'Z': //ZZZZ
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    default:
       implemented=FALSE;
       break;
  }
  return implemented;
}

// called with g_idle_add so that the processing is running on the main thread
int parse_cmd(void *data) {
  COMMAND *info=(COMMAND *)data;
  CLIENT *client=info->client;
  char *command=info->command;
  char reply[80];
  reply[0]='\0';
  gboolean implemented=TRUE;
  gboolean errord=FALSE;

  switch(command[0]) {
    case 'A':
      switch(command[1]) {
        case 'C': //AC
          // set/read internal atu status
          implemented=FALSE;
          break;
        case 'G': //AG
          // set/read AF Gain
          if(command[2]==';') {
            // send reply back (covert from 0..1 to 0..255)
            sprintf(reply,"AG0%03d;",(int)(receiver[0]->volume*255.0));
            send_resp(client->fd,reply) ;
          } else if(command[6]==';') {
            int gain=atoi(&command[3]);
            receiver[0]->volume=(double)gain/255.0;
            update_af_gain();
          }
        case 'I': //AI
          // set/read Auto Information
          implemented=FALSE;
          break;
        case 'L': // AL
          // set/read Auto Notch level
          implemented=FALSE;
          break;
        case 'M': // AM
          // set/read Auto Mode
          implemented=FALSE;
          break;
        case 'N': // AN
          // set/read Antenna Connection
          implemented=FALSE;
          break;
        case 'S': // AS
          // set/read Auto Mode Function Parameters
          implemented=FALSE;
          break;
        case 'I': //AI
          // set/read Auto Information
          implemented=FALSE;
          break;
        case 'L': // AL
          // set/read Auto Notch level
          implemented=FALSE;
          break;
        case 'M': // AM
          // set/read Auto Mode
          implemented=FALSE;
          break;
        case 'N': // AN
          // set/read Antenna Connection
          implemented=FALSE;
          break;
        case 'S': // AS
          // set/read Auto Mode Function Parameters
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'B':
      switch(command[1]) {
        case 'C': //BC
          // set/read Beat Canceller
          implemented=FALSE;
          break;
        case 'D': //BD
          //band down 1 band
          band_minus(receiver[0]->id);
          break;
        case 'P': //BP
          // set/read Manual Beat Canceller frequency
          implemented=FALSE;
          break;
        case 'U': //BU
          //band up 1 band
          band_plus(receiver[0]->id);
          break;
        case 'Y': //BY
          // read busy signal
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'C':
      switch(command[1]) {
        case 'A': //CA
          // set/read CW Auto Tune
          implemented=FALSE;
          break;
        case 'G': //CG
          // set/read Carrier Gain
          implemented=FALSE;
          break;
        case 'I': //CI
          // sets the current frequency to the CALL Channel
          implemented=FALSE;
          break;
        case 'M': //CM
          // sets/reads the Packet Cluster Tune function
          implemented=FALSE;
          break;
        case 'N': //CN
          // sets/reads CTCSS function
          if(command[3]==';') {
            sprintf(reply,"CN%02d;",transmitter->ctcss+1);
            send_resp(client->fd,reply) ;
          } else if(command[4]==';') {
            int i=atoi(&command[2])-1;
            transmitter_set_ctcss(transmitter,transmitter->ctcss_enabled,i);
          }
          break;
        case 'T': //CT
          // sets/reads CTCSS status
          if(command[3]==';') {
            sprintf(reply,"CT%d;",transmitter->ctcss_enabled);
            send_resp(client->fd,reply) ;
          } else if(command[3]==';') {
            int state=atoi(&command[2]);
            transmitter_set_ctcss(transmitter,state,transmitter->ctcss);
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'D':
      switch(command[1]) {
        case 'C': //DC
          // set/read TX band status
          implemented=FALSE;
          break;
        case 'N': //DN
          // move VFO A down 1 step size
          vfo_id_step(VFO_A,-1);
          break;
        case 'Q': //DQ
          // set/read DCS function status
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'E':
      switch(command[1]) {
        case 'X': //EX
          // set/read the extension menu
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'F':
      switch(command[1]) {
        case 'A': //FA
          // set/read VFO-A frequency
          if(command[2]==';') {
            if(vfo[VFO_A].ctun) {
              sprintf(reply,"FA%011lld;",vfo[VFO_A].ctun_frequency);
            } else {
              sprintf(reply,"FA%011lld;",vfo[VFO_A].frequency);
            }
            send_resp(client->fd,reply) ;
          } else if(command[13]==';') {
            long long f=atoll(&command[2]);
            local_set_frequency(VFO_A,f);
            vfo_update();
          }
          break;
        case 'B': //FB
          // set/read VFO-B frequency
          if(command[2]==';') {
            if(vfo[VFO_B].ctun) {
              sprintf(reply,"FB%011lld;",vfo[VFO_B].ctun_frequency);
            } else {
              sprintf(reply,"FB%011lld;",vfo[VFO_B].frequency);
            }
            send_resp(client->fd,reply) ;
          } else if(command[13]==';') {
            long long f=atoll(&command[2]);
            local_set_frequency(VFO_B,f);
            vfo_update();
          }
          break;
        case 'C': //FC
          // set/read the sub receiver VFO frequency menu
          implemented=FALSE;
          break;
        case 'D': //FD
          // set/read the filter display dot pattern
          implemented=FALSE;
          break;
        case 'R': //FR
          // set/read transceiver receive VFO
          if(command[2]==';') {
            sprintf(reply,"FR%d;",active_receiver->id);
            send_resp(client->fd,reply) ;
          } else if(command[3]==';') {
            int id=atoi(&command[2]);
            switch(id) {
              case 0:
                active_receiver=receiver[id];
                break;
              case 1:
                if(receivers==2) {
                  active_receiver=receiver[id];
                } else {
                  implemented=FALSE;
                }
                break;
              default:
                implemented=FALSE;
                break;
            }
          }
          break;
        case 'S': //FS
          // set/read the fine tune function status
          implemented=FALSE;
          break;
        case 'T': //FT
          // set/read transceiver transmit VFO
          if(command[2]==';') {
            sprintf(reply,"FT%d;",split);
            send_resp(client->fd,reply) ;
          } else if(command[3]==';') {
            int val=atoi(&command[2]);
            ext_set_split(GINT_TO_POINTER(val));
          }
          break;
        case 'W': //FW
          // set/read filter width
          // make sure filter is filterVar1
          if(vfo[active_receiver->id].filter!=filterVar1) {
            vfo_filter_changed(filterVar1);
          }
          FILTER *mode_filters=filters[vfo[active_receiver->id].mode];
          FILTER *filter=&mode_filters[filterVar1];
          int val=0;
          if(command[2]==';') {
            switch(vfo[active_receiver->id].mode) {
              case modeCWL:
              case modeCWU:
                val=2*filter->high;
                break;
              case modeAM:
              case modeSAM:
                val=filter->low>=-4000;
                break;
              case modeFMN:
                val=active_receiver->deviation==5000;
                break;
              default:
                implemented=FALSE;
                break;
            }
            if(implemented) {
              sprintf(reply,"FW%04d;",val);
              send_resp(client->fd,reply) ;
            }
          } else if(command[6]==';') {
            int fw=atoi(&command[2]);
            int val=
            filter->low=fw;
            switch(vfo[active_receiver->id].mode) {
              case modeCWL:
              case modeCWU:
                filter->low=fw/2;
                filter->high=fw/2;
                break;
              case modeFMN:
                if(fw==0) {
                  filter->low=-4000;
                  filter->high=4000;
                  active_receiver->deviation=2500;
                } else {
                  filter->low=-8000;
                  filter->high=8000;
                  active_receiver->deviation=5000;
                }
                break;
              case modeAM:
              case modeSAM:
                if(fw==0) {
                  filter->low=-4000;
                  filter->high=4000;
                } else {
                  filter->low=-8000;
                  filter->high=8000;
                }
                break;
              default:
                implemented=FALSE;
                break;
            }
            if(implemented) {
              vfo_filter_changed(filterVar1);
            }
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'G':
      switch(command[1]) {
        case 'T': //GT
          // set/read RX1 AGC
          if(command[2]==';') {
            sprintf(reply,"GT%03d;",receiver[0]->agc*5);
            send_resp(client->fd,reply) ;
          } else if(command[5]==';') {
            // update RX1 AGC
            receiver[0]->agc=atoi(&command[2])/5;
            vfo_update();
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'H':
      switch(command[1]) {
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'I':
      switch(command[1]) {
        case 'D': //ID
          // get ID
          strcpy(reply,"ID019;"); // TS-2000
          send_resp(client->fd,reply);
          break;
        case 'F': //IF
          sprintf(reply,"IF%011lld%04lld%+06lld%d%d000%d%d%d0%d%d%02d0;",
                  vfo[VFO_A].ctun?vfo[VFO_A].ctun_frequency:vfo[VFO_A].frequency,
                  step,vfo[VFO_A].rit,vfo[VFO_A].rit_enabled,transmitter==NULL?0:transmitter->xit_enabled,
                  mox,split,0,split?1:0,transmitter->ctcss_enabled,transmitter->ctcss);
          send_resp(client->fd,reply);
          break;
        case 'S': //IS
          // set/read IF shift
          if(command[2]==';') {
            strcpy(reply,"IS 0000;");
            send_resp(client->fd,reply);
          } else {
            implemented=FALSE;
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'J':
      switch(command[1]) {
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'K':
      switch(command[1]) {
        case 'S': //KS
          // set/read keying speed
          if(command[2]==';') {
            sprintf(reply,"KS%03d;",cw_keyer_speed);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            int speed=atoi(&command[2]);
            if(speed>=1 && speed<=60) {
              cw_keyer_speed=speed;
#ifdef LOCALCW
              keyer_update();
#endif
              vfo_update();
            }
          } else {
          }
          break;
        case 'Y': //KY
          // convert the chaaracters into Morse Code
          if(command[2]==';') {
            sprintf(reply,"KY%d;",cw_busy);
            send_resp(client->fd,reply);
          } else if(command[27]==';') {
            if(cw_busy==0) {
              strncpy(cw_buf,&command[3],24);
              cw_busy=1;
            }
          } else {
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'L':
      switch(command[1]) {
        case 'K': //LK
          // set/read key lock
          if(command[2]==';') {
            sprintf(reply,"LK%d%d;",locked,locked);
            send_resp(client->fd,reply);
          } else if(command[27]==';') {
            locked=command[2]=='1';
            vfo_update();
          }
          break;
        case 'M': //LM
          // set/read keyer recording status
          implemented=FALSE;
          break;
        case 'T': //LT
          // set/read ALT fucntion status
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'M':
      switch(command[1]) {
        case 'C': //MC
          // set/read Memory Channel
          implemented=FALSE;
          break;
        case 'D': //MD
          // set/read operating mode
          if(command[2]==';') {
            int mode=1;
            switch(vfo[VFO_A].mode) {
              case modeLSB:
                mode=1;
                break;
              case modeUSB:
                mode=2;
                break;
              case modeCWL:
                mode=7;
                break;
              case modeCWU:
                mode=3;
                break;
              case modeFMN:
                mode=4;
                break;
              case modeAM:
              case modeSAM:
                mode=5;
                break;
              case modeDIGL:
                mode=6;
                break;
              case modeDIGU:
                mode=9;
                break;
              default:
                break;
            }
            sprintf(reply,"MD%d;",mode);
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            int mode=modeUSB;
            switch(atoi(&command[2])) {
              case 1:
                mode=modeLSB;
                break;
              case 2:
                mode=modeUSB;
                break;
              case 3:
                mode=modeCWU;
                break;
              case 4:
                mode=modeFMN;
                break;
              case 5:
                mode=modeAM;
                break;
              case 6:
                mode=modeDIGL;
                break;
              case 7:
                mode=modeCWL;
                break;
              case 9:
                mode=modeDIGU;
                break;
              default:
                break;
            }
            vfo_mode_changed(mode);
          }
          break;
        case 'F': //MF
          // set/read Menu
          implemented=FALSE;
          break;
        case 'G': //MG
          // set/read Menu Gain (-12..60 converts to 0..100)
          if(command[2]==';') {
            sprintf(reply,"MG%03d;",(int)(((mic_gain+12.0)/72.0)*100.0));
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            double gain=(double)atoi(&command[2]);
            gain=((gain/100.0)*72.0)-12.0;
            set_mic_gain(gain);
          }
          break;
        case 'L': //ML
          // set/read Monitor Function Level
          implemented=FALSE;
          break;
        case 'O': //MO
          // set/read Monitor Function On/Off
          implemented=FALSE;
          break;
        case 'R': //MR
          // read Memory Channel
          implemented=FALSE;
          break;
        case 'U': //MU
          // set/read Memory Group
          implemented=FALSE;
          break;
        case 'W': //MW
          // Write Memory Channel
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'N':
      switch(command[1]) {
        case 'B': //NB
          // set/read noise blanker
          if(command[2]==';') {
            sprintf(reply,"NB%d;",active_receiver->nb);
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            active_receiver->nb=atoi(&command[2]);
            if(active_receiver->nb) {
              active_receiver->nb2=0;
            }
            update_noise();
          }
          break;
        case 'L': //NL
          // set/read noise blanker level
          implemented=FALSE;
          break;
        case 'R': //NR
          // set/read noise reduction
          if(command[2]==';') {
            int n=0;
            if(active_receiver->nr) {
              n=1;
            } else if(active_receiver->nr2) {
              n=2;
            }
            sprintf(reply,"NR%d;",n);
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            int n=atoi(&command[2]);
            switch(n) {
              case 0: // NR OFF
                active_receiver->nr=0;
                active_receiver->nr2=0;
                break;
              case 1: // NR ON
                active_receiver->nr=1;
                active_receiver->nr2=0;
                break;
              case 2: // NR2 ON
                active_receiver->nr=0;
                active_receiver->nr2=1;
                break;
            }
            update_noise();
          }
          break;
        case 'T': //NT
          // set/read ANF
          if(command[2]==';') {
            sprintf(reply,"NT%d;",active_receiver->anf);
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            active_receiver->anf=atoi(&command[2]);
            SetRXAANFRun(active_receiver->id, active_receiver->anf);
            vfo_update();
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'O':
      switch(command[1]) {
        case 'F': //OF
          // set/read offset frequency
          implemented=FALSE;
          break;
        case 'I': //OI
          // set/read offset frequency
          implemented=FALSE;
          break;
        case 'S': //OS
          // set/read offset function status
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'P':
      switch(command[1]) {
        case 'A': //PA
          // set/read preamp function status
          if(command[2]==';') {
            sprintf(reply,"PA%d0;",active_receiver->preamp);
            send_resp(client->fd,reply);
          } else if(command[4]==';') {
            active_receiver->preamp=command[2]=='1';
          }
          break;
        case 'B': //PB
          // set/read FRU-3A playback status
          implemented=FALSE;
          break;
        case 'C': //PC
          // set/read PA Power
          if(command[2]==';') {
            sprintf(reply,"PC%03d;",(int)transmitter->drive);
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            set_drive((double)atoi(&command[2]));
          }
          break;
        case 'I': //PI
          // store in program memory channel
          implemented=FALSE;
          break;
        case 'K': //PK
          // read packet cluster data
          implemented=FALSE;
          break;
        case 'L': //PL
          // set/read speach processor input/output level
          if(command[2]==';') {
            sprintf(reply,"PL%03d000;",(int)((transmitter->compressor_level/20.0)*100.0));
            send_resp(client->fd,reply);
          } else if(command[8]==';') {
            command[5]='\0';
            double level=(double)atoi(&command[2]);
            level=(level/100.0)*20.0;
            transmitter_set_compressor_level(transmitter,level);
            vfo_update();
          }
          break;
        case 'M': //PM
          // recall program memory
          implemented=FALSE;
          break;
        case 'R': //PR
          // set/read speech processor function
          implemented=FALSE;
          break;
        case 'S': //PS
          // set/read Power (always ON)
          if(command[2]==';') {
            sprintf(reply,"PS1;");
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            // ignore set
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'Q':
      switch(command[1]) {
        case 'C': //QC
          // set/read DCS code
          implemented=FALSE;
          break;
        case 'I': //QI
          // store in quick memory
          implemented=FALSE;
          break;
        case 'R': //QR
          // set/read quick memory channel data
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'R':
      switch(command[1]) {
        case 'A': //RA
          // set/read Attenuator function
          if(command[2]==';') {
            int att=0;
            if(have_rx_gain) {
              att=(int)(adc_attenuation[active_receiver->adc]+12);
              att=(int)(((double)att/60.0)*99.0);
            } else {
              att=(int)(adc_attenuation[active_receiver->adc]);
              att=(int)(((double)att/31.0)*99.0);
            }
            sprintf(reply,"RA%02d00;",att);
            send_resp(client->fd,reply);
          } else if(command[4]==';') {
            int att=atoi(&command[2]);
            if(have_rx_gain) {
              att=(int)((((double)att/99.0)*60.0)-12.0);
            } else {
              att=(int)(((double)att/99.0)*31.0);
            }
            set_attenuation_value((double)att);
          }
          break;
        case 'C': //RC
          // clears RIT
          if(command[2]==';') {
            vfo[VFO_A].rit=0;
            vfo_update();
          }
          break;
        case 'D': //RD
          // decrements RIT Frequency
          if(command[2]==';') {
            if(vfo[VFO_A].mode==modeCWL || vfo[VFO_A].mode==modeCWU) {
              vfo[VFO_A].rit-=10;
            } else {
              vfo[VFO_A].rit-=50;
            }
            vfo_update();
          } else if(command[7]==';') {
            vfo[VFO_A].rit=atoi(&command[2]);
            vfo_update();
          }
          break;
        case 'G': //RG
          // set/read RF gain status
          implemented=FALSE;
          break;
        case 'L': //RL
          // set/read noise reduction level
          implemented=FALSE;
          break;
        case 'M': //RM
          // set/read meter function
          implemented=FALSE;
          break;
        case 'T': //RT
          // set/read RIT enable
          if(command[2]==';') {
            sprintf(reply,"RT%d;",vfo[VFO_A].rit_enabled);
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            vfo[VFO_A].rit_enabled=atoi(&command[2]);
            vfo_update();
          }
          break;
        case 'U': //RU
          // increments RIT Frequency
          if(command[2]==';') {
            if(vfo[VFO_A].mode==modeCWL || vfo[VFO_A].mode==modeCWU) {
              vfo[VFO_A].rit+=10;
            } else {
              vfo[VFO_A].rit+=50;
            }
            vfo_update();
          } else if(command[7]==';') {
            vfo[VFO_A].rit=atoi(&command[2]);
            vfo_update();
          }
          break;
        case 'X': //RX
          // set transceiver to RX mode
          if(command[2]==';') {
            mox_update(0);
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'S':
      switch(command[1]) {
        case 'A': //SA
          // set/read stallite mode status
          if(command[2]==';') {
            sprintf(reply,"SA%d%d%d%d%d%d%dSAT?    ;",sat_mode==SAT_MODE|sat_mode==RSAT_MODE,0,0,0,sat_mode=SAT_MODE,sat_mode=RSAT_MODE,0);
            send_resp(client->fd,reply);
          } else if(command[9]==';') {
            if(command[2]=='0') {
              sat_mode=SAT_NONE;
            } else if(command[2]=='1') {
              if(command[6]=='0' && command[7]=='0') {
                sat_mode=SAT_NONE;
              } else if(command[6]=='1' && command[7]=='0') {
                sat_mode=SAT_MODE;
              } else if(command[6]=='0' && command[7]=='1') {
                sat_mode=RSAT_MODE;
              } else {
                implemented=FALSE;
              }
            }
          } else {
            implemented=FALSE;
          }
          break;
        case 'B': //SB
          // set/read SUB,TF-W status
          implemented=FALSE;
          break;
        case 'C': //SC
          // set/read SCAN function status
          implemented=FALSE;
          break;
        case 'D': //SD
          // set/read CW break-in time delay
          if(command[2]==';') {
            sprintf(reply,"SD%04d;",(int)fmin(cw_keyer_hang_time,1000));
            send_resp(client->fd,reply);
          } else if(command[6]==';') {
            int b=fmin(atoi(&command[2]),1000);
            cw_breakin=b==0;
            cw_keyer_hang_time=b;
          } else {
            implemented=FALSE;
          }
          break;
        case 'H': //SH
          {
          // set/read filter high
          // make sure filter is filterVar1
          if(vfo[active_receiver->id].filter!=filterVar1) {
            vfo_filter_changed(filterVar1);
          }
          FILTER *mode_filters=filters[vfo[active_receiver->id].mode];
          FILTER *filter=&mode_filters[filterVar1];
	  if(command[2]==';') {
            int fh=5;
            int high=filter->high;
            if(vfo[active_receiver->id].mode==modeLSB) {
              high=abs(filter->low);
            }
            if(high<=1400) {
              fh=0;
            } else if(high<=1600) {
              fh=1;
            } else if(high<=1800) {
              fh=2;
            } else if(high<=2000) {
              fh=3;
            } else if(high<=2200) {
              fh=4;
            } else if(high<=2400) {
              fh=5;
            } else if(high<=2600) {
              fh=6;
            } else if(high<=2800) {
              fh=7;
            } else if(high<=3000) {
              fh=8;
            } else if(high<=3400) {
              fh=9;
            } else if(high<=4000) {
              fh=10;
            } else {
              fh=11;
            }
            sprintf(reply,"SH%02d;",fh);
            send_resp(client->fd,reply) ;
          } else if(command[4]==';') {
            int i=atoi(&command[2]);
            int fh=100;
            switch(vfo[active_receiver->id].mode) {
              case modeLSB:
              case modeUSB:
              case modeFMN:
                switch(i) {
                  case 0:
                    fh=1400;
                    break;
                  case 1:
                    fh=1600;
                    break;
                  case 2:
                    fh=1800;
                    break;
                  case 3:
                    fh=2000;
                    break;
                  case 4:
                    fh=2200;
                    break;
                  case 5:
                    fh=2400;
                    break;
                  case 6:
                    fh=2600;
                    break;
                  case 7:
                    fh=2800;
                    break;
                  case 8:
                    fh=3000;
                    break;
                  case 9:
                    fh=3400;
                    break;
                  case 10:
                    fh=4000;
                    break;
                  case 11:
                    fh=5000;
                    break;
                  default:
                    fh=100;
                    break;
                }
                break;
              case modeAM:
              case modeSAM:
                switch(i) {
                  case 0:
                    fh=10;
                    break;
                  case 1:
                    fh=100;
                    break;
                  case 2:
                    fh=200;
                    break;
                  case 3:
                    fh=500;
                    break;
                  default:
                    fh=100;
                    break;
                }
                break;
            }
            if(vfo[active_receiver->id].mode==modeLSB) {
              filter->low=-fh;
            } else {
              filter->high=fh;
            }
            vfo_filter_changed(filterVar1);
          }
          }
          break;
        case 'I': //SI
          // enter satellite memory name
          implemented=FALSE;
          break;
        case 'L': //SL
          {
          // set/read filter low
          // make sure filter is filterVar1
          if(vfo[active_receiver->id].filter!=filterVar1) {
            vfo_filter_changed(filterVar1);
          }
          FILTER *mode_filters=filters[vfo[active_receiver->id].mode];
          FILTER *filter=&mode_filters[filterVar1];
	  if(command[2]==';') {
            int fl=2;
            int low=filter->low;
            if(vfo[active_receiver->id].mode==modeLSB) {
              low=abs(filter->high);
            }
          
            if(low<=10) {
              fl=0;
            } else if(low<=50) {
              fl=1;
            } else if(low<=100) {
              fl=2;
            } else if(low<=200) {
              fl=3;
            } else if(low<=300) {
              fl=4;
            } else if(low<=400) {
              fl=5;
            } else if(low<=500) {
              fl=6;
            } else if(low<=600) {
              fl=7;
            } else if(low<=700) {
              fl=8;
            } else if(low<=800) {
              fl=9;
            } else if(low<=900) {
              fl=10;
            } else {
              fl=11;
            }
            sprintf(reply,"SL%02d;",fl);
            send_resp(client->fd,reply) ;
          } else if(command[4]==';') {
            int i=atoi(&command[2]);
            int fl=100;
            switch(vfo[active_receiver->id].mode) {
              case modeLSB:
              case modeUSB:
              case modeFMN:
                switch(i) {
                  case 0:
                    fl=10;
                    break;
                  case 1:
                    fl=50;
                    break;
                  case 2:
                    fl=100;
                    break;
                  case 3:
                    fl=200;
                    break;
                  case 4:
                    fl=300;
                    break;
                  case 5:
                    fl=400;
                    break;
                  case 6:
                    fl=500;
                    break;
                  case 7:
                    fl=600;
                    break;
                  case 8:
                    fl=700;
                    break;
                  case 9:
                    fl=800;
                    break;
                  case 10:
                    fl=900;
                    break;
                  case 11:
                    fl=1000;
                    break;
                  default:
                    fl=100;
                    break;
                }
                break;
              case modeAM:
              case modeSAM:
                switch(i) {
                  case 0:
                    fl=10;
                    break;
                  case 1:
                    fl=100;
                    break;
                  case 2:
                    fl=200;
                    break;
                  case 3:
                    fl=500;
                    break;
                  default:
                    fl=100;
                    break;
                }
                break;
            }
            if(vfo[active_receiver->id].mode==modeLSB) {
              filter->high=-fl;
            } else {
              filter->low=fl;
            }
            vfo_filter_changed(filterVar1);
          }
          }
          break;
        case 'M': //SM
          // read the S meter
          if(command[3]==';') {
            int id=atoi(&command[2]);
            if(id==0 || id==1) {
              sprintf(reply,"SM%04d;",(int)receiver[id]->meter);
              send_resp(client->fd,reply);
            }
          }
          break;
        case 'Q': //SQ
          // set/read Squelch level
          if(command[3]==';') {
            int p1=atoi(&command[2]);
            if(p1==0) { // Main receiver
              sprintf(reply,"SQ%d%03d;",p1,(int)((double)active_receiver->squelch/100.0*255.0));
              send_resp(client->fd,reply);
            }
          } else if(command[6]==';') {
            if(command[2]=='0') {
              int p2=atoi(&command[3]);
              active_receiver->squelch=(int)((double)p2/255.0*100.0);
              set_squelch();
            }
          } else {
          }
          break;
        case 'R': //SR
          // reset transceiver
          implemented=FALSE;
          break;
        case 'S': //SS
          // set/read program scan pause frequency
          implemented=FALSE;
          break;
        case 'T': //ST
          // set/read MULTI/CH channel frequency steps
          implemented=FALSE;
          break;
        case 'U': //SU
          // set/read program scan pause frequency
          implemented=FALSE;
          break;
        case 'V': //SV
          // execute memory transfer function
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'T':
      switch(command[1]) {
        case 'C': //TC
          // set/read internal TNC mode
          implemented=FALSE;
          break;
        case 'D': //TD
          // send DTMF memory channel data
          implemented=FALSE;
          break;
        case 'I': //TI
          // read TNC LED status
          implemented=FALSE;
          break;
        case 'N': //TN
          // set/read sub-tone frequency
          implemented=FALSE;
          break;
        case 'O': //TO
          // set/read TONE function
          implemented=FALSE;
          break;
        case 'S': //TS
          // set/read TF-SET function
          implemented=FALSE;
          break;
        case 'X': //TX
          // set transceiver to TX mode
          if(command[2]==';') {
            mox_update(1);
          }
          break;
        case 'Y': //TY
          // set/read microprocessor firmware type
          implemented=FALSE;
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'U':
      switch(command[1]) {
        case 'L': //UL
          // detects the PLL unlock status
          implemented=FALSE;
          break;
        case 'P': //UP
          // move VFO A up by step
          if(command[2]==';') {
            vfo_step(1);
          }
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'V':
      switch(command[1]) {
        case 'D': //VD
          // set/read VOX delay time
          implemented=FALSE;
          break;
        case 'G': //VG
          // set/read VOX gain (0..9)
          if(command[2]==';') {
            // convert 0.0..1.0 to 0..9
            sprintf(reply,"VG%03d;",(int)((vox_threshold*100.0)*0.9));
            send_resp(client->fd,reply);
          } else if(command[5]==';') {
            // convert 0..9 to 0.0..1.0
            vox_threshold=atof(&command[2])/9.0;
            vfo_update();
          }
          break;
        case 'R': //VR
          // emulate VOICE1 or VOICE2 key
          implemented=FALSE;
          break;
        case 'X': //VX
          // set/read VOX status
          if(command[2]==';') {
            sprintf(reply,"VX%d;",vox_enabled);
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            vox_enabled=atoi(&command[2]);
            vfo_update();
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'W':
      switch(command[1]) {
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'X':
      switch(command[1]) {
        case 'T': //XT
          // set/read XIT enable
          if(command[2]==';') {
            sprintf(reply,"XT%d;",transmitter->xit_enabled);
            send_resp(client->fd,reply);
          } else if(command[3]==';') {
            transmitter->xit_enabled=atoi(&command[2]);
            vfo_update();
          }
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'Y':
      switch(command[1]) {
        default:
          implemented=FALSE;
          break;
      }
      break;
    case 'Z':
      switch(command[1]) {
        case 'Z':
          implemented=parse_extended_cmd (command,client);
          break;
        default:
          implemented=FALSE;
          break;
      }
      break;
    default:
      implemented=FALSE;
      break;
  }

  if(!implemented) {
    if(rigctl_debug) g_print("RIGCTL: UNIMPLEMENTED COMMAND: %s\n",info->command);
    send_resp(client->fd,"?;");
  }

  g_free(info->command);
  g_free(info);
  return 0;
}

// Serial Port Launch
int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                g_print ("RIGCTL: Error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        //tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
        tty.c_iflag |= (IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                g_print( "RIGCTL: Error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}

void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                g_print ("RIGCTL: Error %d from tggetattr\n", errno);
                return;
        }
        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                g_print("RIGCTL: error %d setting term attributes\n", errno);
}

static gpointer serial_server(gpointer data) {
     // We're going to Read the Serial port and
     // when we get data we'll send it to parse_cmd
     CLIENT *client=(CLIENT *)data;
     char cmd_input[MAXDATASIZE];
     char *command=g_new(char,MAXDATASIZE);
     int command_index=0;
     int numbytes;
     int i;
     g_mutex_lock(&mutex_a->m);
     cat_control++;
     if(rigctl_debug) g_print("RIGCTL: SER INC cat_contro=%d\n",cat_control);
     g_mutex_unlock(&mutex_a->m);
     g_idle_add(ext_vfo_update,NULL);
     serial_running=TRUE;
     while(serial_running) {
       numbytes = read (fd, cmd_input, sizeof cmd_input);
       if(numbytes>0) {
         for(i=0;i<numbytes;i++) {
           command[command_index]=cmd_input[i];
           command_index++;
           if(cmd_input[i]==';') { 
             command[command_index]='\0';
             if(rigctl_debug) g_print("RIGCTL: command=%s\n",command);
             COMMAND *info=g_new(COMMAND,1);
             info->client=client;
             info->command=command;
             g_mutex_lock(&mutex_busy->m);
             g_idle_add(parse_cmd,info);
             g_mutex_unlock(&mutex_busy->m);

             command=g_new(char,MAXDATASIZE);
             command_index=0;
           }
         }
       } else if(numbytes<0) {
         break;
       }
       //usleep(100L);
     }
     close(client->fd);
     g_mutex_lock(&mutex_a->m);
     cat_control--;
     if(rigctl_debug) g_print("RIGCTL: SER DEC - cat_control=%d\n",cat_control);
     g_mutex_unlock(&mutex_a->m);
     g_idle_add(ext_vfo_update,NULL);
     return NULL;
}

int launch_serial () {
     g_print("RIGCTL: Launch Serial port %s\n",ser_port);
     if(mutex_b_exists == 0) {
        mutex_b = g_new(GT_MUTEX,1);
        g_mutex_init(&mutex_b->m);
        mutex_b_exists = 1;
     }

     if(mutex_busy==NULL) {
       mutex_busy = g_new(GT_MUTEX,1);
       g_print("launch_serial: mutex_busy=%p\n",mutex_busy);
       g_mutex_init(&mutex_busy->m);
     }
     
     fd = open (ser_port, O_RDWR | O_NOCTTY | O_SYNC);   
     if (fd < 0)
     {
        g_print("RIGCTL: Error %d opening %s: %s\n", errno, ser_port, strerror (errno));
        return 0 ;
     }

     g_print("serial port fd=%d\n",fd);

     set_interface_attribs (fd, serial_baud_rate, serial_parity); 
     set_blocking (fd, 0);                   // set no blocking

     CLIENT *serial_client=g_new(CLIENT,1);
     serial_client->fd=fd;

     serial_server_thread_id = g_thread_new( "Serial server", serial_server, serial_client);
     if(!serial_server_thread_id )
     {
       g_free(serial_client);
       g_print("g_thread_new failed on serial_server\n");
       return 0;
     }
     return 1;
}

// Serial Port close
void disable_serial () {
     g_print("RIGCTL: Disable Serial port %s\n",ser_port);
     serial_running=FALSE;
     g_thread_join(serial_server_thread_id);
     close(fd);
}

//
// 2-25-17 - K5JAE - create each thread with the pointer to the port number  
//                   (Port numbers now const ints instead of defines..) 
//
void launch_rigctl () {
   
   g_print( "LAUNCHING RIGCTL!!\n");

   rigctl_busy = 1;
   mutex_a = g_new(GT_MUTEX,1);
   g_mutex_init(&mutex_a->m);

   if(mutex_b_exists == 0) {
      mutex_b = g_new(GT_MUTEX,1);
      g_mutex_init(&mutex_b->m);
      mutex_b_exists = 1;
   }
   
   mutex_c = g_new(GT_MUTEX,1);
   g_mutex_init(&mutex_c->m);

   mutex_busy = g_new(GT_MUTEX,1);
g_print("launch_rigctl: mutex_busy=%p\n",mutex_busy);
   g_mutex_init(&mutex_busy->m);

   // This routine encapsulates the thread call
   rigctl_server_thread_id = g_thread_new( "rigctl server", rigctl_server, GINT_TO_POINTER(rigctl_port_base));
   if( ! rigctl_server_thread_id )
   {
     g_print("g_thread_new failed on rigctl_server\n");
   }
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

void set_freqB(long long new_freqB) {      

   vfo[VFO_B].frequency = new_freqB;   
   g_idle_add(ext_vfo_update,NULL);
}


int set_alc(gpointer data) {
    int * lcl_ptr = (int *) data;
    alc = *lcl_ptr;
    g_print("RIGCTL: set_alc=%d\n",alc);
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


