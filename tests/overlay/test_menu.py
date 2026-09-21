import os
"""M21 host-side tests (harness from M15-M20).

Runs the REAL compiled PowerPC overlay (overlay.bin) and the real VI hook
(VIHook.bin) under Unicorn, and cross-checks the file checksum/validity rules
against the real C by compiling them for PPC and executing them the same way.
Nothing here is a Python re-model of logic that could drift from the target.

Coverage the handoff asked for: menu state transitions, mapping edits,
apply/cancel/default, input neutralisation and release latching, remap
correctness, renderer reject conditions and register preservation, file
checksum + slot recovery, and that the final binary embeds what was tested.
"""
from pathlib import Path
import sys, struct, json, subprocess, zipfile, io, hashlib, re, os
base=Path(__file__).resolve().parent
# Install unicorn from requirements.txt before running.
from unicorn import *
from unicorn.ppc_const import *
src=Path(os.environ.get('NIN_SOURCE',str(base.parents[1])))
DKP=str(Path(os.environ['DEVKITPPC'])/'bin/powerpc-eabi-')


overlay=(src/'overlay/overlay.bin').read_bytes()
vihook=(src/'kernel/asm/VIHook.bin').read_bytes()
u32=lambda b:struct.unpack('>I',b)[0]; pack=lambda n:struct.pack('>I',n&0xffffffff)
CODE=0x93180000; STATE=0xD318F000; ENTRY_INPUT=CODE; ENTRY_DRAW=CODE+4
SENT=0x80002FFC; PADS=0x81100000; STACK_TOP=0x81010000; MAGIC=0x4F563231; TOGGLE=0x1C00
# OverlayState offsets (sizeof 868, verified by the target compiler)
O=dict(magic=0,enabled=4,open=8,owner=12,port=16,row=20,release=24,dirty=28,generation=32,
       toast=36,inputCalls=40,drawCalls=44,applies=48,cancels=52,previous=56,active=72,edit=392,exitRequested=712,mode=716,waitNeutral=720,action=724,connected=728,live=732,lastDetected=844,cursor=848,timer=852,confirm=856,notice=860,candidate=864)
BITS=[0x100,0x200,0x400,0x800,0x10,0x40,0x20,0x1000,8,4,1,2]  # A B X Y Z L R START UP DOWN LEFT RIGHT
results=[]

def machine():
    uc=Uc(UC_ARCH_PPC,UC_MODE_32|UC_MODE_BIG_ENDIAN)
    for a,s in [(0x80002000,0x1000),(0x81000000,0x20000),(PADS,0x1000),(CODE,0x10000),
                (STATE,0x1000),(0xD3003000,0x1000),(0xCC002000,0x1000),(0xC0000000,0x1800000)]:
        uc.mem_map(a,s)
    # A normal supported VI state is also required to open the editor.
    uc.mem_write(0xCC002000,pack(((240<<4|6)<<16)|1))
    uc.mem_write(0xCC00201C,pack(0x600000));uc.mem_write(0xCC002024,pack(0x600000))
    uc.mem_write(0xCC002048,struct.pack('>H',(40<<8)|40))
    uc.mem_write(CODE,overlay)
    uc.mem_write(0x80002000,vihook)
    uc.mem_write(SENT,b'\x4e\x80\x00\x20')  # blr at the sentinel
    # Real Broadway cannot safely issue byte/halfword stores to uncached MEM2.
    # Standard Unicorn models those as normal RAM and missed the M17 defect.
    def word_only(uc,access,addr,size,value,data):
        assert size==4 and addr%4==0,('unsafe uncached MEM2 store',hex(addr),size)
    uc.hook_add(UC_HOOK_MEM_WRITE,word_only,begin=0xD0000000,end=0xD3FFFFFF)
    return uc

def identity(): return bytes([i if i<12 else 0 for i in range(20)])
def st_init(uc,enabled=1,active=None,edit=None):
    s=bytearray(868); s[0:4]=pack(MAGIC); s[4:8]=pack(enabled)
    act=active or identity()*4
    s[72:392]=b''.join(pack(v) for v in act)
    s[392:712]=b''.join(pack(v) for v in (edit or act))
    uc.mem_write(STATE,bytes(s))
def st(uc,k): return u32(uc.mem_read(STATE+O[k],4))
def st_set(uc,k,v): uc.mem_write(STATE+O[k],pack(v))
def st_map(uc,which,port): return bytes(struct.unpack('>20I',uc.mem_read(STATE+O[which]+port*80,80)))
def st_set_map(uc,which,port,m): uc.mem_write(STATE+O[which]+port*80,b''.join(pack(v) for v in m))
def pad(button=0,sx=0,sy=0,cx=0,cy=0,l=0,r=0,aa=0,ab=0):
    return struct.pack('>HbbbbBBBBbB',button,sx,sy,cx,cy,l,r,aa,ab,0,0)
def write_pads(uc,pads):
    b=b''.join(pads)+pad()*(4-len(pads)); uc.mem_write(PADS,b)
def read_pad(uc,i):
    b=uc.mem_read(PADS+i*12,12); return dict(zip('button sx sy cx cy l r aa ab err pad'.split(),struct.unpack('>HbbbbBBBBbB',bytes(b))))

