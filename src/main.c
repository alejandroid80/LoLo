#define AP_IMPLEMENTATION
#include "apostrophe.h"
#define AP_WIDGETS_IMPLEMENTATION
#include "apostrophe_widgets.h"
#include "config.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>

static AppConfig cfg;
static char last_q[1024] = "";
static char last_a[4096] = "";
static int last_ok = 1;
static int state = 0; /* 0 home 1 answer 2 settings 3 history */
static int chat_scroll = 0;

#define MAXH 50
typedef struct { char q[256]; char a[512]; } HItem;
static HItem hist[MAXH];
static int histn = 0;

static void ensure_udir(void) {
    const char *ud = getenv("USERDATA_PATH");
    if (!ud || !ud[0]) return;
    char d[512]; snprintf(d, sizeof(d), "%s/LoLo", ud);
    mkdir(d, 0755);
}

static void load_history(void) {
    histn = 0;
    const char *ud = getenv("USERDATA_PATH");
    char path[512];
    if (ud && ud[0]) snprintf(path, sizeof(path), "%s/LoLo/history.txt", ud);
    else snprintf(path, sizeof(path), "./history.txt");
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[1024]; HItem cur; memset(&cur, 0, sizeof(cur));
    int have = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "Q: ", 3)) {
            if (have && histn < MAXH) hist[histn++] = cur;
            memset(&cur, 0, sizeof(cur));
            strncpy(cur.q, line + 3, sizeof(cur.q) - 1);
            cur.q[strcspn(cur.q, "\n")] = 0;
            have = 1;
        } else if (!strncmp(line, "A: ", 3)) {
            strncpy(cur.a, line + 3, sizeof(cur.a) - 1);
            cur.a[strcspn(cur.a, "\n")] = 0;
        } else if (!strcmp(line, "---\n")) {
            if (have && histn < MAXH) hist[histn++] = cur;
            memset(&cur, 0, sizeof(cur));
            have = 0;
        }
    }
    if (have && histn < MAXH) hist[histn++] = cur;
    fclose(f);
}

static void save_history_entry(const char *q, const char *a) {
    ensure_udir();
    const char *ud = getenv("USERDATA_PATH");
    char path[512];
    if (ud && ud[0]) snprintf(path, sizeof(path), "%s/LoLo/history.txt", ud);
    else snprintf(path, sizeof(path), "./history.txt");
    FILE *f = fopen(path, "a"); if (!f) return;
    fprintf(f, "Q: %s\nA: %s\n---\n", q, a);
    fclose(f);
}

static void draw_robot(int cx, int cy, int r, bool blink, float talk, float smile, int emotion) {
    SDL_Renderer *ren = ap_get_renderer();
    int stroke = 4;
    /* cara cuadrada (contorno blanco) */
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    for (int s = 0; s < stroke; s++) {
        SDL_Rect face = { cx - r + s, cy - r + s, (r * 2) - s * 2, (r * 2) - s * 2 };
        SDL_RenderDrawRect(ren, &face);
    }
    /* ojos: cuadrados blancos (LED) */
    int eye = r / 5;
    int ex1 = cx - r / 2, ex2 = cx + r / 4;
    int ey = cy - r / 6;
    if (!blink) {
        SDL_Rect e1 = { ex1, ey, eye, eye };
        SDL_Rect e2 = { ex2, ey, eye, eye };
        SDL_RenderFillRect(ren, &e1);
        SDL_RenderFillRect(ren, &e2);
    } else {
        for (int s = 0; s < 2; s++) {
            SDL_Rect e1 = { ex1, ey + eye / 2 - 1 + s, eye, 2 };
            SDL_Rect e2 = { ex2, ey + eye / 2 - 1 + s, eye, 2 };
            SDL_RenderFillRect(ren, &e1);
            SDL_RenderFillRect(ren, &e2);
        }
    }
    /* cejas: solo fruncidas (angulo) cuando esta enfadado; resto normales */
    int browY = ey - 12;
    int browLen = eye + 4;
    for (int e = 0; e < 2; e++) {
        int bx = (e == 0) ? ex1 : ex2;
        for (int i = 0; i < browLen; i++) {
            int x = bx + i;
            int y = browY;
            int mid = browLen / 2;
            int dist = (i > mid) ? (i - mid) : (mid - i);
            if (emotion == 1) { /* feliz: cejas arriba en el centro */
                y = browY - (mid - dist) / 2;
            } else if (emotion == 2) { /* triste: cejas rectas y mas bajas */
                y = browY + 4;
            } else if (emotion == 3) { /* enfadado: cejas inclinadas hacia el centro */
                if (e == 0) y = browY + i / 2; else y = browY + (browLen - i) / 2;
            } else if (emotion == 4) { /* sorprendido: cejas mas arriba */
                y = browY - 10;
            }
            SDL_Rect px = { x, y, 1, 3 };
            SDL_RenderFillRect(ren, &px);
        }
    }
    /* boca: linea que se alarga con talk y se curva con smile */
    int mw = r / 2 + (int)(talk * r / 4);
    int mx = cx - mw / 2;
    int my = cy + r / 3;
    for (int i = 0; i < mw; i++) {
        int x = mx + i;
        int off = (int)(-smile * 0.03f * (float)(i - mw/2) * (float)(i - mw/2));
        for (int s = 0; s < 3; s++) {
            SDL_Rect m = { x, my + off + s, 1, 1 };
            SDL_RenderFillRect(ren, &m);
        }
    }
}

