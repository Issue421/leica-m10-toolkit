#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Offline color-only references. No camera, firmware API, or network access.
800T formula/bridge: zhaoo/m10v b0ea721e01d091044a8e0db52851fd925b57442c,
FRAME's archived color_bridge.c. LUT17 arithmetic: FRAME WNX-L2 pure C kernel.
These software domains are not a physical-camera calibration.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import time

HERE = Path(__file__).resolve().parent

class MissingDependency(ValueError):
    pass


def truncdiv(n, d):
    """C signed integer division: truncate toward zero, without float loss."""
    if d <= 0:
        raise ValueError('positive denominator required')
    return n // d if n >= 0 else -((-n) // d)


def clamp8(v):
    return max(0, min(255, v))


def round_shift(v, bits):
    return (v + (1 << (bits - 1))) >> bits if v >= 0 else -((-v + (1 << (bits - 1))) >> bits)


def rgb_to_ycc(rgb):
    r, g, b = rgb
    return (clamp8(round_shift(19595*r + 38470*g + 7471*b, 16)),
            clamp8(128 + round_shift(-11059*r - 21709*g + 32768*b, 16)),
            clamp8(128 + round_shift(32768*r - 27439*g - 5329*b, 16)))


def ycc_to_rgb(y, cb, cr):
    u, v = cb - 128, cr - 128
    return (clamp8(y + round_shift(91881*v, 16)),
            clamp8(y - round_shift(22554*u + 46802*v, 16)),
            clamp8(y + round_shift(116130*u, 16)))


def current_800t_pair(y0, y1, cb, cr):
    """Exact color portion of current source ID33; spatial effects disabled.
    This is NOT the older f34a... ID33 recipe and NOT the native 4384B fitter.
    """
    if any(type(x) is not int or not 0 <= x <= 255 for x in (y0,y1,cb,cr)):
        raise ValueError('four unsigned bytes required')
    def tone(v):
        return clamp8(128 + truncdiv((v-128)*264,256) + truncdiv(255-v,255)
                      + truncdiv(3*v*(255-v),16384))
    light = (y0+y1)//2
    weight = 255 - (2*light-255 if light > 127 else 255-2*light)
    return (tone(y0), tone(y1),
            clamp8(128+truncdiv((cb-128)*238,256)+truncdiv(10*weight,255)),
            clamp8(128+truncdiv((cr-128)*238,256)+truncdiv(-16*weight,255)))


def current_800t_rgb(rgb):
    y, cb, cr = rgb_to_ycc(rgb)
    a, _, cb, cr = current_800t_pair(y,y,cb,cr)
    return ycc_to_rgb(a,cb,cr)


