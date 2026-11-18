"""Regression: preserve every original SF50 shell triangle through conversion.
Requires numpy. Intentional source openings (windows, intake, exhaust) remain.
"""
from pathlib import Path
from collections import Counter
import json, struct
import numpy as np
from glb_reader import Glb
root=Path(__file__).resolve().parent.parent
meshes=list(Glb(root/'third_party/aircraft/sf50.glb').meshes())
allp=np.concatenate([m[2] for m in meshes]);sx=9.35736/np.ptp(allp[:,2]);sy=11.79576/np.ptp(allp[:,0])
offset=4.05-allp[:,2].max()*sx;expected=[];body=None;old=None
for _,material,p,uv,f in meshes:
 p=np.column_stack([p[:,2]*sx+offset,(p[:,0]-(allp[:,0].min()+allp[:,0].max())/2)*sy,-p[:,1]*sy]);f=f[:,[0,2,1]]
 expected.append(p[f])
 if material==1:
  body=p[f];c=body.mean(1)
  keep=~((body[:,:,2].max(1)>.51)|((abs(c[:,1])>2.8)&(abs(c[:,1])<3.5)&(c[:,2]>.32)))
  old=body[keep]
def boundary_edges(triangles):
 _,ids=np.unique(np.round(triangles.reshape(-1,3),5),axis=0,return_inverse=True)
 edges=Counter(tuple(sorted((int(a),int(b)))) for face in ids.reshape(-1,3) for a,b in zip(face,np.roll(face,1)))
 return sum(n==1 for n in edges.values())
raw=(root/'data/sf50_imported_mesh.bin').read_bytes();count=struct.unpack_from('<I',raw)[0]
actual=np.frombuffer(raw,dtype='<f4',offset=24).reshape(count,19)[:,:18].reshape(count,3,6)[:,:,:3]
expected=np.concatenate(expected).astype('<f4')
assert count==len(expected)==7294 and np.array_equal(actual,expected)
report={'source_shell_triangles':count,'restored_body_triangles':len(body)-len(old),'old_body_boundary_edges':boundary_edges(old),'repaired_body_boundary_edges':boundary_edges(body),'preserved_all_source_triangle_positions':True,'note':'Retains original source openings; no claim of a watertight source model. Additional fan and gear are generated at runtime.'}
(root/'validation/sf50_shell_report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
