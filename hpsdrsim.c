/*
 * HPSDR simulator, (C) Christoph van Wuellen, April/Mai 2019
 *
 * This program simulates a HPSDR board.
 * If an SDR program such as phipsdr "connects" with this program, it
 * writes to stdout what goes on. This is great for debugging.
 *
 * In addition, I have built in the following features:
 *
 * This device has four "RF sources"
 *
 * RF1: ADC noise plus a  800 Hz signal at -100dBm
 * RF2: ADC noise
 * RF3: TX feedback signal with some distortion.
 * RF4: normalized undistorted TX signal
 *
 * RF1 and RF2 signal strenght vary according to Preamp and Attenuator settings
 * RF3 signal strength varies according to TX-drive and TX-ATT settings
 * RF4 signal strength is normalized to amplitude of 0.407 (old protocol) or 0.2899 (new protocol)
 *
 * The connection with the ADCs are:
 * ADC0: RF1 upon receive, RF3 upon transmit
 * ADC1: RF2 (for HERMES: RF4)
 * ADC2: RF4
 *
 * RF4 is the TX DAC signal. Upon TX, it goes to RX2 for Metis, RX4 for Hermes, and RX5 beyond.
 * Since the feedback runs at the RX sample rate while the TX sample rate is fixed (48000 Hz),
 * we have to re-sample and do this in a very stupid way (linear interpolation).
 *
 * The "noise" is a random number of amplitude 0.00003 (last bit on a 16-bit ADC),
 * that is about -90 dBm spread onto a spectrum whose width is the sample rate. Therefore
 * the "measured" noise floor in a filter 5 kHz wide is -102 dBm for a sample rate of 48 kHz
 * but -111 dBm for a sample rate of 384000 kHz. This is a nice demonstration how the
 * spectral density of "ADC noise" goes down when increasing the sample rate.
 *
 * The SDR application has to make the proper ADC settings, except for STEMlab
 * (RedPitaya based SDRs), where there is a fixed association
 * RX1=ADC1, RX2=ADC2, RX3=ADC2, RX4=TX-DAC
 * and the PURESIGNAL feedback signal is connected to the second ADC.
 *
 * Audio sent to the "radio" is played via the first available output channel.
 * This works on MacOS (PORTAUDIO) and Linux (ALSASOUND).
 *
 * If invoked with the "-diversity" flag, broad "man-made" noise is fed to ADC1 and
 * ADC2 upon RXing. The ADC2 signal is phase shifted by 90 degrees and somewhat
 * stronger. This noise can completely be eliminated using DIVERSITY.
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define NEED_DUMMY_AUDIO 1

#ifdef PORTAUDIO
#include "portaudio.h"
#undef NEED_DUMMY_AUDIO
#endif
#ifdef ALSASOUND
#include <alsa/asoundlib.h>
#undef NEED_DUMMY_AUDIO
#endif

#define EXTERN 
#include "hpsdrsim.h"

/*
 * These variables store the state of the "old protocol" SDR.
 * Whenevery they are changed, this is reported.
 */

static int		AlexTXrel = -1;
static int		alexRXout = -1;
static int		alexRXant = -1;
static int		MicTS = -1;
static int		duplex = -1;
static int		receivers = -1;
static int		rate = -1;
static int              preamp = -1;
static int		LTdither = -1;
static int		LTrandom = -1;
static int              ref10 = -1;
static int		src122 = -1;
static int		PMconfig = -1;
static int		MicSrc = -1;
static int		txdrive = 0;
static int		txatt = 0;
static int		sidetone_volume = -1;
static int		cw_internal = -1;
static int		rx_att[2] = {-1,-1};
static int		rx1_attE = -1;
static int              rx_preamp[4] = {-1,-1,-1,-1};
static int		MerTxATT0 = -1;
static int		MerTxATT1 = -1;
static int		MetisDB9 = -1;
static int		PeneSel = -1;
static int		PureSignal = -1;
static int		LineGain = -1;
static int		MicPTT = -1;
static int		tip_ring = -1;
static int		MicBias = -1;
static int		ptt=0;
static int		AlexAtt=-1;
static int		TX_class_E = -1;
static int		OpenCollectorOutputs=-1;
static long		tx_freq=-1;
static long		rx_freq[7] = {-1,-1,-1,-1,-1,-1,-1};
static int		hermes_config=-1;
static int		alex_lpf=-1;
static int		alex_hpf=-1;
static int		alex_manual=-1;
static int		alex_bypass=-1;
static int		lna6m=-1;
static int		alexTRdisable=-1;
static int		vna=-1;
static int		c25_ext_board_i2c_data=-1;
static int		rx_adc[7]={-1,-1,-1,-1,-1,-1,-1};
static int		cw_hang = -1;
static int		cw_reversed = -1;
static int		cw_speed = -1;
static int		cw_mode = -1;
static int		cw_weight = -1;
static int		cw_spacing = -1;
static int		cw_delay = -1;
static int		CommonMercuryFreq = -1;
static int              freq=-1;


// floating-point represeners of TX att, RX att, and RX preamp settings

static double txdrv_dbl = 0.99;
static double txatt_dbl = 1.0;
static double rxatt_dbl[4] = {1.0, 1.0, 1.0, 1.0};   // this reflects both ATT and PREAMP

/*
 * Socket for communicating with the "PC side"
 */
static int sock_TCP_Server = -1;
static int sock_TCP_Client = -1;
static int sock_udp;

/*
 * These two variables monitor whether the TX thread is active
 */
static int enable_thread = 0;
static int active_thread = 0;

static void process_ep2(uint8_t *frame);
static void *handler_ep6(void *arg);


static double  last_i_sample=0.0;
static double  last_q_sample=0.0;
static int  txptr=0;

