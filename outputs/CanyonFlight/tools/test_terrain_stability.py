"""Check actual ARM terrain mesh joins and stable world-grid placement offline."""
from pathlib import Path
import subprocess,struct,json,sys,zipfile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent
out=r/'build/terrain_stability';out.mkdir(parents=True,exist_ok=True);elf=out/'test.elf'
baseline='--baseline-v25' in sys.argv
scenery=r/'source/scenery.c'
if baseline:
 current=scenery.read_text();helper=current[current.index('void sceneryLodLayout'):current.index('int buildSceneryPart')]
 with zipfile.ZipFile(r.parent/'CirrusFlight-v25-update.zip') as z:old=z.read('source/scenery.c').decode()
 scenery=out/'scenery_v25.c';scenery.write_text(old+'\n'+helper+'\n'+current[current.index('float sceneryHaze'):current.index('void scenerySkyColor')])
sources=[r/'source'/n for n in ['flight.c','fbw.c','f16_aero.c','ground.c','airports.c','airport_index.c','terrain.c','terrain_stream.c','terrain_data.c','imagery.c','weather_visual.c']]+[scenery]
subprocess.run(['C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe','-O2',*(['-DBASELINE_V25'] if baseline else []),'-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),*[str(p) for p in sources],str(r/'tests/terrain_stability_test.c'),'-Wl,-e,main','-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x4000000)
with elf.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x3f00000);u.reg_write(UC_ARM_REG_LR,0x3ff0000)
u.emu_start(sy['main'],0x3ff0000,count=3000000000)
assert u.reg_read(UC_ARM_REG_PC)==0x3ff0000,'instruction budget exhausted'
report={k:struct.unpack('<i',u.mem_read(sy[k],4))[0] for k in ['checks','failures','firstFailure','boundaryVertices','largestJob','skirts','sharedJoins']}
report['runtime_gpu_verified']=False
report['baseline_v25']=baseline
name='terrain_stability_baseline_v25.json' if baseline else 'terrain_stability_report.json'
(r/'validation'/name).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
sys.exit(not bool(report['failures']) if baseline else bool(report['failures']))