static int draw_bubble(TTF_Font *f, int x, int y, int maxw, const char *text, int isUser) {
    if (!text || !text[0] || maxw <= 0) return 0;
    SDL_Renderer *ren = ap_get_renderer();
    int pad = 8;
    ap_color textColor = isUser ? (ap_color){0, 0, 0, 255} : ap_hex_to_color("#FFFFFF");
    ap_color bubbleColor = isUser ? ap_hex_to_color("#FFFFFF") : ap_hex_to_color("#2A2A2A");
    int h = ap_draw_text_wrapped(f, text, x + pad, y + pad, maxw - 2 * pad,
                                 textColor, isUser ? AP_ALIGN_RIGHT : AP_ALIGN_LEFT);
    SDL_SetRenderDrawColor(ren, bubbleColor.r, bubbleColor.g, bubbleColor.b, 255);
    SDL_Rect r = { x, y, maxw, h + 2 * pad };
    SDL_RenderFillRect(ren, &r);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDrawRect(ren, &r);
    ap_draw_text_wrapped(f, text, x + pad, y + pad, maxw - 2 * pad,
                         textColor, isUser ? AP_ALIGN_RIGHT : AP_ALIGN_LEFT);
    return h + 2 * pad;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    ap_config acfg = {
        .window_title = "LoLo",
        .log_path = ap_resolve_log_path("lolo"),
        .is_nextui = AP_PLATFORM_IS_DEVICE,
        .font_path = "/mnt/SDCARD/.system/res/font1.ttf"
    };
    if (ap_init(&acfg) != AP_OK) return 1;
    config_load(&cfg);
    load_history();

    Uint32 lastBlink = SDL_GetTicks();
    bool blink = false;
    Uint32 blinkEnd = 0;
    float talk = 0.0f;
    bool thinking = false;
    float smile = 0.0f, target_smile = 0.0f;

    bool running = true;
    while (running) {
        Uint32 now = SDL_GetTicks();
        if (!blink && now - lastBlink > 3000) { blink = true; blinkEnd = now + 150; lastBlink = now; }
        if (blink && now > blinkEnd) blink = false;
        if (thinking) talk = (sinf((float)now / 120.0f) + 1.0f) / 2.0f;
        else talk *= 0.85f;

        if (thinking) { target_smile = 0.15f; }
        else if (state == 1) { target_smile = last_ok ? 0.4f : -0.3f; }
        else { target_smile = 0.0f; }
        smile += (target_smile - smile) * 0.1f;

        ap_input_event ev;
        if (state == 0 || state == 1) {
            while (ap_poll_input(&ev)) {
                if (!ev.pressed) continue;
                if (ev.button == AP_BTN_B) {
                    if (state == 0) running = false;
                    else state = 0;
                }
                if (ev.button == AP_BTN_X) state = 2;
                if (ev.button == AP_BTN_UP) { chat_scroll -= 60; if (chat_scroll < 0) chat_scroll = 0; }
                if (ev.button == AP_BTN_DOWN) { chat_scroll += 60; }
                if (ev.button == AP_BTN_A && state == 0 && !thinking) {
                    ap_keyboard_result kr;
                    int rc = ap_keyboard("", "Escribe tu pregunta:  (B: cancelar)", AP_KB_GENERAL, &kr);
                    if (rc == AP_OK && kr.text[0]) {
                        strncpy(last_q, kr.text, sizeof(last_q) - 1);
                        last_q[sizeof(last_q)-1] = 0;
                        chat_scroll = 0;
                        thinking = true;

                        int w = ap_get_screen_width(), h = ap_get_screen_height();
                        int leftW = w / 3;
                        int rightX = leftW + 10;
                        int rightW = w - rightX - 10;
                        int robotR = leftW / 2 - 16;
                        ap_clear_screen();
                        SDL_SetRenderDrawColor(ap_get_renderer(), 0, 0, 0, 255);
                        SDL_RenderClear(ap_get_renderer());
                        draw_robot(leftW / 2, h / 2, robotR, blink, 0.8f, 0.15f, 4);
                        TTF_Font *f = ap_get_font(AP_FONT_MEDIUM);
                        ap_color txt = ap_hex_to_color("#FFFFFF");
                        int bubbleMaxW = rightW * 2 / 3;
                        draw_bubble(f, rightX, 40, bubbleMaxW, "Pensando...", 0);
                        ap_present();

                        char sys[1024]; strncpy(sys, cfg.system_prompt, sizeof(sys)-1); sys[sizeof(sys)-1]=0;
                        int prov = cfg.provider;
                        if (prov == CFG_PROVIDER_OPENROUTER && !cfg.api_key[0]) prov = CFG_PROVIDER_POLLINATIONS;
                        int rc = net_ask(prov, cfg.model, cfg.api_key, sys, last_q, last_a, sizeof(last_a));
                        last_ok = (rc == 0) && last_a[0] && (strncmp(last_a, "Error", 5) != 0);
                        chat_scroll = 100000;

                        thinking = false;
                        if (last_a[0]) {
                            save_history_entry(last_q, last_a);
                            load_history();
                        }
                        state = 1;
                    }
                }
                if (ev.button == AP_BTN_A && state == 1) state = 0;
            }
        }

        int w = ap_get_screen_width(), h = ap_get_screen_height();
        ap_clear_screen();
        SDL_SetRenderDrawColor(ap_get_renderer(), 0, 0, 0, 255);
        SDL_RenderClear(ap_get_renderer());

        if (state == 0) {
            int leftW = w / 3;
            int rightX = leftW + 10;
            int rightW = w - rightX - 10;
            int robotR = leftW / 2 - 16;
            draw_robot(leftW / 2, h / 2, robotR, blink, talk, smile, thinking ? 4 : 0);
            TTF_Font *f = ap_get_font(AP_FONT_SMALL);
            ap_color txt = ap_hex_to_color("#FFFFFF");
            const char *status = thinking ? "Pensando..." : "Listo";
            ap_draw_text(f, status, leftW / 2 - 30, h / 2 + robotR + 5, txt);
            int bubbleMaxW = rightW * 2 / 3;
            int visibleH = h - 100;
            SDL_Rect clip = { rightX, 40, rightW, visibleH };
            SDL_RenderSetClipRect(ap_get_renderer(), &clip);
            int by = 40 - chat_scroll;
            if (last_q[0]) {
                int ux = rightX + rightW - bubbleMaxW;
                by += draw_bubble(f, ux, by, bubbleMaxW, last_q, 1) + 10;
            }
            if (thinking) {
                by += draw_bubble(f, rightX, by, bubbleMaxW, "Pensando...", 0) + 10;
            } else if (last_a[0]) {
                by += draw_bubble(f, rightX, by, bubbleMaxW, last_a, 0) + 10;
            } else {
                ap_draw_text(f, "Pulsa A para preguntar", rightX, by, txt);
                by += 30;
            }
            SDL_RenderSetClipRect(ap_get_renderer(), NULL);
            int contentH = by + chat_scroll - 40;
            int maxScroll = contentH - visibleH;
            if (maxScroll < 0) maxScroll = 0;
            if (chat_scroll > maxScroll) chat_scroll = maxScroll;
            if (chat_scroll < 0) chat_scroll = 0;
            ap_footer_item foot[] = {
                { .button = AP_BTN_B, .label = "SALIR" },
                { .button = AP_BTN_A, .label = "PREGUNTAR" },
                { .button = AP_BTN_X, .label = "AJUSTES" }
            };
            ap_draw_footer(foot, 3);
        } else if (state == 1) {
            int leftW = w / 3;
            int rightX = leftW + 10;
            int rightW = w - rightX - 10;
            int robotR = leftW / 2 - 16;
            draw_robot(leftW / 2, h / 2, robotR, blink, 0.3f, smile, last_ok ? 1 : 2);
            TTF_Font *f = ap_get_font(AP_FONT_SMALL);
            ap_color txt = ap_hex_to_color("#FFFFFF");
            const char *status = last_ok ? "Listo" : "Error";
            ap_draw_text(f, status, leftW / 2 - 20, h / 2 + robotR + 5, txt);
            int bubbleMaxW = rightW * 2 / 3;
            int visibleH = h - 100;
            SDL_Rect clip = { rightX, 40, rightW, visibleH };
            SDL_RenderSetClipRect(ap_get_renderer(), &clip);
            int by = 40 - chat_scroll;
            if (last_q[0]) {
                int ux = rightX + rightW - bubbleMaxW;
                by += draw_bubble(f, ux, by, bubbleMaxW, last_q, 1) + 10;
            }
            if (last_a[0]) {
                by += draw_bubble(f, rightX, by, bubbleMaxW, last_a, 0) + 10;
            }
            SDL_RenderSetClipRect(ap_get_renderer(), NULL);
            int contentH = by + chat_scroll - 40;
            int maxScroll = contentH - visibleH;
            if (maxScroll < 0) maxScroll = 0;
            if (chat_scroll > maxScroll) chat_scroll = maxScroll;
            if (chat_scroll < 0) chat_scroll = 0;
            ap_footer_item foot[] = {
                { .button = AP_BTN_B, .label = "VOLVER" },
                { .button = AP_BTN_A, .label = "NUEVA" }
            };
            ap_draw_footer(foot, 2);
        } else if (state == 2) {
            const char *items[] = { "Proveedor", "Modelo", "API Key", "Prompt sistema", "Historial", "Volver" };
            ap_list_item li[6];
            memset(li, 0, sizeof(li));
            for (int i = 0; i < 6; i++) li[i].label = items[i];
            ap_footer_item foot[] = {
                { .button = AP_BTN_B, .label = "VOLVER" },
                { .button = AP_BTN_A, .label = "SELECCIONAR" }
            };
            ap_list_opts opts = ap_list_default_opts("Ajustes", li, 6);
            opts.footer = foot;
            opts.footer_count = 2;
            ap_list_result lr;
            int rc = ap_list(&opts, &lr);
            if (rc == AP_OK && lr.selected_index >= 0) {
                int idx = lr.selected_index;
                if (idx == 0) {
                    const char *prov[] = { "Pollinations (sin key)", "OpenRouter", "Gemini" };
                    ap_list_item pl[3];
                    memset(pl, 0, sizeof(pl));
                    for (int i = 0; i < 3; i++) pl[i].label = prov[i];
                    ap_list_opts po = ap_list_default_opts("Proveedor", pl, 3);
                    ap_list_result pr;
                    if (ap_list(&po, &pr) == AP_OK && pr.selected_index >= 0) {
                        cfg.provider = pr.selected_index;
                        config_save(&cfg);
                    }
                } else if (idx == 1) {
                    ap_keyboard_result kr;
                    if (ap_keyboard(cfg.model, "Modelo:  (B: cancelar)", AP_KB_GENERAL, &kr) == AP_OK) {
                        strncpy(cfg.model, kr.text, sizeof(cfg.model) - 1);
                        cfg.model[sizeof(cfg.model)-1] = 0;
                        config_save(&cfg);
                    }
                } else if (idx == 2) {
                    ap_keyboard_result kr;
                    if (ap_keyboard(cfg.api_key, "API Key:  (B: cancelar)", AP_KB_GENERAL, &kr) == AP_OK) {
                        strncpy(cfg.api_key, kr.text, sizeof(cfg.api_key) - 1);
                        cfg.api_key[sizeof(cfg.api_key)-1] = 0;
                        config_save(&cfg);
                    }
                } else if (idx == 3) {
                    ap_keyboard_result kr;
                    if (ap_keyboard(cfg.system_prompt, "Prompt sistema:  (B: cancelar)", AP_KB_GENERAL, &kr) == AP_OK) {
                        strncpy(cfg.system_prompt, kr.text, sizeof(cfg.system_prompt) - 1);
                        cfg.system_prompt[sizeof(cfg.system_prompt)-1] = 0;
                        config_save(&cfg);
                    }
                } else if (idx == 4) {
                    state = 3;
                } else {
                    state = 0;
                }
            } else {
                state = 0;
            }
        } else if (state == 3) {
            if (histn == 0) {
                ap_message_opts m = {
                    .message = "Sin historial",
                    .footer = (ap_footer_item[]) { { .button = AP_BTN_A, .label = "OK", .is_confirm = true } },
                    .footer_count = 1
                };
                ap_confirm_result cr; ap_confirmation(&m, &cr);
                state = 2;
            } else {
                ap_list_item *li = calloc(histn, sizeof(ap_list_item));
                for (int i = 0; i < histn; i++) li[i].label = hist[i].q;
                ap_footer_item foot[] = { { .button = AP_BTN_B, .label = "VOLVER" } };
                ap_list_opts opts = ap_list_default_opts("Historial", li, histn);
                opts.footer = foot;
                opts.footer_count = 1;
                ap_list_result lr;
                int rc = ap_list(&opts, &lr);
                if (rc == AP_OK && lr.selected_index >= 0) {
                    char msg[1024];
                    snprintf(msg, sizeof(msg), "P: %s\n\nR: %s",
                             hist[lr.selected_index].q, hist[lr.selected_index].a);
                    ap_footer_item okf[] = { { .button = AP_BTN_A, .label = "OK", .is_confirm = true } };
                    ap_message_opts m = { .message = msg, .footer = okf, .footer_count = 1 };
                    ap_confirm_result cr; ap_confirmation(&m, &cr);
                }
                free(li);
                state = 2;
            }
        }

        ap_present();
        SDL_Delay(16);
    }

    ap_quit();
    return 0;
}