int main(int argc, char *argv[])
{
	int i, j, size;
	struct sched_param param;
	pthread_attr_t attr;
	pthread_t thread;

	uint8_t reply[11] = { 0xef, 0xfe, 2, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 32, 1 };

	uint8_t id[4] = { 0xef, 0xfe, 1, 6 };
	uint32_t code;
        int16_t  sample,l,r;

	struct sockaddr_in addr_udp;
	uint8_t buffer[1032];
	struct timeval tv;
	struct timespec ts;
	int yes = 1;
        uint8_t *bp;
        unsigned long checksum;
        socklen_t lenaddr;
        struct sockaddr_in addr_from;
        unsigned int seed;

	uint32_t last_seqnum = 0xffffffff, seqnum;  // sequence number of received packet

	int udp_retries=0;
	int bytes_read, bytes_left;
	uint32_t *code0 = (uint32_t *) buffer;  // fast access to code of first buffer
        int fd;
        long cnt;
        double run,off,inc;

/*
 *      Examples for ATLAS:     ATLAS bus with Mercury/Penelope boards
 *      Examples for HERMES:    ANAN10, ANAN100
 *      Examples for ANGELIA:   ANAN100D
 *      Examples for ORION:     ANAN200D
 *      Examples for ORION2:    ANAN7000, ANAN8000
 *
 *      Examples for C25:	RedPitaya based boards with fixed ADC connections
 */

	// seed value for random number generator
	seed = ((uintptr_t) &seed) & 0xffffff;
        diversity=0;
        OLDDEVICE=DEVICE_ORION2;
        NEWDEVICE=NEW_DEVICE_ORION2;

        for (i=1; i<argc; i++) {
            if (!strncmp(argv[i],"-atlas"  ,      6))  {OLDDEVICE=DEVICE_ATLAS;       NEWDEVICE=NEW_DEVICE_ATLAS;}
            if (!strncmp(argv[i],"-hermes" ,      7))  {OLDDEVICE=DEVICE_HERMES;      NEWDEVICE=NEW_DEVICE_HERMES;}
            if (!strncmp(argv[i],"-hermes2" ,     8))  {OLDDEVICE=DEVICE_HERMES2;     NEWDEVICE=NEW_DEVICE_HERMES2;}
            if (!strncmp(argv[i],"-angelia" ,     8))  {OLDDEVICE=DEVICE_ANGELIA;     NEWDEVICE=NEW_DEVICE_ANGELIA;}
            if (!strncmp(argv[i],"-orion" ,       6))  {OLDDEVICE=DEVICE_ORION;       NEWDEVICE=NEW_DEVICE_ORION;}
            if (!strncmp(argv[i],"-orion2" ,      7))  {OLDDEVICE=DEVICE_ORION2;      NEWDEVICE=NEW_DEVICE_ORION2;}
            if (!strncmp(argv[i],"-hermeslite" , 11))  {OLDDEVICE=DEVICE_HERMES_LITE; NEWDEVICE=NEW_DEVICE_HERMES_LITE;}
            if (!strncmp(argv[i],"-c25"    ,      4))  {OLDDEVICE=DEVICE_C25;         NEWDEVICE=NEW_DEVICE_HERMES;}
            if (!strncmp(argv[i],"-diversity",   10))  {diversity=1;}
        }

        switch (OLDDEVICE) {
            case   DEVICE_ATLAS:   fprintf(stderr,"DEVICE is ATLASS\n");      c1=3.3; c2=0.090; break;
            case   DEVICE_HERMES:  fprintf(stderr,"DEVICE is HERMES\n");      c1=3.3; c2=0.095; break;
            case   DEVICE_HERMES2: fprintf(stderr,"DEVICE is HERMES (2)\n");  c1=3.3; c2=0.095; break;
            case   DEVICE_ANGELIA: fprintf(stderr,"DEVICE is ANGELIA\n");     c1=3.3; c2=0.095; break;
            case   DEVICE_ORION:   fprintf(stderr,"DEVICE is ORION\n");       c1=5.0; c2=0.108; break;
            case   DEVICE_ORION2:  fprintf(stderr,"DEVICE is ORION-II\n");    c1=5.0; c2=0.108; break;
            case   DEVICE_C25:     fprintf(stderr,"DEVICE is STEMlab/C25\n"); c1=3.3; c2=0.090; break;
        }

//
//      Initialize the data in the sample tables
//
	fprintf(stderr,".... producing random noise\n");
        // Produce some noise
        j=RAND_MAX / 2;
        for (i=0; i<LENNOISE; i++) {
          noiseItab[i]= ((double) rand_r(&seed) / j - 1.0) * 0.00003;
          noiseQtab[i]= ((double) rand_r(&seed) / j - 1.0) * 0.00003;
        }

	fprintf(stderr,".... producing an 800 Hz signal\n");
	// Produce an 800 Hz tone at 0 dBm
        run=0.0;
	for (i=0; i<LENTONE; i++) {
	  toneQtab[i]=cos(run);
	  toneItab[i]=sin(run);
	  run += 0.0032724923474893679567319201909161;
	}

	if (diversity) {
	  fprintf(stderr,"DIVERSITY testing activated!\n");
	  fprintf(stderr,".... producing some man-made noise\n");
          memset(divtab, 0, LENDIV*sizeof(double));
          for (j=1; j<=200; j++) {
            run=0.0;
            off=0.25*j*j;
	    inc=j*0.00039269908169872415480783042290994;
            for (i=0; i< LENDIV; i++) {
	      divtab[i] += cos(run+off);
	      run += inc;
 	    }
	  }
	  // normalize
	  off=0.0;
	  for (i=0; i<LENDIV; i++) {
	    if ( divtab[i] > off) off=divtab[i];
	    if (-divtab[i] > off) off=-divtab[i];
	  }
	  off=1.0/off;
	  fprintf(stderr,"(normalizing with %f)\n",off);
	  for (i=0; i<LENDIV; i++) {
	    divtab[i]=divtab[i]*off;
	  }
	}
	
//
//      clear TX fifo
//
	memset (isample, 0, OLDRTXLEN*sizeof(double));
	memset (qsample, 0, OLDRTXLEN*sizeof(double));

	audio_get_cards();
        audio_open_output();

	if ((sock_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		return EXIT_FAILURE;
	}

	setsockopt(sock_udp, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes));

	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	setsockopt(sock_udp, SOL_SOCKET, SO_RCVTIMEO, (void *)&tv, sizeof(tv));

	memset(&addr_udp, 0, sizeof(addr_udp));
	addr_udp.sin_family = AF_INET;
	addr_udp.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_udp.sin_port = htons(1024);

	if (bind(sock_udp, (struct sockaddr *)&addr_udp, sizeof(addr_udp)) < 0)
	{
		perror("bind");
		return EXIT_FAILURE;
	}

	if ((sock_TCP_Server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket tcp");
		return EXIT_FAILURE;
	}

	setsockopt(sock_TCP_Server, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes));

	int tcpmaxseg = 1032;
	setsockopt(sock_TCP_Server, IPPROTO_TCP, TCP_MAXSEG, (const char *)&tcpmaxseg, sizeof(int));

	int sndbufsize = 65535;
	int rcvbufsize = 65535;
	setsockopt(sock_TCP_Server, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbufsize, sizeof(int));
	setsockopt(sock_TCP_Server, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbufsize, sizeof(int));
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	setsockopt(sock_TCP_Server, SOL_SOCKET, SO_RCVTIMEO, (void *)&tv, sizeof(tv));

	if (bind(sock_TCP_Server, (struct sockaddr *)&addr_udp, sizeof(addr_udp)) < 0)
	{
		perror("bind tcp");
		return EXIT_FAILURE;
	}

	listen(sock_TCP_Server, 1024);
	fprintf(stderr, "Listening for TCP client connection request\n");

        int flags = fcntl(sock_TCP_Server, F_GETFL, 0);
        fcntl(sock_TCP_Server, F_SETFL, flags | O_NONBLOCK);

	while (1)
	{
		memcpy(buffer, id, 4);

		ts.tv_sec = 0;
		ts.tv_nsec = 1000000;

		if (sock_TCP_Client > -1)
		{
			// Using recvmmsg with a time-out should be used for a byte-stream protocol like TCP
			// (Each "packet" in the datagram may be incomplete). This is especially true if the
			// socket has a receive time-out, but this problem also occurs if the is no such
			// receive time-out.
			// Therefore we read a complete packet here (1032 bytes). Our TCP-extension to the
			// HPSDR protocol ensures that only 1032-byte packets may arrive here.
			bytes_read = 0;
			bytes_left = 1032;
			while (bytes_left > 0)
			{
				size = recvfrom(sock_TCP_Client, buffer + bytes_read, (size_t)bytes_left, 0, NULL, 0);
				if (size < 0 && errno == EAGAIN) continue;
				if (size < 0) break;
				bytes_read += size;
				bytes_left -= size;

			}

			bytes_read=size;
			if (size >= 0)
			{
				// 1032 bytes have successfully been read by TCP.
				// Let the downstream code know that there is a single packet, and its size
				bytes_read=1032;

				// In the case of a METIS-discovery packet, change the size to 63
				if (*code0 == 0x0002feef)
				{
					bytes_read = 63;
				}

				// In principle, we should check on (*code0 & 0x00ffffff) == 0x0004feef,
				// then we cover all kinds of start and stop packets.

				// In the case of a METIS-stop packet, change the size to 64
				if (*code0 == 0x0004feef)
				{
					bytes_read = 64;
				}

				// In the case of a METIS-start TCP packet, change the size to 64
				// The special start code 0x11 has no function any longer, but we shall still support it.
				if (*code0 == 0x1104feef || *code0 == 0x0104feef)
				{
					bytes_read = 64;
				}
			}
		}
		else
		{
			lenaddr=sizeof(addr_from);
			bytes_read = recvfrom(sock_udp, buffer, 1032, 0, (struct sockaddr *)&addr_from, &lenaddr);
			if (bytes_read > 0)
			{
				udp_retries=0;
			}
			else
			{
				udp_retries++;
			}

		}

		if (bytes_read < 0 && errno != EAGAIN)
		{
			perror("recvfrom");
			return EXIT_FAILURE;
		}

		// If nothing has arrived via UDP for some time, try to open TCP connection.
		// "for some time" means 10 subsequent un-successful UDP rcvmmsg() calls
                if (sock_TCP_Client < 0 && udp_retries > 10)
                {
                        if((sock_TCP_Client = accept(sock_TCP_Server, NULL, NULL)) > -1)
                        {
                                fprintf(stderr,"sock_TCP_Client: %d connected to sock_TCP_Server: %d\n", sock_TCP_Client, sock_TCP_Server);
                        }
			// This avoids firing accept() too often if it constantly fails
			udp_retries=0;
		}
		if (bytes_read <= 0) continue;
			memcpy(&code, buffer, 4);

			switch (code)
			{
			    // PC to SDR transmission via process_ep2
			    case 0x0201feef:

				// processing an invalid packet is too dangerous -- skip it!
				if (bytes_read != 1032)
				{
					fprintf(stderr,"InvalidLength: RvcMsg Code=0x%08x Len=%d\n", code, (int)bytes_read);
					break;
				}

				// sequence number check
				seqnum = ((buffer[4]&0xFF)<<24) + ((buffer[5]&0xFF)<<16) + ((buffer[6]&0xFF)<<8) + (buffer[7]&0xFF);

				if (seqnum != last_seqnum + 1)
				{
					fprintf(stderr,"SEQ ERROR: last %ld, recvd %ld\n", (long)last_seqnum, (long)seqnum);
				}

				last_seqnum = seqnum;

				process_ep2(buffer + 11);
				process_ep2(buffer + 523);

				if (active_thread) {
                                  // Put TX IQ samples into the ring buffer
				  // In the old protocol, samples come in groups of 8 bytes L1 L0 R1 R0 I1 I0 Q1 Q0
				  // Here, L1/L0 and R1/R0 are audio samples, and I1/I0 and Q1/Q0 are the TX iq samples
				  // I1 contains bits 8-15 and I0 bits 0-7 of a signed 16-bit integer. We convert this
				  // here to double. If the RX sample rate is larger than the TX on, we perform a
				  // simple linear interpolation between the last and current sample.
				  // Note that this interpolation causes weak "sidebands" at 48/96/... kHz distance (the
				  // strongest ones at 48 kHz).
				  double disample,dqsample,idelta,qdelta;
				  double sum;
                                  bp=buffer+16;  // skip 8 header and 8 SYNC/C&C bytes
				  sum=0.0;
                                  for (j=0; j<126; j++) {
					// write audio samples
					r  = (int)((signed char) *bp++)<<8;
					r |= (int)((signed char) *bp++ & 0xFF);
					l  = (int)((signed char) *bp++)<<8;
					l |= (int)((signed char) *bp++ & 0xFF);
                                        audio_write(r,l);
					sample  = (int)((signed char) *bp++)<<8;
					sample |= (int) ((signed char) *bp++ & 0xFF);
					disample=(double) sample * 0.000030517578125;  // division by 32768
					sample  = (int)((signed char) *bp++)<<8;
					sample |= (int) ((signed char) *bp++ & 0xFF);
					dqsample=(double) sample * 0.000030517578125;
					sum += (disample*disample+dqsample*dqsample);

					switch (rate) {
					    case 0:  // RX sample rate = TX sample rate = 48000
                                        	isample[txptr  ]=disample;
						qsample[txptr++]=dqsample;
						break;
					    case 1: // RX sample rate = 96000; TX sample rate = 48000
						idelta=0.5*(disample-last_i_sample);
						qdelta=0.5*(dqsample-last_q_sample);
                                        	isample[txptr  ]=last_i_sample+idelta;
						qsample[txptr++]=last_q_sample+qdelta;
                                        	isample[txptr  ]=disample;
						qsample[txptr++]=dqsample;
						break;
					    case 2: // RX sample rate = 192000; TX sample rate = 48000
						idelta=0.25*(disample-last_i_sample);
						qdelta=0.25*(dqsample-last_q_sample);
                                        	isample[txptr  ]=last_i_sample+idelta;
						qsample[txptr++]=last_q_sample+qdelta;
                                        	isample[txptr  ]=last_i_sample+2.0*idelta;
						qsample[txptr++]=last_q_sample+2.0*qdelta;
                                        	isample[txptr  ]=last_i_sample+3.0*idelta;
						qsample[txptr++]=last_q_sample+3.0*qdelta;
                                        	isample[txptr  ]=disample;
						qsample[txptr++]=dqsample;
						break;
					    case 3: // RX sample rate = 384000; TX sample rate = 48000
						idelta=0.125*(disample-last_i_sample);
						qdelta=0.125*(dqsample-last_q_sample);
                                        	isample[txptr  ]=last_i_sample+idelta;
						qsample[txptr++]=last_q_sample+qdelta;
                                        	isample[txptr  ]=last_i_sample+2.0*idelta;
						qsample[txptr++]=last_q_sample+2.0*qdelta;
                                        	isample[txptr  ]=last_i_sample+3.0*idelta;
						qsample[txptr++]=last_q_sample+3.0*qdelta;
                                        	isample[txptr  ]=last_i_sample+4.0*idelta;
						qsample[txptr++]=last_q_sample+4.0*qdelta;
                                        	isample[txptr  ]=last_i_sample+5.0*idelta;
						qsample[txptr++]=last_q_sample+5.0*qdelta;
                                        	isample[txptr  ]=last_i_sample+6.0*idelta;
						qsample[txptr++]=last_q_sample+6.0*qdelta;
                                        	isample[txptr  ]=last_i_sample+7.0*idelta;
						qsample[txptr++]=last_q_sample+7.0*qdelta;
                                        	isample[txptr  ]=disample;
						qsample[txptr++]=dqsample;
						break;
					}
					last_i_sample=disample;
					last_q_sample=dqsample;
					if (j == 62) bp+=8; // skip 8 SYNC/C&C bytes of second block
                                 }
				 // wrap-around of ring buffer
                                 if (txptr >= OLDRTXLEN) txptr=0;
				}
				break;

			    // respond to an incoming Metis detection request
			    case 0x0002feef:

				fprintf(stderr, "Respond to an incoming Metis detection request / code: 0x%08x\n", code);

				// processing an invalid packet is too dangerous -- skip it!
				if (bytes_read != 63)
				{
					fprintf(stderr,"InvalidLength: RvcMsg Code=0x%08x Len=%d\n", code, (int)bytes_read);
					break;
				}
				reply[ 2] = 2;
				if (active_thread || new_protocol_running()) {
				    reply[2] = 3;
				}
				reply[10] = OLDDEVICE;
				memset(buffer, 0, 60);
				memcpy(buffer, reply, 11);

				if (sock_TCP_Client > -1)
				{
					// We will get into trouble if we respond via TCP while the radio is
					// running with TCP.
					// We simply suppress the response in this (very unlikely) case.
					if (!active_thread)
					{
					    if (send(sock_TCP_Client, buffer, 60, 0) < 0)
					    {
						fprintf(stderr, "TCP send error occurred when responding to an incoming Metis detection request!\n");
					    }
					    // close the TCP socket which was only used for the detection
					    close(sock_TCP_Client);
					    sock_TCP_Client = -1;
					}
				}
				else
				{
					sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				}

				break;

			    // stop the SDR to PC transmission via handler_ep6
			    case 0x0004feef:

				fprintf(stderr, "STOP the transmission via handler_ep6 / code: 0x%08x\n", code);

				// processing an invalid packet is too dangerous -- skip it!
				if (bytes_read != 64)
				{
					fprintf(stderr,"InvalidLength: RvcMsg Code=0x%08x Len=%d\n", code, bytes_read);
					break;
				}

				enable_thread = 0;
				while (active_thread) usleep(1000);

				if (sock_TCP_Client > -1)
				{
					close(sock_TCP_Client);
					sock_TCP_Client = -1;
				}
				break;

			    case 0x0104feef:
			    case 0x0204feef:
			    case 0x0304feef:

				if (new_protocol_running()) {
				    fprintf(stderr,"OldProtocol START command received but NewProtocol radio already running!\n");
				    break;
				}
				// processing an invalid packet is too dangerous -- skip it!
				if (bytes_read != 64)
				{
					fprintf(stderr,"InvalidLength: RvcMsg Code=0x%08x Len=%d\n", code, bytes_read);
					break;
				}
				fprintf(stderr, "START the PC-to-SDR handler thread / code: 0x%08x\n", code);

				enable_thread = 0;
				while (active_thread) usleep(1000);
				memset(&addr_old, 0, sizeof(addr_old));
				addr_old.sin_family = AF_INET;
				addr_old.sin_addr.s_addr = addr_from.sin_addr.s_addr;
				addr_old.sin_port = addr_from.sin_port;

				//
				// The initial value of txptr defines the delay between
				// TX samples sent to the SDR and PURESIGNAL feedback
				// samples arriving
				//
                                txptr=OLDRTXLEN/2;
				memset(isample, 0, OLDRTXLEN*sizeof(double));
				memset(qsample, 0, OLDRTXLEN*sizeof(double));
				enable_thread = 1;
				active_thread = 1;
				if (pthread_create(&thread, NULL, handler_ep6, NULL) < 0)
				{
					perror("create old protocol thread");
					return EXIT_FAILURE;
				}
				pthread_detach(thread);
				break;

			    default:
				/*
				 * Here we have to handle the following "non standard" cases:
				 * OldProtocol "program"   packet
				 * OldProtocol "erase"     packet
				 * OldProtocol "Set IP"    packet
				 * NewProtocol "Discovery" packet
				 * NewProtocol "program"   packet
				 * NewProtocol "erase"     packet
				 * NewProtocol "Set IP"    packet
				 * NewProtocol "General"   packet  ==> this starts NewProtocol radio
				 */
				if (bytes_read == 264 && buffer[0] == 0xEF && buffer[1] == 0xFE && buffer[2] == 0x03 && buffer[3] == 0x01) {
				  static long cnt=0;
				  unsigned long blks=(buffer[4] << 24) + (buffer[5] << 16) + (buffer[6] << 8) + buffer[7];
				  fprintf(stderr,"OldProtocol Program blks=%lu count=%ld\r", blks, ++cnt);
				  usleep(1000);
				  memset(buffer, 0, 60);
				  buffer[0]=0xEF;
				  buffer[1]=0xFE;
				  buffer[2]=0x04;
				  buffer[3]=0xAA;
				  buffer[4]=0xBB;
				  buffer[5]=0xCC;
				  buffer[6]=0xDD;
				  buffer[7]=0xEE;
				  buffer[8]=0xFF;
				  sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  if (blks == cnt) fprintf(stderr,"\n\n Programming Done!\n");
				  break;
				}
				if (bytes_read == 64 && buffer[0] == 0xEF && buffer[1] == 0xFE && buffer[2] == 0x03 && buffer[3] == 0x02) {
				  fprintf(stderr,"OldProtocol Erase packet received:\n");
				  sleep(1);
				  cnt=0;
				  memset(buffer, 0, 60);
				  buffer[0]=0xEF;
				  buffer[1]=0xFE;
				  buffer[2]=0x03;
				  buffer[3]=0xAA;
				  buffer[4]=0xBB;
				  buffer[5]=0xCC;
				  buffer[6]=0xDD;
				  buffer[7]=0xEE;
				  buffer[8]=0xFF;
				  sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  break;
				
				}
				if (bytes_read == 63 && buffer[0] == 0xEF && buffer[1] == 0xFE && buffer[2] == 0x03) {
				  fprintf(stderr,"OldProtocol SetIP packet received:\n");
				  fprintf(stderr,"MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n", buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8]);
				  fprintf(stderr,"IP  address is %03d:%03d:%03d:%03d\n", buffer[9], buffer[10], buffer[11], buffer[12]);
 				  buffer[2]=0x02;
				  memset(buffer+9, 0, 54);
				  sendto(sock_udp, buffer, 63, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  break;
				}
		   	    	if (code == 0 && buffer[4] == 0x02) {
				  fprintf(stderr,"NewProtocol discovery packet received\n");
				  // prepeare response
				  memset(buffer, 0, 60);
				  buffer [4]=0x02+new_protocol_running();
				  buffer [5]=0xAA;
				  buffer[ 6]=0xBB;
				  buffer[ 7]=0xCC;
				  buffer[ 8]=0xDD;
				  buffer[ 9]=0xEE;
				  buffer[10]=0xFF;
				  buffer[11]=NEWDEVICE;
				  buffer[12]=38;
				  buffer[13]=19;
				  buffer[20]=2;
				  buffer[21]=1;
				  buffer[22]=3;
				  sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  break;
				}
		   	    	if (code == 0 && buffer[4] == 0x04) {
				  fprintf(stderr,"NewProtocol erase packet received\n");
                                  memset(buffer, 0, 60);
                                  buffer [4]=0x02+active_thread;
                                  buffer [5]=0xAA;
                                  buffer[ 6]=0xBB;
                                  buffer[ 7]=0xCC;
                                  buffer[ 8]=0xDD;
                                  buffer[ 9]=0xEE;
                                  buffer[10]=0xFF;
                                  buffer[11]=NEWDEVICE;
                                  buffer[12]=38;
                                  buffer[13]=103;
                                  buffer[20]=2;
                                  buffer[21]=1;
                                  buffer[22]=3;
                                  sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  sleep(5); // pretend erase takes 5 seconds
                                  sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  break;
				}
		   	    	if (bytes_read == 265 && buffer[4] == 0x05) {
				  unsigned long seq, blk;
				  seq=(buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
				  blk=(buffer[5] << 24) + (buffer[6] << 16) + (buffer[7] << 8) + buffer[8];
				  fprintf(stderr,"NewProtocol Program packet received: seq=%lu blk=%lu\r",seq,blk);
				  if (seq == 0) checksum=0;
				  for (j=9; j<=264; j++) checksum += buffer[j];
                                  memset(buffer+4, 0, 56); // keep seq. no
				  buffer[ 4]=0x04;
                                  buffer [5]=0xAA;
                                  buffer[ 6]=0xBB;
                                  buffer[ 7]=0xCC;
                                  buffer[ 8]=0xDD;
                                  buffer[ 9]=0xEE;
                                  buffer[10]=0xFF;
				  buffer[11]=103;
				  buffer[12]=NEWDEVICE;
				  buffer[13]=(checksum >> 8) & 0xFF;
				  buffer[14]=(checksum     ) & 0xFF;
				  sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  if (seq+1 == blk) fprintf(stderr,"\n\nProgramming Done!\n");
				  break;
				}
		   	    	if (bytes_read == 60 && code == 0 && buffer[4] == 0x06) {
				  fprintf(stderr,"NewProtocol SetIP packet received for MAC %2x:%2x:%2x:%2x%2x:%2x IP=%d:%d:%d:%d\n",
                                          buffer[5],buffer[6],buffer[7],buffer[8],buffer[9],buffer[10],
					  buffer[11],buffer[12],buffer[13],buffer[14]);
 				  // only respond if this is for OUR device
				  if (buffer[ 5] != 0xAA) break;
				  if (buffer[ 6] != 0xBB) break;
				  if (buffer[ 7] != 0xCC) break;
				  if (buffer[ 8] != 0xDD) break;
				  if (buffer[ 9] != 0xEE) break;
				  if (buffer[10] != 0xFF) break;
                                  memset(buffer, 0, 60);
                                  buffer [4]=0x02+active_thread;
                                  buffer [5]=0xAA;
                                  buffer[ 6]=0xBB;
                                  buffer[ 7]=0xCC;
                                  buffer[ 8]=0xDD;
                                  buffer[ 9]=0xEE;
                                  buffer[10]=0xFF;
                                  buffer[11]=NEWDEVICE;
                                  buffer[12]=38;
                                  buffer[13]=103;
                                  buffer[20]=2;
                                  buffer[21]=1;
                                  buffer[22]=3;
                                  sendto(sock_udp, buffer, 60, 0, (struct sockaddr *)&addr_from, sizeof(addr_from));
				  break;
				}
		   	    	if (bytes_read == 60 && buffer[4] == 0x00) {
				  // handle "general packet" of the new protocol
				  memset(&addr_new, 0, sizeof(addr_new));
				  addr_new.sin_family = AF_INET;
				  addr_new.sin_addr.s_addr = addr_from.sin_addr.s_addr;
				  addr_new.sin_port = addr_from.sin_port;
				  new_protocol_general_packet(buffer);
				  break;
				}
				else
				{
					fprintf(stderr,"Invalid packet (len=%d) detected: ",bytes_read);
					for (i=0; i<16; i++) fprintf(stderr,"%02x ",buffer[i]);
					fprintf(stderr,"\n");
				}
				break;
			}
	}

	close(sock_udp);

	if (sock_TCP_Client > -1)
	{
		close(sock_TCP_Client);
	}

	if (sock_TCP_Server > -1)
	{
		close(sock_TCP_Server);
	}

	return EXIT_SUCCESS;
}

#define chk_data(a,b,c) if ((a) != b) { b = (a); printf("%20s= %08lx (%10ld)\n", c, (long) b, (long) b ); }

void process_ep2(uint8_t *frame)
{

	uint16_t data;

        chk_data(frame[0] & 1, ptt, "PTT");
	switch (frame[0])
	{
	case 0:
	case 1:
	  chk_data((frame[1] & 0x03) >> 0, rate, "SampleRate");
	  chk_data((frame[1] & 0x0C) >> 3, ref10, "Ref10MHz");
  	  chk_data((frame[1] & 0x10) >> 4, src122, "Source122MHz");
  	  chk_data((frame[1] & 0x60) >> 5, PMconfig, "Penelope/Mercury config");
  	  chk_data((frame[1] & 0x80) >> 7, MicSrc, "MicSource");

          chk_data(frame[2] & 1, TX_class_E, "TX CLASS-E");
          chk_data((frame[2] & 0xfe) >> 1, OpenCollectorOutputs,"OpenCollector");

          chk_data((frame[3] & 0x03) >> 0, AlexAtt, "ALEX Attenuator");
          chk_data((frame[3] & 0x04) >> 2, preamp, "ALEX preamp");
          chk_data((frame[3] & 0x08) >> 3, LTdither, "LT2208 Dither");
          chk_data((frame[3] & 0x10) >> 4, LTrandom, "LT2208 Random");
          chk_data((frame[3] & 0x60) >> 5, alexRXant, "ALEX RX ant");
          chk_data((frame[3] & 0x80) >> 7, alexRXout, "ALEX RX out");

          chk_data(((frame[4] >> 0) & 3), AlexTXrel, "ALEX TX relay");
          chk_data(((frame[4] >> 2) & 1), duplex,    "DUPLEX");
          chk_data(((frame[4] >> 3) & 7) + 1, receivers, "RECEIVERS");
          chk_data(((frame[4] >> 6) & 1), MicTS, "TimeStampMic");
          chk_data(((frame[4] >> 7) & 1), CommonMercuryFreq,"Common Mercury Freq");

	  if (OLDDEVICE == DEVICE_C25) {
              // Charly25: has two 18-dB preamps that are switched with "preamp" and "dither"
              //           and two attenuators encoded in Alex-ATT
	      //           Both only applies to RX1!
              rxatt_dbl[0]=pow(10.0, -0.05*(12*AlexAtt-18*LTdither-18*preamp));
              rxatt_dbl[1]=1.0;
          } else {
	      // Assume that it has ALEX attenuators in addition to the Step Attenuators
              rxatt_dbl[0]=pow(10.0, -0.05*(10*AlexAtt+rx_att[0]));
              rxatt_dbl[1]=1.0;
          }
	  break;

        case 2:
        case 3:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), tx_freq,"TX FREQ");
	   break;

        case 4:
        case 5:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), rx_freq[0],"RX FREQ1");
	   break;

        case 6:
        case 7:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), rx_freq[1],"RX FREQ2");
	   break;

        case 8:
        case 9:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), rx_freq[2],"RX FREQ3");
	   break;

        case 10:
        case 11:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), rx_freq[3],"RX FREQ4");
	   break;

        case 12:
        case 13:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), rx_freq[4],"RX FREQ5");
	   break;

        case 14:
        case 15:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), rx_freq[5],"RX FREQ6");
	   break;

        case 16:
        case 17:
	   chk_data(frame[4] | (frame[3] << 8) | (frame[2] << 16) | (frame[1] << 24), rx_freq[6],"RX FREQ7");
	   break;

	case 18:
	case 19:
	   chk_data(frame[1],txdrive,"TX DRIVE");
	   chk_data(frame[2] & 0x3F,hermes_config,"HERMES CONFIG");
	   chk_data(frame[2] & 0x40, alex_manual,"ALEX manual HPF/LPF");
	   chk_data(frame[2] & 0x80, vna     ,"VNA mode");
	   chk_data(frame[3] & 0x1F,alex_hpf,"ALEX HPF");
	   chk_data(frame[3] & 0x20,alex_bypass,"ALEX Bypass HPFs");
	   chk_data(frame[3] & 0x40,lna6m,"ALEX 6m LNA");
	   chk_data(frame[3] & 0x80,alexTRdisable,"ALEX T/R disable");
	   chk_data(frame[4],alex_lpf,"ALEX LPF");
           // reset TX level. Leve a little head-room for noise
	   txdrv_dbl=(double) txdrive * 0.00390625;  // div. by. 256
	   break;

	case 20:
	case 21:
	   chk_data((frame[1] & 0x01) >> 0, rx_preamp[0], "ADC1 preamp");
	   chk_data((frame[1] & 0x02) >> 1, rx_preamp[1], "ADC2 preamp");
	   chk_data((frame[1] & 0x04) >> 2, rx_preamp[2], "ADC3 preamp");
	   chk_data((frame[1] & 0x08) >> 3, rx_preamp[3], "ADC4 preamp");
	   chk_data((frame[1] & 0x10) >> 4, tip_ring  , "TIP/Ring");
	   chk_data((frame[1] & 0x20) >> 5, MicBias   , "MicBias");
	   chk_data((frame[1] & 0x40) >> 6, MicPTT    , "MicPTT");

   	   chk_data((frame[2] & 0x1F) >> 0, LineGain  , "LineGain");
   	   chk_data((frame[2] & 0x20) >> 5, MerTxATT0 , "Mercury Att on TX/0");
   	   chk_data((frame[2] & 0x40) >> 6, PureSignal, "PureSignal");
   	   chk_data((frame[2] & 0x80) >> 7, PeneSel   , "PenelopeSelect");

   	   chk_data((frame[3] & 0x0F) >> 0, MetisDB9  , "MetisDB9");
   	   chk_data((frame[3] & 0x10) >> 4, MerTxATT1 , "Mercury Att on TX/1");

   	   chk_data((frame[4] & 0x1F) >> 0, rx_att[0], "RX1 ATT");
   	   chk_data((frame[4] & 0x20) >> 5, rx1_attE, "RX1 ATT enable");

	   if (OLDDEVICE != DEVICE_C25) {
	     // Set RX amplification factors. No switchable preamps available normally.
             rxatt_dbl[0]=pow(10.0, -0.05*(10*AlexAtt+rx_att[0]));
             rxatt_dbl[1]=pow(10.0, -0.05*(rx_att[1]));
             rxatt_dbl[2]=1.0;
             rxatt_dbl[3]=1.0;
	   }
	   break;

	case 22:
	case 23:
           chk_data(frame[1] & 0x1f, rx_att[1],"RX2 ATT");
           chk_data((frame[2] >> 6) & 1, cw_reversed, "CW REV");
           chk_data(frame[3] & 63, cw_speed, "CW SPEED");
           chk_data((frame[3] >> 6) & 3, cw_mode, "CW MODE");
           chk_data(frame[4] & 127, cw_weight,"CW WEIGHT");
           chk_data((frame[4] >> 7) & 1, cw_spacing, "CW SPACING");

	   // Set RX amplification factors.
           rxatt_dbl[1]=pow(10.0, -0.05*(rx_att[1]));
	   break;

	case 24:
	case 25:
           data = frame[1];
           data |= frame[2] << 8;
           chk_data((frame[2] << 8) | frame[1], c25_ext_board_i2c_data, "C25 EXT BOARD DATA");
           break;

	case 28:
	case 29:
            chk_data((frame[1] & 0x03) >> 0, rx_adc[0], "RX1 ADC");
            chk_data((frame[1] & 0x0C) >> 2, rx_adc[1], "RX2 ADC");
            chk_data((frame[1] & 0x30) >> 4, rx_adc[2], "RX3 ADC");
            chk_data((frame[1] & 0xC0) >> 6, rx_adc[3], "RX4 ADC");
            chk_data((frame[2] & 0x03) >> 0, rx_adc[4], "RX5 ADC");
            chk_data((frame[2] & 0x0C) >> 2, rx_adc[5], "RX6 ADC");
            chk_data((frame[2] & 0x30) >> 4, rx_adc[6], "RX7 ADC");
	    chk_data((frame[3] & 0x1f), txatt, "TX ATT");
	    txatt_dbl=pow(10.0, -0.05*(double) txatt);
	    if (OLDDEVICE == DEVICE_C25) {
		// RedPitaya: Hard-wired ADC settings.
		rx_adc[0]=0;
		rx_adc[1]=1;
		rx_adc[2]=1;
	    }
	    break;

	case 30:
	case 31:
            chk_data(frame[1] & 1,cw_internal,"CW INT");
            chk_data(frame[2], sidetone_volume,"SIDE TONE VOLUME");
            chk_data(frame[3], cw_delay,"CW DELAY");
	    cw_delay = frame[3];
	    break;

	case 32:
	case 33:
            chk_data((frame[1] << 2) | (frame[2] & 3), cw_hang, "CW HANG");
            chk_data((frame[3] << 4) | (frame[4] & 255), freq, "SIDE TONE FREQ");
	    break;
	}
}

