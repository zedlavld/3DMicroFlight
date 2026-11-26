from pathlib import Path
import subprocess,os
r=Path(__file__).resolve().parent.parent
compiler=Path(os.environ.get('DEVKITARM','C:/devkitPro/devkitARM'))/'bin/arm-none-eabi-gcc.exe'
if not compiler.exists():compiler=Path('C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe')
out=r/'build/validation';out.mkdir(parents=True,exist_ok=True)
base=[str(compiler),'-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),'-I'+str(r/'build')]
physics=['flight','fbw','f16_aero','ground','camera','cockpit','aircraft_mesh','terrain','terrain_stream','terrain_data','airports','airport_index','scenery','imagery','instruments','stream_plan']
subprocess.run(base+[*map(str,[r/'source'/f'{name}.c' for name in physics]),str(r/'build/corridor_index.bin.o'),str(r/'build/sr22_imported_mesh.bin.o'),str(r/'build/sf50_imported_mesh.bin.o'),str(r/'build/b737_imported_mesh.bin.o'),str(r/'build/f16_imported_mesh.bin.o'),str(r/'build/a321_imported_mesh.bin.o'),str(r/'tests/physics_test.c'),'-Wl,-e,main','-lm','-o',str(out/'physics_test.elf')],check=True)
subprocess.run([os.sys.executable,str(r/'tools/run_arm_tests.py'),str(out/'physics_test.elf'),str(r/'validation')],check=True)
subprocess.run(base+['-ffunction-sections','-fdata-sections','-I'+str(r/'tests/stream_stubs'),str(r/'tests/stream_cache_test.c'),str(r/'source/stream_plan.c'),str(r/'build/corridor_index.bin.o'),'-Wl,-e,main,--gc-sections','-lm','-o',str(out/'stream_test.elf')],check=True)
subprocess.run([os.sys.executable,str(r/'tools/run_stream_tests.py'),str(out/'stream_test.elf'),str(r/'validation/stream_report.json')],check=True)
