"""Run compiled production flight/ground C on ARM; no 3DS OS or timing emulation."""
from pathlib import Path
import os,subprocess,struct,json,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
root=Path(__file__).resolve().parent.parent;out=root/'build/phase_tests';out.mkdir(parents=True,exist_ok=True)
cc=Path(os.environ.get('DEVKITARM','C:/devkitPro/devkitARM'))/'bin/arm-none-eabi-gcc.exe'
if not cc.exists():cc=Path('C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe')
elfpath=out/'phase_test.elf'
subprocess.run([str(cc),'-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(root/'include'),*[str(root/'source'/n) for n in ['flight.c','fbw.c','f16_aero.c','ground.c','airports.c']],str(root/'tests/phase_test.c'),'-Wl,-e,main','-lm','-o',str(elfpath)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x4000000)
with elfpath.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x3f00000);u.reg_write(UC_ARM_REG_LR,0x3ff0000)
u.emu_start(sy['main'],0x3ff0000,count=12000000000)
assert u.reg_read(UC_ARM_REG_PC)==0x3ff0000,'instruction budget exhausted'
def integer(n):return struct.unpack('<i',u.mem_read(sy[n],4))[0]
r={k:integer(k) for k in ['checks','failures','firstFailure','caseCount']};r['cases']=[]
labels={13:['reference_condition_matrix',['altitude_m','isa_offset_c','mass_kg','clean_stall_kcas','thrust_n']],4:['approach',['final_kcas','average_fpm','flaps','gear','mass_kg']],9:['sf50_mct_reference_probe',['altitude_m','target_kias_approx_cas','reference_gradient_ft_nm','full_lever_model_gradient_ft_nm','model_setting_required']],10:['differential_braking',['heading_change_rad','final_groundspeed_mps','roll_rad','pitch_rad','crashed']],11:['go_around',['height_gain_m','final_kcas','flaps','gear','stall_warning']],12:['banked_turn',['height_change_m','bank_rad','heading_change_rad','final_kcas','beta_rad']],0:['takeoff',['distance_to_8m_agl_m','airspeed_at_8m_kcas','height_m','final_kcas','crashed']],1:['climb',['initial_altitude_m','target_kcas','average_fpm','final_kcas','roll_rad']],2:['cruise',['target_kcas','average_fpm','final_kcas','throttle','roll_rad']],3:['descent',['target_kcas','average_fpm','final_kcas','throttle','roll_rad']],5:['landing',['distance_to_stop_m','max_contact_sink_mps','final_groundspeed_mps','pitch_rad','crashed']],6:['stall_recovery',['max_warning','max_alpha_rad','minimum_kcas','recovered_kcas','final_warning']],7:['engine_out_glide',['target_kcas','glide_ratio','sink_fpm','final_kcas','initial_path_rad']],8:['ground_settle',['cg_height_m','groundspeed_mps','roll_rad','pitch_rad','ground_load_g']]}
for i in range(r['caseCount']):
 p=struct.unpack('<12f',u.mem_read(sy['results']+48*i,48));kind=struct.unpack('<i',u.mem_read(sy['caseTypes']+4*i,4))[0];label,fields=labels[kind]
 r['cases'].append({'aircraft':'SF50' if p[0] else 'SR22','phase':label,**dict(zip(fields,p[1:6]))})
(root/'validation/phases_report.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));sys.exit(bool(r['failures']))
