/* BURBULI DASHBOARD v0.2 | PB616W | InkView 5.17
 * LANDSCAPE VISUAL TEST ONLY: local time and static example values.
 * No Wi-Fi, no battery/SOC calculations, no settings changes.
 * Preserve v0.1 exit and auto-exit behavior.
 */
#include "inkview.h"
#include <time.h>
#include <stdio.h>

static ifont *f_clock = NULL;
static ifont *f_number = NULL;
static ifont *f_voltage = NULL;
static ifont *f_label = NULL;
static ifont *f_date = NULL;
static ifont *f_small = NULL;
static int last_minute = -1;
static int repaint_count = 0;
static time_t prev_window = 0;
static int prev_count = 0;
static int screen_drawn = 0;

/* TEST VALUES! Do not interpret as live BURBULI telemetry. */
static const char *v_bat = "12.86 V";
static const char *v_inside = "21.6 C";
static const char *v_outside = "7.8 C";

static void draw_battery_symbol(int x, int y) {
    /* Deliberately EMPTY: state-of-charge is not measured in this demo. */
    DrawRect(x, y, 100, 50, BLACK);
    DrawRect(x + 100, y + 15, 9, 20, BLACK);
    if (f_small) SetFont(f_small, BLACK);
    DrawString(x + 12, y + 11, "AKB");
}

static void draw_layout(void) {
    int w = ScreenWidth(), h = ScreenHeight();
    int midx = w / 2, midy = h / 2;
    ClearScreen();
    DrawLine(midx, 12, midx, h - 12, BLACK);
    DrawLine(12, midy, w - 12, midy, BLACK);
    if (f_label) SetFont(f_label, BLACK);
    DrawString(27, 26, "CLOCK");
    DrawString(midx + 26, 26, "HOME BATTERY");
    DrawString(27, midy + 26, "INSIDE");
    DrawString(midx + 26, midy + 26, "OUTSIDE");
    draw_battery_symbol(w - 143, 82);
    if (f_voltage) SetFont(f_voltage, BLACK);
    DrawString(midx + 29, 192, v_bat);
    if (f_small) SetFont(f_small, BLACK);
    DrawString(midx + 29, midy - 45, "DEMO VALUE / NOT LIVE");
    if (f_number) SetFont(f_number, BLACK);
    DrawString(28, midy + 125, v_inside);
    DrawString(midx + 28, midy + 125, v_outside);
    if (f_small) SetFont(f_small, BLACK);
    DrawString(28, h - 53, "ARANET / TEST");
    DrawString(midx + 28, h - 53, "ARANET / TEST");
    DrawString(28, h - 25, "PAGE_PREV x5 EXIT / AUTO EXIT 15 MIN");
}

static void draw_clock(int full) {
    char clock_text[12] = "--:--";
    char date_text[24] = "--.--.----";
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    int w = ScreenWidth(), h = ScreenHeight();
    if (local) {
        strftime(clock_text, sizeof(clock_text), "%H:%M", local);
        strftime(date_text, sizeof(date_text), "%d.%m.%Y", local);
        last_minute = local->tm_min;
    }
    /* Only change clock area. Large bold numerals sit higher in upper-left quadrant. */
    FillArea(18, 95, w/2 - 35, h/2 - 112, WHITE);
    if (f_clock) SetFont(f_clock, BLACK);
    DrawString(25, 114, clock_text);
    if (f_date) SetFont(f_date, BLACK);
    DrawString(38, h/2 - 65, date_text);
    if (full) FullUpdate();
    else PartialUpdate(15, 90, w/2 - 25, h/2 - 92);
}

static void tick(void) {
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (!local || local->tm_min != last_minute) {
        ++repaint_count;
        draw_clock((repaint_count % 12) == 0);
    }
    SetHardTimer("burbuli_dash_v02_tick", tick, 30000);
}

static void auto_exit(void) { CloseApp(); }

static int handle(int type, int p1, int p2) {
    (void)p2;
    switch (type) {
        case EVT_INIT:
            SetOrientation(1);
            f_clock   = OpenFont("LiberationSans-Bold", 146, 0);
            f_number  = OpenFont("LiberationSans-Bold", 108, 0);
            f_voltage = OpenFont("LiberationSans-Bold", 79, 0);
            f_label   = OpenFont("LiberationSans-Bold", 34, 0);
            f_date    = OpenFont("LiberationSans", 35, 0);
            f_small   = OpenFont("LiberationSans", 21, 0);
            SetHardTimer("burbuli_dash_v02_tick", tick, 30000);
            SetHardTimer("burbuli_dash_v02_timeout", auto_exit, 900000);
            return 1;
        case EVT_SHOW:
        case EVT_REPAINT:
        case EVT_ORIENTATION:
            draw_layout();
            draw_clock(1);
            screen_drawn = 1;
            return 1;
        case EVT_KEYDOWN:
            if (p1 == IV_KEY_PREV) {
                time_t now = time(NULL);
                if (!prev_count || now - prev_window > 5) {
                    prev_window = now;
                    prev_count = 1;
                } else ++prev_count;
                if (prev_count >= 5) CloseApp();
            }
            return 1;
        case EVT_KEYUP:
        case EVT_KEYREPEAT:
            return 1;
        case EVT_EXIT:
            ClearTimerByName("burbuli_dash_v02_tick");
            ClearTimerByName("burbuli_dash_v02_timeout");
            if (f_clock) CloseFont(f_clock);
            if (f_number) CloseFont(f_number);
            if (f_voltage) CloseFont(f_voltage);
            if (f_label) CloseFont(f_label);
            if (f_date) CloseFont(f_date);
            if (f_small) CloseFont(f_small);
            return 1;
        default: return 0;
    }
}
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    InkViewMain(handle);
    return 0;
}
