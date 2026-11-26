"""Run compiled ARM physics tests. Requires unicorn and pyelftools."""
import sys,json,struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_PC,UC_ARM_REG_C1_C0_2,UC_ARM_REG_FPEXC
from elftools.elf.elffile import ELFFile
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x04000000) # v1.1: 64 MB; the physics test's data now ends past 32 MB
with Path(sys.argv[1]).open('rb') as f:
 elf=ELFFile(f)
 for seg in elf.iter_segments():
  if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
 symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
u.reg_write(UC_ARM_REG_SP,0x3f00000);u.reg_write(UC_ARM_REG_LR,0x3ff0000)
u.emu_start(symbols['main'],0x3ff0000,count=4000000000)
if u.reg_read(UC_ARM_REG_PC)!=0x3ff0000:raise RuntimeError('Instruction budget exhausted')
def integer(name):return struct.unpack('<i',u.mem_read(symbols[name],4))[0]
report={k:integer(k) for k in ['checks','failures','firstFailure']}
report['diagnostic']=struct.unpack('<24f',u.mem_read(symbols['diagnostic'],96))
report['maxSceneryJobVertices']=integer('maxPartVertices')
report['worldVertices']=struct.unpack('<3i',u.mem_read(symbols['worldCounts'],12))
if 'imageryCounts' in symbols:
 report['imageryVertices']=struct.unpack('<3i',u.mem_read(symbols['imageryCounts'],12))
 report['maxImageryMaterialJobVertices']=integer('maxImageryMaterialJob')
if 'regionalPhotoTotals' in symbols:report['regionalPhotoTerrainVertices']=struct.unpack('<2i',u.mem_read(symbols['regionalPhotoTotals'],8))
report['meshTriangles']=struct.unpack('<2i',u.mem_read(symbols['meshCounts'],8))
if len(sys.argv)>2:
 out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
 for i,count in enumerate(report['meshTriangles']):
  (out/f'mesh_{i}.bin').write_bytes(u.mem_read(symbols['meshOutput']+i*integer('meshCapacity')*76,count*76))
 for i,count in enumerate(report['worldVertices']):
  (out/f'world_{i}.bin').write_bytes(u.mem_read(symbols['worldOutput']+i*integer('worldCapacity')*24,count*24))
 if 'imageryCounts' in symbols:
  count=report['imageryVertices'][0]
  (out/'imagery_vertices.bin').write_bytes(u.mem_read(symbols['imageryOutput'],count*36))
  (out/'imagery_materials.bin').write_bytes(u.mem_read(symbols['imageryMaterials'],count//3*4))
 (out/'test_report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
sys.exit(1 if report['failures'] else 0)


