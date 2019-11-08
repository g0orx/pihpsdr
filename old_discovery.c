/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#include "discovered.h"
#include "discovery.h"
#include "old_discovery.h"

static char interface_name[64];
static struct sockaddr_in interface_addr={0};
static struct sockaddr_in interface_netmask={0};
static int interface_length;

#define DISCOVERY_PORT 1024
static int discovery_socket;
static struct sockaddr_in discovery_addr;

static GThread *discover_thread_id;
static gpointer discover_receive_thread(gpointer data);

static void discover(struct ifaddrs* iface) {
    int rc;
    struct sockaddr_in *sa;
    struct sockaddr_in *mask;
    struct sockaddr_in to_addr={0};
    int flags;
    struct timeval tv;
    int optval;
    socklen_t optlen;
    fd_set fds;
    unsigned char buffer[1032];
    int i, len;

    if (iface == NULL) {
	//
	// This indicates that we want to connect to an SDR which
	// cannot be reached by (UDP) broadcast packets, but that
	// we know its fixed IP address
	// Therefore we try to send a METIS detection packet via TCP 
	// to a "fixed" ip address.
	//
        fprintf(stderr,"Trying to detect at TCP addr %s\n", ipaddr_tcp);
	memset(&to_addr, 0, sizeof(to_addr));
	to_addr.sin_family = AF_INET;
	if (inet_aton(ipaddr_tcp, &to_addr.sin_addr) == 0) {
	    fprintf(stderr,"discover: TCP addr %s is invalid!\n",ipaddr_tcp);
	    return;
	}
	to_addr.sin_port=htons(DISCOVERY_PORT);

        discovery_socket=socket(AF_INET, SOCK_STREAM, 0);
        if(discovery_socket<0) {
            perror("discover: create socket failed for TCP discovery_socket\n");
            return;
	}
	//
	// Here I tried a bullet-proof approach to connect() such that the program
        // does not "hang" under any circumstances.
	// - First, one makes the socket non-blocking. Then, the connect() will
        //   immediately return with error EINPROGRESS.
	// - Then, one uses select() to look for *writeability* and check
	//   the socket error if everything went right. Since one calls select()
        //   with a time-out, one either succeed within this time or gives up.
        // - Do not forget to make the socket blocking again.
	//
        // Step 1. Make socket non-blocking and connect()
	flags=fcntl(discovery_socket, F_GETFL, 0);
	fcntl(discovery_socket, F_SETFL, flags | O_NONBLOCK);
	rc=connect(discovery_socket, (const struct sockaddr *)&to_addr, sizeof(to_addr));
        if ((errno != EINPROGRESS) && (rc < 0)) {
            perror("discover: connect() failed for TCP discovery_socket:");
	    close(discovery_socket);
	    return;
	}
	// Step 2. Use select to wait for the connection
        tv.tv_sec=3;
	tv.tv_usec=0;
	FD_ZERO(&fds);
	FD_SET(discovery_socket, &fds);
	rc=select(discovery_socket+1, NULL, &fds, NULL, &tv);
        if (rc < 0) {
            perror("discover: select() failed on TCP discovery_socket:");
	    close(discovery_socket);
	    return;
        }
	// If no connection occured, return
	if (rc == 0) {
	    // select timed out
	    fprintf(stderr,"discover: select() timed out on TCP discovery socket\n");
	    close(discovery_socket);
	    return;
	}
	// Step 3. select() succeeded. Check success of connect()
	optlen=sizeof(int);
	rc=getsockopt(discovery_socket, SOL_SOCKET, SO_ERROR, &optval, &optlen);
	if (rc < 0) {
	    // this should very rarely happen
            perror("discover: getsockopt() failed on TCP discovery_socket:");
	    close(discovery_socket);
	    return;
	}
	if (optval != 0) {
	    // connect did not succeed
	    fprintf(stderr,"discover: connect() on TCP socket did not succeed\n");
	    close(discovery_socket);
	    return;
	}
	// Step 4. reset the socket to normal (blocking) mode
	fcntl(discovery_socket, F_SETFL, flags &  ~O_NONBLOCK);
    } else {

        strcpy(interface_name,iface->ifa_name);
        fprintf(stderr,"discover: looking for HPSDR devices on %s\n", interface_name);

        // send a broadcast to locate hpsdr boards on the network
        discovery_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(discovery_socket<0) {
            perror("discover: create socket failed for discovery_socket:");
            exit(-1);
        }

        sa = (struct sockaddr_in *) iface->ifa_addr;
        mask = (struct sockaddr_in *) iface->ifa_netmask;
        interface_netmask.sin_addr.s_addr = mask->sin_addr.s_addr;

        // bind to this interface and the discovery port
        interface_addr.sin_family = AF_INET;
        interface_addr.sin_addr.s_addr = sa->sin_addr.s_addr;
        //interface_addr.sin_port = htons(DISCOVERY_PORT*2);
        interface_addr.sin_port = htons(0); // system assigned port
        if(bind(discovery_socket,(struct sockaddr*)&interface_addr,sizeof(interface_addr))<0) {
            perror("discover: bind socket failed for discovery_socket:");
            return;
        }

        fprintf(stderr,"discover: bound to %s\n",interface_name);

        // allow broadcast on the socket
        int on=1;
        rc=setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
        if(rc != 0) {
            fprintf(stderr,"discover: cannot set SO_BROADCAST: rc=%d\n", rc);
            exit(-1);
        }

        // setup to address
        to_addr.sin_family=AF_INET;
        to_addr.sin_port=htons(DISCOVERY_PORT);
        to_addr.sin_addr.s_addr=htonl(INADDR_BROADCAST);
    }
    optval = 1;
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    rc=devices;
    // start a receive thread to collect discovery response packets
    discover_thread_id = g_thread_new( "old discover receive", discover_receive_thread, NULL);
    if( ! discover_thread_id )
    {
        fprintf(stderr,"g_thread_new failed on discover_receive_thread\n");
        exit( -1 );
    }



    // send discovery packet
    // If this is a TCP connection, send a "long" packet
    len=63;
    if (iface == NULL) len=1032;
    buffer[0]=0xEF;
    buffer[1]=0xFE;
    buffer[2]=0x02;
    for(i=3;i<len;i++) {
        buffer[i]=0x00;
    }

    if(sendto(discovery_socket,buffer,len,0,(struct sockaddr*)&to_addr,sizeof(to_addr))<0) {
        perror("discover: sendto socket failed for discovery_socket:");
        return;
    }

    // wait for receive thread to complete
    g_thread_join(discover_thread_id);

    close(discovery_socket);

    if (iface == NULL) {
      fprintf(stderr,"discover: exiting TCP discover for %s\n",ipaddr_tcp);
      if (devices == rc+1) {
	//
	// We have exactly found one TCP device
	// and have to patch the TCP addr into the device field
	// and set the "use TCP" flag.
	//
        memcpy((void*)&discovered[rc].info.network.address,(void*)&to_addr,sizeof(to_addr));
        discovered[rc].info.network.address_length=sizeof(to_addr);
        memcpy((void*)&discovered[rc].info.network.interface_address,(void*)&to_addr,sizeof(to_addr));
        memcpy((void*)&discovered[rc].info.network.interface_netmask,(void*)&to_addr,sizeof(to_addr));
        discovered[rc].info.network.interface_length=sizeof(to_addr);
        strcpy(discovered[rc].info.network.interface_name,"TCP");
	discovered[rc].use_tcp=1;
      }
    } else {
      fprintf(stderr,"discover: exiting discover for %s\n",iface->ifa_name);
    }

}

