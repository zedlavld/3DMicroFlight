"""Write a cleaned, static A321 GLB: cartoon face removed, sheared control surfaces
(flaps, ailerons, elevators, rudder) replaced, gear struts trimmed. Every part is baked
to world space as its own node (original Object_N names kept); no skins or animations.
usage: python tools/clean_a321.py third_party/aircraft/airbus_a321-232.glb third_party/aircraft/a321.glb <in.glb> <out.glb> [extra parts to drop...]"""
import sys, json, struct, numpy as np
SRC, OUT = sys.argv[1], sys.argv[2]
DROP = ['Object_7', 'Object_8', 'Object_10', 'Object_11',            # eyes
        'Object_60', 'Object_61',                                     # mouth (skinned to the face bones)
        'Object_51', 'Object_55', 'Object_77',                        # sheared elevators, rudder
        'Object_83', 'Object_91', 'Object_93', 'Object_95', 'Object_97', 'Object_99',     # right flaps, aileron
        'Object_185', 'Object_197', 'Object_199'] + sys.argv[3:]     # left flaps, aileron
CLAMP = {'Object_25': -0.15, 'Object_39': -0.15}                     # main-gear struts poking through the wing
src = open(SRC, 'rb').read(); n = struct.unpack_from('<I', src, 12)[0]; d = json.loads(src[20:20 + n])
off = 20 + n; blen = struct.unpack_from('<I', src, off)[0]; binary = src[off + 8:off + 8 + blen]
def acc(i):
    a = d['accessors'][i]; v = d['bufferViews'][a['bufferView']]
    dt = {5121: 'u1', 5123: '<u2', 5125: '<u4', 5126: '<f4'}[a['componentType']]
    k = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
    item = np.dtype(dt).itemsize; o = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    return np.ndarray((a['count'], k), dtype=dt, buffer=binary, offset=o, strides=(v.get('byteStride', k * item), item)).astype(np.float64 if dt == '<f4' else np.int64)
def local(node):
    if 'matrix' in node: return np.array(node['matrix']).reshape(4, 4).T
    x, y, z, w = node.get('rotation', [0, 0, 0, 1]); m = np.eye(4)
    m[:3, :3] = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    m[:3, :3] *= np.array(node.get('scale', [1, 1, 1])); m[:3, 3] = node.get('translation', [0, 0, 0]); return m
glob = {}
def walk(i, parent):
    glob[i] = parent @ local(d['nodes'][i])
    for c in d['nodes'][i].get('children', []): walk(c, glob[i])
for r in d['scenes'][d.get('scene', 0)]['nodes']: walk(r, np.eye(4))
parts = []   # (name, material, pos, uv, faces)
for i, node in enumerate(d['nodes']):
    if 'mesh' not in node or i not in glob or node.get('name') in DROP: continue
    name = node['name']
    for p in d['meshes'][node['mesh']]['primitives']:
        at = p['attributes']; pos = acc(at['POSITION'])
        pos = (np.column_stack([pos, np.ones(len(pos))]) @ glob[i].T)[:, :3]
        if name in CLAMP: pos[:, 1] = np.minimum(pos[:, 1], CLAMP[name])
        uv = acc(at['TEXCOORD_0']) if 'TEXCOORD_0' in at else np.zeros((len(pos), 2))
        f = acc(p['indices']).reshape(-1, 3) if 'indices' in p else np.arange(len(pos)).reshape(-1, 3)
        parts.append((name, p.get('material', 0), pos, uv, f))
# Trailing-edge panels (flaps + ailerons) behind each wing, two-sided, wing material and colour.
for wing, side in (('Object_81', 1), ('Object_181', -1)):
    _, mat, wp, wuv, _ = next(t for t in parts if t[0] == wing)
    stations = [0.26, 0.82, 1.74, 2.21]; inner, outer, uvs = [], [], []
    for k, xs in enumerate(stations):
        near = np.abs(wp[:, 0] * side - xs) < 0.03; cand = wp[near]; te = cand[:, 2].min()
        rim = cand[cand[:, 2] < te + 0.02]; y = rim[:, 1].mean()
        inner.append([xs * side, y, te + 0.01])
        outer.append([xs * side, y, -1.66 + (xs - 0.26) / (2.21 - 0.26) * (-1.97 + 1.66)])
        uvs.append(wuv[np.flatnonzero(near)[np.argmin(cand[:, 2])]])
    pos = np.array(inner + outer); uv = np.array(uvs + uvs); m = len(stations); f = []
    for k in range(m - 1): f += [[k, k + 1, m + k + 1], [k, m + k + 1, m + k]]
    f = np.array(f); f = np.vstack([f, f[:, ::-1]])
    parts.append(('flap_panel_' + ('R' if side > 0 else 'L'), mat, pos, uv, f))
# ---------------------------------------------------------------- write GLB
out = {'asset': {'version': '2.0', 'generator': '3DMicroFlight a321_clean_export'}, 'scene': 0,
       'scenes': [{'nodes': []}], 'nodes': [], 'meshes': [], 'accessors': [], 'bufferViews': [],
       'materials': d['materials'], 'textures': d.get('textures', []), 'samplers': d.get('samplers', []), 'images': []}
blob = bytearray()
def view(data, target=None):
    while len(blob) % 4: blob.append(0)
    v = {'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(data)}
    if target: v['target'] = target
    blob.extend(data); out['bufferViews'].append(v); return len(out['bufferViews']) - 1
def accessor(arr, ctype, typ, target, minmax=False):
    a = {'bufferView': view(arr.tobytes(), target), 'componentType': ctype, 'count': len(arr), 'type': typ}
    if minmax: a['min'] = arr.min(0).tolist(); a['max'] = arr.max(0).tolist()
    out['accessors'].append(a); return len(out['accessors']) - 1
for im in d.get('images', []):
    v = d['bufferViews'][im['bufferView']]; o = v.get('byteOffset', 0)
    out['images'].append({'bufferView': view(binary[o:o + v['byteLength']]), 'mimeType': im.get('mimeType', 'image/png')})
total = 0
for name, mat, pos, uv, f in parts:
    prim = {'attributes': {'POSITION': accessor(pos.astype('<f4'), 5126, 'VEC3', 34962, True),
                           'TEXCOORD_0': accessor(uv.astype('<f4'), 5126, 'VEC2', 34962)},
            'indices': accessor(f.reshape(-1).astype('<u4'), 5125, 'SCALAR', 34963), 'material': mat}
    out['meshes'].append({'name': name, 'primitives': [prim]})
    out['nodes'].append({'name': name, 'mesh': len(out['meshes']) - 1}); out['scenes'][0]['nodes'].append(len(out['nodes']) - 1)
    total += len(f)
while len(blob) % 4: blob.append(0)
out['buffers'] = [{'byteLength': len(blob)}]
js = json.dumps(out, separators=(',', ':')).encode(); js += b' ' * (-len(js) % 4)
body = struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(blob), 0x004E4942) + bytes(blob)
open(OUT, 'wb').write(struct.pack('<III', 0x46546C67, 2, 12 + len(body)) + body)
allp = np.vstack([p[2] for p in parts])
print('parts', len(parts), 'triangles', total, 'extent', np.ptp(allp, 0).round(2), 'bytes', 12 + len(body))