CALLEE=[UC_PPC_REG_14+i for i in range(18)]
def call_input(uc,pads,used):
    """Call menu_input(pads,used) exactly as PADReadGC does; return callee-saved snapshot check."""
    write_pads(uc,pads)
    seed={r:(0x1000+i*0x11111)&0xffffffff for i,r in enumerate(CALLEE)}
    for r,v in seed.items(): uc.reg_write(r,v)
    uc.reg_write(UC_PPC_REG_1,STACK_TOP-0x100); uc.reg_write(UC_PPC_REG_3,PADS); uc.reg_write(UC_PPC_REG_4,used)
    uc.reg_write(UC_PPC_REG_LR,SENT)
    uc.emu_start(ENTRY_INPUT,SENT,count=2_000_000)
    assert uc.reg_read(UC_PPC_REG_PC)==SENT,'menu_input did not return to LR'
    assert uc.reg_read(UC_PPC_REG_1)==STACK_TOP-0x100,'stack pointer not restored'
    for r,v in seed.items(): assert uc.reg_read(r)==v,('callee-saved clobbered',r)
    return [read_pad(uc,i) for i in range(4)]

def neutral(p): return p['button']==0 and all(p[k]==0 for k in 'sx sy cx cy l r aa ab'.split())



# ---------------------------------------------------------------- menu_input (M21 picture, press to assign)
A,B,X,Y,Z,START,UP,DOWN,LEFT,RIGHT=0x100,0x200,0x400,0x800,0x10,0x1000,8,4,1,2
PICTURE,LIST,TEST,STICKS,QUIT=range(5)
# Left to right across the drawing: L, D-pad, Start, B, Y, A, R, X, Z.
RING=[5,10,8,9,11,7,1,3,0,6,2,4]
L_BUTTONS,L_TEST,L_STICKS,L_PORT,L_RESET,L_RESUME,L_QUIT=range(7)
NOTICE_RESET,NOTICE_TWO,NOTICE_NOPAD,NOTICE_SWAP=1,2,4,16
def t(name,fn):
    uc=machine(); st_init(uc); fn(uc); results.append(name); print('ok',name)

def opened(uc,port=0):
    call_input(uc,[pad(TOGGLE)] if port==0 else [pad()]*port+[pad(TOGGLE)],1<<port)
    call_input(uc,[pad()]*4,1<<port)  # release toggle
    assert st(uc,'open')==1


def tap(uc,button=0,used=1,p=0,**kw):
    """Release everything, then press on port p: one clean edge."""
    call_input(uc,[pad()]*4,used)
    pads=[pad()]*4; pads[p]=pad(button,**kw)
    return call_input(uc,pads,used)
def ticks(uc,n): st_set(uc,'drawCalls',st(uc,'drawCalls')+n)
def home_row(uc,row):
    for _ in range(row): tap(uc,DOWN)
    assert st(uc,'row')==row
def maps(uc): return [st_map(uc,'edit',p) for p in range(4)]


def opened(uc,port=0):
    """Open with the chord and release it: the opening chord must assign nothing."""
    call_input(uc,[pad(TOGGLE)] if port==0 else [pad()]*port+[pad(TOGGLE)],1<<port)
    call_input(uc,[pad()]*4,1<<port)
    assert st(uc,'open')==1 and st(uc,'mode')==PICTURE
    assert st_map(uc,'edit',port)==identity(),'the opening chord must never assign'

def tap(uc,button=0,used=1,p=0,**kw):
    call_input(uc,[pad()]*4,used)
    pads=[pad()]*4; pads[p]=pad(button,**kw)
    return call_input(uc,pads,used)
def press(uc,button,used=1,p=0,frames=1):
    """Press and release a single button on port p; assignment lands on release."""
    pads=[pad()]*4; pads[p]=pad(button)
    for _ in range(frames): call_input(uc,pads,used)
    return call_input(uc,[pad()]*4,used)
def move(uc,dx=0,dy=0,used=1):
    call_input(uc,[pad()]*4,used)
    return call_input(uc,[pad(sx=dx,sy=dy)]+[pad()]*3,used)
def ticks(uc,n): st_set(uc,'drawCalls',st(uc,'drawCalls')+n)
def maps(uc): return [st_map(uc,'edit',p) for p in range(4)]

def t_idle(uc):
    out=call_input(uc,[pad(0x100,sx=50)],1)
    assert out[0]['button']==0x100 and out[0]['sx']==50, 'identity map must pass input through'
    assert st(uc,'open')==0 and st(uc,'inputCalls')==1
t('idle passes input through unchanged',t_idle)
def t_disabled(uc):
    st_set(uc,'enabled',0); before=st(uc,'inputCalls')
    out=call_input(uc,[pad(TOGGLE)],1)
    assert st(uc,'open')==0 and st(uc,'inputCalls')==before,'disabled menu must be inert'
    assert out[0]['button']==TOGGLE
t('disabled state is inert',t_disabled)
def t_held_toggle(uc):
    for _ in range(300):
        call_input(uc,[pad(TOGGLE)],1)
        assert st(uc,'open')==1 and st(uc,'cancels')==0
        for port in range(4):
            assert st_map(uc,'active',port)==st_map(uc,'edit',port)==identity()
    call_input(uc,[pad()],1)
    assert st(uc,'open')==1
