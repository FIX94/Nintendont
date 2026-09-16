"""Adversarial input regressions, executing the compiled PPC module.
Use NIN_SOURCE to compare the unchanged M21 build with the audit branch.
"""
import os, sys, json, traceback
from pathlib import Path
# Load the harness definitions without running its original top-level suite.
harness=Path(__file__).with_name('test_menu.py')
exec(compile(harness.read_text().split('def t_idle')[0],str(harness),'exec'))
failures=[]
checks=[]
def check(name,fn):
    uc=machine(); st_init(uc)
    try:
        fn(uc)
        print('PASS',name); checks.append(name)
    except Exception as exc:
        print('FAIL',name,repr(exc)); failures.append(name)

def identity_analog(uc):
    for button,aa,ab in [(A,0,0),(B,0,0),(A|B,12,34),(0,5,6)]:
        out=call_input(uc,[pad(button,aa=aa,ab=ab)],1)[0]
        assert (out['aa'],out['ab'])==(aa,ab),out
check('identity mapping preserves analog A/B bytes',identity_analog)

def move_and_press(uc):
    opened(uc)
    call_input(uc,[pad(B,sx=100)],1)
    call_input(uc,[pad()],1)
    assert st_map(uc,'edit',0)==identity(),'a press starting during navigation assigned'
check('button beginning on same sample as navigation is cancelled',move_and_press)

def reset_leave(uc):
    opened(uc); press(uc,B); changed=st_map(uc,'edit',0)
    move(uc,dy=-100); st_set(uc,'row',L_RESET); tap(uc,A)
    tap(uc,B); call_input(uc,[pad()],1)
    move(uc,dy=-100); st_set(uc,'row',L_RESET); tap(uc,A)
    assert st_map(uc,'edit',0)==changed,'old confirmation allowed one-press reset'
    assert st(uc,'confirm')==1
check('reset confirmation expires when leaving list',reset_leave)

def owner_lost(uc):
    opened(uc)
    # A disconnected pad may leave stale button bytes in a caller-owned buffer.
    call_input(uc,[pad(Y)],0)
    assert st(uc,'open')==0
    assert st(uc,'release')==0,'stale disconnected owner traps release latch'
check('disconnect with stale button bytes cannot trap input release',owner_lost)

def no_turbo_synthesis(uc):
    for buttons in [Y]*120+[0]*120+[Y,0]*120:
        out=call_input(uc,[pad(buttons)],1)[0]
        assert out['button']==buttons
    opened(uc)
    for _ in range(120):
        call_input(uc,[pad(Y)],1)
        assert st_map(uc,'edit',0)==identity(),'held input repeatedly assigned'
    call_input(uc,[pad()],1)
    first=st_map(uc,'edit',0)
    for _ in range(120): press(uc,Y)
    assert st_map(uc,'edit',0)==first
check('steady and alternating Y input never synthesizes extra presses',no_turbo_synthesis)

def masks_all_ports(uc):
    opened(uc)
    for bit in BITS:
        out=call_input(uc,[pad(bit),pad(Y),pad(X),pad(A)],15)
        assert all(neutral(p) for p in out)
        call_input(uc,[pad()]*4,15)
check('menu consumes rapid presses on all four ports',masks_all_ports)

print(json.dumps({'passed':checks,'failed':failures},indent=2))
sys.exit(bool(failures))
