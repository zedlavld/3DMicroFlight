"""Compare optimized cache against the published v21 cockpit, on compiled ARM.
This measures geometry equivalence and upload byte counts, not hardware FPS.
"""
from pathlib import Path
import json,struct,subprocess,sys,zipfile,hashlib
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent;out=r/'build/optimization';out.mkdir(parents=True,exist_ok=True)
baseline=r.parent/'CirrusFlight-v21-update.zip'
reference=r/'tests/references/cockpit_v21.c'
if reference.exists():raw=reference.read_bytes()
else:
 with zipfile.ZipFile(baseline) as z:raw=z.read('source/cockpit.c')
(out/'cockpit_v21.c').write_bytes(raw)
cc='C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe';obj=out/'baseline.o';elf=out/'cache.elf'
flags=['-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include')]
subprocess.run([cc,*flags,'-DbuildCockpit=buildReferenceCockpit','-c',str(out/'cockpit_v21.c'),'-o',str(obj)],check=True)
subprocess.run([cc,*flags,str(obj),str(r/'source/cockpit.c'),str(r/'source/cockpit_cache.c'),str(r/'source/airports.c'),str(r/'source/airport_index.c'),str(r/'tests/cockpit_cache_test.c'),'-Wl,-e,main','-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x2000000)
with elf.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x1f00000);u.reg_write(UC_ARM_REG_LR,0x1ff0000)
u.emu_start(sy['main'],0x1ff0000,count=100000000);assert u.reg_read(UC_ARM_REG_PC)==0x1ff0000
report={k:struct.unpack('<i',u.mem_read(sy[k],4))[0] for k in ['checks','failures','firstFailure']};report['baseline_source_sha256']=hashlib.sha256(raw).hexdigest();report['aircraft']=[]
counts=struct.unpack('<2i',u.mem_read(sy['counts'],8));fixed=struct.unpack('<2i',u.mem_read(sy['fixedCounts'],8));live=struct.unpack('<2i',u.mem_read(sy['liveCounts'],8))
def triangles(symbol,type,n):
 data=bytes(u.mem_read(sy[symbol]+type*1024*40,n*40))
 return sorted(tuple(round(x,6) for x in struct.unpack('<9fI',data[i*40:(i+1)*40])[:9])+(struct.unpack_from('<I',data,i*40+36)[0],) for i in range(n))
for i,n in enumerate(counts):
 ref=triangles('reference',i,n-2);new=triangles('optimized',i,n);kept=[f for f in new if f[-1]!=0xff221d1a];assert ref==kept and len(new)-len(kept)==2,'unexpected geometry change'
 before=(n-2)*3*36;after=live[i]*3*36
 report['aircraft'].append({'name':'SF50' if i else 'SR22','total_triangles':n,'cached_static_triangles':fixed[i],'live_triangles':live[i],'baseline_bytes_per_changing_frame':before,'optimized_bytes_per_changing_frame':after,'reduction_percent':100*(1-after/before),'unchanged_frame_upload_bytes':0,'static_initialization_bytes_both_buffers':fixed[i]*3*36*2,'existing_geometry_matches_v21_to_micrometre':True,'added_footwell_triangles':2})
report['limitations']='Counts and geometry measured in ARM tests. GPU synchronization, total frame time and hardware FPS are not measured.'
(r/'validation/optimization_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));sys.exit(bool(report['failures']))
