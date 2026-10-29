# SPDX-License-Identifier: MIT
"""Complete host evaluation of pinned GRKit effect definitions.

Math port: GRKit contributors, cc9ff4e2a97962a47b4bd825d50e84988da5e70f,
fit/model.rs, hue.rs, effects.rs, selective.rs and site.rs (MIT).
Recipe data retain their individual MIT or CC-BY-SA-4.0 notices. The public
gamma has no separately declared license and is referenced, never relicensed.
"""
from copy import deepcopy
import hashlib
import json
import math
from numbers import Real
from pathlib import Path
import struct

HERE = Path(__file__).resolve().parent
INPUT_DOMAIN = "scene-linear-srgb-primaries-unit-v1"
OUTPUT_DOMAIN = "grkit-model-display-rgb-code-unit-v1"
PINS_SHA256 = "28215c6d3319e294a10a063ad0296597b88e1f1f49ac14427b8f544e2e1666ff"
REVISION = "cc9ff4e2a97962a47b4bd825d50e84988da5e70f"
GRKIT = "sandbox/inputs/gr2-portability-20261028/grkit"
BOUNDS = (1490, 745, 0, 0, 1304, 559, 0, 0, 1862, 1118, 373, 0, 1676, 931, 187, 0)
SELECTORS = (3, 2, 1, 0, 0, 11, 10, 9, 0, 0, 9, 8, 7, 6, 0, 6, 5, 4, 3, 0)
LEVEL_CURVE = ((.040, 0.), (.055, .08), (.072, .20), (.093, .37), (.121, .62), (.158, 1.08), (.205, 1.30), (.266, 1.41), (.346, 1.79), (.450, 2.29), (.585, 2.90), (.660, 3.))
BT601 = ((77/256, 150/256, 29/256), (-43/256, -85/256, 128/256), (128/256, -107/256, -21/256))


class SourceError(ValueError):
    """A missing, corrupt or unsupported source cannot be rendered."""


def require(condition, message):
    if not condition:
        raise SourceError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_verified(root, relative, expected):
    p = Path(root) / relative
    require(p.resolve().is_relative_to(Path(root).resolve()), "dependency escapes declared root")
    try:
        raw = p.read_bytes()
    except OSError as error:
        raise SourceError("missing dependency: " + relative) from error
    require(sha(raw) == expected, "source integrity mismatch: " + relative)
    return raw


def clamp(v, lo=0., hi=1.):
    return max(lo, min(hi, v))


def rounded(v):
    return math.floor(v + .5) if v >= 0 else -math.floor(-v + .5)


def mul(m, v):
    return tuple(m[3*r]*v[0] + m[3*r+1]*v[1] + m[3*r+2]*v[2] for r in range(3))


def inverse(m):
    def co(r, c):
        r1, r2, c1, c2 = (r+1)%3, (r+2)%3, (c+1)%3, (c+2)%3
        return m[r1][c1]*m[r2][c2] - m[r1][c2]*m[r2][c1]
    det = sum(m[0][c]*co(0, c) for c in range(3))
    return tuple(co(c, r)/det for r in range(3) for c in range(3))


INV601 = inverse(BT601)
FLAT601 = tuple(v for row in BT601 for v in row)


def lookup(table, x):
    x = clamp(x, 0., 255.)
    low = min(int(x), 254)
    t = x - low
    return table[low]*(1-t) + table[low+1]*t


def integers(values, length, low, high, label):
    require(isinstance(values, list) and len(values) == length and all(type(v) is int and low <= v <= high for v in values), "invalid " + label)
    return values


def interpolate_curve(points):
    """effects.rs:585–617: integer products, f64 divide, ties-to-even."""
    require(isinstance(points, list) and len(points) >= 2, "invalid curve knots")
    for pair in points:
        integers(pair, 2, 0, 16383, "curve knot")
        require(pair[0] <= 255, "invalid curve knot index")
    require(points[0][0] == 0 and points[-1][0] == 255 and all(a[0] < b[0] for a, b in zip(points, points[1:])), "curve knots must span increasing indices 0..255")
    result = []
    for (left, low), (right, high) in zip(points, points[1:]):
        result.extend(round(low + (high-low)*(index-left)/(right-left)) for index in range(left, right))
    return result + [points[-1][1]]


def hue_controls(h):
    starts = [rounded(w*1024/255) for w in h["weight"]]
    b = (0, 164, 655, 4095)
    slopes = [rounded((starts[k+1]-starts[k])*32/(b[k+1]-b[k])) if k < 3 else 0 for k in range(4)]
    return starts, slopes


