/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* 10/12/2016, Rick Koch / N1GP adapted Phil's verilog code from
*             the openHPSDR Hermes iambic.v implementation to work
*             with John's pihpsdr project. This allows one to work
*             CW solely from the pihpsdr unit remotely.
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
* ---------------------------------------------------------------------------------
*         Copywrite (C) Phil Harman VK6PH May 2014
* ---------------------------------------------------------------------------------
*
* The code implements an Iambic CW keyer.  The following features are supported:
*
*         * Variable speed control from 1 to 60 WPM
*         * Dot and Dash memory
*         * Straight, Bug, Iambic Mode A or B Modes
*         * Variable character weighting
*         * Automatic Letter spacing
*         * Paddle swap
*
* Dot and Dash memory works by registering an alternative paddle closure whilst a paddle is pressed.
* The alternate paddle closure can occur at any time during a paddle closure and is not limited to being
* half way through the current dot or dash. This feature could be added if required.
*
* In Straight mode, closing the DASH paddle will result in the output following the input state.  This enables a
* straight morse key or external Iambic keyer to be connected.
*
* In Bug mode closing the dot paddle will send repeated dots.
*
* The difference between Iambic Mode A and B lies in what the keyer does when both paddles are released. In Mode A the
* keyer completes the element being sent when both paddles are released. In Mode B the keyer sends an additional
* element opposite to the one being sent when the paddles are released.
*
* This only effects letters and characters like C, period or AR.
*
* Automatic Letter Space works as follows: When enabled, if you pause for more than one dot time between a dot or dash
* the keyer will interpret this as a letter-space and will not send the next dot or dash until the letter-space time has been met.
* The normal letter-space is 3 dot periods. The keyer has a paddle event memory so that you can enter dots or dashes during the
* inter-letter space and the keyer will send them as they were entered.
*
* Speed calculation -  Using standard PARIS timing, dot_period(mS) = 1200/WPM
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/mman.h>

#include <wiringPi.h>
#include <softTone.h>

#include "gpio.h"
#include "radio.h"
#include "new_protocol.h"
#include "iambic.h"
#include "transmitter.h"

static void* keyer_thread(void *arg);
static pthread_t keyer_thread_id;

// set to 0 to use the PI's hw:0 audio out for sidetone
#define SIDETONE_GPIO 0 // this is in wiringPi notation // tried 4 working great.

#define MY_PRIORITY (90)
#define MAX_SAFE_STACK (8*1024)
#define NSEC_PER_SEC   (1000000000)

/*
enum {
    CHECK = 0,
    PREDOT,
    PREDASH,
    SENDDOT,
    SENDDASH,
    DOTDELAY,
    DASHDELAY,
    DOTHELD,
    DASHHELD,
    LETTERSPACE,
    EXITLOOP
};
*/

static int dot_memory = 0;
static int dash_memory = 0;
int key_state = 0;
static int kdelay = 0;
static int dot_delay = 0;
static int dash_delay = 0;
static int kcwl = 0;
static int kcwr = 0;
int *kdot;
int *kdash;
static int running = 0;
#ifdef __APPLE__
static sem_t *cw_event;
#else
static sem_t cw_event;
#endif

int keyer_out = 0;

// using clock_nanosleep of librt
extern int clock_nanosleep(clockid_t __clock_id, int __flags,
      __const struct timespec *__req,
      struct timespec *__rem);

void stack_prefault(void) {
        unsigned char dummy[MAX_SAFE_STACK];

        memset(dummy, 0, MAX_SAFE_STACK);
        return;
}

void keyer_update() {
    if (!running)
        keyer_init();

    dot_delay = 1200 / cw_keyer_speed;
    // will be 3 * dot length at standard weight
    dash_delay = (dot_delay * 3 * cw_keyer_weight) / 50;

    if (cw_keys_reversed) {
        kdot = &kcwr;
        kdash = &kcwl;
    } else {
        kdot = &kcwl;
        kdash = &kcwr;
    }

}