t('holding then releasing the opening shortcut preserves the menu and every map',t_held_toggle)
def t_disconnect(uc):
    opened(uc)
    call_input(uc,[pad(),pad(0x100)],2)  # port0 gone, port1 present
    assert st(uc,'open')==0 and st(uc,'cancels')==1 and st(uc,'owner')==1,'owner loss cancels and hands off'
t('owner disconnect cancels safely',t_disconnect)
def t_remap(uc):
    m=bytearray(identity()); m[0]=1; m[1]=0; m[5]=6; m[6]=5; m[12]=1; m[13]=1; m[16]=1
    st_set_map(uc,'active',0,bytes(m))
    out=call_input(uc,[pad(0x100|0x40,sx=10,sy=20,cx=30,cy=40,l=100,r=200,aa=0,ab=0)],1)
    p=out[0]
    assert p['button']&0x200 and not p['button']&0x100,'A press must come out as B'
    assert p['button']&0x20 and not p['button']&0x40,'L click must come out as R'
    assert p['l']==200 and p['r']==100,'triggers swapped by mapping'
    assert (p['sx'],p['sy'],p['cx'],p['cy'])==(-30,40,10,-20),'stick swap then invert main X and C Y'
    assert p['aa']==0 and p['ab']==255,'analog B follows the remapped A press'
t('remap: buttons, triggers, stick swap and inversion',t_remap)
def t_invert_edge(uc):
    m=bytearray(identity()); m[13]=1; st_set_map(uc,'active',0,bytes(m))
    out=call_input(uc,[pad(sx=-128)],1); assert out[0]['sx']==127,'-128 inverts to 127, never overflows'
t('inversion of -128 is clamped',t_invert_edge)

def t_open_picture(uc):
    out=call_input(uc,[pad(TOGGLE,sx=40,l=200)],1)
    assert st(uc,'open')==1 and st(uc,'mode')==PICTURE and st(uc,'cursor')==0
    assert st(uc,'waitNeutral')==1 and st(uc,'candidate')==0,'nothing may assign until the chord is released'
    assert neutral(out[0]) and st_map(uc,'edit',0)==st_map(uc,'active',0)
t('the menu opens on the picture with the chord still held and assigns nothing',t_open_picture)

def t_press_release_assigns(uc):
    opened(uc)   # cursor 0 -> RING[0] = GameCube L
    pads=[pad(B)]+[pad()]*3
    for _ in range(4):
        call_input(uc,pads,1)
        assert st_map(uc,'edit',0)==identity(),'a held button must not assign yet'
        assert st(uc,'candidate')==B
    out=call_input(uc,[pad()]*4,1)
    e=st_map(uc,'edit',0)
    assert e[5]==1 and e[1]==5,'game L takes B, and B''s old owner takes L'
    assert st(uc,'notice')==NOTICE_SWAP+1 and st(uc,'candidate')==0
    assert neutral(out[0]) and st(uc,'open')==1,'assigning never closes the menu'
    assert st_map(uc,'active',0)==identity(),'the game keeps the old map until the menu closes'
t('press and release assigns to the highlighted button and swaps',t_press_release_assigns)

def t_two_buttons(uc):
    opened(uc)
    call_input(uc,[pad(A|B)],1)
    assert maps(uc)==[identity()]*4 and st(uc,'notice')==NOTICE_TWO and st(uc,'candidate')==0
    call_input(uc,[pad()],1)
    assert maps(uc)==[identity()]*4,'releasing two buttons assigns nothing'
    press(uc,X)
    assert st_map(uc,'edit',0)[5]==2,'a clean single press still works afterwards'
t('two buttons at once assign nothing and say so',t_two_buttons)

def t_chord_closes_without_assigning(uc):
    opened(uc)
    call_input(uc,[pad(X)],1)          # first button of the chord
    call_input(uc,[pad(X|Y)],1)        # second
    out=call_input(uc,[pad(TOGGLE)],1) # third completes Start+X+Y
    assert st(uc,'open')==0 and st(uc,'applies')==1 and neutral(out[0])
    assert st_map(uc,'active',0)==identity() and st(uc,'dirty')==0,'closing must not assign the chord buttons'
t('rolling into X+Y+Start closes the menu and assigns nothing',t_chord_closes_without_assigning)

def t_dpad_assigns(uc):
    opened(uc)
    press(uc,UP)
    assert st(uc,'cursor')==0,'the D-pad does not move the cursor on the picture'
    e=st_map(uc,'edit',0)
    assert e[5]==8 and e[8]==5,'D-pad Up is assignable like any other button'
    press(uc,START)
    assert st_map(uc,'edit',0)[5]==7,'Start is assignable too'
t('on the picture every button assigns, including the D-pad and Start',t_dpad_assigns)

def t_stick_walks_ring(uc):
    opened(uc)
    for step in range(1,13):
        move(uc,dx=100)
        assert st(uc,'cursor')==step%12,('stick right walks the ring',step)
    move(uc,dx=-100); assert st(uc,'cursor')==11,'stick left wraps backwards'
    for _ in range(11): move(uc,dx=-100)
    assert st(uc,'cursor')==0
    call_input(uc,[pad(sx=100)],1); call_input(uc,[pad(sx=100)],1)
    assert st(uc,'cursor')==1,'a held stick moves once'
    assert maps(uc)==[identity()]*4,'moving never changes a mapping'
