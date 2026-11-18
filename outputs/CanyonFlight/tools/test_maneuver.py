"""v33.09: run tests/maneuver_test.c (every autotest manoeuvre on all five aircraft) on ARM under unicorn."""
from pathlib import Path
import os,subprocess,struct,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
root=Path(__file__).resolve().parent.parent;out=root/'build/maneuver_tests';out.mkdir(parents=True,exist_ok=True)
cc=Path(os.environ.get('DEVKITARM','C:/devkitPro/devkitARM'))/'bin/arm-none-eabi-gcc.exe'
if not cc.exists():cc=Path('C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe')
elfpath=out/'maneuver_test.elf'
sources=['flight','fbw','f16_aero','ground','terrain','terrain_stream','terrain_data','airports','maneuver']
subprocess.run([str(cc),'-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(root/'include'),
 *[str(root/'source'/f'{n}.c') for n in sources],str(root/'tests/maneuver_test.c'),'-Wl,-e,main','-lm','-o',str(elfpath)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x4000000)
with elfpath.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x3f00000);u.reg_write(UC_ARM_REG_LR,0x3ff0000)
u.emu_start(sy['main'],0x3ff0000,count=20000000000)
assert u.reg_read(UC_ARM_REG_PC)==0x3ff0000,'instruction budget exhausted'
def integer(n):return struct.unpack('<i',u.mem_read(sy[n],4))[0]
text=bytes(u.mem_read(sy['logbuf'],integer('logLen'))).decode()
summary=f"checks {integer('checks')}, failures {integer('failures')}, first failure line {integer('firstFailure')}\n"
(root/'validation/maneuver_report.txt').write_text(summary+text);print(summary+text);sys.exit(bool(integer('failures')))
