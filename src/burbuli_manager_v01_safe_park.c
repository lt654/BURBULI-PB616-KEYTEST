/*
 * BURBULI APPLICATION MANAGER v0.1 SAFE PARK -- PocketBook PB616 (InkView 5.17)
 *
 * Lists BURBULI apps installed in internal memory, and versions already parked.
 * Does not delete files. An explicit RIGHT, KEYUP, RIGHT moves one inactive,
 * unprotected .app to a reversible archive directory on the same filesystem.
 * The same operation restores a parked .app. Existing destination files are
 * NEVER intentionally overwritten, and no non-BURBULI names are touched.
 *
 * Controls: UP/DOWN select, RIGHT arm/confirm, LEFT cancel/exit.
 * No central OK, side keys, USB or network connection needed.
 *
 * Release 0.1 intentionally protects v1.5.20, v1.5.23, ALL updaters
 * and ALL managers from being parked, even if they are old copies.
 */

#include "inkview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#ifndef APPLICATIONS_DIR
#define APPLICATIONS_DIR "/mnt/ext1/applications"
#endif
#ifndef ARCHIVE_DIR
#define ARCHIVE_DIR "/mnt/ext1/burbuli_app_archive"
#endif

#define VERSION "v0.1 SAFE PARK"
#define MAX_ENTRIES 96
#define NAME_SIZE 112
#define PATH_SIZE 320

typedef struct {
    char name[NAME_SIZE];
    int parked;
    int protected_file;
} entry_t;

static entry_t entries[MAX_ENTRIES];
static int entries_count=0, selected=0, too_many=0, scan_error=0;
static int initialized=0, armed=0, key_latched=0;
static char armed_name[NAME_SIZE];
static int armed_parked=0;
static char message[128]="Not modified. Select an application.";
static ifont *heading_font=NULL, *main_font=NULL, *small_font=NULL;