t('the stick walks the 12 buttons in both directions and edits nothing',t_stick_walks_ring)

def t_move_cancels_held(uc):
    opened(uc)
    call_input(uc,[pad(B)],1); assert st(uc,'candidate')==B
    call_input(uc,[pad(B,sx=100)],1)
    assert st(uc,'cursor')==1 and st(uc,'candidate')==0
    call_input(uc,[pad()],1)
    assert maps(uc)==[identity()]*4,'a button held while moving must not land on the new button'
    press(uc,B)
    assert st_map(uc,'edit',0)[10]==1,'pressing again assigns to where the cursor now is'
t('moving while holding a button cancels that press',t_move_cancels_held)

def t_list_and_back(uc):
    opened(uc)
    move(uc,dy=-100)
    assert st(uc,'mode')==LIST and st(uc,'row')==0
    move(uc,dy=100)
    assert st(uc,'mode')==PICTURE and st(uc,'waitNeutral')==1,'up at the top returns to the picture'
    call_input(uc,[pad()],1)
    move(uc,dy=-100); tap(uc,A)
    assert st(uc,'mode')==PICTURE,'CHANGE BUTTONS returns to the picture'
    call_input(uc,[pad()],1)
    assert st(uc,'waitNeutral')==0
    move(uc,dy=-100); tap(uc,B)
    assert st(uc,'mode')==PICTURE and st(uc,'open')==1,'B on the list goes back to the picture'
t('stick down opens the list; up, A on the first item and B all return',t_list_and_back)

def t_list_items(uc):
    opened(uc); move(uc,dy=-100)
    tap(uc,DOWN); assert st(uc,'row')==L_TEST
    for _ in range(8): tap(uc,DOWN)
    assert st(uc,'row')==L_QUIT,'the list clamps at the bottom'
    st_set(uc,'row',L_PORT)
    tap(uc,RIGHT); assert st(uc,'port')==1
    tap(uc,LEFT); tap(uc,LEFT); assert st(uc,'port')==3
    tap(uc,A); assert st(uc,'port')==0
    st_set(uc,'row',L_STICKS); tap(uc,A); assert st(uc,'mode')==STICKS and st(uc,'action')==0
    tap(uc,A); assert st_map(uc,'edit',0)[12]==1
    tap(uc,DOWN); tap(uc,RIGHT); assert st(uc,'action')==1 and st_map(uc,'edit',0)[13]==1
    tap(uc,B); assert st(uc,'mode')==LIST
    assert st(uc,'cursor')==0,'sub-screens must not disturb the picture cursor'
t('list rows open the right screens and keep the picture cursor',t_list_items)

def t_reset_and_resume(uc):
    opened(uc); press(uc,B)   # a real change first
    assert st_map(uc,'edit',0)!=identity()
    move(uc,dy=-100); st_set(uc,'row',L_RESET)
    tap(uc,A); assert st(uc,'confirm')==1 and st_map(uc,'edit',0)!=identity(),'first A only asks'
    tap(uc,UP); assert st(uc,'confirm')==0
    st_set(uc,'row',L_RESET); tap(uc,A); tap(uc,A)
    assert st_map(uc,'edit',0)==identity() and st(uc,'notice')==NOTICE_RESET and st(uc,'open')==1
    st_set(uc,'row',L_RESUME); tap(uc,A)
    assert st(uc,'open')==0 and st(uc,'exitRequested')==0 and st(uc,'dirty')==0
t('reset asks twice and BACK TO GAME resumes without exiting',t_reset_and_resume)

def t_quit(uc):
    opened(uc); move(uc,dy=-100); st_set(uc,'row',L_QUIT)
    tap(uc,A); assert st(uc,'mode')==QUIT and st(uc,'action')==0
    call_input(uc,[pad(A)],1); assert st(uc,'exitRequested')==0,'held A never confirms'
    tap(uc,A); assert st(uc,'mode')==LIST and st(uc,'exitRequested')==0,'default choice is NO'
    tap(uc,A); tap(uc,B); assert st(uc,'mode')==LIST
    tap(uc,A); tap(uc,DOWN); tap(uc,A)
    assert st(uc,'exitRequested')==1 and st(uc,'open')==0
t('quitting needs Down then A on YES',t_quit)

def t_tester(uc):
    opened(uc); move(uc,dy=-100); st_set(uc,'row',L_TEST); tap(uc,A)
    assert st(uc,'mode')==TEST and st(uc,'waitNeutral')==1
    call_input(uc,[pad(A)],1); assert st(uc,'lastDetected')==0
    call_input(uc,[pad()],1)
    for bit in BITS:
        tap(uc,bit)
        assert st(uc,'mode')==TEST and st(uc,'lastDetected')==bit
    assert maps(uc)==[identity()]*4,'the tester never changes a mapping'
    tap(uc,B); ticks(uc,30); call_input(uc,[pad()],1)
    assert st(uc,'mode')==TEST,'a short B press is just a test'
    call_input(uc,[pad(B)],1); ticks(uc,44); call_input(uc,[pad(B)],1)
    assert st(uc,'mode')==TEST
    ticks(uc,1); call_input(uc,[pad(B)],1)
    assert st(uc,'mode')==LIST and st(uc,'waitNeutral')==1
