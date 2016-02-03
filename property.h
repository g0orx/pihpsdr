typedef struct _PROPERTY PROPERTY;

/* --------------------------------------------------------------------------*/
/**
* @brief Property structure
*/
struct _PROPERTY {
    char* name;
    char* value;
    PROPERTY* next_property;
};

void loadProperties(char* filename);
char* getProperty(char* name);
void setProperty(char* name,char* value);

