/*
 * BURBULI MONITOR v0.2 LARGE TEXT - PocketBook PB616W, InkView 5.17
 *
 * System and process memory snapshot using Linux /proc.
 * Read-only: no network, no writes, no process termination, no system changes.
 * UP: refresh, DOWN: exit. No central OK or side keys required.
 *
 * Special .app filename prefix BURBULI_DISPLAY_ is required by our
 * currently installed BURBULI UPDATER v0.2 filename validation.
 *
 * IMPORTANT: Starting a separate application may suspend/close another
 * foreground application; this monitor cannot prove concurrent BURBULI
 * instances existed before its own launch. Only processes alive at the
 * time of the sample are shown.
 */

#include "inkview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>

#define MON_VERSION "v0.2 LARGE TEXT"
#define MAX_BURBULI 64
#define SCREEN_ROWS 5
#define RAM_UNKNOWN 0UL

typedef struct {
    unsigned long total, free_kb, available, buffers, cached;
    unsigned long swap_total, swap_free;
    int has_available;
    int meminfo_ok;
    unsigned long uptime_seconds;
    unsigned int proc_count;
} mem_stat;

typedef struct {
    int pid;
    unsigned long rss_kb, virt_kb;
    char name[56];
} proc_stat;

typedef struct {
    mem_stat mem;
    proc_stat procs[MAX_BURBULI];
    int shown, total_matches, truncated;
    unsigned int serial;
} snapshot_t;

static snapshot_t snap;
static ifont *font_heading = NULL;
static ifont *font_normal = NULL;
static ifont *font_small = NULL;
static int started = 0;
static int up_down = 0;

static int is_digits(const char *s) {
    const unsigned char *p=(const unsigned char*)s;
    if (!p || !*p) return 0;
    while(*p) { if(!isdigit(*p))return 0; p++; }
    return 1;
}

static void meminfo_read(mem_stat *m) {
    char line[192], field[80];
    unsigned long value;
    FILE *f=fopen("/proc/meminfo","r");
    if(!f) return;
    m->meminfo_ok = 1;
    while(fgets(line,sizeof line,f)) {
        if(sscanf(line,"%79[^:]: %lu kB",field,&value)!=2)continue;
        if(!strcmp(field,"MemTotal"))m->total=value;
        else if(!strcmp(field,"MemFree"))m->free_kb=value;
        else if(!strcmp(field,"MemAvailable")) {m->available=value;m->has_available=1;}
        else if(!strcmp(field,"Buffers"))m->buffers=value;
        else if(!strcmp(field,"Cached"))m->cached=value;
        else if(!strcmp(field,"SwapTotal"))m->swap_total=value;
        else if(!strcmp(field,"SwapFree"))m->swap_free=value;
    }
    fclose(f);
}

static unsigned long uptime_read(void) {
    double sec=0.0;
    FILE *f=fopen("/proc/uptime","r");
    if(!f)return 0;
    if(fscanf(f,"%lf",&sec)!=1 || sec<0.0)sec=0.0;
    fclose(f);
    return (unsigned long)sec;
}

/* First cmdline token normally contains the executable path. Inspect all
 * tokens because some launchers put the app path in a later argument. */
static int read_cmdline_name(int pid,char *out,size_t cap) {
    char path[64];
    char data[768];
    size_t pos=0, got;
    FILE *f;
    snprintf(path,sizeof path,"/proc/%d/cmdline",pid);
    f=fopen(path,"rb");
    if(!f)return 0;
    got=fread(data,1,sizeof(data)-1,f);
    fclose(f);
    if(!got)return 0;
    data[got]=0;
    while(pos<got) {
        const char *arg=data+pos;
        size_t rem=got-pos;
        size_t n=strnlen(arg,rem);
        const char *basename;
        if(n==rem)break;
        basename=strrchr(arg,'/');
        basename=basename?basename+1:arg;
        if(strstr(basename,"BURBULI_") || strstr(basename,"burbuli_")) {
            snprintf(out,cap,"%s",basename);
            return 1;
        }
        pos+=n+1;
    }
    return 0;
}