def regenerate_current_cube33():
    """Author's exact RGB8 duplicate-pair export and nine-decimal text."""
    lines = ['TITLE "800T"','LUT_3D_SIZE 33','DOMAIN_MIN 0 0 0','DOMAIN_MAX 1 1 1']
    for b in range(33):
        for g in range(33):
            for r in range(33):
                rgb = tuple((i*255+16)//32 for i in (r,g,b))
                lines.append(' '.join(f'{v/255:.9f}' for v in current_800t_rgb(rgb)))
    return ('\n'.join(lines)+'\n').encode()


class Cube:
    def __init__(self, size, nodes, domain_min=(0.,0.,0.), domain_max=(1.,1.,1.)):
        if size < 2 or len(nodes) != size**3:
            raise ValueError('invalid 3D cube dimensions')
        if any(not all(math.isfinite(v) for v in n) or len(n)!=3 for n in nodes):
            raise ValueError('invalid/nonfinite RGB node')
        if len(domain_min)!=3 or len(domain_max)!=3 or any(not a < b for a,b in zip(domain_min,domain_max)):
            raise ValueError('invalid cube domain')
        self.size, self.nodes = size, nodes
        self.domain_min, self.domain_max = domain_min, domain_max

    @classmethod
    def read(cls, path):
        size = None; low=(0.,0.,0.); high=(1.,1.,1.); nodes=[]
        for raw in Path(path).read_text().splitlines():
            p=raw.split('#',1)[0].strip().split()
            if not p or p[0]=='TITLE': continue
            if p[0]=='LUT_3D_SIZE': size=int(p[1]); continue
            if p[0]=='DOMAIN_MIN': low=tuple(map(float,p[1:])); continue
            if p[0]=='DOMAIN_MAX': high=tuple(map(float,p[1:])); continue
            if len(p)!=3: raise ValueError('unsupported cube record')
            nodes.append(tuple(map(float,p)))
        if size is None: raise ValueError('missing LUT_3D_SIZE')
        return cls(size,nodes,low,high)

    def sample(self, rgb):
        """R-fast tetrahedral interpolation with explicit domain clipping."""
        if len(rgb)!=3 or any(not math.isfinite(v) for v in rgb):
            raise ValueError('finite RGB triple required')
        pos=[max(0.,min(1.,(v-a)/(b-a)))*(self.size-1)
             for v,a,b in zip(rgb,self.domain_min,self.domain_max)]
        cell=[min(int(v),self.size-2) for v in pos]
        frac=[v-c for v,c in zip(pos,cell)]
        order=sorted(range(3),key=lambda i:frac[i],reverse=True)
        f=[frac[i] for i in order]
        weights=[1-f[0],f[0]-f[1],f[1]-f[2],f[2]]
        strides=[1,self.size,self.size*self.size]
        at=cell[0]+self.size*(cell[1]+self.size*cell[2]); idx=[at]
        for axis in order: idx.append(idx[-1]+strides[axis])
        return tuple(sum(w*self.nodes[i][c] for w,i in zip(weights,idx)) for c in range(3))


def read_nodes(path):
    b=Path(path).read_bytes()
    if len(b)!=29478: raise ValueError('expected 29478 bytes of big-endian RGB16 nodes')
    return struct.unpack('>14739H',b)


def lut17_rgb8(nodes, rgb):
    """Exact FRAME integer tetrahedral kernel for one RGB8 pixel."""
    if len(nodes)!=14739 or len(rgb)!=3 or any(type(v)is not int or not 0<=v<=255 for v in rgb):
        raise ValueError('17^3 RGB16 table and three unsigned bytes required')
    cells=[v>>4 for v in rgb]; frac=[16*(v&15)+(v>>4) for v in rgb]
    order=sorted(range(3),key=lambda i:frac[i],reverse=True)
    f=[frac[i] for i in order]; weights=[255-f[0],f[0]-f[1],f[1]-f[2],f[2]]
    at=3*(cells[0]+17*(cells[1]+17*cells[2])); idx=[at]
    for axis in order: idx.append(idx[-1]+(3,51,867)[axis])
    return tuple((32767+sum(w*nodes[i+c] for w,i in zip(weights,idx)))//65535 for c in range(3))


def lut17_422_pair(nodes, y0,y1,cb,cr):
    """Actual runtime Q16 422 bridge, unlike RGB source/export bridge."""
    if any(type(v)is not int or not 0<=v<=255 for v in (y0,y1,cb,cr)):
        raise ValueError('four unsigned bytes required')
    u,v=cb-128,cr-128
    rgb=[]
    for y in (y0,y1):
        q=(clamp8(y+((91881*v+32768)>>16)),
           clamp8(y+((-22554*u-46802*v+32768)>>16)),
           clamp8(y+((116130*u+32768)>>16)))
        rgb.append(lut17_rgb8(nodes,q))
    def q16(v,shift): return clamp8((v+(1<<(shift-1)))>>shift)
    ys=[q16(19595*r+38470*g+7471*b,16)for r,g,b in rgb]
    sums=[rgb[0][c]+rgb[1][c]for c in range(3)]
    return (ys[0],ys[1],q16(-11059*sums[0]-21709*sums[1]+32768*sums[2]+128*131072,17),
            q16(32768*sums[0]-27439*sums[1]-5329*sums[2]+128*131072,17))


def render_grkit_without_base_gamma(*args, **kwargs):
    raise MissingDependency('GRKit fitted parameters require the exact Standard base gamma, hue stage, CSY and scene-linear scaling; no display-sRGB renderer is supplied')


def stats(values):
    v=sorted(values);n=len(v)
    return {'samples':n,'mae_code':sum(v)/n,'rmse_code':math.sqrt(sum(x*x for x in v)/n),
            'p95_abs_component_code':v[math.ceil(.95*n)-1],'max_abs_component_code':v[-1]}


def analyze(cube_path,nodes_path,random_pixels=65536):
    cube=Cube.read(cube_path); nodes=read_nodes(nodes_path)
    coarse=Cube(17,[tuple(nodes[i+c]/65535 for c in range(3))for i in range(0,len(nodes),3)])
    rng=random.Random(800107); pixels=[(v,v,v)for v in range(256)]
    pixels += [(r,g,b)for r in (0,255)for g in (0,255)for b in (0,255)]
    pixels += [tuple(rng.randrange(256)for _ in range(3))for _ in range(random_pixels)]
    grid=[];rounding=[];combined=[];source_formula=[];runtime_formula=[];start=time.perf_counter();checksum=hashlib.sha256()
    for rgb in pixels:
        q=tuple(v/255 for v in rgb); a=cube.sample(q);b=coarse.sample(q);c=lut17_rgb8(nodes,rgb)
        checksum.update(bytes(c));direct=current_800t_rgb(rgb)
        source_formula.extend(abs(x*255-y)for x,y in zip(a,direct))
        runtime_formula.extend(abs(x-y)for x,y in zip(c,direct))
        grid.extend(abs(x-y)*255 for x,y in zip(a,b))
        rounding.extend(abs(x*255-y)for x,y in zip(b,c))
        combined.extend(abs(x*255-y)for x,y in zip(a,c))
    return {'schema':'film-layer-color-analysis-v1','pixels':len(pixels),'seed':800107,
            'source33_sha256':hashlib.sha256(Path(cube_path).read_bytes()).hexdigest(),
            'nodes_sha256':hashlib.sha256(Path(nodes_path).read_bytes()).hexdigest(),
            'input':'RGB8 code values /255; no transfer conversion',
            'source33_interpolation':'tetrahedral, explicitly chosen for this comparison',
            'node_reduction_33_to_17':stats(grid),'final_RGB8_quantization_only':stats(rounding),
            'source33_to_exact_runtime_RGB8':stats(combined),
            'direct_upstream_duplicate_pair_vs_source33':stats(source_formula),
            'direct_upstream_duplicate_pair_vs_runtime17':stats(runtime_formula),'output_sha256':checksum.hexdigest(),
            'python_reference_seconds':time.perf_counter()-start,
            'excluded':['422 bridge/subsampling differences','native ISP/Standard transform','HDF/halo/grain/spatial processing','physical camera timing']}


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    a=sub.add_parser('sample17');a.add_argument('r',type=int);a.add_argument('g',type=int);a.add_argument('b',type=int)
    a.add_argument('--nodes',type=Path,default=HERE/'targets/frame-cinestill800t/lut17-rgb16be.bin')
    a=sub.add_parser('analyze');a.add_argument('--pixels',type=int,default=65536)
    a.add_argument('--cube',type=Path,default=HERE/'targets/frame-cinestill800t/source33.cube')
    a.add_argument('--nodes',type=Path,default=HERE/'targets/frame-cinestill800t/lut17-rgb16be.bin')
    args=parser.parse_args()
    if args.command=='sample17': result={'rgb8':lut17_rgb8(read_nodes(args.nodes),(args.r,args.g,args.b))}
    else: result=analyze(args.cube,args.nodes,args.pixels)
    print(json.dumps(result,indent=2))
if __name__=='__main__': main()
