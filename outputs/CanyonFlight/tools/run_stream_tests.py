from pathlib import Path
import sys,struct,json
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x8000000)
with open(sys.argv[1],'rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x7f00000);u.reg_write(UC_ARM_REG_LR,0x7ff0000)
u.emu_start(sy['main'],0x7ff0000,count=4000000000)
assert u.reg_read(UC_ARM_REG_PC)==0x7ff0000,'instruction budget exhausted'
r={k:struct.unpack('<i',u.mem_read(sy[k],4))[0] for k in ['checks','failures','firstFailure','maxResident','maxFrameUpload','completedTiles','failureTiles','readyFrames','pressureTransitions']}
Path(sys.argv[2]).write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));sys.exit(bool(r['failures']))
