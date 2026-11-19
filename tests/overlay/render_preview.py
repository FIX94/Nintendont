"""Render the compiled PPC menu for documentation. Requires Pillow in addition
to the test dependencies; no game image, screenshot or external art is used.
"""
from pathlib import Path
from PIL import Image
base=Path(__file__).resolve().parent
harness=base/'test_menu.py'
s=harness.read_text(encoding='utf-8')
cut=s.index('# ---------------------------------------------------------------- menu_input (M21 picture, press to assign)')
draw=s.index('# ---------------------------------------------------------------- VI hook + menu_draw')
parts=s[:cut]+s[cut:s.index('def t(name,fn):')]+s[draw:s.index('def d(name,fn):',draw)]
exec(compile(parts,str(harness),'exec'))

def render(mode,cursor=0,live=None):
    uc=machine();st_init(uc);vi(uc)
    st_set(uc,'open',1);st_set(uc,'connected',1);st_set(uc,'mode',mode)
    st_set(uc,'cursor',cursor)
    if live:
        uc.mem_write(STATE+O['live'],struct.pack('>IiiiiII',*live))
        st_set(uc,'lastDetected',live[0])
    run_hook(uc)
    im=Image.new('RGB',(384,224));pix=im.load()
    clip=lambda v:max(0,min(255,round(v)))
    for y in range(224):
        raw=uc.mem_read(0xC0600000+(y+8)*1280+256,768)
        for x in range(0,384,2):
            a,u,b,v=raw[x*2:x*2+4];u-=128;v-=128
            for xx,lum in [(x,a),(x+1,b)]:
                c=1.164*(lum-16)
                pix[xx,y]=(clip(c+1.596*v),clip(c-.392*u-.813*v),clip(c+2.017*u))
    return im

images=[render(PICTURE,8),render(LIST),render(TEST,live=(0x140,80,-30,-50,40,200,53)),render(QUIT)]
contact=Image.new('RGB',(768,448))
for i,im in enumerate(images):contact.paste(im,((i%2)*384,(i//2)*224))
out=base.parents[1]/'docs/controller-menu/preview.png'
out.parent.mkdir(parents=True,exist_ok=True);contact.save(out)
print(out)