void *handler_ep6(void *arg)
{
	int i, j, k, n, size;
	int data_offset, header_offset;
	uint32_t counter;
	uint8_t buffer[1032];
	uint8_t *pointer;
	uint8_t id[4] = { 0xef, 0xfe, 1, 6 };
	uint8_t header[40] =
	{
		127, 127, 127, 0, 0, 33, 17, 21,
		127, 127, 127, 8, 0, 0, 0, 0,
		127, 127, 127, 16, 0, 0, 0, 0,
		127, 127, 127, 24, 0, 0, 0, 0,
		127, 127, 127, 32, 66, 66, 66, 66
	};
        int32_t adc1isample,adc1qsample;
        int32_t adc2isample,adc2qsample;
        int32_t dacisample,dacqsample;

	int32_t myisample,myqsample;
        int16_t ssample;

        struct timespec delay;
#ifdef __APPLE__
	struct timespec now;
#endif
        long wait;
        int noiseIQpt,toneIQpt,divpt,rxptr;
        double i1,q1,fac1,fac2,fac3,fac4;
        int decimation;  // for converting 1536 kHz samples to 48, 192, 384, ....
        unsigned int seed;

	seed=((uintptr_t) &seed) & 0xffffff;

	memcpy(buffer, id, 4);

	header_offset = 0;
	counter = 0;

	noiseIQpt=0;
	toneIQpt=0;
	divpt=0;
	// The rxptr should never "overtake" the txptr, but
	// it also must not lag behind by too much. Let's take
	// the typical TX FIFO size
        rxptr=txptr-4096;
        if (rxptr < 0) rxptr += OLDRTXLEN;
        
        clock_gettime(CLOCK_MONOTONIC, &delay);
	while (1)
	{
		if (!enable_thread) break;

		size = receivers * 6 + 2;
		n = 504 / size;  // number of samples per 512-byte-block
                // Time (in nanosecs) to "collect" the samples sent in one sendmsg
                wait = (2*n*1000000L) / (48 << rate);

                // plug in sequence numbers
		data_offset = 0;
		*(uint32_t *)(buffer + 4) = htonl(counter);
		++counter;

//
//		This defines the distortion as well as the amplification
//              Use PA settings such that there is full drive at full
//              power (39 dB)
//

		//  48 kHz   decimation=32
		//  96 kHz   decimation=16
		// 192 kHz   decimation= 8
		// 384 kHz   decimation= 4
		decimation = 32 >> rate;
		for (i = 0; i < 2; ++i)
		{
		    pointer = buffer + i * 516 - i % 2 * 4 + 8;
		    memcpy(pointer, header + header_offset, 8);

		    header_offset = header_offset >= 32 ? 0 : header_offset + 8;

		    pointer += 8;
		    memset(pointer, 0, 504);
		    fac1=rxatt_dbl[0]*0.00001;		// Amplitude of 800-Hz-signal to ADC1
		    if (diversity) {
			fac2=0.0001*rxatt_dbl[0];	// Amplitude of broad "man-made" noise to ADC1
			fac4=0.0002*rxatt_dbl[1];	// Amplitude of broad "man-made" noise to ADC2
							// (phase shifted 90 deg., 6 dB stronger)
		    }
		    for (j=0; j<n; j++) {
			// ADC1: noise + weak tone on RX, feedback sig. on TX (except STEMlab)
		        if (ptt && (OLDDEVICE != DEVICE_C25)) {
			  i1=isample[rxptr]*txdrv_dbl;
			  q1=qsample[rxptr]*txdrv_dbl;
			  fac3=IM3a+IM3b*(i1*i1+q1*q1);
			  adc1isample= (txatt_dbl*i1*fac3+noiseItab[noiseIQpt]) * 8388607.0;
			  adc1qsample= (txatt_dbl*q1*fac3+noiseItab[noiseIQpt]) * 8388607.0;
			} else if (diversity) {
			  // man made noise only to I samples
			  adc1isample= (noiseItab[noiseIQpt]+toneItab[toneIQpt]*fac1+divtab[divpt]*fac2) * 8388607.0;
			  adc1qsample= (noiseQtab[noiseIQpt]+toneQtab[toneIQpt]*fac1                   ) * 8388607.0;
			} else {
			  adc1isample= (noiseItab[noiseIQpt]+toneItab[toneIQpt]*fac1) * 8388607.0;
			  adc1qsample= (noiseQtab[noiseIQpt]+toneQtab[toneIQpt]*fac1) * 8388607.0;
			}
			// ADC2: noise RX, feedback sig. on TX (only STEMlab)
			if (ptt && (OLDDEVICE == DEVICE_C25)) {
			  i1=isample[rxptr]*txdrv_dbl;
			  q1=qsample[rxptr]*txdrv_dbl;
			  fac3=IM3a+IM3b*(i1*i1+q1*q1);
			  adc2isample= (txatt_dbl*i1*fac3+noiseItab[noiseIQpt]) * 8388607.0;
			  adc2qsample= (txatt_dbl*q1*fac3+noiseItab[noiseIQpt]) * 8388607.0;
			} else if (diversity) {
			  // man made noise to Q channel only
			  adc2isample= noiseItab[noiseIQpt]                      * 8388607.0;		// Noise
			  adc2qsample= (noiseQtab[noiseIQpt]+divtab[divpt]*fac4) * 8388607.0;
			} else {
			  adc2isample= noiseItab[noiseIQpt] * 8388607.0;			// Noise
			  adc2qsample= noiseQtab[noiseIQpt] * 8388607.0;
			}
			//
			// TX signal with peak=0.407
			//
			dacisample= isample[rxptr] * 0.407 * 8388607.0;
			dacqsample= qsample[rxptr] * 0.407 * 8388607.0;

			for (k=0; k< receivers; k++) {
			    myisample=0;
			    myqsample=0;
			    switch (rx_adc[k]) {
			      case 0: // ADC1
				myisample=adc1isample;
				myqsample=adc1qsample;
				break;
			      case 1: // ADC2
				myisample=adc2isample;
				myqsample=adc2qsample;
				break;
			      default:
				myisample=0;
				myqsample=0;
				break;
			    }
			    if (OLDDEVICE == DEVICE_ATLAS && ptt && (k==1)) {
				// METIS: TX DAC signal goes to RX2 when TXing
				myisample=dacisample;
				myqsample=dacqsample;
			    }
			    if ((OLDDEVICE==DEVICE_HERMES || OLDDEVICE==DEVICE_HERMES2 || OLDDEVICE==DEVICE_C25) && ptt && (k==3)) {
				// HERMES: TX DAC signal goes to RX4 when TXing
				myisample=dacisample;
				myqsample=dacqsample;
			    }
			    if ((OLDDEVICE==DEVICE_ANGELIA || OLDDEVICE == DEVICE_ORION || OLDDEVICE== DEVICE_ORION2) && ptt && (k==4)) {
				// ANGELIA and beyond: TX DAC signal goes to RX5 when TXing
				myisample=dacisample;
				myqsample=dacqsample;
			    }
			    *pointer++ = (myisample >> 16) & 0xFF;
			    *pointer++ = (myisample >>  8) & 0xFF;
			    *pointer++ = (myisample >>  0) & 0xFF;
			    *pointer++ = (myqsample >> 16) & 0xFF;
			    *pointer++ = (myqsample >>  8) & 0xFF;
			    *pointer++ = (myqsample >>  0) & 0xFF;
		        }
			// Microphone samples: silence
			pointer += 2;
			rxptr++;              if (rxptr >= OLDRTXLEN) rxptr=0;
			noiseIQpt++;          if (noiseIQpt >= LENNOISE) noiseIQpt=rand_r(&seed) / NOISEDIV;
			toneIQpt+=decimation; if (toneIQpt >= LENTONE) toneIQpt=0;
			divpt+=decimation;    if (divpt >= LENDIV) divpt=0;
		    }
		}
		//
		// Wait until the time has passed for all these samples
		//
                delay.tv_nsec += wait;
                while (delay.tv_nsec >= 1000000000) {
                  delay.tv_nsec -= 1000000000;
                  delay.tv_sec++;
                }
#ifdef __APPLE__
		//
		// The (so-called) operating system for Mac does not have clock_nanosleep(),
		// but is has clock_gettime as well as nanosleep.
		// So, to circumvent this problem, we look at the watch and determine
		// how long we should sleep now.
		//
		clock_gettime(CLOCK_MONOTONIC, &now);
		now.tv_sec =delay.tv_sec  - now.tv_sec;
		now.tv_nsec=delay.tv_nsec - now.tv_nsec;
		while (now.tv_nsec < 0) {
		    now.tv_nsec += 1000000000;
		    now.tv_sec--;
		}
		nanosleep(&now, NULL);
#else
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &delay, NULL);
#endif

		if (sock_TCP_Client > -1)
		{
			if (sendto(sock_TCP_Client, buffer, 1032, 0, (struct sockaddr *)&addr_old, sizeof(addr_old)) < 0)
			{
				fprintf(stderr, "TCP sendmsg error occurred at sequence number: %u !\n", counter);
			}
		}
		else
		{
			sendto(sock_udp, buffer, 1032, 0, (struct sockaddr *)&addr_old, sizeof(addr_old));
		}

	}
	active_thread = 0;
	return NULL;
}

