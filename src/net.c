#include "net.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void json_escape(const char *src, char *dst, size_t dstsz) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 6 < dstsz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"') { dst[j++]='\\'; dst[j++]='"'; }
        else if (c == '\\') { dst[j++]='\\'; dst[j++]='\\'; }
        else if (c == '\n') { dst[j++]='\\'; dst[j++]='n'; }
        else if (c == '\r') { dst[j++]='\\'; dst[j++]='r'; }
        else if (c == '\t') { dst[j++]='\\'; dst[j++]='t'; }
        else if (c < 0x20) { /* skip control */ }
        else dst[j++] = (char)c;
    }
    dst[j] = 0;
}

static void urlencode(const char *src, char *dst, size_t dstsz) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 4 < dstsz; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c=='-'||c=='_'||c=='.'||c=='~') {
            dst[j++] = (char)c;
        } else if (c == ' ') {
            dst[j++] = '%'; dst[j++] = '2'; dst[j++] = '0';
        } else {
            dst[j++] = '%'; dst[j++] = hex[c>>4]; dst[j++] = hex[c&15];
        }
    }
    dst[j] = 0;
}

static int read_file(const char *path, char *out, size_t outsz) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    size_t n = fread(out, 1, outsz-1, f); out[n] = 0; fclose(f);
    return 0;
}

int net_ask(int provider, const char *model, const char *api_key,
            const char *system_prompt, const char *question,
            char *out, size_t outsz) {
    out[0] = 0;
    char cmd[8192];

    if (provider == 1 /* OpenRouter */) {
        char esc_sys[4096], esc_q[4096];
        json_escape(system_prompt ? system_prompt : "", esc_sys, sizeof(esc_sys));
        json_escape(question ? question : "", esc_q, sizeof(esc_q));
        FILE *f = fopen("/tmp/ai_req.json", "w");
        if (f) {
            fprintf(f, "{\"model\":\"%s\",\"messages\":["
                       "{\"role\":\"system\",\"content\":\"%s\"},"
                       "{\"role\":\"user\",\"content\":\"%s\"}]}",
                    (model && model[0]) ? model : "meta-llama/llama-3.3-70b-instruct:free",
                    esc_sys, esc_q);
            fclose(f);
        }
        snprintf(cmd, sizeof(cmd),
            "curl -s -m 45 -o /tmp/ai_resp.txt "
            "-H \"Authorization: Bearer %s\" -H \"Content-Type: application/json\" "
            "-d @/tmp/ai_req.json https://openrouter.ai/api/v1/chat/completions",
            (api_key && api_key[0]) ? api_key : "");
        system(cmd);
        char buf[65536]; if (read_file("/tmp/ai_resp.txt", buf, sizeof(buf)) != 0) return -1;
        cJSON *j = cJSON_Parse(buf);
        if (j) {
            cJSON *choices = cJSON_GetObjectItem(j, "choices");
            if (cJSON_IsArray(choices)) {
                cJSON *c0 = cJSON_GetArrayItem(choices, 0);
                cJSON *msg = c0 ? cJSON_GetObjectItem(c0, "message") : NULL;
                cJSON *content = msg ? cJSON_GetObjectItem(msg, "content") : NULL;
                if (cJSON_IsString(content)) {
                    strncpy(out, content->valuestring, outsz-1); out[outsz-1]=0;
                    cJSON_Delete(j); return 0;
                }
            }
            cJSON *err = cJSON_GetObjectItem(j, "error");
            if (err) {
                cJSON *m = cJSON_GetObjectItem(err, "message");
                if (cJSON_IsString(m)) snprintf(out, outsz, "Error: %s", m->valuestring);
            }
            cJSON_Delete(j);
        } else {
            strncpy(out, buf, outsz-1); out[outsz-1]=0;
        }
        return (out[0] ? 0 : -1);
    }

    if (provider == 2 /* Gemini */) {
        char esc_sys[4096], esc_q[4096];
        json_escape(system_prompt ? system_prompt : "", esc_sys, sizeof(esc_sys));
        json_escape(question ? question : "", esc_q, sizeof(esc_q));
        FILE *f = fopen("/tmp/ai_req.json", "w");
        if (f) {
            fprintf(f, "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}],"
                       "\"systemInstruction\":{\"parts\":[{\"text\":\"%s\"}]}}",
                    esc_q, esc_sys);
            fclose(f);
        }
        snprintf(cmd, sizeof(cmd),
            "curl -s -m 45 -o /tmp/ai_resp.txt "
            "-H \"Content-Type: application/json\" "
            "-d @/tmp/ai_req.json "
            "\"https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s\"",
            (model && model[0]) ? model : "gemini-2.0-flash",
            (api_key && api_key[0]) ? api_key : "");
        system(cmd);
        char buf[65536]; if (read_file("/tmp/ai_resp.txt", buf, sizeof(buf)) != 0) return -1;
        cJSON *j = cJSON_Parse(buf);
        if (j) {
            cJSON *cands = cJSON_GetObjectItem(j, "candidates");
            if (cJSON_IsArray(cands)) {
                cJSON *c0 = cJSON_GetArrayItem(cands, 0);
                cJSON *content = c0 ? cJSON_GetObjectItem(c0, "content") : NULL;
                cJSON *parts = content ? cJSON_GetObjectItem(content, "parts") : NULL;
                if (cJSON_IsArray(parts)) {
                    cJSON *p0 = cJSON_GetArrayItem(parts, 0);
                    cJSON *text = p0 ? cJSON_GetObjectItem(p0, "text") : NULL;
                    if (cJSON_IsString(text)) {
                        strncpy(out, text->valuestring, outsz-1); out[outsz-1]=0;
                        cJSON_Delete(j); return 0;
                    }
                }
            }
            cJSON_Delete(j);
        }
        if (!out[0]) strncpy(out, buf, outsz-1);
        return (out[0] ? 0 : -1);
    }

    /* Pollinations (default / fallback) */
    char encq[8192], encs[4096];
    urlencode(question ? question : "", encq, sizeof(encq));
    urlencode(system_prompt ? system_prompt : "", encs, sizeof(encs));
    snprintf(cmd, sizeof(cmd),
        "curl -s -m 45 -o /tmp/ai_resp.txt "
        "\"https://text.pollinations.ai/%s?system=%s&seed=%ld&temperature=0.7\"",
        encq, encs, (long)time(NULL));
    system(cmd);
    if (read_file("/tmp/ai_resp.txt", out, outsz) != 0) return -1;
    char *s = out; while (*s == '\n' || *s == '\r' || *s == ' ') s++;
    if (s != out) memmove(out, s, strlen(s)+1);
    size_t L = strlen(out); while (L > 0 && (out[L-1]=='\n'||out[L-1]=='\r'||out[L-1]==' ')) out[--L]=0;
    return (out[0] ? 0 : -1);
}