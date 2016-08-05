extern int n_speech_samples;
extern int n_max_modem_samples;
extern short *demod_in;
extern short *speech_out;
extern short *speech_in;
extern short *mod_out;

extern int freedv_sync;
extern float freedv_snr;

extern char freedv_text_data[64];

void init_freedv();
void close_freedv();
int demod_sample_freedv(short sample);
int mod_sample_freedv(short sample);
void reset_freedv_tx_text_index();