#ifdef PORTAUDIO
// PORTAUDIO output function

static int padev = -1;
static float playback_buffer[256];
static PaStream  *playback_handle=NULL;
int playback_offset;


void audio_get_cards()
{
  int i, numDevices;
  const PaDeviceInfo *deviceInfo;
  PaStreamParameters inputParameters, outputParameters;

  PaError err;

  err = Pa_Initialize();
  if( err != paNoError )
  {
        fprintf(stderr, "PORTAUDIO ERROR: Pa_Initialize: %s\n", Pa_GetErrorText(err));
        return;
  }
  numDevices = Pa_GetDeviceCount();
  if( numDevices < 0 ) return;

  for( i=0; i<numDevices; i++ )
  {
        deviceInfo = Pa_GetDeviceInfo( i );

        outputParameters.device = i;
        outputParameters.channelCount = 1;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
        outputParameters.hostApiSpecificStreamInfo = NULL;
        if (Pa_IsFormatSupported(NULL, &outputParameters, 48000.0) == paFormatIsSupported) {
          padev=i;
          fprintf(stderr,"PORTAUDIO OUTPUT DEVICE, No=%d, Name=%s\n", i, deviceInfo->name);
	  return;
        }
  }
}

void audio_open_output()
{
  PaError err;
  PaStreamParameters outputParameters;
  long framesPerBuffer=256;

  bzero( &outputParameters, sizeof( outputParameters ) ); //not necessary if you are filling in all the fields
  outputParameters.channelCount = 1;   // Always MONO
  outputParameters.device = padev;
  outputParameters.hostApiSpecificStreamInfo = NULL;
  outputParameters.sampleFormat = paFloat32;
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(padev)->defaultLowOutputLatency ;
  outputParameters.hostApiSpecificStreamInfo = NULL; //See you specific host's API docs for info on using this field

  // Try using AudioWrite without a call-back function

  playback_offset=0;
  err = Pa_OpenStream(&(playback_handle), NULL, &outputParameters, 48000.0, framesPerBuffer, paNoFlag, NULL, NULL);
  if (err != paNoError) {
    fprintf(stderr,"PORTAUDIO ERROR: AOO open stream: %s\n",Pa_GetErrorText(err));
    playback_handle = NULL;
    return;
  }

  err = Pa_StartStream(playback_handle);
  if (err != paNoError) {
    fprintf(stderr,"PORTAUDIO ERROR: AOO start stream:%s\n",Pa_GetErrorText(err));
    playback_handle=NULL;
    return;
  }
  // Write one buffer to avoid under-flow errors
  // (this gives us 5 msec to pass before we have to call audio_write the first time)
  bzero(playback_buffer, (size_t) (256*sizeof(float)));
  err=Pa_WriteStream(playback_handle, (void *) playback_buffer, (unsigned long) 256);

  return;
}

