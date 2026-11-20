#ifndef NET_H
#define NET_H
int net_ask(int provider, const char *model, const char *api_key,
            const char *system_prompt, const char *question,
            char *out, size_t outsz);
#endif