static int read_exe_name(int pid,char *out,size_t cap) {
    char path[64],linkbuf[256];
    ssize_t n;
    const char *basename;
    snprintf(path,sizeof path,"/proc/%d/exe",pid);
    n=readlink(path,linkbuf,sizeof(linkbuf)-1);
    if(n<=0)return 0;
    linkbuf[n]='\0';
    basename=strrchr(linkbuf,'/');
    basename=basename?basename+1:linkbuf;
    if(!strstr(basename,"BURBULI_") && !strstr(basename,"burbuli_"))return 0;
    snprintf(out,cap,"%s",basename);
    return 1;
}

static int read_comm_name(int pid,char *out,size_t cap) {
    char path[64],line[96];
    FILE *f;
    snprintf(path,sizeof path,"/proc/%d/comm",pid);
    f=fopen(path,"r");
    if(!f)return 0;
    if(!fgets(line,sizeof line,f)){fclose(f);return 0;}
    fclose(f);
    line[strcspn(line,"\r\n")]=0;
    /* Linux comm is generally truncated at 15 chars, so we cannot rely on
       it for full version names; it is enough to detect BURBULI_. */
    if(!strstr(line,"BURBULI_") && !strstr(line,"burbuli_"))return 0;
    snprintf(out,cap,"%s",line);
    return 1;
}

static void read_process_memory(int pid,proc_stat *p) {
    char path[64],line[200];
    unsigned long kb;
    FILE *f;
    snprintf(path,sizeof path,"/proc/%d/status",pid);
    f=fopen(path,"r");
    if(!f)return;
    while(fgets(line,sizeof line,f)) {
        if(sscanf(line,"VmRSS: %lu kB",&kb)==1) p->rss_kb=kb;
        else if(sscanf(line,"VmSize: %lu kB",&kb)==1)p->virt_kb=kb;
    }
    fclose(f);
}

static int proc_desc_rss(const void *a,const void *b) {
    const proc_stat *x=(const proc_stat*)a,*y=(const proc_stat*)b;
    if(x->rss_kb<y->rss_kb)return 1;
    if(x->rss_kb>y->rss_kb)return -1;
    return x->pid-y->pid;
}

static void collect_snapshot(void) {
    DIR *d;
    struct dirent *de;
    snapshot_t next;
    unsigned int serial=snap.serial+1;
    memset(&next,0,sizeof next);
    next.serial=serial;
    meminfo_read(&next.mem);
    next.mem.uptime_seconds=uptime_read();
    d=opendir("/proc");
    if(d) {
        while((de=readdir(d))!=NULL) {
            proc_stat p;
            char name[56]="";
            int pid;
            if(!is_digits(de->d_name))continue;
            next.mem.proc_count++;
            pid=atoi(de->d_name);
            if(pid<=0)continue;
            if(!read_cmdline_name(pid,name,sizeof name) &&
               !read_exe_name(pid,name,sizeof name) &&
               !read_comm_name(pid,name,sizeof name))continue;
            next.total_matches++;
            if(next.shown>=MAX_BURBULI) {next.truncated=1;continue;}
            memset(&p,0,sizeof p);
            p.pid=pid;
            snprintf(p.name,sizeof p.name,"%s",name);
            read_process_memory(pid,&p);
            next.procs[next.shown++]=p;
        }
        closedir(d);
    }
    qsort(next.procs,(size_t)next.shown,sizeof next.procs[0],proc_desc_rss);
    snap=next;
}

static void line_at(int x,int y,const char *s,int which) {
    if(which==0 && font_heading)SetFont(font_heading,BLACK);
    else if(which==2 && font_small)SetFont(font_small,BLACK);
    else if(font_normal)SetFont(font_normal,BLACK);
    DrawString(x,y,(char*)s);
}

