/* BURBULI UPDATER v0.1 READONLY - PB616W InkView 5.17
 * Diagnostic prototype. READ-ONLY: downloads update manifest and .app to RAM,
 * verifies length and SHA256, shows result. Never writes files or executes code.
 * Raspberry Pi HTTP server: 192.168.0.195:8081/assets/updates/
 * Buttons: UP = check again; DOWN = exit. No use of central OK or side keys.
 * Do NOT install this preliminary prototype on the main Lux 2 yet.
 */
#include "inkview.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define FW_VERSION "v0.1 READONLY"
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

static ifont *screen_font;
static int started, running;
static char status[80]="READY", ver[64]="-", fname[96]="-", detail[100]="";
static void screen_draw(void) {
    ClearScreen();
    if(screen_font)SetFont(screen_font,BLACK);
    DrawString(30,34,"BURBULI UPDATER v0.1");
    DrawString(30,78,"READ-ONLY CHECK");
    DrawString(30,135,"Server: 192.168.0.195:8081");
    DrawString(30,195,"Manifest + APP + SHA256");
    DrawString(30,262,status);
    DrawString(30,322,ver);
    DrawString(30,375,fname);
    DrawString(30,440,detail);
    DrawString(30,850,"NO INSTALL / NO FILE WRITES");
    DrawString(30,905,"UP: retry     DOWN: exit");
    FullUpdate();
}
static void fail(const char *msg) {
    snprintf(status,sizeof status,"FAILED: %.60s",msg);
}
static void check_release(void) {
    void *buf=NULL, *binary=NULL;
    int n=0,bsize=0;
    release_info r;
    char url[250],hash[65];
    running=1;
    strcpy(status,"CHECKING SERVER...");
    strcpy(ver,"-");strcpy(fname,"-");strcpy(detail,"");
    screen_draw();
    buf=QuickDownload(MANIFEST_URL,&n,8);
    if(!buf || n<=0 || n>MAX_MANIFEST) {fail("MANIFEST DOWNLOAD");goto done;}
    if(!parse_manifest((const char *)buf,(size_t)n,&r)) {
        fail("MANIFEST FORMAT");goto done;
    }
    snprintf(ver,sizeof ver,"VERSION: %s",r.version);
    snprintf(fname,sizeof fname,"FILE: %.76s",r.file);
    if(snprintf(url,sizeof url,"%s%s",BASE_URL,r.file)>=(int)sizeof url) {
        fail("URL TOO LONG");goto done;
    }
    strcpy(status,"DOWNLOADING APP...");screen_draw();
    binary=QuickDownload(url,&bsize,8);
    if(!binary || bsize<=0 || bsize>MAX_APP) {fail("APP DOWNLOAD");goto done;}
    if((size_t)bsize!=r.size) {fail("SIZE MISMATCH");goto done;}
    sha256_hex(binary,(size_t)bsize,hash);
    if(strcmp(hash,r.hash)!=0) {fail("SHA256 MISMATCH");goto done;}
    strcpy(status,"VERIFIED OK");
    snprintf(detail,sizeof detail,"%d bytes / SHA256 OK",bsize);
done:
    free(binary);
    free(buf);
    running=0;
    screen_draw();
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
            SetHardTimer("burbuli_updater_readonly",check_release,1200);
        }
        return 1;
      case EVT_KEYDOWN:
        if(p1==IV_KEY_DOWN) { CloseApp();return 1; }
        if(p1==IV_KEY_UP && !running) {
            SetHardTimer("burbuli_updater_readonly",check_release,100);
            return 1;
        }
        return 1;
      case EVT_EXIT:
        ClearTimerByName("burbuli_updater_readonly");
        if(screen_font) CloseFont(screen_font);
        return 1;
      default: return 0;
    }
}
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    InkViewMain(handler);
    return 0;
}