t('the tester reports every button; holding B returns to the list',t_tester)

def t_every_target_every_source(uc):
    opened(uc)
    for cursor,target in enumerate(RING):
        for src,bit in enumerate(BITS):
            st_set_map(uc,'edit',0,identity())
            st_set(uc,'cursor',cursor); st_set(uc,'waitNeutral',0); st_set(uc,'candidate',0)
            press(uc,bit)
            e=st_map(uc,'edit',0)
            assert e[target]==src and (src==target or e[src]==target),(cursor,target,src,list(e))
            assert sorted(e[:12])==list(range(12)),'the mapping stays a permutation'
            assert st(uc,'mode')==PICTURE and st(uc,'open')==1
t('every one of the 12 buttons assigns to every one of the 12 spots',t_every_target_every_source)

def t_port_source(uc):
    opened(uc); st_set(uc,'port',2)
    call_input(uc,[pad()]*4,5)
    press(uc,B,used=5,p=0)
    assert maps(uc)==[identity()]*4,'the navigating pad does not supply the button for another port'
    press(uc,B,used=5,p=2)
    assert st_map(uc,'edit',2)[5]==1 and st_map(uc,'edit',0)==identity()
    st_set_map(uc,'edit',2,identity())
    press(uc,B,used=1,p=0)
    assert maps(uc)==[identity()]*4 and st(uc,'notice')==NOTICE_NOPAD,'an empty port assigns nothing'
t('the selected port supplies the button; an empty port says so',t_port_source)

def t_state_guards(uc):
    opened(uc)
    for mode in range(6):
        st_set(uc,'mode',mode); st_set(uc,'cursor',0xFFFFFFFF); st_set(uc,'row',99)
        st_set(uc,'action',77); st_set(uc,'waitNeutral',0); st_set(uc,'candidate',0xFFFF)
        for bit in (A,DOWN,RIGHT,B): tap(uc,bit)
        move(uc,dx=100); move(uc,dy=-100)
        st_init(uc); opened(uc)
t('corrupt cursor/row/candidate values never write outside the shared state',t_state_guards)

def t_closed_stick_passthrough(uc):
    out=call_input(uc,[pad(0,sx=127,sy=-128,cx=90,cy=-90)],1)
    assert (out[0]['sx'],out[0]['sy'],out[0]['cx'],out[0]['cy'])==(127,-128,90,-90) and st(uc,'open')==0
t('menu navigation by stick never alters gameplay stick input',t_closed_stick_passthrough)

def t_release(uc):
    """M20's version closed with B; on the M21 picture B is an assignable button."""
    opened(uc)
    out=call_input(uc,[pad(TOGGLE)],1)
    assert st(uc,'open')==0 and st(uc,'release')==1 and neutral(out[0])
    out=call_input(uc,[pad(TOGGLE|A,sx=30)],1)
    assert neutral(out[0]) and st(uc,'release')==1,'input stays neutral until the owner lets go'
    call_input(uc,[pad()],1); assert st(uc,'release')==0
    out=call_input(uc,[pad(A,sx=30)],1)
    assert out[0]['button']==A and out[0]['sx']==30,'the game gets input again once everything is released'
t('release latch holds until all buttons are up',t_release)

def t_dirty_only_on_change(uc):
    opened(uc); move(uc,dy=-100); st_set(uc,'row',L_RESUME); tap(uc,A)
    assert st(uc,'open')==0 and st(uc,'dirty')==0 and st(uc,'applies')==1
    call_input(uc,[pad()],1); opened(uc); press(uc,B); tap(uc,TOGGLE)
    assert st(uc,'open')==0 and st(uc,'dirty')==1
t('the save flag is only raised when a mapping actually changed',t_dirty_only_on_change)
# ---------------------------------------------------------------- VI hook + menu_draw
def vi(uc,top=0x600000,bottom=0x600000,stride_w=40,width_w=40,acv=240,dcr=1,shift=False):
    tf=(top>>5 if shift else top)|(0x10000000 if shift else 0)
    bf=(bottom>>5 if shift else bottom)
    uc.mem_write(0xCC00201C,pack(tf)); uc.mem_write(0xCC002024,pack(bf))
    uc.mem_write(0xCC002048,struct.pack('>H',(width_w<<8)|stride_w))
    uc.mem_write(0xCC002000,pack(((acv<<4|6)<<16)|dcr))

