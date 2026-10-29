#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""GRKit source-model host reference, ported from ahonn/grkit cc9ff4e2.
Original MIT code attribution: GRKit contributors, copied LICENSE in targets/reference-sources/grkit.
Effects: Andrea Volpato/spektrafilm + GRKit fits, CC-BY-SA-4.0.
Input is scene-linear sRGB-primaries RGB. Output is display RGB code values.
This is the published GRKit software model, never an M10 output simulator.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
HERE=Path(__file__).resolve().parent
CATALOG_SHA='a434cdfbaf3ce0e89e8183813f84f4412087f19551ab7664d04516feed53415b'
GAMMA_SHA='2c8f21661161934be5b419e613dca60b9c2b9ba7eafec747c0afdbaf3077ae0f'
IDS=('velvia-100-v1','ektachrome-e100-v1','kodachrome-64-v1')
BOUNDS=(1490,745,0,0,1304,559,0,0,1862,1118,373,0,1676,931,187,0)
SELECTORS=(3,2,1,0,0,11,10,9,0,0,9,8,7,6,0,6,5,4,3,0)
LEVEL_CURVE=((.040,0.),(.055,.08),(.072,.20),(.093,.37),(.121,.62),(.158,1.08),(.205,1.30),(.266,1.41),(.346,1.79),(.450,2.29),(.585,2.90),(.660,3.))
BT601=((77/256,150/256,29/256),(-43/256,-85/256,128/256),(128/256,-107/256,-21/256))

def require(v,message):
    if not v:raise ValueError(message)

def sha(b):return hashlib.sha256(b).hexdigest()

def clamp(v,lo,hi):return max(lo,min(hi,v))

def rounded(v):return math.floor(v+.5)if v>=0 else -math.floor(-v+.5)

def mul(m,v):return tuple(m[3*r]*v[0]+m[3*r+1]*v[1]+m[3*r+2]*v[2]for r in range(3))

def inverse(m):
    def co(r,c):
        r1,r2,c1,c2=(r+1)%3,(r+2)%3,(c+1)%3,(c+2)%3
        return m[r1][c1]*m[r2][c2]-m[r1][c2]*m[r2][c1]
    det=sum(m[0][c]*co(0,c)for c in range(3))
    return tuple(co(c,r)/det for r in range(3)for c in range(3))

INV601=inverse(BT601)

def lookup(table,x):
    x=clamp(x,0.,255.);low=min(int(x),254);t=x-low
    return table[low]*(1-t)+table[low+1]*t

def srgb_decode(v):return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4

def hue_controls(h):
    starts=[rounded(w*1024/255)for w in h['weight']];b=(0,164,655,4095)
    slopes=[rounded((starts[k+1]-starts[k])*32/(b[k+1]-b[k]))if k<3 else 0 for k in range(4)]
    return starts,slopes