#ifdef GPIO
void keyer_event(int gpio, int level) {
    int state = (level == 0);

    if (gpio == CWL_BUTTON)
        kcwl = state;
    else  // CWR_BUTTON
        kcwr = state;

    if (state || cw_keyer_mode == KEYER_STRAIGHT) {
#ifdef __APPLE__
        sem_post(cw_event);
#else
        sem_post(&cw_event);
#endif
    }
}
#endif

void clear_memory() {
    dot_memory  = 0;
    dash_memory = 0;
}

void set_keyer_out(int state) {
    if (keyer_out != state) {
        keyer_out = state;
        if(protocol==NEW_PROTOCOL) schedule_high_priority(9);
		fprintf(stderr,"set_keyer_out keyer_out= %d\n", keyer_out);
        if (state)
            if (SIDETONE_GPIO)
                softToneWrite (SIDETONE_GPIO, cw_keyer_sidetone_frequency);
            else {
				cw_sidetone_mute(1);
            }
        else
            if (SIDETONE_GPIO)
                softToneWrite (SIDETONE_GPIO, 0);
            else  {
				cw_sidetone_mute(0);
            }
    }
}

static void* keyer_thread(void *arg) {
    int pos;
    struct timespec loop_delay;
    int interval = 1000000; // 1 ms

fprintf(stderr,"keyer_thread  state running= %d\n", running);
    while(running) {
#ifdef __APPLE__
        sem_wait(cw_event);
#else
        sem_wait(&cw_event);
#endif

        key_state = CHECK;

        while (key_state != EXITLOOP) {
            switch(key_state) {
            case CHECK: // check for key press
                if (cw_keyer_mode == KEYER_STRAIGHT) {       // Straight/External key or bug
                    if (*kdash) {                  // send manual dashes
                        set_keyer_out(1);
                        key_state = EXITLOOP;
                    }
                    else if (*kdot)                // and automatic dots
                        key_state = PREDOT;
                    else {
                        set_keyer_out(0);
                        key_state = EXITLOOP;
                    }
                }
                else {
                    if (*kdot)
                        key_state = PREDOT;
                    else if (*kdash)
                        key_state = PREDASH;
                    else {
                        set_keyer_out(0);
                        key_state = EXITLOOP;
                    }
                }
                break;
            case PREDOT:                         // need to clear any pending dots or dashes
                clear_memory();
                key_state = SENDDOT;
                break;
            case PREDASH:
                clear_memory();
                key_state = SENDDASH;
                break;

            // dot paddle  pressed so set keyer_out high for time dependant on speed
            // also check if dash paddle is pressed during this time
            case SENDDOT:
                set_keyer_out(1);
                if (kdelay == dot_delay) {
                    kdelay = 0;
                    set_keyer_out(0);
                    key_state = DOTDELAY;        // add inter-character spacing of one dot length
                }
                else kdelay++;

                // if Mode A and both paddels are relesed then clear dash memory
                if (cw_keyer_mode == KEYER_MODE_A)
                    if (!*kdot & !*kdash)
                        dash_memory = 0;
                    else if (*kdash)                   // set dash memory
                        dash_memory = 1;
                break;

            // dash paddle pressed so set keyer_out high for time dependant on 3 x dot delay and weight
            // also check if dot paddle is pressed during this time
            case SENDDASH:
                set_keyer_out(1);
                if (kdelay == dash_delay) {
                    kdelay = 0;
                    set_keyer_out(0);
                    key_state = DASHDELAY;       // add inter-character spacing of one dot length
                }
                else kdelay++;

                // if Mode A and both padles are relesed then clear dot memory
                if (cw_keyer_mode == KEYER_MODE_A)
                    if (!*kdot & !*kdash)
                        dot_memory = 0;
                    else if (*kdot)                    // set dot memory
                        dot_memory = 1;
                break;

            // add dot delay at end of the dot and check for dash memory, then check if paddle still held
            case DOTDELAY:
                if (kdelay == dot_delay) {
                    kdelay = 0;
                    if(!*kdot && cw_keyer_mode == KEYER_STRAIGHT)   // just return if in bug mode
                        key_state = EXITLOOP;
                    else if (dash_memory)                 // dash has been set during the dot so service
                        key_state = PREDASH;
                    else key_state = DOTHELD;             // dot is still active so service
                }
                else kdelay++;

                if (*kdash)                                 // set dash memory
                    dash_memory = 1;
                break;

            // add dot delay at end of the dash and check for dot memory, then check if paddle still held
            case DASHDELAY:
                if (kdelay == dot_delay) {
                    kdelay = 0;

                    if (dot_memory)                       // dot has been set during the dash so service
                        key_state = PREDOT;
                    else key_state = DASHHELD;            // dash is still active so service
                }
                else kdelay++;

                if (*kdot)                                  // set dot memory
                    dot_memory = 1;
                break;

            // check if dot paddle is still held, if so repeat the dot. Else check if Letter space is required
            case DOTHELD:
                if (*kdot)                                  // dot has been set during the dash so service
                    key_state = PREDOT;
                else if (*kdash)                            // has dash paddle been pressed
                    key_state = PREDASH;
                else if (cw_keyer_spacing) {    // Letter space enabled so clear any pending dots or dashes
                    clear_memory();
                    key_state = LETTERSPACE;
                }
                else key_state = EXITLOOP;
                break;

            // check if dash paddle is still held, if so repeat the dash. Else check if Letter space is required
            case DASHHELD:
                if (*kdash)                   // dash has been set during the dot so service
                    key_state = PREDASH;
                else if (*kdot)               // has dot paddle been pressed
                    key_state = PREDOT;
                else if (cw_keyer_spacing) {    // Letter space enabled so clear any pending dots or dashes
                    clear_memory();
                    key_state = LETTERSPACE;
                }
                else key_state = EXITLOOP;
                break;

            // Add letter space (3 x dot delay) to end of character and check if a paddle is pressed during this time.
            // Actually add 2 x dot_delay since we already have a dot delay at the end of the character.
            case LETTERSPACE:
                if (kdelay == 2 * dot_delay) {
                    kdelay = 0;
                    if (dot_memory)         // check if a dot or dash paddle was pressed during the delay.
                        key_state = PREDOT;
                    else if (dash_memory)
                        key_state = PREDASH;
                    else key_state = EXITLOOP;   // no memories set so restart
                }
                else kdelay++;

                // save any key presses during the letter space delay
                if (*kdot) dot_memory = 1;
                if (*kdash) dash_memory = 1;
                break;

            default:
                key_state = EXITLOOP;

            }

            clock_gettime(CLOCK_MONOTONIC, &loop_delay);
            loop_delay.tv_nsec += interval;
            while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                loop_delay.tv_nsec -= NSEC_PER_SEC;
                loop_delay.tv_sec++;
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
        }

    }
fprintf(stderr,"keyer_thread: EXIT\n");
}

void keyer_close() {
    running=0;
}

int keyer_init() {
    int rc;
    struct sched_param param;

    param.sched_priority = MY_PRIORITY;
    if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            perror("sched_setscheduler failed");
            running = 0;
    }

    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
            perror("mlockall failed");
            running = 0;
    }

    if (SIDETONE_GPIO){
        softToneCreate(SIDETONE_GPIO);
    }

#ifdef __APPLE__
    cw_event=sem_open("CW", O_CREAT, 0700, 0);
    rc = (cw_event == SEM_FAILED);
#else
    rc = sem_init(&cw_event, 0, 0);
#endif
    rc |= pthread_create(&keyer_thread_id, NULL, keyer_thread, NULL);
    running = 1;
    if(rc < 0) {
        fprintf(stderr,"pthread_create for keyer_thread failed %d\n", rc);
        exit(-1);
    }

    return 0;
}