static int prefix(const char *value,const char *p) {
    return strncmp(value,p,strlen(p))==0;
}
static int name_ok(const char *s) {
    size_t i,n;
    if(!s)return 0;
    n=strlen(s);
    if(n<6 || n>=NAME_SIZE || strcmp(s+n-4,".app")!=0)return 0;
    if(!(prefix(s,"BURBULI_DISPLAY_") || prefix(s,"BURBULI_UPDATER_") ||
         prefix(s,"BURBULI_MONITOR_")))return 0;
    if(strstr(s,".."))return 0;
    for(i=0;i<n;i++) {
        unsigned char c=(unsigned char)s[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
    }
    return 1;
}
static int permanently_protected(const char *s) {
    /* Refuse to park the two known good galleries, any updater, or a manager. */
    return prefix(s,"BURBULI_UPDATER_") ||
           prefix(s,"BURBULI_DISPLAY_MANAGER_") ||
           prefix(s,"BURBULI_DISPLAY_v1.5.20_") ||
           prefix(s,"BURBULI_DISPLAY_v1.5.23_");
}
static int join_path(char *out,size_t cap,const char *dir,const char *name) {
    int n=snprintf(out,cap,"%s/%s",dir,name);
    return n>=0 && (size_t)n<cap;
}
static int valid_regular_file(const char *path) {
    struct stat st;
    return lstat(path,&st)==0 && S_ISREG(st.st_mode);
}
static int same_executable_is_running(const char *path) {
    DIR *d;
    struct dirent *item;
    struct stat wanted,exe;
    int result=0;
    if(stat(path,&wanted)!=0)return 1; /* conservative */
    d=opendir("/proc");
    if(!d)return 1; /* can't establish safety */
    while((item=readdir(d))!=NULL) {
        char linkpath[96];
        const char *c=item->d_name;
        if(!*c)continue;
        while(*c && isdigit((unsigned char)*c))c++;
        if(*c)continue;
        if(snprintf(linkpath,sizeof linkpath,"/proc/%s/exe",item->d_name)>=(int)sizeof linkpath)continue;
        if(stat(linkpath,&exe)==0 && exe.st_dev==wanted.st_dev && exe.st_ino==wanted.st_ino) {
            result=1;
            break;
        }
    }
    closedir(d);
    return result;
}
static int compare_entries(const void *a,const void *b) {
    const entry_t *x=(const entry_t *)a,*y=(const entry_t *)b;
    if(x->parked!=y->parked)return x->parked-y->parked;
    return strcmp(x->name,y->name);
}
static void scan_folder(const char *dir,int parked) {
    DIR *d=opendir(dir);
    struct dirent *item;
    if(!d) {
        if(errno!=ENOENT || !parked)scan_error=1;
        return;
    }
    while((item=readdir(d))!=NULL) {
        char path[PATH_SIZE];
        entry_t *e;
        if(!name_ok(item->d_name))continue;
        if(!join_path(path,sizeof path,dir,item->d_name))continue;
        if(!valid_regular_file(path))continue; /* symlinks are never moved */
        if(entries_count>=MAX_ENTRIES) {too_many=1;continue;}
        e=&entries[entries_count++];
        memset(e,0,sizeof *e);
        memcpy(e->name,item->d_name,strlen(item->d_name)+1);
        e->parked=parked;
        e->protected_file=permanently_protected(e->name);
        /* Check running status when user requests a move; not on each redraw. */
    }
    closedir(d);
}
static void scan_apps(const char *preferred,int wanted_parked) {
    int i;
    entries_count=0;too_many=0;scan_error=0;
    scan_folder(APPLICATIONS_DIR,0);
    scan_folder(ARCHIVE_DIR,1);
    qsort(entries,(size_t)entries_count,sizeof entries[0],compare_entries);
    if(preferred) {
        for(i=0;i<entries_count;i++) {
            if(entries[i].parked==wanted_parked && !strcmp(entries[i].name,preferred)) {
                selected=i;
                return;
            }
        }
    }
    if(selected>=entries_count)selected=entries_count>0?entries_count-1:0;
}
static void font_line(int x,int y,const char *s,int kind) {
    if(kind==0 && heading_font)SetFont(heading_font,BLACK);
    else if(kind==2 && small_font)SetFont(small_font,BLACK);
    else if(main_font)SetFont(main_font,BLACK);
    DrawString(x,y,(char *)s);
}
/* Keep long .app file names legible by splitting into rows, not truncating them. */
static void draw_wrapped_name(const char *name,int x,int y) {
    size_t n=strlen(name),at=0;
    int row=0;
    while(at<n && row<4) {
        size_t k=n-at;
        char part[36];
        if(k>35)k=35;
        memcpy(part,name+at,k);
        part[k]=0;
        font_line(x,y+row*49,part,1);
        at+=k;row++;
    }
}
static void draw_screen(void) {
    char s[128];
    int act=0,parked=0,i;
    ClearScreen();
    font_line(20,20,"BURBULI APP MANAGER",0);
    font_line(20,72,"v0.1  -  reversible parking only",2);
    for(i=0;i<entries_count;i++)if(entries[i].parked)parked++;else act++;
    snprintf(s,sizeof s,"Active: %d    Parked: %d",act,parked);
    font_line(20,120,s,1);
    if(scan_error || too_many) {
        font_line(20,174,"SCAN ERROR / LIMIT: WRITE DISABLED",1);
        font_line(20,264,"LEFT: exit",1);
        FullUpdate();return;
    }
    if(entries_count==0) {
        font_line(20,200,"No BURBULI apps found",1);
        font_line(20,275,"LEFT: exit",1);
        FullUpdate();return;
    }
    snprintf(s,sizeof s,"APPLICATION  %d / %d",selected+1,entries_count);
    font_line(20,192,s,1);
    font_line(20,248,entries[selected].parked?"PARKED / CAN RESTORE":"INSTALLED / ACTIVE",0);
    draw_wrapped_name(entries[selected].name,20,320);
    if(entries[selected].protected_file) {
        font_line(20,560,"PROTECTED - cannot park",1);
    } else if(armed) {
        font_line(20,550,entries[selected].parked?"CONFIRM RESTORE?":"CONFIRM PARK?",0);
        font_line(20,600,"RIGHT AGAIN: confirm",1);
        font_line(20,647,"LEFT: cancel",1);
    } else {
        font_line(20,560,entries[selected].parked?"RIGHT: request RESTORE":"RIGHT: request PARK",1);
    }
    font_line(20,735,message,2);
    font_line(20,820,"UP/DOWN: select app",1);
    font_line(20,866,"RIGHT: arm / confirm",1);
    font_line(20,912,"LEFT: cancel / exit",1);
    FullUpdate();
}

static void do_move(void) {
    const entry_t *e;
    char src[PATH_SIZE],dst[PATH_SIZE];
    struct stat st;
    const char *from,*to;
    int new_status;
    char name[NAME_SIZE];
    if(!armed || entries_count==0 || scan_error || too_many)return;
    e=&entries[selected];
    if(e->protected_file || strcmp(armed_name,e->name)!=0 || armed_parked!=e->parked) {
        snprintf(message,sizeof message,"STOP: selection changed");return;
    }
    from=e->parked?ARCHIVE_DIR:APPLICATIONS_DIR;
    to=e->parked?APPLICATIONS_DIR:ARCHIVE_DIR;
    new_status=!e->parked;
    snprintf(name,sizeof name,"%s",e->name);
    if(!join_path(src,sizeof src,from,name) || !join_path(dst,sizeof dst,to,name)) {
        snprintf(message,sizeof message,"STOP: path too long");return;
    }
    if(!valid_regular_file(src)) {
        snprintf(message,sizeof message,"STOP: source missing or unsafe");return;
    }
    if(lstat(dst,&st)==0) {
        snprintf(message,sizeof message,"STOP: destination already exists");return;
    }
    if(errno!=ENOENT) {
        snprintf(message,sizeof message,"STOP: destination check error");return;
    }
    if(!e->parked) {
        if(lstat(ARCHIVE_DIR,&st)!=0) {
            if(errno!=ENOENT || mkdir(ARCHIVE_DIR,0700)!=0) {
                snprintf(message,sizeof message,"STOP: archive folder error");return;
            }
        }
        if(lstat(ARCHIVE_DIR,&st)!=0 || !S_ISDIR(st.st_mode)) {
            snprintf(message,sizeof message,"STOP: unsafe archive folder");return;
        }
    }
    if(same_executable_is_running(src)) {
        snprintf(message,sizeof message,"STOP: application is running");return;
    }
    /* Both directories are in internal memory. This is a rename, not deletion.
       No other copy of the manager should be used concurrently. */
    if(rename(src,dst)!=0) {
        snprintf(message,sizeof message,"STOP: cannot move file (errno=%d)",errno);
        return;
    }
    sync();
    snprintf(message,sizeof message,new_status?"PARKED OK - reversible":"RESTORED OK");
    scan_apps(name,new_status);
}

static int handler(int type,int p1,int p2) {
    (void)p2;
    switch(type) {
    case EVT_INIT:
        SetOrientation(0);
        SetPanelType(PANEL_DISABLED);
        heading_font=OpenFont("LiberationSans-Bold",32,0);
        main_font=OpenFont("LiberationSans-Bold",27,0);
        small_font=OpenFont("LiberationSans",23,0);
        return 1;
    case EVT_SHOW:
    case EVT_REPAINT:
        if(!initialized) {
            initialized=1;
            scan_apps(NULL,0);
        }
        draw_screen();
        return 1;
    case EVT_KEYUP:
        if(p1==key_latched)key_latched=0;
        return 1;
    case EVT_KEYDOWN:
        if(key_latched==p1)return 1;
        key_latched=p1;
        if(p1==IV_KEY_LEFT) {
            if(armed) {
                armed=0;
                snprintf(message,sizeof message,"CANCELLED - no changes");
                draw_screen();
            } else CloseApp();
            return 1;
        }
        if(armed && p1!=IV_KEY_RIGHT) {
            armed=0;
            snprintf(message,sizeof message,"CANCELLED - selection unchanged");
        }
        if(p1==IV_KEY_UP || p1==IV_KEY_DOWN) {
            if(entries_count>0) {
                selected=(selected+(p1==IV_KEY_UP?1:entries_count-1))%entries_count;
                snprintf(message,sizeof message,"Choose with UP/DOWN; RIGHT to park");
            }
            draw_screen();
            return 1;
        }
        if(p1==IV_KEY_RIGHT) {
            if(!entries_count || scan_error || too_many) {
                snprintf(message,sizeof message,"STOP: no safe selection");
            } else if(entries[selected].protected_file) {
                snprintf(message,sizeof message,"PROTECTED: file never moved");
            } else if(!armed) {
                armed=1;
                snprintf(armed_name,sizeof armed_name,"%s",entries[selected].name);
                armed_parked=entries[selected].parked;
                snprintf(message,sizeof message,"No changes yet - RIGHT again");
            } else {
                do_move();
                armed=0;
            }
            draw_screen();
            return 1;
        }
        return 1;
    case EVT_EXIT:
        if(heading_font)CloseFont(heading_font);
        if(main_font)CloseFont(main_font);
        if(small_font)CloseFont(small_font);
        return 1;
    default:return 0;
    }
}

int main(int argc,char **argv) {
    (void)argc;(void)argv;
    InkViewMain(handler);
    return 0;
}
