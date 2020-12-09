#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include <pulse/simple.h>

#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "audio.h"
#include "mode.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif

int n_input_devices;
AUDIO_DEVICE input_devices[MAX_AUDIO_DEVICES];
int n_output_devices;
AUDIO_DEVICE output_devices[MAX_AUDIO_DEVICES];

//
// Ring buffer for "local microphone" samples
// NOTE: lead large buffer for some "loopback" devices which produce
//       samples in large chunks if fed from digimode programs.
//
#define MICRINGLEN 6000
static float  *mic_ring_buffer=NULL;
static int     mic_ring_read_pt=0;
static int     mic_ring_write_pt=0;

static pa_glib_mainloop *main_loop;
static pa_mainloop_api *main_loop_api;
static pa_operation *op;
static pa_context *pa_ctx;
static pa_simple* microphone_stream;
static gint local_microphone_buffer_offset;
static float *local_microphone_buffer;
static GThread *mic_read_thread_id;
static gboolean running;

gint local_microphone_buffer_size=720;
GMutex audio_mutex;

static void source_list_cb(pa_context *context,const pa_source_info *s,int eol,void *data) {
  int i;
  if(eol>0) {
    for(i=0;i<n_input_devices;i++) {
      g_print("Input: %d: %s (%s)\n",input_devices[i].index,input_devices[i].name,input_devices[i].description);
    }
    g_mutex_unlock(&audio_mutex);
  } else if(n_input_devices<MAX_AUDIO_DEVICES) {
    input_devices[n_input_devices].name=g_new0(char,strlen(s->name)+1);
    strncpy(input_devices[n_input_devices].name,s->name,strlen(s->name));
    input_devices[n_input_devices].description=g_new0(char,strlen(s->description)+1);
    strncpy(input_devices[n_input_devices].description,s->description,strlen(s->description));
    input_devices[n_input_devices].index=s->index;
    n_input_devices++;
  }
}

static void sink_list_cb(pa_context *context,const pa_sink_info *s,int eol,void *data) {
  int i;
  if(eol>0) {
    for(i=0;i<n_output_devices;i++) {
      g_print("Output: %d: %s (%s)\n",output_devices[i].index,output_devices[i].name,output_devices[i].description);
    }
    op=pa_context_get_source_info_list(pa_ctx,source_list_cb,NULL);
  } else if(n_output_devices<MAX_AUDIO_DEVICES) {
    output_devices[n_output_devices].name=g_new0(char,strlen(s->name)+1);
    strncpy(output_devices[n_output_devices].name,s->name,strlen(s->name));
    output_devices[n_output_devices].description=g_new0(char,strlen(s->description)+1);
    strncpy(output_devices[n_output_devices].description,s->description,strlen(s->description));
    output_devices[n_output_devices].index=s->index;
    n_output_devices++;
  }
}

static void state_cb(pa_context *c, void *userdata) {
  pa_context_state_t state;

  state = pa_context_get_state(c);

g_print("%s: %d\n",__FUNCTION__,state);
  switch  (state) {
    // There are just here for reference
    case PA_CONTEXT_UNCONNECTED:
g_print("audio: state_cb: PA_CONTEXT_UNCONNECTED\n");
      break;
    case PA_CONTEXT_CONNECTING:
g_print("audio: state_cb: PA_CONTEXT_CONNECTING\n");
      break;
    case PA_CONTEXT_AUTHORIZING:
g_print("audio: state_cb: PA_CONTEXT_AUTHORIZING\n");
      break;
    case PA_CONTEXT_SETTING_NAME:
g_print("audio: state_cb: PA_CONTEXT_SETTING_NAME\n");
      break;
    case PA_CONTEXT_FAILED:
g_print("audio: state_cb: PA_CONTEXT_FAILED\n");
      g_mutex_unlock(&audio_mutex);
      break;
    case PA_CONTEXT_TERMINATED:
g_print("audio: state_cb: PA_CONTEXT_TERMINATED\n");
      g_mutex_unlock(&audio_mutex);
      break;
    case PA_CONTEXT_READY:
g_print("audio: state_cb: PA_CONTEXT_READY\n");
// get a list of the output devices
      n_input_devices=0;
      n_output_devices=0;
      op = pa_context_get_sink_info_list(pa_ctx,sink_list_cb,NULL);
      break;
    default:
      g_print("audio: state_cb: unknown state %d\n",state);
      break;
  }
}

void audio_get_cards() {
  g_mutex_init(&audio_mutex);
  g_mutex_lock(&audio_mutex);
  main_loop=pa_glib_mainloop_new(NULL);
  main_loop_api=pa_glib_mainloop_get_api(main_loop);
  pa_ctx=pa_context_new(main_loop_api,"linhpsdr");
  pa_context_connect(pa_ctx,NULL,0,NULL);
  pa_context_set_state_callback(pa_ctx, state_cb, NULL);
}

