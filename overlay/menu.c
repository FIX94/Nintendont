#include "../common/include/Overlay.h"
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
static volatile OverlayState *const state=(volatile OverlayState*)OVL_STATE_PPC;
/* Canonical GameCube inputs, before this menu's remapping. */
static const u16 bits[12]={0x100,0x200,0x400,0x800,0x10,0x40,0x20,0x1000,8,4,1,2};
static const char *const names[13]={"A","B","X","Y","Z","L","R","START","UP","DOWN","LEFT","RIGHT","NONE"};
#define TOGGLE 0x1C00u /* Start + X + Y, independent of the active mapping */
#define BUTTONS_MASK 0x1F7Fu /* the 12 real buttons; ignores origin/status bits */
static void identity(volatile u32 *m) {u32 i;for(i=0;i<20;i++)m[i]=i<12?i:0;}
static void neutral(OverlayPad *p) {
    p->button=0;p->stickX=p->stickY=p->substickX=p->substickY=0;
    p->triggerLeft=p->triggerRight=p->analogA=p->analogB=0;
}
static u8 pressure(const OverlayPad *p,u32 source) {
    if(source==5)return p->triggerLeft;
    if(source==6)return p->triggerRight;
    if(source==0)return p->analogA ? p->analogA : ((p->button&0x100)?255:0);
    if(source==1)return p->analogB ? p->analogB : ((p->button&0x200)?255:0);
    return source<12 && (p->button&bits[source])?255:0;
}
static signed char invert(signed char x) {return x==-128?127:-x;}
static void remap(OverlayPad *p,volatile u32 *m) {
    u16 original=p->button, result=original&~0x1F7F;
    u8 l=pressure(p,m[5]),r=pressure(p,m[6]);
    /* Unchanged outputs must preserve the game's original pressure bytes. */
    u8 a=m[0]==0?p->analogA:pressure(p,m[0]);
    u8 b=m[1]==1?p->analogB:pressure(p,m[1]);
    u32 i;
    for(i=0;i<12;i++) if(m[i]<12 && (original&bits[m[i]]))result|=bits[i];
    p->button=result;p->triggerLeft=l;p->triggerRight=r;p->analogA=a;p->analogB=b;
    if(m[12]) {signed char x=p->stickX,y=p->stickY;p->stickX=p->substickX;p->stickY=p->substickY;p->substickX=x;p->substickY=y;}
    if(m[13])p->stickX=invert(p->stickX);
    if(m[14])p->stickY=invert(p->stickY);
    if(m[15])p->substickX=invert(p->substickX);
    if(m[16])p->substickY=invert(p->substickY);
}
/* M21: the menu opens on the controller picture.
 *   - the MAIN STICK is the only thing that moves: left/right walks the
 *     buttons in the order they sit on the controller, down opens the list.
 *   - every button stays assignable, because no button navigates here.
 *     Press one and it takes the highlighted spot, deciding on release so a
 *     chord (X+Y+Start) closes the menu instead of assigning anything.
 * On the list screens nothing is being assigned, so D-pad and stick both move,
 * A opens, B goes back to the picture. */
#define PICTURE 0u
#define LIST 1u
#define TEST 2u
#define STICKS 3u
#define QUIT 4u
enum {L_BUTTONS,L_TEST,L_STICKS,L_PORT,L_RESET,L_RESUME,L_QUIT,LIST_ITEMS};
#define NOTICE_RESET 1u
#define NOTICE_TWO 2u
#define NOTICE_NOPAD 4u
#define NOTICE_SWAP 16u /* + GameCube output index */
#define HOLD_TICKS 45u /* retraces of held B to leave the tester */
/* Left to right across the drawing: L, D-pad, Start, B, Y, A, R, X, Z. */
static const u8 ring[12]={5,10,8,9,11,7,1,3,0,6,2,4};
static void close_menu(u32 apply) {
    u32 i;
    if(apply) {
        for(i=0;i<80;i++) {
            u32 v=((volatile u32*)state->edit)[i];
            if(((volatile u32*)state->active)[i]!=v) {((volatile u32*)state->active)[i]=v;state->dirty=1;}
        }
        state->applies++;
    } else state->cancels++;
    state->open=0;state->release=1;state->mode=PICTURE;state->confirm=0;state->notice=0;
    state->candidate=0;state->toast=90;
}
static u32 stickdirs(const OverlayPad *p) {
    u32 d=0;
    if(p->stickX<-52)d|=1;
    if(p->stickX>52)d|=2;
    if(p->stickY<-52)d|=4;
    if(p->stickY>52)d|=8;
    return d<<16;
}
/* Swap, so one press never leaves a button doing two things or nothing. */
static void assign(u32 target,u32 source) {
    volatile u32 *m=state->edit[state->port&3];
    u32 j,old=m[target];
    state->notice=0;
    if(old==source)return;
    for(j=0;j<12;j++)if(j!=target && m[j]==source) {m[j]=old;state->notice=NOTICE_SWAP+j;break;}
    m[target]=source;
}
static void enter_picture(void) {
    state->mode=PICTURE;state->waitNeutral=1;state->candidate=0;state->confirm=0;
}
/* ownerRaw drives nothing but the release latch; the pad in the SELECTED port
 * supplies the button being assigned, and the owner's stick does the moving. */
