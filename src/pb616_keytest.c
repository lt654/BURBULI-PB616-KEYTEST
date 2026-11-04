/* BURBULI / PB616 KEYTEST v0.3
 * Target PB616W, firmware U616.5.17.2182, InkView 5.17
 * Application is read-only with respect to system settings.
 * Writes a diagnostic log only to /mnt/ext1/PB616_KEYTEST_LOG.txt
 */
#include "inkview.h"
#include <stdio.h>
#include <string.h>

#define LINES 12
#define LOG_PATH "/mnt/ext1/PB616_KEYTEST_LOG.txt"

static ifont *font = NULL;
static FILE *log_file = NULL;
static char recent[LINES][96];
static int seen = 0;
static int used = 0;
static int drawings = 0;

static const char *key_name(int key) {
    switch (key) {
        case IV_KEY_OK: return "OK";
        case IV_KEY_LEFT: return "LEFT";
        case IV_KEY_RIGHT: return "RIGHT";
        case IV_KEY_UP: return "UP";
        case IV_KEY_DOWN: return "DOWN";
        case IV_KEY_PREV: return "PAGE_PREV";
        case IV_KEY_NEXT: return "PAGE_NEXT";
        case IV_KEY_PREV2: return "PREV2";
        case IV_KEY_NEXT2: return "NEXT2";
        case IV_KEY_MENU: return "MENU";
        case IV_KEY_HOME: return "HOME";
        case IV_KEY_BACK: return "BACK";
        case IV_KEY_POWER: return "POWER";
        default: return "?";
    }
}

static const char *event_name(int type) {
    if (type == EVT_KEYDOWN) return "DOWN";
    if (type == EVT_KEYUP) return "UP";
    if (type == EVT_KEYREPEAT) return "REPEAT";
    return "OTHER";
}

static void draw_screen(int full) {
    char line[120];
    int w = ScreenWidth();
    int h = ScreenHeight();
    int i;
    int y = 144;
    ClearScreen();
    if (font) SetFont(font, BLACK);
    DrawString(18, 34, "BURBULI - PB616 KEYTEST v0.3");
    snprintf(line, sizeof(line), "Events: %d", seen);
    DrawString(18, 74, line);
    DrawString(18, 111, "Last events (newest first):");
    for (i = 0; i < used && y < h - 120; ++i) {
        DrawString(18, y, recent[i]);
        y += 40;
    }
    DrawString(18, h - 83, "AUTO EXIT: 4 minutes");
    DrawString(18, h - 49, "LOG: /mnt/ext1/PB616_KEYTEST_LOG.txt");
    if (full) FullUpdate();
    else PartialUpdate(0, 0, w, h);
}

static void redraw(void) {
    drawings++;
    draw_screen(drawings % 18 == 0);
}

static void auto_exit(void) {
    CloseApp();
}

static void add_event(int type, int p1, int p2) {
    char row[96];
    int i;
    seen++;
    snprintf(row, sizeof(row), "%04d %s %s key=%d p2=%d", seen,
             event_name(type), key_name(p1), p1, p2);
    if (used < LINES) used++;
    for (i = used - 1; i > 0; --i) {
        memcpy(recent[i], recent[i - 1], sizeof(recent[i]));
    }
    snprintf(recent[0], sizeof(recent[0]), "%s", row);
    if (log_file) {
        fprintf(log_file, "%s\n", row);
        fflush(log_file); /* If app unexpectedly disappears, keep last event. */
    }
    SetWeakTimer("draw_keylog", redraw, 350);
}

static int handle(int type, int p1, int p2) {
    switch (type) {
        case EVT_INIT:
            font = OpenFont("LiberationSans", 22, 0);
            log_file = fopen(LOG_PATH, "a");
            if (log_file) {
                fputs("\n== BURBULI PB616 KEYTEST v0.3 START ==\n", log_file);
                fflush(log_file);
            }
            SetHardTimer("keytest_timeout", auto_exit, 240000);
            return 1;
        case EVT_SHOW:
        case EVT_REPAINT:
            draw_screen(1);
            return 1;
        case EVT_KEYDOWN:
        case EVT_KEYUP:
        case EVT_KEYREPEAT:
            add_event(type, p1, p2);
            return 1;  /* Consume events; don't navigate book's main menu. */
        case EVT_EXIT:
            ClearTimerByName("draw_keylog");
            ClearTimerByName("keytest_timeout");
            if (log_file) {
                fputs("== EXIT ==\n", log_file);
                fclose(log_file);
                log_file = NULL;
            }
            if (font) { CloseFont(font); font = NULL; }
            return 1;
        default:
            return 0;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    InkViewMain(handle);
    return 0;
}