class Model:
    def __init__(self,effect,base_gamma):
        require(base_gamma is not None,'exact base gamma required; no default')
        require(len(base_gamma)==256 and all(type(x)is int and 0<=x<=16384 for x in base_gamma),'invalid gamma')
        require(base_gamma[0]==0 and 16000<=base_gamma[-1]<=16384 and all(a<=b for a,b in zip(base_gamma,base_gamma[1:])),'noncanonical gamma')
        self.gamma=base_gamma;self.matrix=tuple(v/8192 for v in effect['matrix']);self.post=tuple(v/8192 for v in effect['post_matrix']);self.curves=effect['curves'];self.hue=effect['hue_sectors']
        self.starts,self.slopes=hue_controls(self.hue)
        self.levels=[]
        for level in [self.hue['matrices']]+self.hue['levels']:
            matrices=[]
            for offsets in level:
                m=[0.]*9
                for r in range(3):
                    a,b=offsets[r*2]/128,offsets[r*2+1]/128
                    m[3*r+(r+1)%3]=a;m[3*r+(r+2)%3]=b;m[4*r]=1-a-b
                matrices.append(m)
            self.levels.append(matrices)

    def hue_apply(self,v):
        y,cb,cr=mul(tuple(x for row in BT601 for x in row),v)
        if cr>=0:
            quarter,a,b=(0,cr,cb)if cb>=0 else(3,-cb,cr)
        else:quarter,a,b=(1,cb,-cr)if cb>=0 else(2,-cr,-cb)
        if a+b<=1e-12:return v
        coordinate=quarter*2048+2048*b/(a+b);q=(0,3,2,1)[min(int(coordinate/2048),3)]
        local=math.floor(coordinate-min(int(coordinate/2048),3)*2048)
        n=next((i for i,bound in enumerate(BOUNDS[q*4:q*4+4])if local>=bound),4);bank=SELECTORS[q*5+n]
        if bank==0:return v
        x=clamp(3000*y,0.,4095.);breaks=(0,164,655,4095);k=next(k for k in (3,2,1,0)if x>=breaks[k])
        weight=clamp((self.starts[k]+self.slopes[k]*(x-breaks[k])/32)/1024,0.,1.)
        if y<=LEVEL_CURVE[0][0]:position=0.
        elif y>=LEVEL_CURVE[-1][0]:position=3.
        else:
            k=next(k for k in range(11)if y<LEVEL_CURVE[k+1][0]);a,b=LEVEL_CURVE[k],LEVEL_CURVE[k+1]
            position=a[1]+(y-a[0])/(b[0]-a[0])*(b[1]-a[1])
        low=min(math.floor(position),3);high=min(low+1,3);f=position-low
        a,b=self.levels[low][bank-1],self.levels[high][bank-1];m=[x+f*(z-x)for x,z in zip(a,b)];mixed=mul(m,v)
        return tuple(max(0.,x+weight*(z-x))for x,z in zip(v,mixed))

    def render(self,linear):
        require(len(linear)==3 and all(math.isfinite(x)for x in linear),'finite linear triple required')
        mixed=self.hue_apply(mul(self.matrix,linear))
        codes=tuple(lookup(self.curves[c],lookup(self.gamma,7.8/.18*mixed[c])/64)/16384 for c in range(3))
        codes=mul(self.post,codes);ycc=mul(tuple(x for row in BT601 for x in row),codes)
        y=clamp(ycc[0]*1023,0.,1023.);b=(0.,56.,768.,856.);k=next(k for k in (3,2,1,0)if y>=b[k])
        gain=clamp((0.,512.,512.,432.)[k]+(73.,0.,-7.,-14.)[k]*(y-b[k])/8,0.,1023.)/512
        if gain!=1.:codes=mul(INV601,(ycc[0],ycc[1]*gain,ycc[2]*gain))
        return tuple(clamp(v,0.,1.)for v in codes)

    def grid(self,size=33):
        axis=[srgb_decode(i/(size-1))for i in range(size)];result=bytearray()
        for b in axis:
            for g in axis:
                for r in axis:result.extend(rounded(v*255)for v in self.render((r,g,b)))
        return bytes(result)

def verify_grf_parameters(blob,e):
    require(len(blob)==8704 and blob[:8]==b'GRFX0001','unexpected GRF')
    require(struct.unpack_from('<I',blob,8)[0]==len(blob),'GRF length mismatch')
    require(struct.unpack_from('<I',blob,12)[0]==zlib.crc32(blob[:12]+bytes(4)+blob[16:]),'GRF CRC mismatch')
    require(list(struct.unpack_from('<9h',blob,128))==e['matrix'],'published look matrix differs')
    require(list(struct.unpack_from('<9h',blob,88))==e['post_matrix'],'published post differs')
    require([list(struct.unpack_from('<256H',blob,148+512*c))for c in range(3)]==e['curves'],'published curves differ')
    h=e['hue_sectors'];require(list(struct.unpack_from('<66b',blob,8084))==[v for a in h['matrices']for v in a],'published hue base differs')
    require(list(struct.unpack_from('<198b',blob,8192))==[v for l in h['levels']for a in l for v in a],'published hue levels differ')
    starts,slopes=hue_controls(h);fields=starts+[v&511 for v in slopes]+[164,655,4095]
    require(list(struct.unpack_from('<11H',blob,8150))==fields,'published hue controls differ')
