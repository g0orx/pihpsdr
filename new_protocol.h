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

#ifndef _NEW_PROTOCOL_H
#define _NEW_PROTOCOL_H

#include <semaphore.h>
#include "receiver.h"

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
#define RX_IQ_TO_HOST_PORT_0 1035
#define RX_IQ_TO_HOST_PORT_1 1036
#define RX_IQ_TO_HOST_PORT_2 1037
#define RX_IQ_TO_HOST_PORT_3 1038
#define RX_IQ_TO_HOST_PORT_4 1039
#define RX_IQ_TO_HOST_PORT_5 1040
#define RX_IQ_TO_HOST_PORT_6 1041
#define RX_IQ_TO_HOST_PORT_7 1042


#define MIC_SAMPLES 64

extern int data_socket;
#ifdef __APPLE__
extern sem_t *response_sem;
#else
extern sem_t response_sem;
#endif

/*
extern long response_sequence;
extern int response;
*/

extern unsigned int exciter_power;
extern unsigned int alex_forward_power;
extern unsigned int alex_reverse_power;

/*
extern int send_high_priority;
extern int send_general;
*/

extern void schedule_high_priority();
extern void schedule_general();
extern void schedule_receive_specific();
extern void schedule_transmit_specific();

extern void new_protocol_init(int pixels);
extern void new_protocol_stop();
extern void new_protocol_run();

extern void filter_board_changed();
extern void pa_changed();
extern void tuner_changed();

extern void setMox(int state);
extern int getMox();
extern void setTune(int state);
extern int getTune();

extern void new_protocol_audio_samples(RECEIVER *rx,short left_audio_sample,short right_audio_sample);
extern void new_protocol_iq_samples(int isample,int qsample);
extern void new_protocol_flush_iq_samples();
extern void new_protocol_cw_audio_samples(short l, short r);
#endif
