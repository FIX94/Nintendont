"""Run all compiled-binary suites, retaining logs and a machine-readable summary."""
import os, sys, subprocess, json
from pathlib import Path
base=Path(__file__).resolve().parent
for key in ('DEVKITPRO','DEVKITPPC','DEVKITARM'):
    if not os.environ.get(key):sys.exit('Set '+key+' to the pinned devkitPro toolchain path.')
records=[]
for name in ('test_menu','test_audit','test_save','test_entry','test_exit','test_patch'):
    p=subprocess.run([sys.executable,str(base/(name+'.py'))],capture_output=True,text=True)
    (base/(name+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
    records.append({'suite':name,'exit_code':p.returncode})
    print(name, 'PASS' if p.returncode==0 else 'FAIL',flush=True)
    if p.returncode:print(p.stdout+p.stderr)
(base/'suite-results.json').write_text(json.dumps(records,indent=2))
sys.exit(any(r['exit_code'] for r in records))
