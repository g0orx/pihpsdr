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

/*
 ***************************************************************************************************************
 * Description of a major overhaul,
 * Sep/Oct/Nov 2018, by DL1YCF Christoph van Wullen
 ***************************************************************************************************************
 *
 * a) SOME MINOR TECHNICAL ISSUES:
 * ===============================
 *
 * -keyer_close was actually unused. It is now called when local CW is no longer used
 *      and "joins" (terminates) the keyer thread.
 *
 * - GPIO pin names are no longer used in iambic.c
 *
 * - cw_keyer_spacing can now be set/un-set in the CW menu (cw_menu.c)
 *
 * - changing the scheduling policy now becomes only effective for the keyer thread, because
 *   sched_setscheduler() is called at the beginning and the end of the keyer thread with
 *   pid argument 0 (even better: use gettid() return value).
 *   NOTE: this is Linux-specific. Better switch to POSIX calls (e.g. pthread_setschedparam).
 *   mlockall and munlockall still apply process-wide and are therefore executed in
 *   keyer_init/keyer_close.
 *
 *   APPLE MacOS: if we run this in MacOS, most likely we are not "root". Therefore no locking/scheduling.
 *
 * b) SIDE TONE GENERATION
 * =======================
 *
 * There is a possibility to generate a side tone on one of the GPIO ports. This is necessary at
 * speeds greater than 20 wpm since there is a 50 msec delay (due to the LINUX sound system) between
 * generating the side tone and when it finally appears in the head-phone. The GPIO side tone comes
 * without a delay.
 *
 * The volume of the CW side tone in the standard audio channel is reduced to zero while producing
 * a square wave on the GPIO pin. The idea is to low-pass this signal and combine it with the audio
 * output (hardware mixer).
 *
 * In the previous version, the side tone was generate using the softTone functionality of wiringPI
 * within the GPIO module. However, this has a drawback:
 * one creates two high-priority threads (the tone generator and the keyer)
 * both firing at high speed (once a milli-sec and faster). This made the frequency of the tone quite
 * unstable. Instead, the side tone is now explicitly generated within the keyer (if GPIO side tone is activated)
 * by periodically writing "1" and "0" to the GPIO output while waiting for the end of the just-being-sent
 * dot or dash. Furthermore, the frequency of the side tone has been stabilized by "sleeping" UNTIL the
 * pre-calculated output-switching time (and not FOR the calculated amount, this is, using TIMER_ABSTIME
 * in clock_nanosleep).
 *
 * c) CW VOX
 * =========
 *
 * Suppose you hit the paddle while in RX mode. In this case, the SDR automatically switches
 * to TX, and remains so until a certain time (actually cw_keyer_hang_time, can be set in
 * the CW menu) has passed since sending the last element (then, PTT is removed).
 *
 * - cw_keyer_spacing can now be set/un-set in the CW menu (cw_menu.c)
 *
 * - during a dot or dash when no GPIO side tone is produced, the keyer thread simply waits and
 *   does no busy spinning.
 *
 * d) DOT/DASH MEMORY
 * ==================
 *
 * For reasons explained below, it is necessary to have TWO such memories for both dot and dash,
 * they are called dot_memory/dot_held and dash_memory/dash_held. Everything explained here and below
 * likewise holds for dot/dash exchanged.
 *
 * dot_memory is set whenever the dot paddle is hit. The natural way to do this is in the interrupt
 * service routine. So, keyer_event is the ONLY place where dot_memory is set. When the keyer wants to
 * know whether the dot paddle has been hit in some interval in time, it has to RESET dot_memory at the
 * beginning of the interval and can then read it out any time later.
 *
 * While the dot paddle may have been hit long ago, it is of interest whether is was still held at
 * the beginning of the last dash. Therefore, the keyer can store the state of the dot paddle in the
 * variable dot_held. This is done at the beginning of sending a dash. dot_held is separate from
 * dot_memory because only dot_held (but not dot_memory) is cleared in iambic mode A when both
 * paddles are released.
 *
 * e) IAMBIC MODES A AND B, AND SINGLE-LEVER PADDLES
 * =================================================
 *
 * It seems that there are lively discussions about what is what, so I distilled out the
 * following and added one clarification that becomes important when using this mode
 * with single-lever paddles:
 *
 * Suppose you hit the dash paddle and squeeze immediately afterwards (both paddles held),
 * then the keyer is supposed to produce a "dah dit dah dit dah dit .... " sequence. This is
 * what "iambic" is all about.
 *
 * Now what happens if you release BOTH paddles just after the second "dah" starts to sound?
 * In Mode-A you produce the letter "K" (dah dit dah), while in Mode-B, you produce the letter
 * "C" (dah dit dah dit). This is consensus. In this implementation, I follow the plead of
 * KO0B which I found on the internet and which essentially states that the "time window" for
 * releasing both paddles should extend until the end of the delay following the second "dah".
 * This is not only more convenient but also mimics the behaviour of the original Curtis
 * mode A 8044 chip (KO0B says).
 *
 * This means that at the end of the delay following a dash, in Mode-B the condition for
 * producing a dot is:
 *
 * -dot paddle has been held at the beginning of the last dash OR
 * -dot paddle is just being held OR
 * -dot paddle has been hit (and possibly already been released) during the time window
 *     from the beginning of the dash until now.
 *
 * This implies that in dot_held, the state of the dot key at the beginning of a dash is
 * stored, and that dot_memory is cleared at the beginning of a dash.
 *
 * To implement MODE-A, we clear dot_held at the end of the delay following the dash,
 * when both keys are released at that point in time.
 * We do not clear dot_memory in this case! Why?
 *
 * All the definitions on mode A/B I found relate to releasing keys that have been
 * squeezed for some time. Nothing is said about what happens if a key has been
 * recently hit. I am sometimes using a single-lever paddle, and at higher speeds,
 * I have also observed that I use a standard (dual-lever) paddle this way, namely
 * alternatingly hitting the paddle (shortly releasing the dash paddle when hitting
 * the dot one, and then possibly re-activating the dash paddle).
 *
 * It seems consensus that using a single-lever paddle, there should be no difference
 * between mode-A and mode-B.
 *
 * So imagine you want to produce the letter "N" (dah dit) this way. You will hit the dash key,
 * then immediately afterwards the dot key, and possibly have both keys released 
 * when the final dot of the letter should start. The final dot should not be suppressed
 * even in mode-A.
 * Thus: if both keys have been held at the beginning of a dash, and are relased
 * during the dash or the delay following, this dash is the last element being produced in mode-A.
 * BUT, if the dot key has been hit during the dash (or the delay following it), then a dot
 * element will be produced both in mode-A and mode-B, it is not necessary to keep on holding
 * the dot key.
 *
 * The present implementation will make both types happy: those doing pure squeeze and
 * those releasing one paddle when hitting the other.
 *
 **************************************************************************************************************
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

#ifdef GPIO
#include "gpio.h"
#endif
#include "radio.h"
#include "new_protocol.h"
#include "iambic.h"
#include "transmitter.h"
#include "ext.h"

static void* keyer_thread(void *arg);
static pthread_t keyer_thread_id;

#define MY_PRIORITY (90)
#define NSEC_PER_SEC   (1000000000)

static int dot_memory = 0;
static int dash_memory = 0;
static int dot_held = 0;
static int dash_held = 0;
static int key_state = 0;
static int dot_length = 0;
static int dash_length = 0;
static int kcwl = 0;
static int kcwr = 0;
int *kdot;
int *kdash;
int *kmemr;
int *kmeml;
static int running = 0;
#ifdef __APPLE__
static sem_t *cw_event;
#else
static sem_t cw_event;
#endif

static int cwvox = 0;

#ifndef __APPLE__
// using clock_nanosleep of librt
extern int clock_nanosleep(clockid_t __clock_id, int __flags,
      __const struct timespec *__req,
      struct timespec *__rem);
#endif

#ifndef GPIO
//
// Dummy functions if compiled without GPIO
//
int gpio_cw_sidetone_enabled() { return 0; }
void gpio_cw_sidetone_set(int level) {}
#endif

void keyer_update() {
    //
    // This function will take notice of changes in the following variables
    //
    // cw_keyer_internal
    // cw_keyer_speed
    // cw_keyer_weight
    // cw_keys_reversed
    //
    // that might occur asynchronously by changing settings in the CW menu.
    // Changes to cw_letter_spacing are notices without calling keyer_update.
    //
    // The most important thing here is to start/stop the keyer thread.
    //

    dot_length = 1200 / cw_keyer_speed;
    // will be 3 * dot length at standard weight
    dash_length = (dot_length * 3 * cw_keyer_weight) / 50;

    if (cw_keys_reversed) {
        kdot  = &kcwr;
        kdash = &kcwl;
        kmemr = &dot_memory;
        kmeml = &dash_memory;
    } else {
        kdot  = &kcwl;
        kdash = &kcwr;
        kmeml = &dot_memory;
        kmemr = &dash_memory;
    }

    if (cw_keyer_internal == 0) {
      if (!running) keyer_init();
    } else {
      if (running) keyer_close();
    }
}

//
// This is called by the paddle interrupt service routine
//
// left=1: left  paddle triggered event
// left=0: right paddle triggered event
//
// state=0: paddle has been released
// state=1: paddle has been hit
//
static int enforce_cw_vox;

void keyer_event(int left, int state) {
    if (!running) return;
    if (state) {
        // This is to remember whether the key stroke interrupts a running CAT CW 
	// Since in this case we return to RX after vox delay.
	if (CAT_cw_is_active) enforce_cw_vox=1;
        // This is for aborting CAT CW messages if a key is hit.
	cw_key_hit = 1;
    }
    if (left) {
      // left paddle hit or released
      kcwl = state;
      if (state) *kmeml=1;  // trigger dot/dash memory
    } else {
      // right paddle hit or released
      kcwr = state;
      if (state) *kmemr=1;  // trigger dot/dash memory
    }
    if (state) {
#ifdef __APPLE__
        sem_post(cw_event);
#else
        sem_post(&cw_event);
#endif
    }
}

void set_keyer_out(int state) {
  if (state) {
    cw_hold_key(1); // this starts a CW pulse in transmitter.c
  } else {
    cw_hold_key(0); // this stops a CW pulse in transmitter.c
  }
}

static void* keyer_thread(void *arg) {
    int pos;
    struct timespec loop_delay;
    int interval = 1000000; // 1 ms
    int sidewait;
    int sideloop;
    int i;
    int kdelay;
    int old_volume;
#ifdef __APPLE__
    struct timespec now;
#endif

#ifndef __APPLE__
    struct sched_param param;
    param.sched_priority = MY_PRIORITY;
    if(sched_setscheduler((pid_t)0, SCHED_FIFO, &param) == -1) {
            perror("sched_setscheduler failed");
    }
#endif

    fprintf(stderr,"keyer_thread  state running= %d\n", running);
    while(running) {
        enforce_cw_vox=0;
#ifdef __APPLE__
        sem_wait(cw_event);
#else
        sem_wait(&cw_event);
#endif

	// swallow any cw_events posted during the last "cw hang" time.
        if (!kcwl && !kcwr) continue;

        if (!mox && cw_breakin) {
          g_idle_add(ext_mox_update, (gpointer)(long) 1);
          // Wait for mox, that is, wait for WDSP shutting down the RX and
          // firing up the TX. This induces a small delay when hitting the key for
          // the first time, but excludes that the first dot is swallowed.
          // Note: if out-of-band, mox will never come, therefore
          // give up after 200 msec.
          i=200;
          while ((!mox || cw_not_ready) && i-- > 0) usleep(1000L);
          cwvox=(int) cw_keyer_hang_time;
	}
	// Trigger VOX if CAT CW was active and we have interrupted it by hitting a key
	if (enforce_cw_vox) cwvox=(int) cw_keyer_hang_time;

        key_state = CHECK;

        clock_gettime(CLOCK_MONOTONIC, &loop_delay);
        while (key_state != EXITLOOP || cwvox > 0) {
	  //
	  // if key_state == EXITLOOP and cwvox == 0, then
	  // just leave the while-loop without removing MOX
	  //
	  // re-trigger VOX if *not* busy-spinning
          if (cwvox > 0 && key_state != EXITLOOP && key_state != CHECK) cwvox=(int) cw_keyer_hang_time;

	  switch (key_state) {

	    case EXITLOOP:
		// If we arrive here, cwvox is greater than zero, since key_state==EXITLOOP
		// AND cwvox==0 leaves the outer "while" loop.
		cwvox--;
		// If CW-vox still hanging, continue "busy-spinning"
                if (cwvox == 0) {
		    // we have just reduced cwvox from 1 to 0.
		    g_idle_add(ext_mox_update,(gpointer)(long) 0);
		} else {
		    key_state=CHECK;
		}
	        break;

            case CHECK: // check for key press
		key_state = EXITLOOP;  // default next state
		// Do not decrement cwvox until zero here, otherwise
		// we won't enter the code 10 lines above that de-activates MOX.
		if (cwvox > 1) cwvox--;
                if (cw_keyer_mode == KEYER_STRAIGHT) {       // Straight/External key or bug
                    if (*kdash) {                  // send manual dashes
                      set_keyer_out(1);
            	      clock_gettime(CLOCK_MONOTONIC, &loop_delay);
		      // wait until dash is released
		      if (gpio_cw_sidetone_enabled()) {
		        // produce tone
		        // Note. Using clock_nanosleep with ABSTIME is absolutely
		        //       necessary to produce a stable frequency.
		        //       You still may painfully recognize that LINUX
		        //       e.g. on a RaspberryPi is not a real-time
		        //       operating system.
			// Mute "normal" CW side tone, it will be reactivated
			// at the end of the following delay.
                        old_volume=cw_keyer_sidetone_volume;
                        cw_keyer_sidetone_volume=0;
                        sidewait=500000000/cw_keyer_sidetone_frequency;
		        for (;;) {
                      	  gpio_cw_sidetone_set(1);
			  loop_delay.tv_nsec += sidewait;
            		  while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                	    loop_delay.tv_nsec -= NSEC_PER_SEC;
                	    loop_delay.tv_sec++;
            		  }
#ifdef __APPLE__
                	  clock_gettime(CLOCK_MONOTONIC, &now);
                	  now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                	  now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                	  while (now.tv_nsec < 0) {
                    	    now.tv_nsec += 1000000000;
                    	    now.tv_sec--;
                	  }
                	  nanosleep(&now, NULL);
#else
            		  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
                    	  gpio_cw_sidetone_set(0);
			  loop_delay.tv_nsec += sidewait;
            		  while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                	    loop_delay.tv_nsec -= NSEC_PER_SEC;
                	    loop_delay.tv_sec++;
            		  }
			  if (!*kdash) break;
#ifdef __APPLE__
                	  clock_gettime(CLOCK_MONOTONIC, &now);
                	  now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                	  now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                	  while (now.tv_nsec < 0) {
                    	    now.tv_nsec += 1000000000;
                    	    now.tv_sec--;
                	  }
                	  nanosleep(&now, NULL);
#else
            		  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
		        }
		      } else {
			// No-GPIO-sidetone case:
			// wait until dash is released. Check once a milli-sec
			for (;;) {
			  loop_delay.tv_nsec += interval;
            		  while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                	    loop_delay.tv_nsec -= NSEC_PER_SEC;
                	    loop_delay.tv_sec++;
            		  }
			  if (!*kdash) break;
#ifdef __APPLE__
                	  clock_gettime(CLOCK_MONOTONIC, &now);
                	  now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                	  now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                	  while (now.tv_nsec < 0) {
                    	    now.tv_nsec += 1000000000;
                    	    now.tv_sec--;
                	  }
                	  nanosleep(&now, NULL);
#else
            		  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
			}
                      }
		      // dash released.
                      set_keyer_out(0);
		      // since we stay in CHECK mode, re-trigger cwvox here
		      cwvox=cw_keyer_hang_time;
		      // wait at least 10ms before re-activating sidetone,
		      // to allow the envelope of the side tone reaching zero
                      if (gpio_cw_sidetone_enabled()) {
			  loop_delay.tv_nsec += 10*interval;
            		  while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                	    loop_delay.tv_nsec -= NSEC_PER_SEC;
                	    loop_delay.tv_sec++;
            		  }
#ifdef __APPLE__
                	  clock_gettime(CLOCK_MONOTONIC, &now);
                	  now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                	  now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                	  while (now.tv_nsec < 0) {
                    	    now.tv_nsec += 1000000000;
                    	    now.tv_sec--;
                	  }
                	  nanosleep(&now, NULL);
#else
            		  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
			  cw_keyer_sidetone_volume=old_volume;
                      }
                    }
		    if (*kdot) {
			// "bug" mode: dot key activates automatic dots
                        key_state = SENDDOT;
                    }
		    // end of KEYER_STRAIGHT case
                } else {
		    // Paddle
		    // If both following if-statements are true, which one should win?
		    // I think a "simultaneous squeeze" means a dot-dash sequence, since in 
		    // a dash-dot sequence there is a larger time window to hit the dot.
                    if (*kdash) key_state = SENDDASH;
                    if (*kdot) key_state = SENDDOT;
                }
		break;

	    case SENDDOT:
		dash_memory = 0;
                dash_held = *kdash;
                set_keyer_out(1);
            	clock_gettime(CLOCK_MONOTONIC, &loop_delay);
		if (gpio_cw_sidetone_enabled()) {
                  old_volume=cw_keyer_sidetone_volume;
                  cw_keyer_sidetone_volume=0;
                  sidewait=500000000/cw_keyer_sidetone_frequency;
                  sideloop=(500000*dot_length)/sidewait;
                  for (i=0; i<sideloop; i++) {
                    gpio_cw_sidetone_set(1);
                    loop_delay.tv_nsec += sidewait;
                    while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                      loop_delay.tv_nsec -= NSEC_PER_SEC;
                      loop_delay.tv_sec++;
                    } 
#ifdef __APPLE__
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                    now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                    while (now.tv_nsec < 0) {
                      now.tv_nsec += 1000000000;
                      now.tv_sec--;
                    }
                    nanosleep(&now, NULL);
#else
                    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
                    gpio_cw_sidetone_set(0);
                    loop_delay.tv_nsec += sidewait;
                    while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                      loop_delay.tv_nsec -= NSEC_PER_SEC;
                      loop_delay.tv_sec++;
		    }
#ifdef __APPLE__
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                    now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                    while (now.tv_nsec < 0) {
                      now.tv_nsec += 1000000000;
                      now.tv_sec--;
                    }
                    nanosleep(&now, NULL);
#else
                    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
                  }
		} else {
		  // No-GPIO-sidetone case: just wait
                  loop_delay.tv_nsec += 1000000 * dot_length;
                  while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                    loop_delay.tv_nsec -= NSEC_PER_SEC;
                    loop_delay.tv_sec++;
		  }
#ifdef __APPLE__
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                    now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                    while (now.tv_nsec < 0) {
                      now.tv_nsec += 1000000000;
                      now.tv_sec--;
                    }
                    nanosleep(&now, NULL);
#else
                  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
		}
                set_keyer_out(0);
                key_state = DOTDELAY;       // add inter-character spacing of one dot length
		kdelay=0;
                break;

            case DOTDELAY:
                kdelay++;
                if (kdelay > dot_length) {
		  if (gpio_cw_sidetone_enabled()) {
		    cw_keyer_sidetone_volume=old_volume;
		  }
		  if (cw_keyer_mode == KEYER_STRAIGHT) {
		    // bug mode: continue sending dots or exit, depending on current dot key status
		    key_state = EXITLOOP;
		    if (*kdot) key_state=SENDDOT;
		    // end of bug/straight case
                  } else {
//
//                  DL1YCF:
//                  This is my understanding where MODE A comes in:
//                  If at the end of the delay, BOTH keys are
//                  released, then do not start the next element.
//                  However, if  the dash has been hit DURING the preceeding
//                  dot, produce a dash in either case
//
                    if (cw_keyer_mode == KEYER_MODE_A && !*kdot && !*kdash) dash_held=0;

                    if (dash_memory || *kdash || dash_held)
                        key_state = SENDDASH;
                    else if (*kdot)                                 // dot still held, so send a dot
			key_state = SENDDOT;
		    else if (cw_keyer_spacing) {
                        dot_memory = dash_memory = 0;
                        key_state = LETTERSPACE;
		        kdelay=0;
		    } else
			key_state = EXITLOOP;
		    // end of iambic case
                  }
                }   
                break;

	    case SENDDASH:
		dot_memory =  0;
		dot_held = *kdot;  // remember if dot is still held at beginning of the dash
                set_keyer_out(1);
                clock_gettime(CLOCK_MONOTONIC, &loop_delay);
		if (gpio_cw_sidetone_enabled()) {
                  old_volume=cw_keyer_sidetone_volume;
                  cw_keyer_sidetone_volume=0;
                  sidewait=500000000/cw_keyer_sidetone_frequency;
                  sideloop=(500000*dash_length)/sidewait;
                  for (i=0; i<sideloop; i++) {
                    gpio_cw_sidetone_set(1);
                    loop_delay.tv_nsec += sidewait;
                    while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                      loop_delay.tv_nsec -= NSEC_PER_SEC;
                      loop_delay.tv_sec++;
                    }
#ifdef __APPLE__
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                    now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                    while (now.tv_nsec < 0) {
                      now.tv_nsec += 1000000000;
                      now.tv_sec--;
                    }
                    nanosleep(&now, NULL);
#else
                    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
                    gpio_cw_sidetone_set(0);
                    loop_delay.tv_nsec += sidewait;
                    while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                      loop_delay.tv_nsec -= NSEC_PER_SEC;
                      loop_delay.tv_sec++;
                    }
#ifdef __APPLE__
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                    now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                    while (now.tv_nsec < 0) {
                      now.tv_nsec += 1000000000;
                      now.tv_sec--;
                    }
                    nanosleep(&now, NULL);
#else
                    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
                  }
		} else {
		  // No-GPIO-sidetone case: just wait
                  loop_delay.tv_nsec += 1000000L * dash_length;
                  while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                    loop_delay.tv_nsec -= NSEC_PER_SEC;
                    loop_delay.tv_sec++;
                  }
#ifdef __APPLE__
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
                    now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
                    while (now.tv_nsec < 0) {
                      now.tv_nsec += 1000000000;
                      now.tv_sec--;
                    }
                    nanosleep(&now, NULL);
#else
                  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
		}
                set_keyer_out(0);
                key_state = DASHDELAY;       // add inter-character spacing of one dot length
		kdelay=0;
		// do not fall through, update VOX at beginning of loop
                break;

            case DASHDELAY:
		// we never arrive here in STRAIGHT/BUG mode
                kdelay++;
                if (kdelay > dot_length) {
		  if (gpio_cw_sidetone_enabled()) {
		    cw_keyer_sidetone_volume=old_volume;
		  }
//
//                  DL1YCF:
//                  This is my understanding where MODE A comes in:
//                  If at the end of the dash delay, BOTH keys are
//                  released, then do not start the next element.
//                  However, if  the dot has been hit DURING the preceeding
//                  dash, produce a dot in either case
//
                    if (cw_keyer_mode == KEYER_MODE_A && !*kdot && !*kdash) dot_held=0;
                    if (dot_memory || *kdot || dot_held)
                        key_state = SENDDOT;
                    else if (*kdash)
			key_state = SENDDASH;
		    else if (cw_keyer_spacing) {
                        dot_memory = dash_memory = 0;
                        key_state = LETTERSPACE;
		        kdelay=0;
		    } else key_state = EXITLOOP;
                }
                break;

            case LETTERSPACE:
                // Add letter space (3 x dot delay) to end of character and check if a paddle is pressed during this time.
                // Actually add 2 x dot_length since we already have a dot delay at the end of the character.
                kdelay++;
                if (kdelay > 2 * dot_length) {
                    if (dot_memory)         // check if a dot or dash paddle was pressed during the delay.
                        key_state = SENDDOT;
                    else if (dash_memory)
                        key_state = SENDDASH;
                    else key_state = EXITLOOP;   // no memories set so restart
                }
                break;

            default:
		fprintf(stderr,"KEYER THREAD: unknown state=%d",(int) key_state);
                key_state = EXITLOOP;
            }

	    // time stamp in loop_delay is either the last time stamp from the
	    // top of the loop, or the last time stamp from tone generation
	    // NOTE: we are using ABSTIME here to produce accurate delays.
            loop_delay.tv_nsec += interval;
            while (loop_delay.tv_nsec >= NSEC_PER_SEC) {
                loop_delay.tv_nsec -= NSEC_PER_SEC;
                loop_delay.tv_sec++;
            }
#ifdef __APPLE__
            clock_gettime(CLOCK_MONOTONIC, &now);
            now.tv_sec =loop_delay.tv_sec  - now.tv_sec;
            now.tv_nsec=loop_delay.tv_nsec - now.tv_nsec;
            while (now.tv_nsec < 0) {
              now.tv_nsec += 1000000000;
              now.tv_sec--;
            }
            nanosleep(&now, NULL);
#else
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &loop_delay, NULL);
#endif
        }

    }
    fprintf(stderr,"keyer_thread: EXIT\n");
#ifndef __APPLE__
    param.sched_priority = 0;
    sched_setscheduler((pid_t) 0, SCHED_OTHER, &param);
#endif
    return NULL;
}

void keyer_close() {
    fprintf(stderr,".... closing keyer thread.\n");
    running=0;
    // keyer thread may be sleeping, so wake it up
#ifdef __APPLE__
    sem_post(cw_event);
#else
    sem_post(&cw_event);
#endif
    pthread_join(keyer_thread_id, NULL);
#ifdef __APPLE__
    sem_close(cw_event);
#else
    sem_close(&cw_event);
#endif

#ifndef __APPLE__
    munlockall();
#endif
}

int keyer_init() {
    int rc;

    fprintf(stderr,".... starting keyer thread.\n");
    
#ifndef __APPLE__
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
            perror("mlockall failed");
    }
#endif

#ifdef __APPLE__
    sem_unlink("CW");
    cw_event=sem_open("CW", O_CREAT | O_EXCL, 0700, 0);
    rc = (cw_event == SEM_FAILED);
#else
    rc = sem_init(&cw_event, 0, 0);
#endif
    running = 1;
    rc |= pthread_create(&keyer_thread_id, NULL, keyer_thread, NULL);
    if(rc < 0) {
        fprintf(stderr,"pthread_create for keyer_thread failed %d\n", rc);
        exit(-1);
    }
    return 0;
}