static void picture_input(u32 ownerRaw,u32 srcRaw,u32 stickdir) {
    u32 s=srcRaw&BUTTONS_MASK,i;
    if(stickdir&4) {state->mode=LIST;state->row=0;state->candidate=0;state->notice=0;state->confirm=0;state->waitNeutral=1;return;}
    if(stickdir&3) {
        state->cursor=((state->cursor<12?state->cursor:0)+((stickdir&2)?1:11))%12;
        state->notice=0;
        /* Moving while a button is held cancels it, so nothing lands on the
         * button you just moved to. Release and press again to assign. */
        if(state->candidate || s) {state->candidate=0;state->waitNeutral=1;}
    }
    if(state->waitNeutral) {
        state->candidate=0;
        if(!(ownerRaw&BUTTONS_MASK) && !s)state->waitNeutral=0;
        return;
    }
    if(!(state->connected&(1u<<(state->port&3)))) {state->candidate=0;state->notice=NOTICE_NOPAD;return;}
    if(s) {
        /* Two buttons at once is never an assignment: wait for a clean release. */
        if(s&(s-1) || (state->candidate && state->candidate!=s)) {
            state->candidate=0;state->waitNeutral=1;state->notice=NOTICE_TWO;
        } else state->candidate=s;
    } else if(state->candidate) {
        for(i=0;i<12 && state->candidate!=bits[i];i++);
        if(i<12 && state->cursor<12)assign(ring[state->cursor],i);
        state->candidate=0;
    }
}
static void test_input(u32 owner,u32 source) {
    u32 sample=source&BUTTONS_MASK;
    if(state->waitNeutral) {if(!(owner&BUTTONS_MASK))state->waitNeutral=0;return;}
    if((state->connected&(1u<<(state->port&3))) && sample)state->lastDetected=sample;
    if(owner&0x200) {
        if(!state->confirm) {state->confirm=1;state->timer=state->drawCalls;}
        else if(state->drawCalls-state->timer>=HOLD_TICKS) {state->mode=LIST;state->confirm=0;state->waitNeutral=1;}
    } else state->confirm=0;
}
static void list_input(u32 btn,u32 dir) {
    if(dir&8) {
        if(state->row)state->row--;
        else enter_picture();
        state->confirm=0;
    }
    else if(dir&4) {if(state->row<LIST_ITEMS-1)state->row++;state->confirm=0;}
    else if(state->row==L_PORT && (btn&3)) {state->port=((state->port&3)+((btn&2)?1:3))%4;state->lastDetected=0;}
    else if(btn&0x200)enter_picture();
    else if(btn&0x100) switch(state->row) {
        case L_BUTTONS: enter_picture();break;
        case L_TEST: state->mode=TEST;state->waitNeutral=1;state->lastDetected=0;state->confirm=0;break;
        case L_STICKS: state->mode=STICKS;state->action=0;break;
        case L_PORT: state->port=((state->port&3)+1)%4;state->lastDetected=0;break;
        case L_RESET:
            if(state->confirm) {identity(state->edit[state->port&3]);state->confirm=0;state->notice=NOTICE_RESET;}
            else state->confirm=1;
            break;
        case L_RESUME: close_menu(1);break;
        case L_QUIT: state->mode=QUIT;state->action=0;break;
        default: state->row=0;break;
    }
}
static void sticks_input(u32 btn,u32 dir) {
    u32 c=state->action;
    if(c>4)c=0;
    if(dir&8) {if(c)c--;}
    else if(dir&4) {if(c<4)c++;}
    else if(btn&0x103)state->edit[state->port&3][12+c]^=1; /* A, D-pad Left or Right */
    else if(btn&0x200)state->mode=LIST;
    state->action=c;
}
static void quit_input(u32 btn,u32 dir) {
    if(dir&8)state->action=0;
    else if(dir&4)state->action=1;
    else if(btn&0x200)state->mode=LIST;
    else if(btn&0x100) {
        if(state->action==1) {close_menu(1);state->exitRequested=1;}
        else state->mode=LIST;
    }
}
typedef struct {u32 top,bottom,stride,step,fields,x,y,rawTop,rawBottom,dcr,vtr;} VideoLayout;
/* The input path uses exactly the same acceptance rules as the renderer. */
static int video_layout(VideoLayout *v) {
    volatile u16 *vi=(volatile u16*)0xCC002000;
    u32 top,bottom,rawTop,rawBottom,stride,width,acv,dcr,step,fields,x,y;
    rawTop=((u32)vi[14]<<16)|vi[15];rawBottom=((u32)vi[18]<<16)|vi[19];
    dcr=vi[1];acv=(vi[0]>>4)&1023;stride=(vi[36]&255)*32;width=((vi[36]>>8)&127)*16;
    if((dcr&11)!=1 || width<384 || width>stride/2 || acv<112)return 0;
    top=rawTop&0xFFFFFF;bottom=rawBottom&0xFFFFFF;
    if(rawTop&0x10000000){top<<=5;bottom<<=5;}
    if(dcr&4)bottom=top;
    step=top!=bottom?2:1;fields=step;
    if(acv*step<224)return 0;
    x=((width-384)/2)&~1u;y=((acv*step-224)/2)&~1u;
    if(!top || top<0x10000 || (top&3) || top+((y+223)/step)*stride+(x+384)*2>0x01800000 ||
       (fields==2 && (!bottom || bottom<0x10000 || (bottom&3) || bottom+((y+223)/step)*stride+(x+384)*2>0x01800000)))return 0;
    if(v) {
        v->top=top;v->bottom=bottom;v->stride=stride;v->step=step;
        v->fields=fields;v->x=x;v->y=y;v->rawTop=rawTop;v->rawBottom=rawBottom;
        v->dcr=dcr;v->vtr=vi[0];
    }
    return 1;
}
/* Called after all sources are translated, with IRQs disabled. Shared state
 * never contains narrow stores; the renderer sees one complete transaction. */