void audio_write (int16_t l, int16_t r)
{
  PaError err;
  if (playback_handle != NULL) {
    playback_buffer[playback_offset++] = (r + l) *0.000015259;  //   65536 --> 1.0
    if (playback_offset == 256) {
      playback_offset=0;
      err=Pa_WriteStream(playback_handle, (void *) playback_buffer, (unsigned long) 256);
    }
  }
}
#endif
#ifdef ALSASOUND
//
// Audio functions based on LINUX ALSA
//

static snd_pcm_t *playback_handle = NULL;
static unsigned char playback_buffer[1024]; // 256 samples, left-and-right, two bytes per sample
static int playback_offset;

static char *device_id = NULL;

void audio_get_cards() {
  snd_ctl_card_info_t *info;
  snd_pcm_info_t *pcminfo;
  snd_ctl_card_info_alloca(&info);
  snd_pcm_info_alloca(&pcminfo);
  int i;
  int card = -1;


  while (snd_card_next(&card) >= 0 && card >= 0) {
    int err = 0;
    snd_ctl_t *handle;
    char name[20];
    snprintf(name, sizeof(name), "hw:%d", card);
    if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
      continue;
    }

    if ((err = snd_ctl_card_info(handle, info)) < 0) {
      snd_ctl_close(handle);
      continue;
    }

    int dev = -1;

    while (snd_ctl_pcm_next_device(handle, &dev) >= 0 && dev >= 0 && device_id == NULL) {
      snd_pcm_info_set_device(pcminfo, dev);
      snd_pcm_info_set_subdevice(pcminfo, 0);

      // ouput devices
      snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
      if ((err = snd_ctl_pcm_info(handle, pcminfo)) == 0) {
        device_id=malloc(64);
        snprintf(device_id, 64, "plughw:%d,%d %s", card, dev, snd_ctl_card_info_get_name(info));
        fprintf(stderr,"ALSA output_device: %s\n",device_id);
      }
    }

    snd_ctl_close(handle);
   
  }

  if (device_id != NULL) return; // found one

  // look for dmix
  void **hints, **n;
  char *name, *descr, *io;

  if (snd_device_name_hint(-1, "pcm", &hints) < 0)
    return;
  n = hints;
  while (*n != NULL && device_id == NULL) {
    name = snd_device_name_get_hint(*n, "NAME");
    descr = snd_device_name_get_hint(*n, "DESC");
    io = snd_device_name_get_hint(*n, "IOID");
    
    if(strncmp("dmix:", name, 5)==0) {
      fprintf(stderr,"name=%s descr=%s io=%s\n",name, descr, io);
      device_id=malloc(64);
      
      snprintf(device_id, 64, "%s", name);
      fprintf(stderr,"ALSA output_device: %s\n",device_id);
    }

    if (name != NULL)
      free(name);
    if (descr != NULL)
      free(descr);
    if (io != NULL)
      free(io);
    n++;
  }
  snd_device_name_free_hint(hints);
}


