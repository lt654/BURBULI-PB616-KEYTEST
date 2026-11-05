/* BURBULI DASHBOARD v0.1 / PB616W / InkView 5.17
 * Landscape layout test.
 * Left: big clock. Right: battery state/voltage + two temperatures.
 * Data values are TEST placeholders for layout verification only.
 * No Wi-Fi and no network requests in this version.
 */
#include "inkview.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static ifont *font_title = NULL;
static ifont *font_clock = NULL;
static ifont *font_label = NULL;
static ifont *font_value = NULL;
static ifont *font_small = NULL;
static int last_minute = -1;
static int repaint_count = 0;
static time_t prev_window = 0;
static int prev_count = 0;

/* TEST VALUES FOR LAYOUT ONLY */
static const char *BAT_STATE = "GOOD";
static const char *BAT_VOLT  = "12.86 V";
static const char *TEMP_IN   = "21.6 C";
static const char *TEMP_OUT  = "7.8 C";

static void draw_battery_icon(int x, int y, int w, int h, int level_pct) {
    int termw = w / 10;
    if (termw < 4) termw = 4;
    DrawRect(x, y, w, h, BLACK);
    DrawRect(x + w, y + h/3, termw, h/3, BLACK);
    {
        int inner = (w - 8) * level_pct / 100;
        if (inner < 0) inner = 0;
        if (inner > w - 8) inner = w - 8;
        FillArea(x + 4, y + 4, inner, h - 8, BLACK);
    }
}

static void draw_static_layout(void) {
    int w = ScreenWidth();
    int h = ScreenHeight();
    int left_w = (w * 54) / 100;
    int right_x = left_w + 12;

    ClearScreen();

    /* Frame and separators */
    DrawRect(8, 8, w - 16, h - 16, BLACK);
    DrawLine(left_w, 20, left_w, h - 20, BLACK);
    DrawLine(right_x, h/3, w - 20, h/3, BLACK);
    DrawLine(right_x, (2*h)/3, w - 20, (2*h)/3, BLACK);

    if (font_title) SetFont(font_title, BLACK);
    DrawString(24, 20, "BURBULI HOME");

    if (font_label) SetFont(font_label, BLACK);
    DrawString(right_x + 8, 34, "BATTERY");
    DrawString(right_x + 8, h/3 + 18, "INSIDE");
    DrawString(right_x + 8, (2*h)/3 + 18, "OUTSIDE");

    draw_battery_icon(w - 150, 28, 96, 34, 72);

    if (font_small) SetFont(font_small, BLACK);
    DrawString(24, h - 36, "PAGE_PREV x5: exit   |   Layout test only   |   No Wi-Fi");
}

static void draw_dynamic_data(int full) {
    char hhmm[16] = "--:--";
    char date[32] = "--.--.----";
    int w = ScreenWidth();
    int h = ScreenHeight();
    int left_w = (w * 54) / 100;
    int right_x = left_w + 20;
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);

    if (lt) {
        strftime(hhmm, sizeof(hhmm), "%H:%M", lt);
        strftime(date, sizeof(date), "%d.%m.%Y", lt);
        last_minute = lt->tm_min;
    }

    /* repaint left clock area */
    FillArea(20, 54, left_w - 30, h - 110, WHITE);
    if (font_clock) SetFont(font_clock, BLACK);
    DrawString(34, h/2 - 86, hhmm);
    if (font_value) SetFont(font_value, BLACK);
    DrawString(62, h/2 + 46, date);

    /* repaint right info blocks */
    FillArea(right_x, 68, w - right_x - 26, h/3 - 58, WHITE);
    FillArea(right_x, h/3 + 42, w - right_x - 26, h/3 - 52, WHITE);
    FillArea(right_x, (2*h)/3 + 42, w - right_x - 26, h/3 - 66, WHITE);

    if (font_value) SetFont(font_value, BLACK);
    DrawString(right_x + 6, 82, BAT_STATE);
    DrawString(right_x + 6, 126, BAT_VOLT);
    DrawString(right_x + 6, h/3 + 62, TEMP_IN);
    DrawString(right_x + 6, (2*h)/3 + 62, TEMP_OUT);

    if (font_small) SetFont(font_small, BLACK);
    DrawString(right_x + 6, 170, "status + voltage");
    DrawString(right_x + 6, h/3 + 110, "house sensor");
    DrawString(right_x + 6, (2*h)/3 + 110, "outdoor sensor");

    if (full) FullUpdate();
    else PartialUpdate(0, 0, w, h);
}

static void tick(void) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (!lt || lt->tm_min != last_minute) {
        repaint_count++;
        draw_dynamic_data((repaint_count % 10) == 0);
    }
    SetHardTimer("burbuli_dash_tick", tick, 30000);
}

static void auto_exit(void) { CloseApp(); }

static int handle(int type, int p1, int p2) {
    (void)p2;
    switch (type) {
        case EVT_INIT:
            SetOrientation(1); /* landscape 90 */
            font_title = OpenFont("LiberationSans-Bold", 24, 0);
            font_clock = OpenFont("LiberationSans-Bold", 120, 0);
            font_label = OpenFont("LiberationSans-Bold", 22, 0);
            font_value = OpenFont("LiberationSans", 34, 0);
            font_small = OpenFont("LiberationSans", 18, 0);
            SetHardTimer("burbuli_dash_tick", tick, 30000);
            SetHardTimer("burbuli_dash_timeout", auto_exit, 900000);
            return 1;
        case EVT_SHOW:
        case EVT_REPAINT:
        case EVT_ORIENTATION:
            draw_static_layout();
            draw_dynamic_data(1);
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
            ClearTimerByName("burbuli_dash_tick");
            ClearTimerByName("burbuli_dash_timeout");
            if (font_title) { CloseFont(font_title); font_title = NULL; }
            if (font_clock) { CloseFont(font_clock); font_clock = NULL; }
            if (font_label) { CloseFont(font_label); font_label = NULL; }
            if (font_value) { CloseFont(font_value); font_value = NULL; }
            if (font_small) { CloseFont(font_small); font_small = NULL; }
            return 1;
        default:
            return 0;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    InkViewMain(handle);
    return 0;
}