void menu_input(OverlayPad *pads,u32 used) {
    u32 p,i,justOpened=0,toggle,held[4],pressed,btn,dir,stick;
    u16 raw[4];
    if(state->magic!=OVL_MAGIC || !state->enabled)return;
    state->inputCalls++;state->connected=used;
    for(p=0;p<4;p++) {
        raw[p]=pads[p].button;
        held[p]=raw[p]|stickdirs(&pads[p]);
        state->live[p][0]=(used&(1u<<p))?raw[p]:0;
        state->live[p][1]=(used&(1u<<p))?(int)pads[p].stickX:0;
        state->live[p][2]=(used&(1u<<p))?(int)pads[p].stickY:0;
        state->live[p][3]=(used&(1u<<p))?(int)pads[p].substickX:0;
        state->live[p][4]=(used&(1u<<p))?(int)pads[p].substickY:0;
        state->live[p][5]=(used&(1u<<p))?pads[p].triggerLeft:0;
        state->live[p][6]=(used&(1u<<p))?pads[p].triggerRight:0;
    }
    /* A mode change must not leave an invisible editor eating input. */
    if(state->open && !video_layout(0))close_menu(0);
    if(!state->open && !state->release) for(p=0;p<4;p++) {
        if((used&(1u<<p)) && (raw[p]&TOGGLE)==TOGGLE && (state->previous[p]&TOGGLE)!=TOGGLE && video_layout(0)) {
            for(i=0;i<80;i++)((volatile u32*)state->edit)[i]=((volatile u32*)state->active)[i];
            state->open=1;state->owner=p;state->port=p;state->row=0;state->cursor=0;
            state->mode=PICTURE;state->waitNeutral=1;state->lastDetected=0;
            state->confirm=0;state->notice=0;state->candidate=0;
            justOpened=1;break;
        }
    }
    if(state->open) {
        p=state->owner&3;
        if(!(used&(1u<<p))) {
            close_menu(0);
            for(p=0;p<4;p++)if(used&(1u<<p)){state->owner=p;break;}
        } else if(!justOpened) {
            pressed=held[p]&~state->previous[p];
            btn=pressed&BUTTONS_MASK;
            stick=(pressed>>16)&15;
            dir=(btn|stick)&15;
            toggle=(raw[p]&TOGGLE)==TOGGLE && (state->previous[p]&TOGGLE)!=TOGGLE;
            if(toggle)close_menu(1);
            else if(state->mode==PICTURE)picture_input(raw[p],raw[state->port&3],stick);
            else if(state->mode==TEST)test_input(raw[p],raw[state->port&3]);
            else if(state->waitNeutral) {if(!(raw[p]&BUTTONS_MASK))state->waitNeutral=0;}
            else if(btn || dir) {
                state->notice=0;
                if(state->mode==STICKS)sticks_input(btn,dir);
                else if(state->mode==QUIT)quit_input(btn,dir);
                else list_input(btn,dir);
            }
        }
    }
    for(p=0;p<4;p++)state->previous[p]=held[p];
    if(state->open || state->release || justOpened) {
        for(p=0;p<4;p++)neutral(&pads[p]);
        if(!state->open && (!(used&(1u<<(state->owner&3))) ||
                           !(raw[state->owner&3]&BUTTONS_MASK)))state->release=0;
    } else for(p=0;p<4;p++)if(used&(1u<<p))remap(&pads[p],state->active[p]);
    __asm__ volatile("sync":::"memory");
}
/* Original 5x7 bitmap alphabet, stored as five columns (LSB at top). */
static const u8 letters[26][5]={
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {7,8,112,8,7},{97,81,73,69,67}};
static const u8 digits[10][5]={{62,65,65,65,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},{39,69,69,69,57},{62,73,73,73,50},{1,113,9,5,3},{54,73,73,73,54},{38,73,73,73,62}};
static u8 column(char c,u32 col) {
    if(c>='A'&&c<='Z')return letters[c-'A'][col];
    if(c>='0'&&c<='9')return digits[c-'0'][col];
    if(c=='-')return 8;
    if(c=='+')return col==2?62:8;
    if(c=='<')return (u8[]){8,20,34,65,0}[col];
    if(c=='>')return (u8[]){0,65,34,20,8}[col];
    if(c==':')return col==2?36:0;
    if(c=='/')return (u8[]){64,32,16,8,4}[col];
    if(c=='?')return (u8[]){2,1,81,9,6}[col];
    if(c=='.')return col==2?64:0;
    return 0;
}
static void put(char *s,u32 at,const char *v) {while(*v && at<30)s[at++]=*v++;}
static void line(char *s,const char *v) {u32 i;for(i=0;i<30;i++)s[i]=' ';s[30]=0;put(s,0,v);}
static void fill(u32 base,u32 stride,u32 step,u32 parity,u32 x,u32 y,u32 w,u32 h,u32 color) {
    u32 yy,xx;for(yy=y;yy<y+h;yy++)if(yy%step==parity) {
        volatile u32 *dst=(volatile u32*)(base+(yy/step)*stride+x*2);
        for(xx=0;xx<w;xx+=2)*dst++=color;
    }
}
static void textrow(u32 base,u32 stride,u32 step,u32 parity,u32 x,u32 y,const char *s,u32 color) {
    u32 c,xx,yy,b;for(c=0;s[c] && c<30;c++)for(xx=0;xx<5;xx++) {
        b=column(s[c],xx);
        for(yy=0;yy<14;yy++)if((b&(1u<<(yy/2))) && (y+yy)%step==parity)
            *(volatile u32*)(base+((y+yy)/step)*stride+(x+c*12+xx*2)*2)=color;
    }
}
/* Drawing is clipped to the SAME validated 384x224 rectangle as M18-M20.
 * Context and text live in cached module .bss, not the game's interrupt stack. */