def validate_hue(h):
    require(isinstance(h, dict) and {"matrices", "weight"} <= h.keys() <= {"matrices", "weight", "levels"}, "invalid hue sector fields")
    integers(h["weight"], 4, 0, 255, "hue weights")
    require("levels" not in h or isinstance(h["levels"], list) and len(h["levels"]) == 3, "invalid hue levels")
    for level in [h["matrices"]] + h.get("levels", []):
        require(isinstance(level, list) and len(level) == 11, "invalid hue sectors")
        for offsets in level:
            integers(offsets, 6, -127, 127, "hue coefficients")
            require(all(offsets[k] + offsets[k+1] >= -127 for k in (0, 2, 4)), "hue row leaves register range")
    require(all(-128 <= v <= 127 for v in hue_controls(h)[1]), "hue weight changes too fast")


def selective_parameters(s):
    require(isinstance(s, dict) and set(s) == {"hue", "lower", "higher", "chroma"}, "unsupported selective color fields")
    h = s["hue"]
    require(type(h) in (int, float) and math.isfinite(h) and 0 <= h < 360, "invalid selective hue")
    sides = []
    for key in ("lower", "higher"):
        a = s[key]
        require(isinstance(a, list) and len(a) == 2 and all(type(v) in (int, float) and math.isfinite(v) for v in a) and 0 <= a[0] < a[1] <= 75, "invalid selective width")
        sides.append([rounded(math.tan(math.radians(v))*256) for v in a])
    c = integers(s["chroma"], 2, 0, 181, "selective chroma")
    require(c[0] < c[1], "invalid selective chroma gate")
    direction = [rounded(math.cos(math.radians(h))*4096), rounded(math.sin(math.radians(h))*4096)]
    require(15000000 <= sum(v*v for v in direction) <= 18000000 and all(a[0] < a[1] <= 1024 for a in sides), "invalid quantized selective parameters")
    return direction, sides, c


