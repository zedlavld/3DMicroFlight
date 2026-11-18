"""Test the production synthesizer compiled for ARM and export actual PCM output.
NDSP playback, thread scheduling and device speakers are not emulated here.
"""
from pathlib import Path
import json,struct,subprocess,sys,wave
import numpy as np
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parent.parent;out=r/'build/audio_tests';out.mkdir(parents=True,exist_ok=True);elf=out/'audio.elf'
subprocess.run(['C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe','-Wall','-O2','-marm','-march=armv6k','-mfloat-abi=hard','-mfpu=vfp','-specs=nosys.specs','-I'+str(r/'include'),str(r/'source/audio_synth.c'),str(r/'tests/audio_test.c'),'-Wl,-e,main','-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x2000000)
with elf.open('rb') as f:
 e=ELFFile(f)
 for s in e.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
 sy={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_C1_C0_2,0xf<<20);u.reg_write(UC_ARM_REG_FPEXC,0x40000000);u.reg_write(UC_ARM_REG_SP,0x1f00000);u.reg_write(UC_ARM_REG_LR,0x1ff0000)
u.emu_start(sy['main'],0x1ff0000,count=500000000);assert u.reg_read(UC_ARM_REG_PC)==0x1ff0000
report={k:struct.unpack('<i',u.mem_read(sy[k],4))[0] for k in ['checks','failures','firstFailure']};report['scenarios']=[]
art=r/'art/audio';art.mkdir(parents=True,exist_ok=True)
names=['sr22_idle','sr22_power','sf50_idle','sf50_power','wind','stall_warning']
for i,name in enumerate(names):
 raw=bytes(u.mem_read(sy['previews']+i*44100*2,44100*2));samples=np.frombuffer(raw,dtype='<i2').astype(float)
 with wave.open(str(art/(name+'.wav')),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(22050);f.writeframes(raw)
 stable=samples[22050:];freq=np.fft.rfftfreq(len(stable),1/22050);spectrum=abs(np.fft.rfft(stable))
 report['scenarios'].append({'name':name,'peak':int(abs(samples).max()),'rms':float(np.sqrt(np.mean(stable*stable))),'strongest_frequency_hz':float(freq[np.argmax(spectrum)]),'clipped_samples':int((abs(samples)>=31130).sum())})
assert report['scenarios'][1]['rms']>report['scenarios'][0]['rms']
assert report['scenarios'][3]['strongest_frequency_hz']>report['scenarios'][2]['strongest_frequency_hz']
assert abs(report['scenarios'][5]['strongest_frequency_hz']-850)<2
report['playback_verified']=False;report['sample_rate']=22050
(r/'validation/audio_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));sys.exit(bool(report['failures']))