#ifdef STEMLAB_DISCOVERY
//
// We have just started the SDR app on the RedPitaya without using AVAHI.
// Therefore we must send a discovery packet and analyze its response
// to have all information to start the radio.
// Since this essentially what happens in the original discovery process,
// we put this function HERE and not into stemlab_discovery.c
//
int stemlab_get_info(int id) {
  int ret;
  unsigned char buffer[1032];
  int optval,i;

  // Allow RP app to come up
  sleep(2);

  devices=id;

  discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (discovery_socket < 0) {
    perror("stemlab_get_info socket():");
    return(1);
  }

  optval = 1;
  setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

  memset(&discovery_addr, 0, sizeof(discovery_addr));
  discovery_addr.sin_family=AF_INET;
  discovery_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  // use system-assigned port in case 1024 is in use
  // e.g. by an HPSDR simulator
  discovery_addr.sin_port=htons(0);
  ret=bind(discovery_socket, (struct sockaddr *)&discovery_addr, sizeof(discovery_addr));
  if (ret < 0) {
    perror("stemlab_get_info bind():");
    return 1;
  }
  setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

  // start a receive thread to collect discovery response packets
  discover_thread_id = g_thread_new( "old discover receive", discover_receive_thread, NULL);
  if( ! discover_thread_id )
  {
      fprintf(stderr,"g_thread_new failed on discover_receive_thread\n");
      return 1;
  }

  // send discovery packet
  buffer[0]=0xEF;
  buffer[1]=0xFE;
  buffer[2]=0x02;
  for(i=3;i<63;i++) {
      buffer[i]=0x00;
  }

  ret=sendto(discovery_socket,buffer,63,0,
             (struct sockaddr*)&(discovered[id].info.network.address),
             sizeof(discovered[id].info.network.address));
  if (ret < 63) {
    perror("stemlab_get_info UDP sendto():");
    return -1;
  }

  // wait for receive thread to complete
  g_thread_join(discover_thread_id);

  close(discovery_socket);

  // if all went well, we have now filled in data for exactly
  // one device.
  if (devices == id+1) return 0;

  if (devices > id+1) return 1;

  // If we have not received an answer, then possibly the STEMlab is located
  // on a different subnet and UDP packets are not correctly routed through.
  // In this case, we try to open a connection using TCP.

  discovery_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (discovery_socket < 0) {
    perror("stemlab_get_info socket():");
    return 1;
  }

  optval = 1;
  setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
  ret=connect(discovery_socket, (struct sockaddr*)&(discovered[id].info.network.address),
                                  sizeof(discovered[id].info.network.address));
  if (ret < 0) {
    perror("stemlab_get_info connect():");
    return 1;
  }

  // send discovery packet
  buffer[0]=0xEF;
  buffer[1]=0xFE;
  buffer[2]=0x02;
  for(i=3;i<1032;i++) {
      buffer[i]=0x00;
  }

  // start a receive thread to collect discovery response packets
  discover_thread_id = g_thread_new( "old discover receive", discover_receive_thread, NULL);
  if( ! discover_thread_id )
  {
      fprintf(stderr,"g_thread_new failed on discover_receive_thread\n");
      return 1;
  }

  ret=sendto(discovery_socket,buffer,1032,0,
             (struct sockaddr*)&(discovered[id].info.network.address),
             sizeof(discovered[id].info.network.address));
  if (ret < 1032) {
    perror("stemlab_get_info TCP  sendto():");
    return -1;
  }

  // wait for receive thread to complete
  g_thread_join(discover_thread_id);

  close(discovery_socket);

  // if all went well, we have now filled in data for exactly
  // one device. We must set a flag that this one can ONLY do
  // TCP
  if (devices == id+1) {
     fprintf(stderr,"UDP did not work, but TCP does!\n");
     discovered[id].use_tcp=1;
     return 0;
  }
  return 1;
}
#endif

