#define SMETER 0
#define POWER 1


GtkWidget* meter_init(int width,int height);
void meter_update(int meter_type,double value,double reverse);
