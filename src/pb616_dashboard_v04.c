/* BURBULI DASHBOARD v0.4 FIX1 | PB616W | layout-only experiment
 * All sensor/energy numbers are DEMO. Clock/date are device-local.
 * A larger clock area, full Russian weekday, energy group, temperature+RH.
 * NO WiFi; NO live measurements; NO invented total consumption or SOC.
 */
#include "inkview.h"
#include <time.h>
#include <stdio.h>

static ifont *clock_font, *metric_font, *temp_font, *date_font, *day_font;
static ifont *secondary_font, *load_font, *tiny_font;
static int last_min = -1, refreshes=0, exit_count=0;
static time_t exit_window=0;

/* deliberately fixed test values */
static const char voltage[]="12.86 V";
static const char solar[]="185 W";
static const char load12[]="24 W";
static const char home_temp[]="21.6 C";
static const char outside_temp[]="7.8 C";
static const char home_rh[]="54 %";
static const char outside_rh[]="82 %";

static void thick_line(int x1,int y1,int x2,int y2,int thickness) {
    int i;
    for(i=0;i<thickness;i++) DrawLine(x1,y1+i,x2,y2+i,BLACK);
}
/* InkView 5.17 supports DrawLine, but not DrawEllipse.
 * Approximate outlined circles with 16 straight segments (no libm). */