static struct {u32 base,stride,step,parity,x,y;} canvas;
static char text[3][31];
static char buf[31];
#define BG 0x20802080u
#define WHITE 0xEB80EB80u
#define DARK 0x30803080u
#define BODY 0x53985394u
#define GREY 0xA080A080u
#define SELECT 0xD020D094u
#define LIVE 0xAC54AC32u
static void box(int x,int y,int w,int h,u32 color) {
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(x+w>384)w=384-x;
    if(y+h>224)h=224-y;
    if(w<=0 || h<=0)return;
    x&=~1;w&=~1;
    fill(canvas.base,canvas.stride,canvas.step,canvas.parity,canvas.x+x,canvas.y+y,w,h,color);
}
static void oval(int cx,int cy,int rx,int ry,u32 color) {
    int yy,xx,limit=rx*rx*ry*ry;
    for(yy=-ry;yy<=ry;yy++) {
        for(xx=rx;xx>0 && xx*xx*ry*ry+yy*yy*rx*rx>limit;xx--);
        box(cx-xx,cy+yy,xx*2+2,1,color);
    }
}
static void label(u32 x,u32 y,const char *s,u32 color) {
    textrow(canvas.base,canvas.stride,canvas.step,canvas.parity,canvas.x+x,canvas.y+y,s,color);
}
static void blank(void) {line(buf,"");}
static void hint(u32 y,const char *s,u32 color) {blank();put(buf,0,s);label(12,y,buf,color);}
/* One list row: a full-width bar plus a '>' marker, so focus never relies on colour. */
static void row(u32 y,u32 selected) {
    if(selected) {box(6,y-4,372,22,SELECT);buf[0]='>';}
    label(12,y,buf,selected?DARK:WHITE);
}
static void title(const char *s,u32 showPort) {
    blank();put(buf,0,s);
    if(showPort) {
        if(!(state->connected&(1u<<(state->port&3))))put(buf,17,"NO PAD");
        put(buf,24,"PORT ");buf[29]='1'+(state->port&3);
    }
    label(12,6,buf,WHITE);
}
static void notice_line(u32 y) {
    u32 n=state->notice;
    blank();
    if(n==NOTICE_RESET) {put(buf,0,"PORT 1 IS BACK TO DEFAULT");buf[5]='1'+(state->port&3);}
    else if(n==NOTICE_TWO)put(buf,0,"ONE BUTTON AT A TIME");
    else if(n==NOTICE_NOPAD) {put(buf,0,"NO CONTROLLER ON PORT 1");buf[22]='1'+(state->port&3);}
    else if(n>=NOTICE_SWAP && n<NOTICE_SWAP+12) {put(buf,0,"SWAPPED WITH GAME ");put(buf,18,names[n-NOTICE_SWAP]);}
    else return;
    label(12,y,buf,SELECT);
}
static const u8 bx[12]={135,119,151,135,160,46,144,95,70,70,64,76}; /* x/2 */
static const u8 by[12]={83,97,77,53,53,35,35,74,101,125,113,113};
static const u8 brx[12]={17,10,9,17,16,25,25,9,6,6,6,6};
static const u8 bry[12]={17,10,16,8,8,8,8,9,6,6,6,6};
/* Original controller artwork: integer primitives, no external asset. */
static void controller(u32 highlight) {
    u32 i,port=state->port&3,raw=state->live[port][0];
    u32 target=highlight?ring[state->cursor<12?state->cursor:0]:12;
    oval(192,78,132,37,GREY);
    oval(94,106,38,32,GREY);oval(290,106,38,32,GREY);
    oval(192,78,130,35,BODY);
    oval(94,106,36,30,BODY);oval(290,106,36,30,BODY);
    box(100,77,186,39,BODY);
    oval(140,117,26,23,BODY);oval(224,117,32,26,BODY);
    oval(184,145,24,19,BG);
    oval(100,75,25,24,DARK);oval(100,75,21,20,GREY);
    oval(224,121,19,18,DARK);oval(224,121,15,14,SELECT);
    oval(100+(int)state->live[port][1]/8,75-(int)state->live[port][2]/8,7,7,WHITE);
    oval(224+(int)state->live[port][3]/12,121-(int)state->live[port][4]/12,5,5,WHITE);
    box(122,107,36,12,DARK);box(134,95,12,36,DARK);
    for(i=0;i<12;i++) {
        u32 sel=i==target;
        u32 color=(raw&bits[i])?LIVE:(sel?SELECT:GREY);
        u32 rim=sel?WHITE:DARK;
        int x=bx[i]*2,y=by[i];
        if(i>=8) {box(x-9,y-9,18,18,rim);box(x-6,y-6,12,12,color);}
        else {
            oval(x,y,brx[i]+3,bry[i]+3,rim);oval(x,y,brx[i],bry[i],color);
            label(x-4,y-6,i==7?"S":names[i],DARK);
        }
    }
    if(!highlight) {
        box(68,23,50,3,DARK);box(264,23,50,3,DARK);
        box(68,23,state->live[port][5]*50/255,3,LIVE);
        box(264,23,state->live[port][6]*50/255,3,LIVE);
    }
}
static void number(char *s,u32 at,int value,int sign) {
    if(sign){s[at++]=value<0?'-':'+';if(value<0)value=-value;}
    s[at++]='0'+(value/100)%10;s[at++]='0'+(value/10)%10;s[at]='0'+value%10;
}
static void picture_screen(void) {
    u32 target=ring[state->cursor<12?state->cursor:0];
    u32 source=state->edit[state->port&3][target];
    u32 n=0;
    controller(1);
    title("CHANGE BUTTONS",1);
    while(names[target][n])n++;
    blank();put(buf,0,"GAME ");put(buf,5,names[target]);
    put(buf,6+n,"USES INPUT ");put(buf,17+n,names[source<12?source:12]);
    label(12,144,buf,SELECT);
    if(state->notice)notice_line(163);
    else hint(163,"PRESS ANY BUTTON TO CHANGE",WHITE);
    hint(182,"STICK LEFT/RIGHT TO MOVE",GREY);
    hint(201,"STICK DOWN FOR MORE OPTIONS",GREY);
}
static void list_screen(void) {
    static const char *const items[LIST_ITEMS]={"CHANGE BUTTONS","TEST BUTTONS","STICK SETTINGS","CONTROLLER PORT","RESET TO DEFAULT","BACK TO GAME","QUIT GAME"};
    u32 i;
    title("CONTROLLER MENU",0);box(6,24,372,2,GREY);
    for(i=0;i<LIST_ITEMS;i++) {
        blank();put(buf,2,i==L_RESET && state->confirm?"PRESS A AGAIN TO RESET":items[i]);
        if(i==L_PORT) {
            put(buf,19,"< 1 >");buf[21]='1'+(state->port&3);
            if(!(state->connected&(1u<<(state->port&3))))put(buf,25,"EMPTY");
        }
        row(32+i*22,i==state->row);
    }
    if(state->notice)notice_line(190);
    else if(state->row==L_PORT)hint(190,"LEFT/RIGHT PICKS THE PORT",GREY);
    else if(state->row==L_RESET)hint(190,state->confirm?"RESETS BUTTONS AND STICKS":"UNDO ALL CHANGES ON THIS PORT",GREY);
    else hint(190,"UP/DOWN MOVES THE HIGHLIGHT",GREY);
    hint(208,"A SELECT   B BACK TO BUTTONS",WHITE);
}
static void test_screen(void) {
    u32 i,at=6,port=state->port&3;
    controller(0);
    title("TEST BUTTONS",1);
    blank();put(buf,0,"LAST:");
    if(!state->lastDetected)put(buf,6,"PRESS ANY BUTTON");
    else for(i=0;i<12;i++)if(state->lastDetected&bits[i]) {
        const char *n=names[i];
        while(*n && at<30)buf[at++]=*n++;
        at++;
    }
    label(12,144,buf,SELECT);
    line(buf,"MAIN      /      C      /     ");
    number(buf,5,(int)state->live[port][1],1);number(buf,10,(int)state->live[port][2],1);
    number(buf,18,(int)state->live[port][3],1);number(buf,23,(int)state->live[port][4],1);
    label(12,163,buf,WHITE);
    line(buf,"L     R      GREEN IS HELD");
    number(buf,2,state->live[port][5],0);number(buf,8,state->live[port][6],0);
    label(12,182,buf,WHITE);
    if(state->confirm) {
        u32 elapsed=state->drawCalls-state->timer;
        hint(201,"KEEP HOLDING B",SELECT);
        box(192,205,elapsed>=HOLD_TICKS?180:elapsed*180/HOLD_TICKS,6,SELECT);
    } else hint(201,"HOLD B TO GO BACK",WHITE);
}
static void sticks_screen(void) {
    static const char *const items[5]={"SWAP MAIN AND C STICK","FLIP MAIN LEFT/RIGHT","FLIP MAIN UP/DOWN","FLIP C LEFT/RIGHT","FLIP C UP/DOWN"};
    u32 i,port=state->port&3;
    int mx=(int)state->live[port][1],my=(int)state->live[port][2],cx=(int)state->live[port][3],cy=(int)state->live[port][4],t;
    title("STICK SETTINGS",1);box(6,24,372,2,GREY);
    for(i=0;i<5;i++) {
        blank();put(buf,2,items[i]);put(buf,26,state->edit[port][12+i]?"ON":"OFF");
        row(34+i*22,i==state->action);
    }
    /* Live preview of what the game will receive with these settings. */
    if(state->edit[port][12]) {t=mx;mx=cx;cx=t;t=my;my=cy;cy=t;}
    if(state->edit[port][13])mx=-mx;
    if(state->edit[port][14])my=-my;
    if(state->edit[port][15])cx=-cx;
    if(state->edit[port][16])cy=-cy;
    label(12,158,"GAME SEES",GREY);
    label(140,158,"MAIN",WHITE);box(196,146,36,36,DARK);box(212+mx/8,162-my/8,4,4,WHITE);
    label(260,158,"C",WHITE);box(292,146,36,36,DARK);box(308+cx/8,162-cy/8,4,4,SELECT);
    hint(208,"A TURN ON/OFF   B BACK",WHITE);
}
static void quit_screen(void) {
    title("QUIT GAME?",0);box(6,24,372,2,GREY);
    hint(40,"UNSAVED GAME PROGRESS IS LOST",SELECT);
    hint(62,"SAVE BUTTON SETTINGS ON EXIT",WHITE);
    blank();put(buf,2,"NO - GO BACK");row(100,state->action!=1);
    blank();put(buf,2,"YES - QUIT GAME");row(126,state->action==1);
    hint(208,"A SELECT   B BACK",WHITE);
}
static void menu_panel(void) {
    u32 mode=state->mode;
    if(mode==LIST)list_screen();
    else if(mode==TEST)test_screen();
    else if(mode==STICKS)sticks_screen();
    else if(mode==QUIT)quit_screen();
    else picture_screen();
}
void menu_draw(void) {
    VideoLayout v;
    volatile u32 *diag=(volatile u32*)0xD30030A0;
    u32 top,bottom,stride,step,fields,f,x,y,i;
    diag[0]++;
    if(state->magic!=OVL_MAGIC || !state->enabled)return;
    state->drawCalls++;
    if(!state->open && !state->toast)return;
    if(!video_layout(&v)){diag[2]++;return;}
    top=v.top;bottom=v.bottom;stride=v.stride;step=v.step;fields=v.fields;x=v.x;y=v.y;
    diag[3]=v.rawTop;diag[4]=v.rawBottom;diag[5]=stride;
    diag[6]=top;diag[7]=bottom;diag[8]=(v.vtr<<16)|v.dcr;
    if(!state->open) {
        line(text[0],state->dirty?"BUTTON SETTINGS ON":"MENU CLOSED");
        line(text[1],"X+Y+START OPENS THE MENU");
        line(text[2],state->dirty?"QUIT GAME TO SAVE SETTINGS":"");
        state->toast--;
    }
    for(f=0;f<fields;f++) {
        u32 base=(f?bottom:top)|0xC0000000;
        fill(base,stride,step,f,x,y,384,state->open?224:62,BG);
        if(state->open) {
            canvas.base=base;canvas.stride=stride;canvas.step=step;canvas.parity=f;canvas.x=x;canvas.y=y;
            menu_panel();
        } else for(i=0;i<3;i++)textrow(base,stride,step,f,x+12,y+6+i*18,text[i],WHITE);
        diag[1]++;
    }
    __asm__ volatile("sync":::"memory");
}
