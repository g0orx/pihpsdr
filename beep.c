/*

    10/16/2016, Rick Koch / N1GP, added to give an alternative sidetone
                by using the RPi's audio output.

--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------

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
#include <signal.h>
#include <semaphore.h>
#include <math.h>
#include <alsa/asoundlib.h>

double beep_freq = 800;                                 /* sinusoidal wave frequency in Hz */

static char *device = "hw:0";
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;    /* sample format */
static unsigned int rate = 48000;                       /* stream rate */
static unsigned int channels = 2;                       /* count of channels */
static unsigned int buffer_time = 36000;                /* ring buffer length in us */
static unsigned int period_time = 6000;                 /* period time in us */
static int period_event = 0;                            /* produce poll event after each period */
static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static snd_output_t *output = NULL;

static void* beep_thread(void *arg);
static pthread_t beep_thread_id;

static snd_pcm_t *handle;
static signed short *samples;
static snd_pcm_channel_area_t *areas;

static void generate_sine(const snd_pcm_channel_area_t *areas, 
                          snd_pcm_uframes_t offset,
                          int count, double *_phase)
{
        static double max_phase = 2. * M_PI;
        double phase = *_phase;
        double step = max_phase*beep_freq/(double)rate;
        unsigned char *samples[channels];
        int steps[channels];
        unsigned int chn;
        int format_bits = snd_pcm_format_width(format);
        unsigned int maxval = (1 << (format_bits - 1)) - 1;
        int bps = format_bits / 8;  /* bytes per sample */
        int phys_bps = snd_pcm_format_physical_width(format) / 8;

        /* verify and prepare the contents of areas */
        for (chn = 0; chn < channels; chn++) {
                if ((areas[chn].first % 8) != 0) {
                        printf("areas[%i].first == %i, aborting...\n", chn, areas[chn].first);
                        exit(EXIT_FAILURE);
                }
                samples[chn] = /*(signed short *)*/(((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
                if ((areas[chn].step % 16) != 0) {
                        printf("areas[%i].step == %i, aborting...\n", chn, areas[chn].step);
                        exit(EXIT_FAILURE);
                }
                steps[chn] = areas[chn].step / 8;
                samples[chn] += offset * steps[chn];
        }
        /* fill the channel areas */
        while (count-- > 0) {
                union {
                        float f;
                        int i;
                } fval;
                int res, i;

                res = sin(phase) * maxval;
                res ^= 1U << (format_bits - 1);
                for (chn = 0; chn < channels; chn++) {
                        /* Generate data in native endian format */
                            for (i = 0; i < bps; i++)
                                *(samples[chn] + i) = (res >>  i * 8) & 0xff;
                        samples[chn] += steps[chn];
                }
                phase += step;
                if (phase >= max_phase)
                        phase -= max_phase;
        }
        *_phase = phase;
}

static int xrun_recovery(snd_pcm_t *handle, int err)
{
        if (err == -EPIPE) {    /* under-run */
                err = snd_pcm_prepare(handle);
                if (err < 0)
                        printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
                return 0;
        } else if (err == -ESTRPIPE) {
                while ((err = snd_pcm_resume(handle)) == -EAGAIN)
                        sleep(1);       /* wait until the suspend flag is released */
                if (err < 0) {
                        err = snd_pcm_prepare(handle);
                        if (err < 0)
                                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
                }
                return 0;
        }
        return err;
}

static int write_loop(snd_pcm_t *handle,
                      signed short *samples,
                      snd_pcm_channel_area_t *areas)
{
        double phase = 0;
        signed short *ptr;
        int err, cptr;

        while (1) {
                generate_sine(areas, 0, period_size, &phase);
                ptr = samples;
                cptr = period_size;
                while (cptr > 0) {
                        err = snd_pcm_writei(handle, ptr, cptr);
                        if (err == -EAGAIN)
                                continue;
                        if (err < 0) {
                                if (xrun_recovery(handle, err) < 0) {
                                        printf("Write error: %s\n", snd_strerror(err));
                                        exit(EXIT_FAILURE);
                                }
                                break;  /* skip one period */
                        }
                        ptr += err * channels;
                        cptr -= err;
                }
        }
}

static int set_hwparams(snd_pcm_t *handle,
                        snd_pcm_hw_params_t *params,
                        snd_pcm_access_t access)
{
        unsigned int rrate;
        snd_pcm_uframes_t size;
        int err, dir;
        /* choose all parameters */
        err = snd_pcm_hw_params_any(handle, params);
        if (err < 0) {
                printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
                return err;
        }
        /* set the interleaved read/write format */
        err = snd_pcm_hw_params_set_access(handle, params, access);
        if (err < 0) {
                printf("Access type not available for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* set the sample format */
        err = snd_pcm_hw_params_set_format(handle, params, format);
        if (err < 0) {
                printf("Sample format not available for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* set the count of channels */
        err = snd_pcm_hw_params_set_channels(handle, params, channels);
        if (err < 0) {
                printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
                return err;
        }
        /* set the stream rate */
        rrate = rate;
        err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
        if (err < 0) {
                printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
                return err;
        }
        if (rrate != rate) {
                printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
                return -EINVAL;
        }
        /* set the buffer time */
        err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
        if (err < 0) {
                printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
                return err;
        }
        err = snd_pcm_hw_params_get_buffer_size(params, &size);
        if (err < 0) {
                printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
                return err;
        }
        buffer_size = size;
        /* set the period time */
        err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
        if (err < 0) {
                printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
                return err;
        }
        err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
        if (err < 0) {
                printf("Unable to get period size for playback: %s\n", snd_strerror(err));
                return err;
        }
        period_size = size;
        /* write the parameters to device */
        err = snd_pcm_hw_params(handle, params);
        if (err < 0) {
                printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
                return err;
        }
        return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
        int err;
        /* get the current swparams */
        err = snd_pcm_sw_params_current(handle, swparams);
        if (err < 0) {
                printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* start the transfer when the buffer is almost full: */
        /* (buffer_size / avail_min) * avail_min */
        err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
        if (err < 0) {
                printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* allow the transfer when at least period_size samples can be processed */
        /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
        err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
        if (err < 0) {
                printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
                return err;
        }
        /* enable period events when requested */
        if (period_event) {
                err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
                if (err < 0) {
                        printf("Unable to set period event: %s\n", snd_strerror(err));
                        return err;
                }
        }
        /* write the parameters to the playback device */
        err = snd_pcm_sw_params(handle, swparams);
        if (err < 0) {
                printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
                return err;
        }
        return 0;
}

void beep_mute(int mute)
{
    int err;
    snd_ctl_elem_id_t *id;
    snd_hctl_elem_t *elem;
    snd_ctl_elem_value_t *control;
    snd_hctl_t *hctl;

    err = snd_hctl_open(&hctl, device, 0);
    err = snd_hctl_load(hctl);
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, "PCM Playback Switch");

    elem = snd_hctl_find_elem(hctl, id);

    snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_value_set_id(control, id);   

    snd_ctl_elem_value_set_integer(control, 0, mute);
    err = snd_hctl_elem_write(elem, control);
    snd_hctl_close(hctl);
    if (err) fprintf(stderr, "ERROR beep_mute()\n");
}

void beep_vol(long volume)
{
    long min, max, output;
    snd_mixer_selem_id_t *sid;
    snd_mixer_t *mhandle;
    const char *selem_name = "PCM";
    int do_once = 1;
    snd_mixer_elem_t* elem;

    beep_mute(1);
    if (volume > 100) volume = 100; // sounds raspy any higher
    if (volume < 0) volume = 0;
    snd_mixer_open(&mhandle, 0);
    snd_mixer_attach(mhandle, device);
    snd_mixer_selem_register(mhandle, NULL, NULL);
    snd_mixer_load(mhandle);
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    elem = snd_mixer_find_selem(mhandle, sid);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    min = -2000; // PI's audio mixer range is broken
    output = (((max - min) * volume) / 100) + min;
    snd_mixer_selem_set_playback_volume_all(elem, output);

    beep_mute(0);
    snd_mixer_close(mhandle);
}

static void* beep_thread(void *arg) {
        int err;
        snd_pcm_hw_params_t *hwparams;
        snd_pcm_sw_params_t *swparams;
        unsigned int chn;
        snd_pcm_hw_params_alloca(&hwparams);
        snd_pcm_sw_params_alloca(&swparams);

        err = snd_output_stdio_attach(&output, stdout, 0);
        if (err < 0) {
                printf("Output failed: %s\n", snd_strerror(err));
                return 0;
        }

		
        if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
                printf("Playback open error: %s\n", snd_strerror(err));
                return 0;
        }
        
        if ((err = set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
                printf("Setting of hwparams failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }

        if ((err = set_swparams(handle, swparams)) < 0) {
                printf("Setting of swparams failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }
		
        samples = malloc((period_size * channels * snd_pcm_format_physical_width(format)) / 8);
        if (samples == NULL) {
                printf("No enough memory\n");
                exit(EXIT_FAILURE);
        }
        
        areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
        if (areas == NULL) {
                printf("No enough memory\n");
                exit(EXIT_FAILURE);
        }
        for (chn = 0; chn < channels; chn++) {
                areas[chn].addr = samples;
                areas[chn].first = chn * snd_pcm_format_physical_width(format);
                areas[chn].step = channels * snd_pcm_format_physical_width(format);
        }

        write_loop(handle, samples, areas);

        free(areas);
        free(samples);
        snd_pcm_close(handle);

        return 0;
}

void beep_init() {
    int i = pthread_create(&beep_thread_id, NULL, beep_thread, NULL);
    if(i < 0) {
        fprintf(stderr,"pthread_create for beep_thread failed %d\n", i);
        exit(-1);
    }
}

void beep_close() {
//    beep_vol(0);
    beep_mute(1);
    pthread_cancel(beep_thread_id);
}
