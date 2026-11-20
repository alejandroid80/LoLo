#include "config.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static char g_path[512];

const char *config_path(void) {
    if (g_path[0]) return g_path;
    const char *ud = getenv("USERDATA_PATH");
    if (ud && ud[0])
        snprintf(g_path, sizeof(g_path), "%s/LoLo/config.json", ud);
    else
        snprintf(g_path, sizeof(g_path), "./config.json");
    return g_path;
}

static void ensure_dir(void) {
    const char *ud = getenv("USERDATA_PATH");
    if (!ud || !ud[0]) return;
    char d[512]; snprintf(d, sizeof(d), "%s/LoLo", ud);
    mkdir(d, 0755);
}

void config_init(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->provider = CFG_PROVIDER_OPENROUTER;
    strcpy(cfg->model, "meta-llama/llama-3.3-70b-instruct:free");
    strcpy(cfg->language, "es");
    strcpy(cfg->system_prompt,
           "Eres un asistente amable y conciso. Responde en español.");
}

void config_load(AppConfig *cfg) {
    config_init(cfg);
    ensure_dir();
    FILE *f = fopen(config_path(), "r");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }
    char *buf = malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = 0; fclose(f);
    cJSON *j = cJSON_Parse(buf); free(buf);
    if (!j) return;
    cJSON *p = cJSON_GetObjectItem(j, "provider");
    if (cJSON_IsNumber(p)) cfg->provider = p->valueint;
    cJSON *m = cJSON_GetObjectItem(j, "model");
    if (cJSON_IsString(m)) strncpy(cfg->model, m->valuestring, sizeof(cfg->model)-1);
    cJSON *k = cJSON_GetObjectItem(j, "api_key");
    if (cJSON_IsString(k)) strncpy(cfg->api_key, k->valuestring, sizeof(cfg->api_key)-1);
    cJSON *s = cJSON_GetObjectItem(j, "system_prompt");
    if (cJSON_IsString(s)) strncpy(cfg->system_prompt, s->valuestring, sizeof(cfg->system_prompt)-1);
    cJSON *l = cJSON_GetObjectItem(j, "language");
    if (cJSON_IsString(l)) strncpy(cfg->language, l->valuestring, sizeof(cfg->language)-1);
    cJSON_Delete(j);
}

void config_save(const AppConfig *cfg) {
    ensure_dir();
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "provider", cfg->provider);
    cJSON_AddStringToObject(j, "model", cfg->model);
    cJSON_AddStringToObject(j, "api_key", cfg->api_key);
    cJSON_AddStringToObject(j, "system_prompt", cfg->system_prompt);
    cJSON_AddStringToObject(j, "language", cfg->language);
    char *s = cJSON_Print(j);
    FILE *f = fopen(config_path(), "w");
    if (f) { fputs(s, f); fclose(f); }
    free(s); cJSON_Delete(j);
}