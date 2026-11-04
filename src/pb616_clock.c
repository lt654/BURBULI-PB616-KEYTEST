/* BURBULI CLOCK v0.1 / PB616W / InkView 5.17
 * Local date/time only. No Wi-Fi, no user data changes.
 * Experimental: exit automatically after 15 minutes.
 * Five PAGE_PREV key presses within 5 seconds also exit.
 */
#include "inkview.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static ifont *bigfont = NULL;
static ifont *smallfont = NULL;
static int repaint_count = 0;
static int last_minute = -1;
static time_t prev_window = 0;
static int prev_count = 0;

static void draw_clock(int full) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    char hhmm[24] = "--:--";
    char date[64] = "--.--.----";
    char bottom[96] = "PAGE_PREV x5: exit   |   Auto exit: 15 min";
    int w = ScreenWidth();
    int h = ScreenHeight();
    if (lt) {
        strftime(hhmm, sizeof(hhmm), "%H:%M", lt);
        strftime(date, sizeof(date), "%d.%m.%Y", lt);
        last_minute = lt->tm_min;
    }
    ClearScreen();
    if (smallfont) SetFont(smallfont, BLACK);
    DrawString(24, 35, "BURBULI CLOCK v0.1");
    if (bigfont) SetFont(bigfont, BLACK);
    DrawString(38, h / 2 - 100, hhmm);
    if (smallfont) SetFont(smallfont, BLACK);
    DrawString(36, h / 2 + 100, date);
    DrawString(24, h - 95, "Local time / Wi-Fi is not used");
    DrawString(24, h - 57, bottom);
    if (full) FullUpdate();
    else PartialUpdate(0, 0, w, h);
}

static void clock_tick(void) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (!lt || lt->tm_min != last_minute) {
        repaint_count++;
        draw_clock(repaint_count % 12 == 0);
    }
    SetHardTimer("burbuli_clock_tick", clock_tick, 30000);
}

static void auto_exit(void) { CloseApp(); }

static int handle(int type, int p1, int p2) {
    (void)p2;
    switch (type) {
        case EVT_INIT:
            bigfont = OpenFont("LiberationSans", 120, 0);
            smallfont = OpenFont("LiberationSans", 25, 0);
            SetHardTimer("burbuli_clock_tick", clock_tick, 30000);
            SetHardTimer("burbuli_clock_timeout", auto_exit, 900000);
            return 1;
        case EVT_SHOW:
        case EVT_REPAINT:
            draw_clock(1);
            return 1;
        case EVT_KEYDOWN:
            if (p1 == IV_KEY_PREV) {
                time_t now = time(NULL);
                if (prev_count == 0 || now - prev_window > 5) {
                    prev_window = now;
                    prev_count = 1;
                } else {
                    prev_count++;
                }
                if (prev_count >= 5) CloseApp();
            }
            return 1;
        case EVT_KEYUP:
        case EVT_KEYREPEAT:
            return 1;
        case EVT_EXIT:
            ClearTimerByName("burbuli_clock_tick");
            ClearTimerByName("burbuli_clock_timeout");
            if (bigfont) { CloseFont(bigfont); bigfont = NULL; }
            if (smallfont) { CloseFont(smallfont); smallfont = NULL; }
            return 1;
        default: return 0;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    InkViewMain(handle);
    return 0;
}