def selective_table(s):
    (hc, hs), sides, (c0, c1) = selective_parameters(s)
    angle = [[0] + [(256 << 16)//((t1-t0)*a) for a in range(1, 182)] for t0, t1 in sides]
    gate = (256 << 16)//(c1-c0)
    table = []
    for j in range(128):
        for i in range(128):
            u, v = 2*i+1-128, 2*j+1-128
            along, across = (u*hc+v*hs) >> 12, (v*hc-u*hs) >> 12
            w = 0
            if along > 0:
                a, b, side = min(along, 181), abs(across), int(across > 0)
                t0, t1 = sides[side]
                limit, full = t1*a, t0*a
                if b << 8 < limit and a > c0:
                    w = 256 if b << 8 <= full else ((limit-(b << 8))*angle[side][a]) >> 16
                    if a < c1:
                        w = (w * (((a-c0)*gate) >> 16)) >> 8
            table.append(min(w, 255))
    return table


def selective_apply(table, rgb):
    r, g, b = [rounded(clamp(v)*255) for v in rgb]
    y = clamp((77*r+150*g+29*b+128) >> 8, 0, 255)
    cb = int(clamp(128+((-43*r-85*g+128*b+128) >> 8), 0, 255))
    cr = int(clamp(128+((128*r-107*g-21*b+128) >> 8), 0, 255))
    w = table[(cr >> 1)*128+(cb >> 1)]
    w += int(w == 255)
    u, v = ((((c-128)*w+128) >> 8) for c in (cb, cr))
    return tuple(clamp(c/255) for c in (y+1.402*v, y-.344136*u-.714136*v, y+1.772*u))


def parse_effect(effect):
    require(isinstance(effect, dict), "effect must be an object")
    required = {"schema", "effect_id", "name", "flags", "matrix", "attribution"}
    allowed = required | {"icon", "curves", "curve_knots", "post_matrix", "hue_sectors", "selective_color"}
    require(required <= effect.keys() <= allowed and effect["schema"] == "grkit-effect-v1", "missing or unsupported effect fields")
    require(("curves" in effect) != ("curve_knots" in effect), "exactly one curve representation required")
    effect = deepcopy(effect)
    require(isinstance(effect["effect_id"], str) and 0 < len(effect["effect_id"]) <= 63 and all(c in "abcdefghijklmnopqrstuvwxyz0123456789-" for c in effect["effect_id"]), "invalid effect ID")
    require(isinstance(effect["name"], str) and 1 <= len(effect["name"]) <= 16 and all(32 <= ord(c) <= 126 for c in effect["name"]), "invalid effect name")
    require(type(effect["flags"]) is int and effect["flags"] in (0, 1), "unsupported effect flags")
    integers(effect["matrix"], 9, -32768, 32767, "front matrix")
    if "curve_knots" in effect:
        require(isinstance(effect["curve_knots"], list) and len(effect["curve_knots"]) == 3, "invalid knot channels")
        effect["curves"] = [interpolate_curve(p) for p in effect.pop("curve_knots")]
    require(isinstance(effect["curves"], list) and len(effect["curves"]) == 3, "missing curve channels")
    for curve in effect["curves"]:
        integers(curve, 256, 0, 16383, "14-bit curve")
    if "post_matrix" in effect:
        integers(effect["post_matrix"], 9, -32768, 32767, "post matrix")
    if "hue_sectors" in effect:
        validate_hue(effect["hue_sectors"])
    if "selective_color" in effect:
        selective_parameters(effect["selective_color"])
    if effect["flags"] == 1:
        require(not any(k in effect for k in ("post_matrix", "hue_sectors", "selective_color")), "monochrome forbids post/hue/selective stages")
        m, c = effect["matrix"], effect["curves"]
        require(m[:3] == m[3:6] == m[6:] and c[0] == c[1] == c[2], "monochrome requires equal matrix rows and curves")
    return effect


class Model:
    """Pure source-software model. No sensor, M10, physical-film or device claim."""
    def __init__(self, effect, gamma):
        self.effect = parse_effect(effect)
        integers(gamma, 256, 0, 16384, "base gamma")
        require(gamma[0] == 0 and 16000 <= gamma[-1] <= 16384 and all(a <= b for a, b in zip(gamma, gamma[1:])), "noncanonical gamma")
        self.gamma = gamma
        self.matrix = tuple(v/8192 for v in self.effect["matrix"])
        # site.rs:65,67 explicitly defines these absent-stage semantics.
        self.post = tuple(v/8192 for v in self.effect.get("post_matrix", [8192, 0, 0, 0, 8192, 0, 0, 0, 8192]))
        self.curves = self.effect["curves"]
        self.hue = self.effect.get("hue_sectors")
        self.selective = selective_table(self.effect["selective_color"]) if "selective_color" in self.effect else None
        self.levels = []
        if self.hue:
            self.starts, self.slopes = hue_controls(self.hue)
            for level in [self.hue["matrices"]] + self.hue.get("levels", []):
                matrices = []
                for offsets in level:
                    m = [0.]*9
                    for r in range(3):
                        a, b = offsets[r*2]/128, offsets[r*2+1]/128
                        m[3*r+(r+1)%3], m[3*r+(r+2)%3], m[4*r] = a, b, 1-a-b
                    matrices.append(m)
                self.levels.append(matrices)

    def hue_apply(self, v):
        if not self.hue:
            return v
        y, cb, cr = mul(FLAT601, v)
        if cr >= 0:
            quarter, a, b = (0, cr, cb) if cb >= 0 else (3, -cb, cr)
        else:
            quarter, a, b = (1, cb, -cr) if cb >= 0 else (2, -cr, -cb)
        if a+b <= 1e-12:
            return v
        coordinate = quarter*2048 + 2048*b/(a+b)
        quarter = min(int(coordinate/2048), 3)
        q = (0, 3, 2, 1)[quarter]
        local = math.floor(coordinate-quarter*2048)
        n = next((i for i, bound in enumerate(BOUNDS[q*4:q*4+4]) if local >= bound), 4)
        bank = SELECTORS[q*5+n]
        if bank == 0:
            return v
        x = clamp(3000*y, 0., 4095.)
        breaks = (0, 164, 655, 4095)
        k = next(k for k in (3, 2, 1, 0) if x >= breaks[k])
        weight = clamp((self.starts[k]+self.slopes[k]*(x-breaks[k])/32)/1024)
        position = 0.
        if len(self.levels) > 1:
            if y >= LEVEL_CURVE[-1][0]:
                position = 3.
            elif y > LEVEL_CURVE[0][0]:
                k = next(k for k in range(11) if y < LEVEL_CURVE[k+1][0])
                a, b = LEVEL_CURVE[k], LEVEL_CURVE[k+1]
                position = a[1]+(y-a[0])/(b[0]-a[0])*(b[1]-a[1])
        low = min(math.floor(position), len(self.levels)-1)
        high = min(low+1, len(self.levels)-1)
        f = position-low
        a, b = self.levels[low][bank-1], self.levels[high][bank-1]
        mixed = mul([x+f*(z-x) for x, z in zip(a, b)], v)
        return tuple(max(0., x+weight*(z-x)) for x, z in zip(v, mixed))

    def render(self, linear):
        mixed = self.hue_apply(mul(self.matrix, linear))
        codes = tuple(lookup(self.curves[c], lookup(self.gamma, 7.8/.18*mixed[c])/64)/16384 for c in range(3))
        codes = mul(self.post, codes)
        ycc = mul(FLAT601, codes)
        y = clamp(ycc[0]*1023, 0., 1023.)
        b = (0., 56., 768., 856.)
        k = next(k for k in (3, 2, 1, 0) if y >= b[k])
        gain = clamp((0., 512., 512., 432.)[k]+(73., 0., -7., -14.)[k]*(y-b[k])/8, 0., 1023.)/512
        if gain != 1.:
            codes = mul(INV601, (ycc[0], ycc[1]*gain, ycc[2]*gain))
        display = tuple(clamp(v) for v in codes)
        return selective_apply(self.selective, display) if self.selective is not None else display


def recipe_delta(effect):
    """Keep only each look's parameters and attribution; no per-look engine or icon."""
    e = deepcopy(effect)
    e.pop("icon", None)
    e["attribution"].pop("fit", None)
    return e