void audio_open_output() {
  int err;
  snd_pcm_hw_params_t *hw_params;
  int rate=48000;
  int dir=0;

  int i;
  char hw[64];
  char *selected=device_id;
 
  i=0;
  while(selected[i]!=' ') {
    hw[i]=selected[i];
    i++;
  }
  hw[i]='\0';
  
  if ((err = snd_pcm_open (&playback_handle, hw, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf (stderr, "audio_open_output: cannot open audio device %s (%s)\n", 
            hw,
            snd_strerror (err));
    playback_handle = NULL;
    return;
  }

  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot allocate hardware parameter structure (%s)\n",
            snd_strerror (err));
    playback_handle=NULL;
    return;
  }

  if ((err = snd_pcm_hw_params_any (playback_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot initialize hardware parameter structure (%s)\n",
            snd_strerror (err));
    playback_handle=NULL;
    return;
  }

  if ((err = snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set access type (%s)\n",
            snd_strerror (err));
    playback_handle=NULL;
    return;
  }
	
  if ((err = snd_pcm_hw_params_set_format (playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set sample format (%s)\n",
            snd_strerror (err));
    playback_handle=NULL;
    return;
  }
	

  if ((err = snd_pcm_hw_params_set_rate_near (playback_handle, hw_params, &rate, &dir)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set sample rate (%s)\n",
            snd_strerror (err));
    playback_handle=NULL;
    return;
  }
	
  if ((err = snd_pcm_hw_params_set_channels (playback_handle, hw_params, 2)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set channel count (%s)\n",
            snd_strerror (err));
    playback_handle=NULL;
    return;
  }
	
  if ((err = snd_pcm_hw_params (playback_handle, hw_params)) < 0) {
    fprintf (stderr, "audio_open_output: cannot set parameters (%s)\n",
            snd_strerror (err));
    playback_handle=NULL;
    return;
  }
	
  snd_pcm_hw_params_free (hw_params);

  playback_offset=0;
  
  return;
}

void audio_write(int16_t left_sample,int16_t right_sample) {
  snd_pcm_sframes_t delay;
  int error;
  long trim;

  if(playback_handle!=NULL) {
    playback_buffer[playback_offset++]=right_sample;
    playback_buffer[playback_offset++]=right_sample>>8;
    playback_buffer[playback_offset++]=left_sample;
    playback_buffer[playback_offset++]=left_sample>>8;

    if(playback_offset==1024) {
      trim=0;

      if(snd_pcm_delay(playback_handle,&delay)==0) {
        if(delay>2048) {
          trim=delay-2048;
        }
      }

      if ((error = snd_pcm_writei (playback_handle, playback_buffer, 256-trim)) != 256-trim) {
        if(error==-EPIPE) {
          if ((error = snd_pcm_prepare (playback_handle)) < 0) {
            fprintf (stderr, "audio_write: cannot prepare audio interface for use (%s)\n",
                    snd_strerror (error));
            return;
          }
          if ((error = snd_pcm_writei (playback_handle, playback_buffer, 256-trim)) != 256) {
            fprintf (stderr, "audio_write: write to audio interface failed (%s)\n",
                    snd_strerror (error));
            return;
          }
        }
      }
      playback_offset=0;
    }
  }
}
#endif

//
// Dummy audio functions if this is compiled without audio support
//
#ifdef NEED_DUMMY_AUDIO
void audio_get_cards()
{
}
void audio_open_output()
{
}
void audio_write (int16_t l, int16_t r)
{
}
#endif