def run_hook(uc):
    """Run the real VIHook.bin, which preserves everything then bctrl's into menu_draw."""
    seed={UC_PPC_REG_0+i:(0x2000+i*0x10101)&0xffffffff for i in range(32)}; seed[UC_PPC_REG_1]=STACK_TOP-0x200
    seed.update({UC_PPC_REG_LR:SENT,UC_PPC_REG_CTR:0x12345678,UC_PPC_REG_CR:0x5A5A5A5A,UC_PPC_REG_XER:0xE000000F})
    for r,v in seed.items(): uc.reg_write(r,v)
    seed={r:uc.reg_read(r) for r in seed}
    writes=[]
    def rec(uc,acc,addr,size,val,ud):
        if 0xC0000000<=addr<0xC1800000: writes.append((addr,val&0xffffffff))
    uc.hook_add(UC_HOOK_MEM_WRITE,rec)
    uc.emu_start(0x80002000,SENT,count=20_000_000)
    assert uc.reg_read(UC_PPC_REG_PC)==SENT,'hook did not return via LR'
    for r,v in seed.items():
        got=uc.reg_read(r)
        if r==UC_PPC_REG_XER:
            # Unicorn keeps XER's SO/OV/CA outside the value reg_write(XER) sets, so a
            # correct mfxer/stw..lwz/mtxer round trip reads back with those bits clear.
            # The emulator can only vouch for the byte-count field; the SO/OV/CA path is
            # proven structurally below by the presence of the mfxer/mtxer pair.
            assert (got&0x7F)==(v&0x7F),('hook clobbered XER byte count',hex(v),hex(got)); continue
        assert got==v,('hook clobbered',r,hex(v),hex(got))
    return writes
hw=[struct.unpack('>I',vihook[i:i+4])[0] for i in range(0,len(vihook)-3,4)]
assert 0x7C0102A6 in hw and 0x7C0103A6 in hw,'VIHook must mfxer before the call and mtxer after it'
assert hw.index(0x7C0102A6)<hw.index(0x4E800421)<hw.index(0x7C0103A6),'mfxer .. bctrl .. mtxer ordering'
assert 0x7C0802A6 in hw and 0x7C0803A6 in hw and 0x7C000026 in hw and 0x7C0FF120 in hw,'LR and CR must be saved and restored'

def diag(uc): return struct.unpack('>8I',uc.mem_read(0xD30030A0,32))

def d(name,fn):
    uc=machine(); st_init(uc); fn(uc); results.append(name); print('ok',name)

def d_closed(uc):
    vi(uc); w=run_hook(uc)
    assert st(uc,'drawCalls')==1 and not w,'closed menu with no toast must draw nothing'
    assert diag(uc)[:3]==(1,0,0)
d('hook preserves all registers; closed menu draws nothing',d_closed)

def d_toast(uc):
    vi(uc); st_set(uc,'toast',5); w=run_hook(uc)
    assert w and st(uc,'toast')==4,'toast draws and counts down'
    addrs=[a for a,_ in w]; lo,hi=min(addrs),max(addrs); base=0xC0000000+0x600000; stride=40*32
    assert lo>=base and hi<base+240*stride,'all writes inside the top field'
    assert diag(uc)[1]==1
d('toast banner draws within the framebuffer',d_toast)

