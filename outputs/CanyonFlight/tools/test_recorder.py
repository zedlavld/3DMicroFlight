from pathlib import Path
import os,subprocess,struct,json,sys,csv,io,math
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent;out=r/'build/recorder_tests';out.mkdir(parents=True,exist_ok=True)
cc=Path(os.environ.get('DEVKITARM','C:/devkitPro/devkitARM'))/'bin/arm-none-eabi-gcc.exe'
if not cc.exists():cc=Path('C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe')
elfpath=out/'recorder_test.elf'
subprocess.run([str(cc),'-O2','-fno-builtin','-Dfopen=test_fopen','-Dfclose=test_fclose','-Dfputs=test_fputs','-Dfprintf=test_fprintf','-Drename=test_rename','-Dremove=test_remove','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),str(r/'source/flight_recorder.c'),str(r/'tests/recorder_test.c'),'-Wl,-e,main','-lm','-o',str(elfpath)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x2000000)
with elfpath.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x1f00000);u.reg_write(UC_ARM_REG_LR,0x1ff0000)
u.emu_start(sy['main'],0x1ff0000,count=1000000000);assert u.reg_read(UC_ARM_REG_PC)==0x1ff0000
integer=lambda n:struct.unpack('<i',u.mem_read(sy[n],4))[0]
report={k:integer(k) for k in ['checks','failures','firstFailure']}
text=bytes(u.mem_read(sy['outputCsv'],integer('outputLength'))).decode('ascii');rows=list(csv.DictReader(io.StringIO(text)))
assert len(rows)==51 and all(len(x)==40 and None not in x for x in rows)
assert float(rows[-1]['time_s'])==10 and float(rows[0]['time_s'])==0
assert abs(float(rows[0]['pitch_deg'])-math.degrees(.1))<.001
assert rows[0]['east_m']=='1234.000' and rows[0]['brake_left']=='0.250' and rows[0]['start_profile']=='4'
assert rows[0]['model_version']=='29' and float(rows[0]['isa_offset_c'])==20
assert float(rows[0]['thrust_n'])==1234.5 and float(rows[0]['effective_elevator'])==1
assert abs(float(rows[0]['q_rad_s'])+.23)<.00001
(r/'validation/recorder_sample.csv').write_text(text);report.update(csv_rows=len(rows),csv_columns=len(rows[0]),synthetic_fixture=True)
(r/'validation/recorder_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));sys.exit(bool(report['failures']))
