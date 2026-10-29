"""Deterministic source closure, never a firmware assembler."""
import hashlib
import io
import json
from pathlib import Path
import zipfile
from .config import ROOT, canonical, selected_modules, strict_json, validate

class FirmwareUnavailable(ValueError): pass

def refuse_firmware(*_args, **_kwargs):
    raise FirmwareUnavailable('Custom .FW unavailable: no admitted native patch provider/placement/ABI/loader/recovery. Ordinary SD also requires a strictly newer version; baseline version will not be relabelled.')

def public_files(config):
    modules=selected_modules(config); result={}
    for module in modules:
        if not (ROOT/'modules'/module).is_dir():raise ValueError('Selected module is absent from this source bundle: '+module+'. Use a source checkout that includes it.')
    for p in sorted(ROOT.rglob('*')):
        if not p.is_file(): continue
        rel=p.relative_to(ROOT).as_posix(); parts=rel.split('/')
        if p.is_symlink(): raise ValueError('symlink in source tree: '+rel)
        if any(x in {'__pycache__','.git','build','dist','.pytest_cache'} for x in parts): continue
        if parts[0]=='modules' and (len(parts)<2 or parts[1] not in modules): continue
        if parts[0]=='host' and len(parts)>1 and parts[1] not in modules: continue
        if p.suffix.lower() in {'.pyc','.o','.so','.dylib','.dll','.exe','.bin','.fw','.img','.zip'}: continue
        if parts[0] not in {'leica_source','web','codec','modules','host','LICENSES','docs','tests','.github','examples'} and rel not in {'README.md','README.en.md','LICENSE','NOTICE.md','CONTRIBUTING.md','ROADMAP.md','.gitignore','launch.py','provenance.json'}: continue
        if parts[0]=='examples' and p.suffix=='.json':
            example=strict_json(p.read_bytes())
            if type(example) is dict and example.get('schema') in ('m10-native-composition/v1','m10-native-composition/v2'):
                from .native.portable import normalize as native_normalize
                native_normalize(example)
            elif not set(selected_modules(example))<=set(modules): continue
        result[rel]=p.read_bytes()
    provenance=json.loads(result['provenance.json'])
    provenance['files']=[r for r in provenance['files'] if r['path'] in result]
    result['provenance.json']=canonical(provenance)
    result['selection.json']=canonical(validate(config))
    result['SOURCE-BUNDLE.md']=('Development source bundle / 开发源码包\n\nSelected source modules: '+(', '.join(modules) or 'none (normal reference only)')+'\n\nThe UI and CLI start from the frozen selection.json. Re-export with:\n\n    python3 -m leica_source validate selection.json\n    python3 -m leica_source build-source --config selection.json --output source.zip\n    python3 -m leica_source run-reference --config selection.json --rgb 120,80,40\n\nOnly examples whose required modules are present are included. ASC and slide defaults still require their documented local inputs.\n\nThis ZIP is not firmware. Do not copy it to an SD card as an update. Custom .FW export is unavailable. See README.md.\n').encode()
    return result

def archive_bytes(config):
    files=public_files(config)
    files['SOURCE-HASHES.json']=canonical({n:hashlib.sha256(b).hexdigest() for n,b in sorted(files.items())})
    out=io.BytesIO()
    with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_STORED) as z:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo('leica-m10-open-firmware/'+name,(2026,10,29,0,0,0))
            info.create_system=3; info.external_attr=0o100644<<16
            z.writestr(info,data)
    return out.getvalue()

def build(config, output):
    p=Path(output)
    if p.suffix.lower()!='.zip': refuse_firmware()
    data=archive_bytes(config)
    with p.open('xb') as f: f.write(data)
    return dict(artifact_class='development-source', bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), firmware=False)