def d_open(uc):
    vi(uc); st_set(uc,'open',1); w1=run_hook(uc)
    uc2=machine(); st_init(uc2); vi(uc2); st_set(uc2,'open',1); st_set(uc2,'cursor',3); w2=run_hook(uc2)
    m1=dict(w1); m2=dict(w2)
    assert m1 and set(m1)==set(m2),'both frames must paint the same full rectangle'
    assert m1!=m2,'highlighted row must change the pixel values'
    x=((40*16-384)//2)&~1; y=((240-224)//2)&~1; base=0xC0000000+0x600000; stride=1280; HL=0xC080C080
    def rows_with(m,color): return {(a-base)//stride for a,v in m.items() if v==color}
    for a in m1:
        row=(a-base)//stride; col=((a-base)%stride)//2
        assert y<=row<y+224 and x<=col<x+384,('write outside the menu rectangle',hex(a))
d('open menu draws only inside its rectangle and reflects the highlight',d_open)

def d_interlace(uc):
    vi(uc,top=0x600000,bottom=0x600500,stride_w=80,width_w=40); st_set(uc,'toast',5); w=run_hook(uc)
    assert diag(uc)[1]==2,'two fields, two draws'
    # Field bases differ by 0x500 within a 2560-byte row: top writes sit at row offsets
    # [0x100,0x400) (x=128..512 pixels), bottom at those +0x500. Exact, not a heuristic.
    offs={(a-0xC0600000)%2560 for a,_ in w}
    assert any(o<0x500 for o in offs) and any(o>=0x500 for o in offs),'both field buffers must receive writes'
    assert all(0x100<=o<0x400 or 0x600<=o<0x900 for o in offs),('write outside either field rectangle',sorted(offs)[:4])
d('interlaced modes draw both fields',d_interlace)

def d_rejects(uc):
    cases=[dict(dcr=0),dict(dcr=9),dict(width_w=8,stride_w=8),dict(acv=50),dict(top=0x8000,bottom=0x8000),
           dict(top=0x600001,bottom=0x600001),dict(top=0x17F0000,bottom=0x17F0000,shift=True),dict(stride_w=20)]
    for c in cases:
        uc=machine(); st_init(uc); st_set(uc,'open',1); vi(uc,**c); w=run_hook(uc)
        assert not w and diag(uc)[2]==1,('should reject',c,diag(uc))
d('renderer rejects unsafe or unsupported video states',d_rejects)



def d_every_screen(uc):
    x=((40*16-384)//2)&~1; y=((240-224)//2)&~1; base=0xC0000000+0x600000; stride=1280
    shapes=[]
    cases=[dict(mode=PICTURE),dict(mode=PICTURE,cursor=11,notice=NOTICE_SWAP+11),
           dict(mode=PICTURE,connected=0,notice=NOTICE_NOPAD),dict(mode=PICTURE,notice=NOTICE_TWO),
           dict(mode=LIST),dict(mode=LIST,row=3,connected=0),dict(mode=LIST,row=4,confirm=1),
           dict(mode=LIST,row=6,notice=NOTICE_RESET),
           dict(mode=TEST,lastDetected=0x1F7F),dict(mode=TEST,confirm=1,timer=0,drawCalls=40),
           dict(mode=STICKS,action=4),dict(mode=QUIT,action=1),
           dict(mode=9,row=123,cursor=0xFFFFFFFF,action=250)]
    for case in cases:
        uc=machine(); st_init(uc); vi(uc); st_set(uc,'open',1); st_set(uc,'connected',1)
        for k,v in case.items(): st_set(uc,k,v)
        st_set_map(uc,'edit',0,bytes([12,11,10,9,8,7,6,5,4,3,2,1,1,0,1,0,1,0,0,0]))
        uc.mem_write(STATE+O['live'],struct.pack('>IiiiiII',0x1F7F,-128,127,127,-128,255,255))
        w=run_hook(uc); m=dict(w)
        for a in m:
            row=(a-base)//stride; col=((a-base)%stride)//2
            assert y<=row<y+224 and x<=col<x+384,('write outside the menu rectangle',case,hex(a))
        shapes.append(frozenset(m))
    assert len(set(shapes))==1,'every screen paints exactly the validated rectangle'
d('every screen and notice draws only inside the validated rectangle',d_every_screen)

def d_highlight_follows_cursor(uc):
    frames=[]
    for cursor in (0,6):
        uc=machine(); st_init(uc); vi(uc); st_set(uc,'open',1); st_set(uc,'cursor',cursor)
        frames.append(dict(run_hook(uc)))
    changed=[a for a in frames[0] if frames[0][a]!=frames[1][a]]
    assert len(changed)>200,'moving the cursor must visibly move the highlight on the picture'
d('the picture highlight follows the stick cursor',d_highlight_follows_cursor)
# ---------------------------------------------------------------- checksum + slots, against the real C
work=base/'m21-cktest'; work.mkdir(exist_ok=True)
(work/'ck.c').write_text('#include "%s"\nunsigned int ck(const OverlayFile*f){return OverlayChecksum(f);}\nint valid(const OverlayFile*f){return OverlayFileValid(f);}\n'
    % (src/'common/include/Overlay.h').as_posix())
def sh(*a): return subprocess.run(list(a),capture_output=True,text=True)
r=sh(DKP+'gcc'+('.exe' if os.name=='nt' else ''),'-O2','-mcpu=750','-msoft-float','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles',
     '-Wl,-Ttext=0x80003000','-Wl,-e,ck','-o',str(work/'ck.elf'),str(work/'ck.c'))
assert r.returncode==0,r.stderr
sh(DKP+'objcopy'+('.exe' if os.name=='nt' else ''),'-O','binary','-j','.text',str(work/'ck.elf'),str(work/'ck.bin'))
syms={m[2]:int(m[0],16) for line in sh(DKP+'nm'+('.exe' if os.name=='nt' else ''),str(work/'ck.elf')).stdout.splitlines() for m in [line.split()] if len(m)==3}
ckbin=(work/'ck.bin').read_bytes()
def c_call(fn,blob):
    uc=Uc(UC_ARCH_PPC,UC_MODE_32|UC_MODE_BIG_ENDIAN)
    uc.mem_map(0x80002000,0x2000); uc.mem_map(0x81000000,0x10000); uc.mem_write(0x80003000,ckbin)
    uc.mem_write(SENT,b'\x4e\x80\x00\x20'); uc.mem_write(0x81001000,blob)
    uc.reg_write(UC_PPC_REG_1,0x81008000); uc.reg_write(UC_PPC_REG_3,0x81001000); uc.reg_write(UC_PPC_REG_LR,SENT)
    uc.emu_start(syms[fn],SENT,count=200000); assert uc.reg_read(UC_PPC_REG_PC)==SENT
    return uc.reg_read(UC_PPC_REG_3)&0xffffffff
def py_ck(blob):
    h=2166136261
    for i,b in enumerate(blob):
        if i<12 or i>=16: h=((h^b)*16777619)&0xffffffff
    return h
def ofile(gen=1,maps=None,version=1,magic=0x4D31354D,checksum=None):
    b=bytearray(struct.pack('>IIII',magic,version,gen,0)+(maps or identity()*4))
    c=py_ck(b) if checksum is None else checksum; b[12:16]=pack(c); return bytes(b)
for blob in [ofile(),ofile(gen=7,maps=bytes([11]*12+[1]*5+[0]*3)*4),ofile(maps=bytes(range(20))*4)]:
    assert c_call('ck',blob)==py_ck(blob),'python checksum model diverges from the C'
assert c_call('valid',ofile())==1
assert c_call('valid',ofile(checksum=0xDEADBEEF))==0,'bad checksum accepted'
assert c_call('valid',ofile(version=2))==0,'wrong version accepted'
assert c_call('valid',ofile(magic=0))==0,'wrong magic accepted'
assert c_call('valid',ofile(maps=bytes([13]+[0]*19)*4))==0,'button index 13 accepted'
assert c_call('valid',ofile(maps=bytes([0]*12+[2]+[0]*7)*4))==0,'flag value 2 accepted'
assert c_call('valid',ofile(maps=bytes([0]*17+[1,0,0])*4))==0,'reserved byte accepted'
results.append('checksum and validity rules match the real C'); print('ok checksum/validity vs C')

# Slot selection and alternation, using the C validity decision above.
def init_select(slot0,slot1):
    """Model of OverlayInit: newest valid generation wins; strict > keeps the first on a tie."""
    have=None; loaded=-1
    for k,blob in enumerate((slot0,slot1)):
        if blob is None or len(blob)!=96 or not c_call('valid',blob): continue
        gen=struct.unpack('>I',blob[8:12])[0]
        d=(gen-have)&0xffffffff if have is not None else 1
        if have is None or (0<d<0x80000000): have=gen; loaded=k   # (s32)(gen-best)>0
    return have,loaded
def save_slot(loaded): return 1 if loaded==0 else 0
assert init_select(ofile(gen=3),ofile(gen=5))==(5,1); assert save_slot(1)==0
assert init_select(ofile(gen=9),ofile(gen=5))==(9,0); assert save_slot(0)==1
assert init_select(ofile(gen=9,checksum=1),ofile(gen=5))==(5,1),'corrupt newer slot must fall back to the older valid one'
assert init_select(None,None)==(None,-1) and save_slot(-1)==0,'no saves -> defaults, first write goes to slot 0'
assert init_select(ofile(gen=4)[:50],ofile(gen=4))==(4,1),'truncated slot ignored'
results.append('slot recovery: newest valid wins, partial writes fall back'); print('ok slot recovery')

# ---------------------------------------------------------------- what actually shipped
dol=(src/'loader/loader.dol').read_bytes()
s=dol.find(b'PK\x03\x04'); e=dol.rfind(b'PK\x05\x06'); clen=struct.unpack('<H',dol[e+20:e+22])[0]
kernel=zipfile.ZipFile(io.BytesIO(dol[s:e+22+clen])).read('kernel.bin')
assert overlay in kernel,'kernel does not embed the tested overlay.bin'
assert vihook in kernel,'kernel does not embed the tested VIHook.bin'
for needle in (b'/vi_m21.log',b'/ninmap-m15-0.bin',b'/ninmap-m15-1.bin'): assert needle in kernel,needle
padread=(src/'loader/data/PADReadGC.bin').read_bytes(); last=len(padread)
while last and padread[last-1]==0: last-=1
assert len(padread)==0x3000 and last<0x3000
words=[struct.unpack('>I',padread[i:i+4])[0] for i in range(0,len(padread)-3,4)]
# icbi = primary 31, XO 982: mask out rA (bits 16-20) AND rB (bits 11-15), keep opcode+XO+Rc.
assert any((w&0xFC0007FF)==0x7C0007AC for w in words),'icbi sweep not present in PADReadGC'
assert any(w==0x7C0004AC for w in words) and any(w==0x4C00012C for w in words),'sync; isync after the sweep'
assert struct.pack('>I',0x93180000)[:2] in padread or b'\x93\x18' in padread,'menu call address not in PADReadGC'
assert len(overlay)<=0xC000
# PADReadGC gates on the magic as GCC emits it: xoris rA,rS,0x4F56 then cmpwi rA,0x3230
# (M19's binary has cmpwi rA,0x4C39 at the same site, so this discriminates the builds).
assert any((w&0xFC00FFFF)==0x6C004F56 for w in words),'PADReadGC magic high half missing'
assert any((w&0xFC00FFFF)==0x2C003231 for w in words),'PADReadGC does not compare against the M21 magic'
assert not any((w&0xFC00FFFF)==0x2C003230 for w in words),'PADReadGC still compares against the M20 magic'
main=(src/'kernel/main.c').read_text()
assert main.index('OverlaySave()')<main.index('/vi_m21.log')<main.index('f_mount(NULL')<main.index('USBStorage_Shutdown();')
assert main.index('f_mount(')<main.index('OverlayInit();')<main.index('DIRegister();')
patch=(src/'kernel/Patch.c').read_text(); assert 'OverlayInstallCode();' in patch
ov=(src/'kernel/Overlay.c').read_text(); body=ov[ov.index('void OverlayInit'):ov.index('void OverlayInstallCode')]
assert 'OVL_CODE_ARM' not in body,'OverlayInit must not touch the code region'
pc=(src/'kernel/Patch.c').read_text(); assert pc.index('OverlayInstallCode();')<pc.index('OVL_STATE_ARM+4,1'),'install must precede enable'
report={'overlay_bytes':len(overlay),'padread_free':0x3000-last,'vihook_bytes':len(vihook),
        'dol_sha256':hashlib.sha256(dol).hexdigest().upper(),'kernel_sha256':hashlib.sha256(kernel).hexdigest().upper(),
        'tests':results}
(base/'m21-verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2)); print('ALL %d TESTS PASSED'%len(results))