static void draw_snapshot(void) {
    char s[200];
    unsigned long approx,uptime=snap.mem.uptime_seconds;
    int i,n;
    ClearScreen();
    /* Large, readable typography for a 6-inch 758x1024 e-ink panel.
       Each process has its own two-line entry; never cram names into
       the RAM figures.  No data collection or controls changed. */
    line_at(20,18,"BURBULI MONITOR v0.2",0);
    line_at(20,75,"READ ONLY  |  PB616 / Linux",2);
    line_at(20,122,"SYSTEM MEMORY (MiB)",0);
    if(!snap.mem.meminfo_ok) {
        line_at(20,177,"ERROR: /proc/meminfo",1);
    } else {
        snprintf(s,sizeof s,"RAM total: %lu    free: %lu",
                 snap.mem.total/1024,snap.mem.free_kb/1024);
        line_at(20,178,s,1);
        if(snap.mem.has_available) {
            snprintf(s,sizeof s,"RAM available: %lu",snap.mem.available/1024);
            line_at(20,231,s,1);
        } else {
            approx=snap.mem.free_kb+snap.mem.buffers+snap.mem.cached;
            snprintf(s,sizeof s,"Free + buffers + cache: %lu*",approx/1024);
            line_at(20,231,s,1);
        }
        snprintf(s,sizeof s,"Buffers: %lu    cache: %lu",
                 snap.mem.buffers/1024,snap.mem.cached/1024);
        line_at(20,284,s,1);
        snprintf(s,sizeof s,"Swap total: %lu    free: %lu",
                 snap.mem.swap_total/1024,snap.mem.swap_free/1024);
        line_at(20,337,s,1);
    }
    snprintf(s,sizeof s,"Uptime: %luh %lum   processes: %u",
             uptime/3600,(uptime/60)%60,snap.mem.proc_count);
    line_at(20,393,s,2);
    snprintf(s,sizeof s,"BURBULI processes: %d",snap.total_matches);
    line_at(20,447,s,0);
    n=snap.shown<SCREEN_ROWS?snap.shown:SCREEN_ROWS;
    for(i=0;i<n;i++) {
        const proc_stat *p=&snap.procs[i];
        char shortname[45];
        const char *name=p->name;
        int y=511+i*77;
        snprintf(s,sizeof s,"PID %d   RSS %lu MiB   VIRT %lu MiB",
                 p->pid,p->rss_kb/1024,p->virt_kb/1024);
        line_at(20,y,s,1);
        if(strncmp(name,"BURBULI_DISPLAY_",16)==0) name+=16;
        snprintf(shortname,sizeof shortname,"%.39s",name);
        line_at(40,y+37,shortname,2);
    }
    if(!snap.total_matches)line_at(20,525,"No BURBULI process visible",1);
    if(snap.total_matches>SCREEN_ROWS) {
        snprintf(s,sizeof s,"Top %d of %d processes by RSS",SCREEN_ROWS,snap.total_matches);
        line_at(20,915,s,2);
    } else if(snap.truncated)line_at(20,915,"Process list truncated",2);
    line_at(20,945,"* Estimate, not MemAvailable",2);
    snprintf(s,sizeof s,"Sample #%u    UP: refresh    DOWN: exit",snap.serial);
    line_at(20,981,s,2);
    FullUpdate();
}

static int event_handler(int type,int p1,int p2) {
    (void)p2;
    switch(type) {
    case EVT_INIT:
        SetOrientation(0);
        SetPanelType(PANEL_DISABLED);
        font_heading=OpenFont("LiberationSans-Bold",34,0);
        font_normal=OpenFont("LiberationSans-Bold",28,0);
        font_small=OpenFont("LiberationSans",24,0);
        return 1;
    case EVT_SHOW:
        if(!started) {started=1;collect_snapshot();}
        draw_snapshot();
        return 1;
    case EVT_REPAINT:
        draw_snapshot();
        return 1;
    case EVT_KEYDOWN:
        if(p1==IV_KEY_DOWN) {CloseApp();return 1;}
        if(p1==IV_KEY_UP && !up_down) {
            up_down=1;
            collect_snapshot();
            draw_snapshot();
            return 1;
        }
        return 1;
    case EVT_KEYUP:
        if(p1==IV_KEY_UP)up_down=0;
        return 1;
    case EVT_EXIT:
        if(font_heading)CloseFont(font_heading);
        if(font_normal)CloseFont(font_normal);
        if(font_small)CloseFont(font_small);
        return 1;
    default:
        return 0;
    }
}

int main(int argc,char **argv) {
    (void)argc;(void)argv;
    InkViewMain(event_handler);
    return 0;
}
