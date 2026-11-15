/* BURBULI UPDATER v0.2 SAFE INSTALL - PB616W InkView 5.17
 * Receives manifest + .app in RAM, validates size and SHA256.
 * First UP arms installation, second UP installs (only after KEYUP).
 * Destination: /mnt/ext1/applications/<validated BURBULI_DISPLAY_*.app>.
 * Refuses to replace any existing file. Uses staged write + readback hash
 * and rename. Never starts the installed app. DOWN exits at any time.
 * No central OK or side buttons required. No self-update capability.
 * Server: http://192.168.0.195:8081/assets/updates/
 */
#include "inkview.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define FW_VERSION "v0.2 SAFE INSTALL"
#define BASE_URL "http://192.168.0.195:8081/assets/updates/"
#define MANIFEST_URL BASE_URL "release.txt"
#define MAX_MANIFEST 4096
#define MAX_APP (2*1024*1024)

typedef struct {
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t bits;
    size_t used;
} sha256_ctx;

static uint32_t ror32(uint32_t x, unsigned int n) {
    return (x >> n) | (x << (32 - n));
}
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static void sha256_transform(sha256_ctx *ctx, const uint8_t b[64]) {
    uint32_t w[64], a,bb,c,d,e,f,g,h,t1,t2;
    int i;
    for (i=0;i<16;i++) {
        w[i] = ((uint32_t)b[4*i]<<24)|((uint32_t)b[4*i+1]<<16)|
               ((uint32_t)b[4*i+2]<<8)|((uint32_t)b[4*i+3]);
    }
    for (i=16;i<64;i++) {
        uint32_t x=w[i-15],y=w[i-2];
        uint32_t s0=ror32(x,7)^ror32(x,18)^(x>>3);
        uint32_t s1=ror32(y,17)^ror32(y,19)^(y>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    a=ctx->h[0];bb=ctx->h[1];c=ctx->h[2];d=ctx->h[3];
    e=ctx->h[4];f=ctx->h[5];g=ctx->h[6];h=ctx->h[7];
    for (i=0;i<64;i++) {
        uint32_t S1=ror32(e,6)^ror32(e,11)^ror32(e,25);
        uint32_t ch=(e&f)^(~e&g);
        uint32_t S0=ror32(a,2)^ror32(a,13)^ror32(a,22);
        uint32_t maj=(a&bb)^(a&c)^(bb&c);
        t1=h+S1+ch+K256[i]+w[i];
        t2=S0+maj;
        h=g;g=f;f=e;e=d+t1;d=c;c=bb;bb=a;a=t1+t2;
    }
    ctx->h[0]+=a;ctx->h[1]+=bb;ctx->h[2]+=c;ctx->h[3]+=d;
    ctx->h[4]+=e;ctx->h[5]+=f;ctx->h[6]+=g;ctx->h[7]+=h;
}
static void sha256_init(sha256_ctx *ctx) {
    static const uint32_t init[8]={
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(ctx->h,init,sizeof init);
    ctx->bits=0;ctx->used=0;
}
static void sha256_update(sha256_ctx *ctx,const void *data,size_t len) {
    const uint8_t *p=(const uint8_t *)data;
    size_t take;
    ctx->bits+=(uint64_t)len*8u;
    while(len) {
        take=64-ctx->used;
        if(take>len)take=len;
        memcpy(ctx->buf+ctx->used,p,take);
        ctx->used+=take;p+=take;len-=take;
        if(ctx->used==64) {
            sha256_transform(ctx,ctx->buf);
            ctx->used=0;
        }
    }
}
static void sha256_final(sha256_ctx *ctx,uint8_t out[32]) {
    uint64_t bits=ctx->bits;
    int i;
    ctx->buf[ctx->used++]=0x80;
    if(ctx->used>56) {
        memset(ctx->buf+ctx->used,0,64-ctx->used);
        sha256_transform(ctx,ctx->buf);
        ctx->used=0;
    }
    memset(ctx->buf+ctx->used,0,56-ctx->used);
    for(i=0;i<8;i++)ctx->buf[56+i]=(uint8_t)(bits>>(56-8*i));
    sha256_transform(ctx,ctx->buf);
    for(i=0;i<8;i++) {
        out[4*i]=(uint8_t)(ctx->h[i]>>24);
        out[4*i+1]=(uint8_t)(ctx->h[i]>>16);
        out[4*i+2]=(uint8_t)(ctx->h[i]>>8);
        out[4*i+3]=(uint8_t)(ctx->h[i]);
    }
}
static void sha256_hex(const void *data,size_t len,char out[65]) {
    static const char hex[]="0123456789abcdef";
    sha256_ctx ctx;
    uint8_t digest[32];
    int i;
    sha256_init(&ctx);
    sha256_update(&ctx,data,len);
    sha256_final(&ctx,digest);
    for(i=0;i<32;i++) {
        out[2*i]=hex[digest[i]>>4];
        out[2*i+1]=hex[digest[i]&15];
    }
    out[64]=0;
}

typedef struct {
    char version[32];
    char file[96];
    size_t size;
    char hash[65];
} release_info;
static int only_name(const char *p) {
    size_t n=strlen(p),i;
    if(n<5 || n>=96 || strcmp(p+n-4,".app")!=0)return 0;
    if(strncmp(p,"BURBULI_DISPLAY_",16)!=0)return 0;
    if(p[0]=='.')return 0;
    for(i=0;i<n;i++) {
        unsigned char c=(unsigned char)p[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return 0;
    }
    if(strstr(p,".."))return 0;
    return 1;
}
/* Read only exact key=value lines; reject duplicates, extra or malformed fields. */
static int parse_manifest(const char *data,size_t n,release_info *r) {
    char text[MAX_MANIFEST+1], *line,*save=NULL;
    unsigned int found=0;
    size_t i;
    if(!data || n==0 || n>MAX_MANIFEST || memchr(data,'\0',n))return 0;
    memcpy(text,data,n);text[n]=0;
    memset(r,0,sizeof *r);
    line=strtok_r(text,"\n",&save);
    while(line) {
        char *eq=strchr(line,'=');
        unsigned int bit=0;
        if(!eq || strchr(eq+1,'=') || line[0]==' ' || strchr(line,'\r'))return 0;
        *eq++=0;
        if(strcmp(line,"format")==0) {
            bit=1;
            if(strcmp(eq,"1")!=0)return 0;
        } else if(strcmp(line,"version")==0) {
            bit=2;
            if(strlen(eq)<1 || strlen(eq)>=sizeof r->version)return 0;
            for(i=0;eq[i];i++) if(!(isdigit((unsigned char)eq[i])||eq[i]=='.'))return 0;
            strcpy(r->version,eq);
        } else if(strcmp(line,"file")==0) {
            bit=4;
            if(!only_name(eq))return 0;
            strcpy(r->file,eq);
        } else if(strcmp(line,"size")==0) {
            unsigned long val=0;
            bit=8;
            if(!*eq)return 0;
            for(i=0;eq[i];i++) {
                if(!isdigit((unsigned char)eq[i]))return 0;
                val=val*10+(unsigned)(eq[i]-'0');
                if(val>MAX_APP)return 0;
            }
            if(val<4096)return 0;
            r->size=(size_t)val;
        } else if(strcmp(line,"sha256")==0) {
            bit=16;
            if(strlen(eq)!=64)return 0;
            for(i=0;i<64;i++) if(!((eq[i]>='0' && eq[i]<='9') || (eq[i]>='a' && eq[i]<='f')))return 0;
            strcpy(r->hash,eq);
        } else return 0;
        if(found&bit)return 0;
        found|=bit;
        line=strtok_r(NULL,"\n",&save);
    }
    return found==31;
}


#define APPLICATIONS_DIR "/mnt/ext1/applications"
#define TMP_PREFIX APPLICATIONS_DIR "/.burbuli_updater_stage_"

typedef enum { ST_CHECKING, ST_FAILED, ST_VERIFIED, ST_CONFIRM, ST_INSTALLED, ST_EXISTS_OK, ST_CONFLICT } state_t;
static ifont *screen_font;
static int started=0, running=0, up_pressed=0;
static state_t state=ST_CHECKING;
static char status[110]="READY", ver[64]="-", fname[110]="-", detail[130]="";
static void *verified_app=NULL;
static size_t verified_len=0;
static release_info verified_release;

static void discard_buffer(void) {
    free(verified_app);
    verified_app=NULL;
    verified_len=0;
    memset(&verified_release,0,sizeof verified_release);
}

static void screen_draw(void) {
    ClearScreen();
    if(screen_font) SetFont(screen_font,BLACK);
    DrawString(28,34,"BURBULI UPDATER v0.2");
    DrawString(28,78,"SAFE INSTALL - NO OVERWRITE");
    DrawString(28,135,"Server: 192.168.0.195:8081");
    DrawString(28,195,"Manifest + APP + SHA256");
    DrawString(28,262,status);
    DrawString(28,322,ver);
    DrawString(28,375,fname);
    DrawString(28,440,detail);
    if(state==ST_VERIFIED) {
        DrawString(28,760,"UP: request INSTALL");
        DrawString(28,810,"No files written yet");
    } else if(state==ST_CONFIRM) {
        DrawString(28,760,"UP AGAIN: INSTALL new name");
        DrawString(28,810,"DOWN: cancel / exit");
    } else if(state==ST_INSTALLED) {
        DrawString(28,760,"DONE: launch new app in menu");
        DrawString(28,810,"Old version preserved");
    } else if(state==ST_FAILED) {
        DrawString(28,760,"UP: retry download");
    } else if(state==ST_EXISTS_OK || state==ST_CONFLICT) {
        DrawString(28,760,"Existing file NOT changed");
    }
    DrawString(28,910,"DOWN: exit");
    FullUpdate();
}

static void fail(const char *msg) {
    state=ST_FAILED;
    snprintf(status,sizeof status,"FAILED: %.85s",msg);
}

static void check_release(void) {
    void *manifest=NULL, *binary=NULL;
    int n=0,bsize=0;
    release_info r;
    char url[250],hash[65];
    running=1;
    discard_buffer();
    state=ST_CHECKING;
    strcpy(status,"CHECKING SERVER...");
    strcpy(ver,"-"); strcpy(fname,"-"); strcpy(detail,"");
    screen_draw();
    manifest=QuickDownload(MANIFEST_URL,&n,8);
    if(!manifest || n<=0 || n>MAX_MANIFEST) {fail("MANIFEST DOWNLOAD");goto done;}
    if(!parse_manifest((const char *)manifest,(size_t)n,&r)) {
        fail("MANIFEST FORMAT");goto done;
    }
    snprintf(ver,sizeof ver,"VERSION: %s",r.version);
    snprintf(fname,sizeof fname,"FILE: %.90s",r.file);
    if(snprintf(url,sizeof url,"%s%s",BASE_URL,r.file)>=(int)sizeof url) {
        fail("URL TOO LONG");goto done;
    }
    strcpy(status,"DOWNLOADING APP...");screen_draw();
    binary=QuickDownload(url,&bsize,8);
    if(!binary || bsize<=0 || bsize>MAX_APP) {fail("APP DOWNLOAD");goto done;}
    if((size_t)bsize!=r.size) {fail("SIZE MISMATCH");goto done;}
    sha256_hex(binary,(size_t)bsize,hash);
    if(strcmp(hash,r.hash)!=0) {fail("SHA256 MISMATCH");goto done;}
    verified_app=binary;
    binary=NULL;
    verified_len=(size_t)bsize;
    verified_release=r;
    state=ST_VERIFIED;
    strcpy(status,"VERIFIED OK - READY");
    snprintf(detail,sizeof detail,"%d bytes / SHA256 OK",bsize);
done:
    free(binary);
    free(manifest);
    running=0;
    screen_draw();
}

/* Read a written staged .app back and verify its checksum before activation. */
static int verify_disk_file(const char *path,size_t expected,const char *sha) {
    int fd;
    unsigned char buf[8192],digest[32];
    sha256_ctx ctx;
    char hex[65];
    static const char digits[]="0123456789abcdef";
    size_t total=0;
    ssize_t count;
    int i,ok=0;
    struct stat st;
    fd=open(path,O_RDONLY
#ifdef O_NOFOLLOW
        | O_NOFOLLOW
#endif
    );
    if(fd<0)return 0;
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size!=(off_t)expected)goto done;
    sha256_init(&ctx);
    while((count=read(fd,buf,sizeof buf))!=0) {
        if(count<0) {
            if(errno==EINTR)continue;
            goto done;
        }
        total+=(size_t)count;
        if(total>expected)goto done;
        sha256_update(&ctx,buf,(size_t)count);
    }
    if(total!=expected)goto done;
    sha256_final(&ctx,digest);
    for(i=0;i<32;i++) {
        hex[i*2]=digits[digest[i]>>4];
        hex[i*2+1]=digits[digest[i]&15];
    }
    hex[64]=0;
    ok=strcmp(hex,sha)==0;
done:
    close(fd);
    return ok;
}

/* Stage, fsync, verify, and rename to a NEW name. Never overwrite an existing app. */
static void install_verified(void) {
    char dest[256],tmp[256];
    struct stat st;
    size_t sent=0;
    int fd=-1, tmp_created=0;
    ssize_t n;
    int close_ok;
    if(!verified_app || !verified_len || !only_name(verified_release.file)) {
        fail("NO VERIFIED APP");return;
    }
    if(stat(APPLICATIONS_DIR,&st)!=0 || !S_ISDIR(st.st_mode)) {
        fail("APPLICATIONS FOLDER MISSING");return;
    }
    if(snprintf(dest,sizeof dest,"%s/%s",APPLICATIONS_DIR,verified_release.file)>=(int)sizeof dest) {
        fail("DEST PATH TOO LONG");return;
    }
    if(lstat(dest,&st)==0) {
        if(verify_disk_file(dest,verified_len,verified_release.hash)) {
            state=ST_EXISTS_OK;
            strcpy(status,"ALREADY INSTALLED - SAME SHA");
        } else {
            state=ST_CONFLICT;
            strcpy(status,"FILE EXISTS - NOT OVERWRITTEN");
        }
        discard_buffer();
        return;
    }
    if(errno!=ENOENT) {fail("DEST STAT ERROR");return;}
    if(snprintf(tmp,sizeof tmp,"%s%ld.tmp",TMP_PREFIX,(long)getpid())>=(int)sizeof tmp) {
        fail("TEMP PATH TOO LONG");return;
    }
    fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL
#ifdef O_NOFOLLOW
        | O_NOFOLLOW
#endif
    ,0600);
    if(fd<0) {fail("TEMP CREATE ERROR");return;}
    tmp_created=1;
    while(sent<verified_len) {
        n=write(fd,(const char*)verified_app+sent,verified_len-sent);
        if(n<0 && errno==EINTR)continue;
        if(n<=0) {fail("TEMP WRITE ERROR");goto cleanup;}
        sent+=(size_t)n;
    }
    if(fsync(fd)!=0) {fail("TEMP FSYNC ERROR");goto cleanup;}
    close_ok=close(fd);
    fd=-1;
    if(close_ok!=0) {fail("TEMP CLOSE ERROR");goto cleanup;}
    if(!verify_disk_file(tmp,verified_len,verified_release.hash)) {
        fail("DISK SHA256 MISMATCH");goto cleanup;
    }
    /* No other updater instance may run while installing. A preexisting
       destination is never replaced. Do not remove another version. */
    if(lstat(dest,&st)==0) {fail("NAME CREATED DURING INSTALL");goto cleanup;}
    if(errno!=ENOENT) {fail("DEST CHECK ERROR");goto cleanup;}
    if(rename(tmp,dest)!=0) {fail("INSTALL RENAME ERROR");goto cleanup;}
    tmp_created=0;
    sync();
    state=ST_INSTALLED;
    strcpy(status,"INSTALLED - SHA256 OK");
    snprintf(detail,sizeof detail,"New file: %.86s",verified_release.file);
    discard_buffer();
cleanup:
    if(fd>=0)close(fd);
    if(tmp_created)unlink(tmp);
}

static int handler(int type,int p1,int p2) {
    (void)p2;
    switch(type) {
      case EVT_INIT:
        SetOrientation(0);
        SetPanelType(PANEL_DISABLED);
        screen_font=OpenFont("LiberationSans-Bold",24,0);
        return 1;
      case EVT_SHOW:
      case EVT_REPAINT:
        screen_draw();
        if(!started) {
            started=1;
            SetHardTimer("burbuli_updater_v02",check_release,1200);
        }
        return 1;
      case EVT_KEYUP:
        if(p1==IV_KEY_UP)up_pressed=0;
        return 1;
      case EVT_KEYDOWN:
        if(p1==IV_KEY_DOWN) {CloseApp();return 1;}
        if(p1==IV_KEY_UP && !running && !up_pressed) {
            up_pressed=1; /* KEYUP required before another confirmation */
            if(state==ST_FAILED) {
                SetHardTimer("burbuli_updater_v02",check_release,100);
            } else if(state==ST_VERIFIED) {
                state=ST_CONFIRM;
                strcpy(status,"CONFIRM INSTALL NEW APP?");
                screen_draw();
            } else if(state==ST_CONFIRM) {
                running=1;
                strcpy(status,"INSTALLING - DO NOT POWER OFF");
                screen_draw();
                install_verified();
                running=0;
                screen_draw();
            }
            return 1;
        }
        return 1;
      case EVT_EXIT:
        ClearTimerByName("burbuli_updater_v02");
        discard_buffer();
        if(screen_font)CloseFont(screen_font);
        return 1;
      default:return 0;
    }
}
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    InkViewMain(handler);
    return 0;
}
