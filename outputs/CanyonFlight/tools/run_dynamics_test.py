"""Build and run tests/dynamics_test.c (v33) under unicorn; print the metrics.
Requires unicorn and pyelftools (same as run_arm_tests.py).
  python tools/run_dynamics_test.py [report.json]"""
import sys, json, struct, subprocess
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC
from elftools.elf.elffile import ELFFile
r = Path(__file__).resolve().parent.parent
out = r / 'build/validation'; out.mkdir(parents=True, exist_ok=True); elf = out / 'dynamics_test.elf'
cc = 'C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe'
src = [r / 'source' / f'{n}.c' for n in ['flight', 'fbw', 'f16_aero', 'ground', 'terrain', 'terrain_stream', 'terrain_data', 'airports', 'airport_index']]
subprocess.run([cc, '-O2', '-marm', '-march=armv6k', '-mfloat-abi=hard', '-mfpu=vfp', '-specs=nosys.specs', '-I' + str(r / 'include'),
                *map(str, src), str(r / 'tests/dynamics_test.c'), '-Wl,-e,main', '-lm', '-o', str(elf)], check=True)
u = Uc(UC_ARCH_ARM, UC_MODE_ARM); u.mem_map(0, 0x02000000)
with elf.open('rb') as f:
    e = ELFFile(f)
    for seg in e.iter_segments():
        if seg['p_type'] == 'PT_LOAD': u.mem_write(seg['p_vaddr'], seg.data())
    sym = {s.name: s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2, 0xf << 20); u.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
u.reg_write(UC_ARM_REG_SP, 0x1f00000); u.reg_write(UC_ARM_REG_LR, 0x1ff0000)
u.emu_start(sym['main'], 0x1ff0000)
if u.reg_read(UC_ARM_REG_PC) != 0x1ff0000: raise RuntimeError('did not finish')
count = struct.unpack('<i', u.mem_read(sym['metricCount'], 4))[0]
values = struct.unpack(f'<{count}f', u.mem_read(sym['metrics'], 4 * count))
names = bytes(u.mem_read(sym['metricNames'], 8192)).split(b'\0')[0].decode().split(';')[:count]
report = dict(zip(names, values))
for k, v in report.items(): print(f'{k:40s} {v:10.2f}')
if len(sys.argv) > 1: Path(sys.argv[1]).write_text(json.dumps(report, indent=1))