int audio_open_output(RECEIVER *rx) {
  int result=0;
  pa_sample_spec sample_spec;
  int err;

  if(rx->audio_name==NULL) {
    result=-1;
  } else {
    g_mutex_lock(&rx->local_audio_mutex);
    sample_spec.rate=48000;
    sample_spec.channels=2;
    sample_spec.format=PA_SAMPLE_FLOAT32NE;

    char stream_id[16];
    sprintf(stream_id,"RX-%d",rx->id);

    rx->playstream=pa_simple_new(NULL,               // Use the default server.
                    "piHPSDR",           // Our application's name.
                    PA_STREAM_PLAYBACK,
                    rx->audio_name,
                    stream_id,            // Description of our stream.
                    &sample_spec,                // Our sample format.
                    NULL,               // Use default channel map
                    NULL,               // Use default buffering attributes.
                    &err               // error code if returns NULL
                    );

    if(rx->playstream!=NULL) {
      rx->local_audio_buffer_offset=0;
      rx->local_audio_buffer=g_new0(float,2*rx->local_audio_buffer_size);
      g_print("%s: allocated local_audio_buffer %p size %ld bytes\n",__FUNCTION__,rx->local_audio_buffer,2*rx->local_audio_buffer_size*sizeof(float));
    } else {
      result=-1;
      g_print("%s: pa-simple_new failed: err=%d\n",__FUNCTION__,err);
    }
    g_mutex_unlock(&rx->local_audio_mutex);
  }

  return result;
}

static void *mic_read_thread(gpointer arg) {
  int rc;
  int err;

  g_print("%s: running=%d\n",__FUNCTION__,running);
  while(running) {
    g_mutex_lock(&audio_mutex);
    if(local_microphone_buffer==NULL) {
      running=0;
    } else {
      rc=pa_simple_read(microphone_stream,
            local_microphone_buffer,
            local_microphone_buffer_size*sizeof(float),
            &err);
      if(rc<0) {
        running=0;
        g_print("%s: returned %d error=%d (%s)\n",__FUNCTION__,rc,err,pa_strerror(err));
      } else {
	gint newpt;
	for(gint i=0;i<local_microphone_buffer_size;i++) {
          gfloat sample=local_microphone_buffer[i];
          switch(protocol) {
            case ORIGINAL_PROTOCOL:
            case NEW_PROTOCOL:
#ifdef SOAPYSDR
            case SOAPYSDR_PROTOCOL:
#endif
              //
              // put sample into ring buffer
              //
              if (mic_ring_buffer != NULL) {
                // the "existence" of the ring buffer is now guaranteed for 1 msec,
                // see audio_close_input().
                newpt=mic_ring_write_pt +1;
                if (newpt == MICRINGLEN) newpt=0;
                if (newpt != mic_ring_read_pt) {
                  // buffer space available, do the write
                  mic_ring_buffer[mic_ring_write_pt]=sample;
                  // atomic update of mic_ring_write_pt
                  mic_ring_write_pt=newpt;
                }
              }
              break;
            default:
              break;
          }
        }
      }
    }
    g_mutex_unlock(&audio_mutex);
  }
  g_print("%s: exit\n",__FUNCTION__);
}

int audio_open_input() {
  pa_sample_spec sample_spec;
  int result=0;
  int err;

  if(transmitter->microphone_name==NULL) {
    return -1;
  }

  g_mutex_lock(&audio_mutex);


  pa_buffer_attr attr;
  attr.maxlength = (uint32_t) -1;
  attr.tlength = (uint32_t) -1;
  attr.prebuf = (uint32_t) -1;
  attr.minreq = (uint32_t) -1;
  attr.fragsize = 512;


  sample_spec.rate=48000;
  sample_spec.channels=1;
  sample_spec.format=PA_SAMPLE_FLOAT32NE;

  microphone_stream=pa_simple_new(NULL,               // Use the default server.
                  "piHPSDR",           // Our application's name.
                  PA_STREAM_RECORD,
                  transmitter->microphone_name,
                  "TX",            // Description of our stream.
                  &sample_spec,                // Our sample format.
                  NULL,               // Use default channel map
                  //NULL,
                  &attr,               // Use default buffering attributes.
                  NULL               // Ignore error code.
                  );

  if(microphone_stream!=NULL) {
    local_microphone_buffer_offset=0;
    local_microphone_buffer=g_new0(float,local_microphone_buffer_size);

    g_print("%s: allocating ring buffer\n",__FUNCTION__);
    mic_ring_buffer=(float *) g_new(float,MICRINGLEN);
    mic_ring_read_pt = mic_ring_write_pt=0;
    if (mic_ring_buffer == NULL) {
      audio_close_input();
      return -1;
    }

    running=TRUE;
    g_print("%s: PULSEAUDIO mic_read_thread\n",__FUNCTION__);
    mic_read_thread_id=g_thread_new("mic_thread",mic_read_thread,NULL);
    if(!mic_read_thread_id ) {
      g_print("%s: g_thread_new failed on mic_read_thread\n",__FUNCTION__);
      g_free(local_microphone_buffer);
      local_microphone_buffer=NULL;
      running=FALSE;
      result=-1;
    }
  } else {
    result=-1;
  }
  g_mutex_unlock(&audio_mutex);

  return result;
}

