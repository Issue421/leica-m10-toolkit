"""One strict, portable configuration shared by CLI, UI and host execution."""
import copy
import json
import re
from fractions import Fraction
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'M10-4.26.36.10'
OFFICIAL = dict(url='https://leica-camera.com/sites/default/files/M10-4.26.36.10-Customer.FW',
                bytes=127615696, sha256='d6ef5b77b70451d2b6a31fc4a090bf8f664c1e07a8bebade657aff7da692423c',
                img_sha256='393d3c13a13e88ef43b58a07585dce06ce667f7c596da849f41af4653d9a0460')
DEFAULT = {'schema':'leica.source-selection/v1', 'baseline':BASELINE, 'features':{
    'touch':False, 'styles':{'enabled':['normal'],'default':'normal'},
    'frame_guides':{'enabled':False,'preset':0}, 'jpeg_crop':False,
    'wb':{'enabled':False,'gains':['1','1','1']}, 'low_noise_shutter':False}}
STYLE_CATALOG=json.loads((ROOT/'leica_source/style_catalog.json').read_text())
# Original public v1 configuration IDs remain stable. New recipes use their
# accepted canonical IDs; no second active-selection/controller model is added.
STYLE_ALIASES={'normal':'normal','asc_classic':'asc-classic','asc_contemporary':'asc-contemporary','cinestill800t':'800t','velvia100':'velvia-100-v1','e100':'ektachrome-e100-v1','kodachrome64':'kodachrome-64-v1'}
STYLES=tuple(STYLE_ALIASES)+tuple(r['id'] for r in STYLE_CATALOG if r['id'] not in STYLE_ALIASES.values())
STYLE_FILMS={s:STYLE_ALIASES.get(s,s) for s in STYLES if STYLE_ALIASES.get(s,s) not in ('normal','asc-classic','asc-contemporary')}
MODULE_DEPENDENCIES={'controller':('asc','framing')}

class ConfigError(ValueError): pass

def strict_json(raw):
    def pairs(items):
        out={}
        for k,v in items:
            if k in out: raise ConfigError('duplicate key: '+k)
            out[k]=v
        return out
    try:
        return json.loads(raw, object_pairs_hook=pairs,
                          parse_constant=lambda x: (_ for _ in ()).throw(ConfigError('nonfinite JSON: '+x)))
    except (ValueError, TypeError) as e: raise ConfigError(str(e)) from e

def keys(obj, expected, name):
    if type(obj) is not dict or set(obj)!=set(expected):
        raise ConfigError(name+' requires exactly: '+', '.join(expected))

def validate(value):
    keys(value, DEFAULT, 'config')
    if value['schema'] != DEFAULT['schema'] or value['baseline'] != BASELINE:
        raise ConfigError('only schema v1 and ordinary '+BASELINE+' are supported')
    f=value['features']; keys(f, DEFAULT['features'], 'features')
    for k in ('touch','jpeg_crop','low_noise_shutter'):
        if type(f[k]) is not bool: raise ConfigError(k+' must be boolean')
    styles=f['styles'];keys(styles,('enabled','default'),'styles')
    if type(styles['enabled']) is not list or not styles['enabled'] or any(type(v) is not str or v not in STYLES for v in styles['enabled']) or len(set(styles['enabled']))!=len(styles['enabled']): raise ConfigError('enabled styles must be a nonempty unique list of known style IDs')
    if 'normal' not in styles['enabled'] or type(styles['default']) is not str or styles['default'] not in styles['enabled']: raise ConfigError('normal and the one default style must be enabled')
    g=f['frame_guides']; keys(g, ('enabled','preset'), 'frame_guides')
    if type(g['enabled']) is not bool or type(g['preset']) is not int or not 0<=g['preset']<8: raise ConfigError('frame preset must be integer 0..7')
    w=f['wb']; keys(w, ('enabled','gains'), 'wb')
    if type(w['enabled']) is not bool or type(w['gains']) is not list or len(w['gains'])!=3: raise ConfigError('WB requires enabled and three rational strings')
    for v in w['gains']:
        if type(v) is not str or len(v)>32 or not re.fullmatch(r'[0-9]+(?:\.[0-9]+|/[0-9]+)?',v): raise ConfigError('WB gains must be positive decimal or rational strings')
        try: q=Fraction(v)
        except (ValueError, ZeroDivisionError) as e: raise ConfigError('invalid WB rational') from e
        if not Fraction(1,8)<=q<=8 or max(q.numerator,q.denominator)>0xffffffff: raise ConfigError('WB gains require 1/8..8 and uint32 rational terms')
    result=copy.deepcopy(value)
    result['features']['styles']['enabled']=[v for v in STYLES if v in styles['enabled']]
    missing=set(_selected_modules(result['features']))-set(available_modules())
    if missing: raise ConfigError('Selected modules are unavailable in this source bundle: '+', '.join(sorted(missing))+'. Use a source checkout that includes them.')
    return result