//static void *discover_receive_thread(void* arg) {
static gpointer discover_receive_thread(gpointer data) {
    struct sockaddr_in addr;
    socklen_t len;
    unsigned char buffer[2048];
    int bytes_read;
    struct timeval tv;
    int i;

fprintf(stderr,"discover_receive_thread\n");

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    len=sizeof(addr);
    while(1) {
        bytes_read=recvfrom(discovery_socket,buffer,sizeof(buffer),1032,(struct sockaddr*)&addr,&len);
        if(bytes_read<0) {
            fprintf(stderr,"discovery: bytes read %d\n", bytes_read);
            perror("old_discovery: recvfrom socket failed for discover_receive_thread");
            break;
        }
        if (bytes_read == 0) break;
        fprintf(stderr,"old_discovery: received %d bytes\n",bytes_read);
        if ((buffer[0] & 0xFF) == 0xEF && (buffer[1] & 0xFF) == 0xFE) {
            int status = buffer[2] & 0xFF;
            if (status == 2 || status == 3) {
                if(devices<MAX_DEVICES) {
                    discovered[devices].protocol=ORIGINAL_PROTOCOL;
                    discovered[devices].device=buffer[10]&0xFF;
                    switch(discovered[devices].device) {
                        case DEVICE_METIS:
                            strcpy(discovered[devices].name,"Metis");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=61440000.0;
                            break;
                        case DEVICE_HERMES:
                            strcpy(discovered[devices].name,"Hermes");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=61440000.0;
                            break;
                        case DEVICE_GRIFFIN:
                            strcpy(discovered[devices].name,"Griffin");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=61440000.0;
                            break;
                        case DEVICE_ANGELIA:
                            strcpy(discovered[devices].name,"Angelia");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=61440000.0;
                            break;
                        case DEVICE_ORION:
                            strcpy(discovered[devices].name,"Orion");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=61440000.0;
                            break;
                        case DEVICE_HERMES_LITE:
#ifdef RADIOBERRY
                            strcpy(discovered[devices].name,"Radioberry");
#else
                            strcpy(discovered[devices].name,"Hermes Lite");		
#endif
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=30720000.0;
                            break;
                        case DEVICE_ORION2:
                            strcpy(discovered[devices].name,"Orion2");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=61440000.0;
                            break;
			case DEVICE_STEMLAB:
			    // This is in principle the same as HERMES but has two ADCs
			    // (and therefore, can do DIVERSITY).
                            strcpy(discovered[devices].name,"STEMlab");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=30720000.0;
                            break;
                        default:
                            strcpy(discovered[devices].name,"Unknown");
                            discovered[devices].frequency_min=0.0;
                            discovered[devices].frequency_max=61440000.0;
                            break;
                    }
g_print("old_discovery: name=%s min=%f max=%f\n",discovered[devices].name, discovered[devices].frequency_min, discovered[devices].frequency_max);
                    discovered[devices].software_version=buffer[9]&0xFF;
                    for(i=0;i<6;i++) {
                        discovered[devices].info.network.mac_address[i]=buffer[i+3];
                    }
                    discovered[devices].status=status;
                    memcpy((void*)&discovered[devices].info.network.address,(void*)&addr,sizeof(addr));
                    discovered[devices].info.network.address_length=sizeof(addr);
                    memcpy((void*)&discovered[devices].info.network.interface_address,(void*)&interface_addr,sizeof(interface_addr));
                    memcpy((void*)&discovered[devices].info.network.interface_netmask,(void*)&interface_netmask,sizeof(interface_netmask));
                    discovered[devices].info.network.interface_length=sizeof(interface_addr);
                    strcpy(discovered[devices].info.network.interface_name,interface_name);
		    discovered[devices].use_tcp=0;
		    fprintf(stderr,"old_discovery: found device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s min=%f max=%f\n",
                            discovered[devices].device,
                            discovered[devices].software_version,
                            discovered[devices].status,
                            inet_ntoa(discovered[devices].info.network.address.sin_addr),
                            discovered[devices].info.network.mac_address[0],
                            discovered[devices].info.network.mac_address[1],
                            discovered[devices].info.network.mac_address[2],
                            discovered[devices].info.network.mac_address[3],
                            discovered[devices].info.network.mac_address[4],
                            discovered[devices].info.network.mac_address[5],
                            discovered[devices].info.network.interface_name,
                            discovered[devices].frequency_min,
                            discovered[devices].frequency_max);
                    devices++;
                }
            }
        }

    }
    fprintf(stderr,"discovery: exiting discover_receive_thread\n");
    g_thread_exit(NULL);
    return NULL;
}

