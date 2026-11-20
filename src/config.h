#ifndef CONFIG_H
#define CONFIG_H
#include <stdbool.h>

#define CFG_PROVIDER_POLLINATIONS 0
#define CFG_PROVIDER_OPENROUTER   1
#define CFG_PROVIDER_GEMINI       2

typedef struct {
    int   provider;
    char  model[128];
    char  api_key[256];
    char  system_prompt[512];
    char  language[16];
} AppConfig;

void config_init(AppConfig *cfg);
void config_load(AppConfig *cfg);
void config_save(const AppConfig *cfg);
const char *config_path(void);

#endif