def load(path):
    raw=Path(path).read_bytes()
    if len(raw)>65536: raise ConfigError('config exceeds 64 KiB')
    return validate(strict_json(raw))

def canonical(value): return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2)+'\n').encode()

def _selected_modules(f):
    result=set()
    if f['touch']: result.add('touch')
    if any(v.startswith('asc_') for v in f['styles']['enabled']): result.add('asc')
    if any(v in STYLE_FILMS for v in f['styles']['enabled']): result.add('film')
    if f['frame_guides']['enabled']: result.add('framing')
    if f['wb']['enabled']: result.add('wb')
    if 'asc' in result or 'framing' in result:result.add('controller')
    for module in tuple(result):result.update(MODULE_DEPENDENCIES.get(module,()))
    return sorted(result)

def selected_modules(config):
    return _selected_modules(validate(config)['features'])

def available_modules():
    return [m for m in ('asc','controller','film','framing','touch','wb') if (ROOT/'modules'/m).is_dir()]

def capabilities():
    modules=available_modules()
    styles=[s for s in STYLES if s=='normal' or ('asc' if s.startswith('asc_') else 'film') in modules]
    return {'modules':modules,'styles':styles,'features':{
        'touch':'touch' in modules,
        'frame_guides':'framing' in modules,'wb':'wb' in modules,
        'jpeg_crop':True,'low_noise_shutter':True}}

def default_config():
    frozen=ROOT/'selection.json'
    if frozen.is_file(): return load(frozen)
    result=copy.deepcopy(DEFAULT)
    result['features']['styles']['enabled']=capabilities()['styles']
    return validate(result)

CATALOG = [
 dict(id='touch',name='普通触摸 / Normal touch',module='touch',source_status='Executable C producer tail + persistent caller/result pool',native_blockers='Real producer callsite/7-argument ABI, pool owner lifetime/address/synchronization, loader and recovery unbound.'),
 dict(id='styles.asc',name='ASC Classic / Contemporary',module='asc',source_status='Unified enabled/default styles; real resource parser, CC1/RGBY plans and ordered C apply; local pinned combined B2Y required',native_blockers='Not a pixel renderer. Native acquire/idle/cache/hardware/epoch/restore/publish imports closed. Licensed original resource is not redistributed.'),
 dict(id='styles.film',name='800T + 18 GRKit recipes (14 color / 4 monochrome)',module='film',source_status='Same style selector; actual accepted generalized engine handles optional post/hue, monochrome, curve knots and selective color. Exact local gamma required.',native_blockers='Host reference domains only. M10 color domain, bank codec, native selector, capacity/performance and loader remain unqualified.'),
 dict(id='frame_guides',name='ASC 取景框 / Frame guides',module='framing',source_status='Executable 8-preset geometry + accepted ordinary M10 u16-ID backend',native_blockers='Real GUI owner/lock/mapping/display, same-frame stock-hide and redraw/exit restoration + five owner imports unbound.'),
 dict(id='jpeg_crop',name='JPEG 裁切 / JPEG crop',module=None,source_status='Selection recorded; native crop implementation unavailable',native_blockers='Separate output ROI/encoder/capture ownership/metadata restoration required; guides do not crop JPEG or DNG.'),
 dict(id='wb',name='WB / DNG 白平衡校准',module='wb',source_status='Executable rational AsShotNeutral C core + capture adapter/purpose source',native_blockers='Native same-capture asynchronous completion/config publication and serializer owner unbound; no full DNG file writer.'),
 dict(id='low_noise_shutter',name='低噪快门 / Low-noise shutter',module=None,source_status='No qualified implementation',native_blockers='Hardware/drive/acoustic behavior and safe ordinary-M10 control path unproved; no software switch invents it.')]