void old_discovery() {
    struct ifaddrs *addrs,*ifa;

fprintf(stderr,"old_discovery\n");
    getifaddrs(&addrs);
    ifa = addrs;
    while (ifa) {
        g_main_context_iteration(NULL, 0);
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
			#ifdef RADIOBERRY
			if((ifa->ifa_flags&IFF_UP)==IFF_UP
                && (ifa->ifa_flags&IFF_RUNNING)==IFF_RUNNING) {
				discover(ifa);
			}
			#else
            if((ifa->ifa_flags&IFF_UP)==IFF_UP
                && (ifa->ifa_flags&IFF_RUNNING)==IFF_RUNNING
                && (ifa->ifa_flags&IFF_LOOPBACK)!=IFF_LOOPBACK) {
                discover(ifa);
            }
			#endif 
        }
        ifa = ifa->ifa_next;
    }
    freeifaddrs(addrs);

    // Do one additional "discover" for a fixed TCP address
    discover(NULL);

    fprintf(stderr, "discovery found %d devices\n",devices);

    int i;
    for(i=0;i<devices;i++) {
                    fprintf(stderr,"discovery: found device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n",
                            discovered[i].device,
                            discovered[i].software_version,
                            discovered[i].status,
                            inet_ntoa(discovered[i].info.network.address.sin_addr),
                            discovered[i].info.network.mac_address[0],
                            discovered[i].info.network.mac_address[1],
                            discovered[i].info.network.mac_address[2],
                            discovered[i].info.network.mac_address[3],
                            discovered[i].info.network.mac_address[4],
                            discovered[i].info.network.mac_address[5],
                            discovered[i].info.network.interface_name);
    }

}