void audio_close_output(RECEIVER *rx) {
  g_mutex_lock(&rx->local_audio_mutex);
  if(rx->playstream!=NULL) {
    pa_simple_free(rx->playstream);
    rx->playstream=NULL;
  }
  if(rx->local_audio_buffer!=NULL) {
    g_free(rx->local_audio_buffer);
    rx->local_audio_buffer=NULL;
  }
  g_mutex_unlock(&rx->local_audio_mutex);
}

void audio_close_input() {
  g_mutex_lock(&audio_mutex);
  if(microphone_stream!=NULL) {
    pa_simple_free(microphone_stream);
    microphone_stream=NULL;
    g_free(local_microphone_buffer);
    local_microphone_buffer=NULL;
  }
  g_mutex_unlock(&audio_mutex);
}

//
// Utility function for retrieving mic samples
// from ring buffer
//
float audio_get_next_mic_sample() {
  int newpt;
  float sample;

  if ((mic_ring_buffer == NULL) || (mic_ring_read_pt == mic_ring_write_pt)) {
    // no buffer, or nothing in buffer: insert silence
    g_print("%s: no samples\n",__FUNCTION__);
    sample=0.0;
  } else {
    // the "existence" of the ring buffer is now guaranteed for 1 msec,
    // see audio_close_input(),
    newpt = mic_ring_read_pt+1;
    if (newpt == MICRINGLEN) newpt=0;
    sample=mic_ring_buffer[mic_ring_read_pt];
    // atomic update of read pointer
    mic_ring_read_pt=newpt;
  }
  return sample;
}

int cw_audio_write(RECEIVER *rx,float sample) {
  int result=0;
  int rc;
  int err;

  g_mutex_lock(&rx->local_audio_mutex);
  if(rx->local_audio_buffer==NULL) {
    rx->local_audio_buffer_offset=0;
    rx->local_audio_buffer=g_new0(float,2*rx->local_audio_buffer_size);
  }

  rx->local_audio_buffer[rx->local_audio_buffer_offset*2]=sample;
  rx->local_audio_buffer[(rx->local_audio_buffer_offset*2)+1]=sample;
  rx->local_audio_buffer_offset++;
  if(rx->local_audio_buffer_offset>=rx->local_audio_buffer_size) {
    rc=pa_simple_write(rx->playstream,
                       rx->local_audio_buffer,
                       rx->local_audio_buffer_size*sizeof(float)*2,
                       &err);
    if(rc!=0) {
      fprintf(stderr,"audio_write failed err=%d\n",err);
    }
    rx->local_audio_buffer_offset=0;
  }
  g_mutex_unlock(&rx->local_audio_mutex);

  return result;
}

int audio_write(RECEIVER *rx,float left_sample,float right_sample) {
  int result=0;
  int rc;
  int err;

  g_mutex_lock(&rx->local_audio_mutex);
  if(rx->local_audio_buffer==NULL) {
    rx->local_audio_buffer_offset=0;
    rx->local_audio_buffer=g_new0(float,2*rx->local_audio_buffer_size);
  }

  rx->local_audio_buffer[rx->local_audio_buffer_offset*2]=left_sample;
  rx->local_audio_buffer[(rx->local_audio_buffer_offset*2)+1]=right_sample;
  rx->local_audio_buffer_offset++;
  if(rx->local_audio_buffer_offset>=rx->local_audio_buffer_size) {
    rc=pa_simple_write(rx->playstream,
                       rx->local_audio_buffer,
                       rx->local_audio_buffer_size*sizeof(float)*2,
                       &err);
    if(rc!=0) {
      fprintf(stderr,"audio_write failed err=%d\n",err);
    }
    rx->local_audio_buffer_offset=0;
  }
  g_mutex_unlock(&rx->local_audio_mutex);

  return result;
}

