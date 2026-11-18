"""Actual ARM flight model + embedded terrain, not emulator/GPU validation."""
from pathlib import Path
import subprocess,struct,json,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent
out=r/'build/airport_playtest';out.mkdir(parents=True,exist_ok=True);elf=out/'test.elf'
subprocess.run(['C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe','-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),*[str(r/'source'/n) for n in ['flight.c','fbw.c','f16_aero.c','ground.c','airports.c','terrain.c','terrain_stream.c','terrain_data.c','camera.c','scenery.c']],str(r/'tests/airport_playtest.c'),'-Wl,-e,main','-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x4000000)
with elf.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x3f00000);u.reg_write(UC_ARM_REG_LR,0x3ff0000)
u.emu_start(sy['main'],0x3ff0000,count=20000000000)  # v31: 13 start runways
assert u.reg_read(UC_ARM_REG_PC)==0x3ff0000,'instruction budget exhausted'
report={k:struct.unpack('<i',u.mem_read(sy[k],4))[0] for k in ['checks','failures','firstFailure']}
keys=['aircraft','airport','crashed','lifted','distance_m','ias_kt','pitch_rad','roll_rad','agl_m','ground_load']
n=struct.unpack('<i',u.mem_read(sy['startCount'],4))[0]
report['takeoffs']=[dict(zip(keys,struct.unpack('<10f',u.mem_read(sy['results']+i*40,40)))) for i in range(2*n)]
report['runtime_gpu_verified']=False
report['airport_part_vertices']=list(struct.unpack(f'<{n}i',u.mem_read(sy['airportVertices'],4*n)))
report['asphalt_vertices']=list(struct.unpack(f'<{n}i',u.mem_read(sy['asphaltVertices'],4*n)))
report['pre_impact']=[list(struct.unpack('<8f',u.mem_read(sy['impact']+i*32,32))) for i in range(2*n)]
(r/'validation/airport_playtest_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));sys.exit(bool(report['failures']))
