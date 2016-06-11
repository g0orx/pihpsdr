#define BUFFER_SIZE 1024

void lime_protocol_init(int rx,int pixels);
void lime_protocol_stop();
void lime_protocol_set_frequency(long long f);
void lime_protocol_set_antenna(int ant);
void lime_protocol_set_attenuation(int attenuation);
