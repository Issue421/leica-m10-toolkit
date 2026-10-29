"""Host-only exact specification for the NOT-INSTALLABLE fixed M10 component."""
from fractions import Fraction
import struct

class ConfigError(ValueError):
    pass

GRID_DENOMINATOR=256
GRID_MIN=64
GRID_MAX=1024
PRODUCT_MAX=32767*GRID_MAX

def normalize(config):
    if config is None:
        return {"schema_version":1,"multipliers":None}, None
    if type(config) is not dict or set(config) != {"schema_version","multipliers"}:
        raise ConfigError("expected only schema_version and multipliers")
    if type(config["schema_version"]) is not int or config["schema_version"] != 1:
        raise ConfigError("schema_version must be integer1")
    values=config["multipliers"]
    if values is None:
        return {"schema_version":1,"multipliers":None},None
    if type(values) is not dict or set(values) != {"R","G","B"}:
        raise ConfigError("multipliers require exactly R/G/B")
    gains=[]
    for channel in ("R","G","B"):
        pair=values[channel]
        if type(pair) is not dict or set(pair) != {"numerator","denominator"}:
            raise ConfigError("each multiplier requires numerator/denominator")
        n,d=pair["numerator"],pair["denominator"]
        if type(n) is not int or type(d) is not int or not 1<=n<=0xffffffff or not 1<=d<=0xffffffff:
            raise ConfigError("positive u32 numerator and denominator required")
        value=Fraction(n,d)
        if not Fraction(1,4)<=value<=4:
            raise ConfigError("dimensionless multiplier outside [1/4,4]")
        gains.append(value)
    normalized={"schema_version":1,"multipliers":{
        ch:{"numerator":v.numerator,"denominator":v.denominator}
        for ch,v in zip(("R","G","B"),gains)}}
    if gains[0]==gains[1]==gains[2]:
        return normalized,None
    grid=tuple(g*GRID_DENOMINATOR for g in gains)
    if any(v.denominator!=1 for v in grid):
        raise ConfigError("unequal multipliers require exact 1/256 grid in [1/4,4]")
    return normalized,tuple(int(v) for v in grid)

def original24(channels):
    return struct.pack("<6I",*(v for w in channels for v in (256,w & 0xffffffff)))

def apply24(original,config):
    """Apply after factory rebuild. Never mutate the supplied bytes."""
    if type(original) is not bytes or len(original)!=24:
        raise ValueError("exact24-byte immutable original required")
    try:
        _,grid=normalize(config)
    except ConfigError:
        return original,"invalid-or-unsupported-config"
    if grid is None:
        return original,"original"
    values=struct.unpack("<6I",original)
    channels=values[1::2]
    if values[0::2]!=(256,256,256) or any(not 1<=w<=32767 for w in channels):
        return original,"invalid-original"
    out=[]
    for index in (0,2):
        numerator=channels[1]*grid[1]
        denominator=channels[index]*grid[index]
        value=Fraction(numerator,denominator)
        if not 1<=value.numerator<=0xffffffff or not 1<=value.denominator<=0xffffffff:
            return original,"overflow"
        out.append((value.numerator,value.denominator))
    result=struct.pack("<6I",*out[0],1,1,*out[1])
    return result,"applied"
