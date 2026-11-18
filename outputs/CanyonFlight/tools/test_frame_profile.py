"""Run frame statistics/CSV tests on ARM. Timings here are synthetic fixtures,
not measured simulator performance.
"""
from pathlib import Path
import csv,io,json,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent;out=r/'build/frame_tests';out.mkdir(parents=True,exist_ok=True);elf=out/'frames.elf'
redirect=['-D'+n+'=test_'+n for n in ['fopen','fputs','fprintf','fclose','remove','rename']]
subprocess.run(['C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe','-Wall','-O2','-fno-builtin',*redirect,'-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),str(r/'source/frame_profile.c'),str(r/'tests/frame_profile_test.c'),'-Wl,-e,main','-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x2000000)
with elf.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x1f00000);u.reg_write(UC_ARM_REG_LR,0x1ff0000)
u.emu_start(sy['main'],0x1ff0000,count=100000000);assert u.reg_read(UC_ARM_REG_PC)==0x1ff0000
integer=lambda n:struct.unpack('<i',u.mem_read(sy[n],4))[0]
report={k:integer(k) for k in ['checks','failures','firstFailure']}
text=bytes(u.mem_read(sy['outputCsv'],integer('outputLength'))).decode('ascii');rows=list(csv.DictReader(io.StringIO(text)))
assert len(rows)==300 and float(rows[0]['frame_ms'])==51 and float(rows[-1]['frame_ms'])==350
assert all(len(row)==8 and None not in row for row in rows)
report.update(csv_rows=300,synthetic_fixture=True,runtime_fps_measured=False)
(r/'validation/frame_profile_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));sys.exit(bool(report['failures']))
