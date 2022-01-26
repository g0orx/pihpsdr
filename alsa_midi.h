typedef struct _midi_device {
  char *name;
  char *port;
} MIDI_DEVICE;

#define MAX_MIDI_DEVICES 10

extern MIDI_DEVICE midi_devices[MAX_MIDI_DEVICES];
extern int n_midi_devices;

extern void get_midi_devices(void);
extern int register_midi_device(char *myname);
