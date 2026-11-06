/* PB616 FRONTLIGHT TEST v0.1
 *
 * Standalone safe brightness tester for PocketBook 616 / InkView 5.17.
 * Does not use Wi-Fi and does not modify BURBULI DISPLAY.
 *
 * Original brightness is read and retained at startup.
 * RIGHT PAGE: next brightness level (OFF,5,10,20,35,50,75,100).
 * LEFT PAGE: previous brightness level.
 * FIVE QUICK LEFT PAGE presses in 4s: restore original level and exit.
 * Safety: restore brightness and exit automatically after 15 min.
 *
 * Firmware 5.17 has old frontlight API:
 *   GetFrontlightState() returns -1 when off.
 *   SetFrontlightState(-1) turns off.
 * Newer firmware versions use SetFrontlightEnabled; NOT used here.
 */

#include "inkview.h"
#include <stdio.h>
#include <time.h>

#define FW_VERSION "v0.1"
#define AUTO_EXIT_MS 900000

static ifont *font_title = NULL;
static ifont *font_value = NULL;
static ifont *font_label = NULL;

static int original_level = 0;
static int requested_level = 0;
static int restored = 0;
static time_t left_window = 0;
static int left_count = 0;

/* OFF encoded as -1 for firmware v5.17. */
static const int levels[] = {-1, 5, 10, 20, 35, 50, 75, 100};
#define LEVEL_COUNT ((int)(sizeof(levels)/sizeof(levels[0])))

static int clamp_level(int l) {
    if (l < -1) return -1;
    if (l > 100) return 100;
    return l;
}

static const char *level_str(int level, char *buf, size_t cap) {
    if (level < 0) return "OFF";
    snprintf(buf, cap, "%d %%", level);
    return buf;
}

static void draw_page(void) {
    char original_buf[24];
    char requested_buf[24];
    char current_buf[24];
    int current = GetFrontlightState();
    int w = ScreenWidth();
    int h = ScreenHeight();
    int x = 44;
    int y = h / 2 - 170;
    char label[96];

    ClearScreen();
    if (font_title) SetFont(font_title, BLACK);
    DrawString(x, 35, "PB616 FRONTLIGHT TEST");

    if (font_label) SetFont(font_label, BLACK);
    DrawString(x, 125, "Original level:");
    DrawString(x, 240, level_str(original_level, original_buf, sizeof(original_buf)));
    DrawString(x, y+80, "Requested brightness:");

    if (font_value) SetFont(font_value, BLACK);
    DrawString(x, y+120, level_str(requested_level, requested_buf, sizeof(requested_buf)));

    if (font_label) SetFont(font_label, BLACK);
    snprintf(label, sizeof(label), "Device reports: %s",
             level_str(current, current_buf, sizeof(current_buf)));
    DrawString(x, y+270, label);
    DrawString(x, h-230, "RIGHT PAGE = BRIGHTER");
    DrawString(x, h-187, "LEFT PAGE  = DIMMER");
    DrawString(x, h-144, "5 FAST LEFT = RESTORE + EXIT");
    DrawString(x, h-101, "AUTO EXIT = 15 MIN");
    DrawString(w-120, h-48, FW_VERSION);
    FullUpdate();
}

static void restore_original(void) {
    if (!restored) {
        SetFrontlightState(original_level);
        restored = 1;
    }
}

static void exit_test(void) {
    restore_original();
    CloseApp();
}

static void increase(void) {
    int i;
    for (i = 0; i < LEVEL_COUNT; ++i) {
        if (levels[i] > requested_level) {
            requested_level = levels[i];
            SetFrontlightState(requested_level);
            draw_page();
            return;
        }
    }
}

static void decrease(void) {
    int i;
    for (i = LEVEL_COUNT-1; i >= 0; --i) {
        if (levels[i] < requested_level) {
            requested_level = levels[i];
            SetFrontlightState(requested_level);
            draw_page();
            return;
        }
    }
}

static int handler(int type, int p1, int p2) {
    (void)p2;
    switch (type) {
        case EVT_INIT:
            /* IMPORTANT: no frontlight change on application startup. */
            original_level = GetFrontlightState();
            requested_level = clamp_level(original_level);
            font_title = OpenFont("LiberationSans-Bold", 33, 0);
            font_value = OpenFont("LiberationSans-Bold", 110, 0);
            font_label = OpenFont("LiberationSans", 29, 0);
            SetHardTimer("pb616_fl_test_exit", exit_test, AUTO_EXIT_MS);
            return 1;
        case EVT_SHOW:
        case EVT_REPAINT:
            draw_page();
            return 1;
        case EVT_KEYDOWN:
            if (p1 == IV_KEY_NEXT) {
                left_count = 0;
                increase();
            } else if (p1 == IV_KEY_PREV) {
                time_t now = time(NULL);
                if (!left_count || (now - left_window) > 4) {
                    left_window = now;
                    left_count = 1;
                } else {
                    left_count++;
                }
                if (left_count >= 5) {
                    exit_test();
                } else {
                    decrease();
                }
            }
            return 1;
        case EVT_KEYUP:
        case EVT_KEYREPEAT:
            return 1;
        case EVT_EXIT:
            ClearTimerByName("pb616_fl_test_exit");
            restore_original();
            if (font_title) CloseFont(font_title);
            if (font_value) CloseFont(font_value);
            if (font_label) CloseFont(font_label);
            return 1;
        default:
            return 0;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    InkViewMain(handler);
    return 0;
}