static void outline_circle(int cx,int cy,int radius) {
    static const int vx[16]={100,92,71,38,0,-38,-71,-92,-100,-92,-71,-38,0,38,71,92};
    static const int vy[16]={0,38,71,92,100,92,71,38,0,-38,-71,-92,-100,-92,-71,-38};
    int i;
    for(i=0;i<16;i++) {
        int next=(i+1)%16;
        DrawLine(cx+radius*vx[i]/100,cy+radius*vy[i]/100,
                 cx+radius*vx[next]/100,cy+radius*vy[next]/100,BLACK);
    }
}
static void icon_battery(int x,int y) {
    int i;
    for(i=0;i<5;i++) DrawRect(x+i,y+i,81-2*i,47-2*i,BLACK);
    FillArea(x+81,y+13,9,22,BLACK);
    /* empty on purpose: no real SOC signal */
}
static void icon_sun(int x,int y) {
    int i;
    for(i=0;i<5;i++) outline_circle(x+44,y+44,26-i);
    thick_line(x+43,y,x+43,y+13,4);
    thick_line(x+43,y+78,x+43,y+91,4);
    thick_line(x-2,y+44,x+12,y+44,4);
    thick_line(x+78,y+44,x+92,y+44,4);
    DrawLine(x+11,y+12,x+23,y+24,BLACK);
    DrawLine(x+64,y+65,x+78,y+79,BLACK);
    DrawLine(x+11,y+79,x+24,y+66,BLACK);
    DrawLine(x+65,y+24,x+78,y+11,BLACK);
}
static void icon_load(int x,int y) {
    /* electric plug, explicitly LOAD 12V, not the inverter total */
    DrawRect(x+18,y+25,48,31,BLACK);
    FillArea(x+30,y+8,7,19,BLACK);
    FillArea(x+49,y+8,7,19,BLACK);
    thick_line(x+42,y+56,x+42,y+70,4);
}
static void icon_house(int x,int y) {
    int i;
    for(i=0;i<6;i++) {
        DrawLine(x+2,y+48+i,x+61,y+i,BLACK);
        DrawLine(x+61,y+i,x+120,y+48+i,BLACK);
    }
    FillArea(x+18,y+53,9,66,BLACK);
    FillArea(x+95,y+53,9,66,BLACK);
    FillArea(x+18,y+114,87,8,BLACK);
    FillArea(x+52,y+85,22,32,BLACK);
}
static void icon_tree(int x,int y) {
    int i;
    for(i=0;i<6;i++) {
        DrawLine(x+62,y+i,x+5+i,y+84+i,BLACK);
        DrawLine(x+62,y+i,x+120-i,y+84+i,BLACK);
    }
    FillArea(x+24,y+84,77,7,BLACK);
    FillArea(x+54,y+89,16,38,BLACK);
}
static void icon_drop(int x,int y) {
    /* boldly outlined droplet, no special character/font dependencies */
    int i;
    for(i=0;i<4;i++) {
        DrawLine(x+14,y+i,x+2+i,y+22,BLACK);
        DrawLine(x+14,y+i,x+27-i,y+22,BLACK);
        outline_circle(x+15,y+27,12-i);
    }
}
static void arrow_trend_up(int x,int y) {
    /* trend is illustrative in this version, not calculated */
    int i;
    for(i=0;i<5;i++) {
        DrawLine(x+i,y+39,x+36+i,y+3,BLACK);
        DrawLine(x+17,y+3+i,x+38,y+3+i,BLACK);
        DrawLine(x+38+i,y+3,x+38+i,y+25,BLACK);
    }
}
static void draw_values(void) {
    int w=ScreenWidth(), h=ScreenHeight();
    int column=(w*58)/100, horizon=(h*59)/100, midway=w/2;
    ClearScreen();
    DrawLine(12,horizon,w-12,horizon,BLACK);
    DrawLine(column,12,column,horizon,BLACK);
    DrawLine(midway,horizon,midway,h-12,BLACK);

    /* Upper right: 3 energy rows. Icons have no small descriptive headings. */
    icon_battery(column+31,35);
    icon_sun(column+31,164);
    icon_load(column+37,335);
    if(metric_font) SetFont(metric_font,BLACK);
    DrawString(column+133,30,voltage);
    DrawString(column+133,160,solar);
    if(load_font) SetFont(load_font,BLACK);
    DrawString(column+141,322,load12);
    arrow_trend_up(w-61,220);
    if(tiny_font) SetFont(tiny_font,BLACK);
    DrawString(column+143,387,"LOAD 12V");

    /* Lower half: each sensor has big temp and smaller humidity */
    icon_house(34,horizon+52);
    icon_tree(midway+24,horizon+46);
    if(temp_font) SetFont(temp_font,BLACK);
    DrawString(169,horizon+47,home_temp);
    DrawString(midway+165,horizon+47,outside_temp);
    icon_drop(186,horizon+160);
    icon_drop(midway+178,horizon+160);
    if(secondary_font) SetFont(secondary_font,BLACK);
    DrawString(233,horizon+151,home_rh);
    DrawString(midway+226,horizon+151,outside_rh);
    if(tiny_font) SetFont(tiny_font,BLACK);
    DrawString(w-91,h-28,"DEMO");
}
static void draw_clock(int full) {
    char hhmm[12]="--:--", date[24]="--.--.----";
    const char *weekday="--------";
    static const char *days[]={"ВОСКРЕСЕНЬЕ","ПОНЕДЕЛЬНИК","ВТОРНИК",
                               "СРЕДА","ЧЕТВЕРГ","ПЯТНИЦА","СУББОТА"};
    time_t now=time(NULL);
    struct tm *t=localtime(&now);
    int w=ScreenWidth(),h=ScreenHeight(), col=(w*58)/100, hz=(h*59)/100;
    if(t) {
       strftime(hhmm,sizeof(hhmm),"%H:%M",t);
       strftime(date,sizeof(date),"%d.%m.%Y",t);
       weekday=days[t->tm_wday]; last_min=t->tm_min;
    }
    FillArea(15,17,col-31,hz-30,WHITE);
    if(clock_font) SetFont(clock_font,BLACK);
    DrawString(28,46,hhmm);
    if(day_font) SetFont(day_font,BLACK);
    DrawString(38,hz-153,weekday);
    if(date_font) SetFont(date_font,BLACK);
    DrawString(38,hz-90,date);
    if(full) FullUpdate();
    else PartialUpdate(15,17,col-31,hz-30);
}
static void tick(void) {
    time_t now=time(NULL); struct tm *t=localtime(&now);
    if(!t || t->tm_min!=last_min) draw_clock((++refreshes%12)==0);
    SetHardTimer("burbuli04_tick",tick,30000);
}
static void timeout_exit(void) { CloseApp(); }
static int handler(int type,int p1,int p2) {
    (void)p2;
    switch(type) {
      case EVT_INIT:
        SetOrientation(1);
        clock_font=OpenFont("LiberationSans-Bold",176,0);
        metric_font=OpenFont("LiberationSans-Bold",58,0);
        temp_font=OpenFont("LiberationSans-Bold",72,0);
        date_font=OpenFont("LiberationSans-Bold",46,0);
        day_font=OpenFont("LiberationSans-Bold",43,0);
        secondary_font=OpenFont("LiberationSans-Bold",43,0);
        load_font=OpenFont("LiberationSans-Bold",46,0);
        tiny_font=OpenFont("LiberationSans-Bold",23,0);
        SetHardTimer("burbuli04_tick",tick,30000);
        SetHardTimer("burbuli04_timeout",timeout_exit,900000);
        return 1;
      case EVT_SHOW:
      case EVT_REPAINT:
      case EVT_ORIENTATION:
        draw_values(); draw_clock(1); return 1;
      case EVT_KEYDOWN:
        if(p1==IV_KEY_PREV) {
          time_t now=time(NULL);
          if(exit_count==0 || now-exit_window>5) { exit_window=now; exit_count=1; }
          else exit_count++;
          if(exit_count>=5) CloseApp();
        }
        return 1;
      case EVT_KEYUP:
      case EVT_KEYREPEAT: return 1;
      case EVT_EXIT:
        ClearTimerByName("burbuli04_tick");
        ClearTimerByName("burbuli04_timeout");
        if(clock_font) CloseFont(clock_font);
        if(metric_font) CloseFont(metric_font);
        if(temp_font) CloseFont(temp_font);
        if(date_font) CloseFont(date_font);
        if(day_font) CloseFont(day_font);
        if(secondary_font) CloseFont(secondary_font);
        if(load_font) CloseFont(load_font);
        if(tiny_font) CloseFont(tiny_font);
        return 1;
      default: return 0;
    }
}
int main(int argc,char **argv) {
    (void)argc; (void)argv; InkViewMain(handler); return 0;
}
