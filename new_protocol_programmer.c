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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <sys/stat.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <math.h>
#include <errno.h>


#include "discovered.h"
#include "new_protocol.h"

static long sequence;
static long block_sequence;

static struct sockaddr_in program_addr;
static int program_addr_length;

static char* source;
static long blocks;

static pthread_t programmer_thread_id;
static pthread_t programmer_response_thread_id;

static int running;

void *programmer_thread(void *arg);
void *programmer_response_thread(void *arg);

void new_protocol_programmer(char *filename ) {
    int rc;

    fprintf(stderr,"new_protocol_programmer: %s\n",filename);

    sequence=0L;
    block_sequence=0L;

    DISCOVERED* d=&discovered[selected_device];

    memcpy(&program_addr,&d->info.network.address,d->info.network.address_length);
    program_addr_length=d->info.network.address_length;
    program_addr.sin_port=htons(PROGRAMMING_FROM_HOST_PORT);

    FILE *fp;
    long length;

    fp=fopen(filename,"rb");

    if(fp==NULL) {
       fprintf(stderr, "file %s not found!\n", filename);
       exit(-1);
    }

    fseek(fp,0L,SEEK_END);
    length=ftell(fp);
    fseek(fp,0L,SEEK_SET);
    fprintf(stderr,"the file's length is %ld bytes\n",length);

    long source_size=((length+511)/512)*512;
    fprintf(stderr,"allocating %ld bytes\n",source_size);
    source=(char *)malloc(source_size);

    blocks=source_size/256;
 
    // read the file in
    int r;
    int b=0;
    while((r==fread(&source[b],sizeof(char),512, fp))>0) {
        b+=r;
    }

    fclose(fp);

    // start the thread to program 
    rc=pthread_create(&programmer_thread_id,NULL,programmer_thread,NULL);
    if(rc != 0) {
        fprintf(stderr,"pthread_create failed on programmer_thread: rc=%d\n", rc);
        exit(-1);
    }

}

void programmer_erase() {
    char buffer[60];

    fprintf(stderr,"programmer_erase\n");
    memset(buffer, 0, sizeof(buffer));

    buffer[0]=sequence>>24;
    buffer[1]=sequence>>16;
    buffer[2]=sequence>>8;
    buffer[3]=sequence;

    buffer[4]=0x04; // Erase command

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&program_addr,program_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for erase\n");
        exit(1);
    }

}

void programmer_send_block(long block) {
    char buffer[265];

    fprintf(stderr,"programmer_send_block: %ld\n",block);

    memset(buffer, 0, sizeof(buffer));

    buffer[0]=block_sequence>>24;
    buffer[1]=block_sequence>>16;
    buffer[2]=block_sequence>>8;
    buffer[3]=block_sequence;

    buffer[4]=0x05; // Program command


    buffer[5]=blocks>>24;
    buffer[6]=blocks>>16;
    buffer[7]=blocks>>8;
    buffer[8]=blocks;
    

    // copy a block (256 bytes)
    memcpy(&buffer[9],&source[block*256],256);

    if(sendto(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&program_addr,program_addr_length)<0) {
        fprintf(stderr,"sendto socket failed for erase\n");
        exit(1);
    }
}

void *programmer_thread(void *arg) {
    int response;
    int result;
    struct timespec ts;

    fprintf(stderr,"programmer_thread\n");
    programmer_erase();

    // wait for the response to the erase command
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec+=120; // wait for 30 seconds
    result=sem_timedwait(&response_sem,&ts);
    if(result==-1) {
        if(errno == ETIMEDOUT) {
            fprintf(stderr,"timedout waiting for response for erase (start)\n");
            exit(1);
        } else {
            fprintf(stderr,"sem_timedwait failed for response for erase (start): %d\n",errno);
            exit(1);
        }
    }
    if(response!=0x03) {
        fprintf(stderr,"response error %d for erase (start)\n", response);
        exit(1);
    }

    // wait for the erase to complete
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec+=120; // wait for 30 seconds
    result=sem_timedwait(&response_sem,&ts);
    if(result==-1) {
        if(errno == ETIMEDOUT) {
            fprintf(stderr,"timedout waiting for response for erase (complete)\n");
            exit(1);
        } else {
            fprintf(stderr,"sem_timedwait failed for response for erase (complete): %d\n",errno);
            exit(1);
        }
    }
    if(response!=0x03) {
        fprintf(stderr,"response error %d for erase (complete)\n", response);
        exit(1);
    }

    long b=0;

    while(b<blocks) {
        programmer_send_block(b);
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec+=5; // wait for 5 seconds
        result=sem_timedwait(&response_sem,&ts);
        if(result==-1) {
            if(errno == ETIMEDOUT) {
                fprintf(stderr,"timedout waiting for response for sent block\n");
                exit(1);
            } else {
                fprintf(stderr,"sem_timedwait failed for response for sent block: %d\n",errno);
                exit(1);
            }
        }
        if(response!=0x04) {
            fprintf(stderr,"response error %d waiting for sent block\n", response);
            exit(1);
        }
        block_sequence++;
    }
    
}
