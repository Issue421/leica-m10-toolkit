import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
from .config import OFFICIAL, ROOT, canonical, default_config, load
from .export import build, refuse_firmware

def verify_official(path):
    p=Path(path)
    if not stat.S_ISREG(p.lstat().st_mode) or p.is_symlink(): raise ValueError('choose a regular local official FW file')
    flags=os.O_RDONLY|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_NONBLOCK',0)
    with os.fdopen(os.open(p,flags),'rb') as stream:
        before=os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or before.st_size!=OFFICIAL['bytes']: raise ValueError('official M10 size mismatch')
        h=hashlib.sha256()
        while True:
            block=stream.read(1024*1024)
            if not block: break
            h.update(block)
        after=os.fstat(stream.fileno())
    if (before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns)!=(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns): raise ValueError('input changed during verification')
    if h.hexdigest()!=OFFICIAL['sha256']: raise ValueError('official M10 SHA256 mismatch')
    return {'verified_original':True,'bytes':before.st_size,'sha256':h.hexdigest(),'selected_features_applied':False}

def main(argv=None):
    p=argparse.ArgumentParser(description='Leica M10 portable development source configurator; no camera writer.')
    sub=p.add_subparsers(dest='command',required=True)
    a=sub.add_parser('serve');a.add_argument('--port',type=int,default=8780)
    a=sub.add_parser('default-config');a.add_argument('--output',type=Path)
    a=sub.add_parser('validate');a.add_argument('config',type=Path)
    a=sub.add_parser('build-source');a.add_argument('--config',required=True,type=Path);a.add_argument('--output',required=True,type=Path)
    a=sub.add_parser('build-firmware');a.add_argument('--config',required=True,type=Path);a.add_argument('--output',required=True,type=Path)
    a=sub.add_parser('compile-sources');a.add_argument('--build-dir',type=Path,default=ROOT/'build/objects')
    a=sub.add_parser('run-reference');a.add_argument('--config',required=True,type=Path);a.add_argument('--build-dir',type=Path,default=ROOT/'build/host');a.add_argument('--gamma',type=Path);a.add_argument('--asc-resource',type=Path);a.add_argument('--original-serial',default='1000001',help='Declared synthetic host original example');a.add_argument('--next-reported-serial',default='0000002',help='Next software report example; old captured photo stays frozen');a.add_argument('--rgb',default='128,128,128')
    a=sub.add_parser('source-eval');a.add_argument('--style',default='all');a.add_argument('--gamma',type=Path);a.add_argument('--rgb',default='128,128,128')
    a=sub.add_parser('verify-official');a.add_argument('input',type=Path)
    a=sub.add_parser('rebuild-reference');a.add_argument('--input',required=True,type=Path);a.add_argument('--output-dir',required=True,type=Path)
    a=sub.add_parser('prepare-asc');a.add_argument('--ordinary-b2y',required=True,type=Path);a.add_argument('--asc-b2y',required=True,type=Path);a.add_argument('--output',required=True,type=Path)
    a=sub.add_parser('extract-asc');a.add_argument('--official-fw',required=True,type=Path);a.add_argument('--asc-fw',required=True,type=Path);a.add_argument('--output-dir',required=True,type=Path)
    a=sub.add_parser('native-default-config')
    a=sub.add_parser('validate-native');a.add_argument('config',type=Path)
    a=sub.add_parser('extract-native');a.add_argument('--official-fw',required=True,type=Path);a.add_argument('--asc-fw',type=Path);a.add_argument('--output-dir',required=True,type=Path)
    a=sub.add_parser('build-native');a.add_argument('--inputs',required=True,type=Path);a.add_argument('--config',required=True,type=Path);a.add_argument('--output-dir',required=True,type=Path)
    a=sub.add_parser('verify-native');a.add_argument('--inputs',required=True,type=Path);a.add_argument('--input-dir',required=True,type=Path)
    a=sub.add_parser('reverse-native');a.add_argument('--inputs',required=True,type=Path);a.add_argument('--input-dir',required=True,type=Path);a.add_argument('--output-dir',required=True,type=Path)
    a=sub.add_parser('package-info');a.add_argument('input',type=Path)
    a=sub.add_parser('package-list');a.add_argument('input',type=Path)
    a=sub.add_parser('package-extract');a.add_argument('input',type=Path);a.add_argument('component');a.add_argument('--output',required=True,type=Path)
    a=sub.add_parser('package-diff');a.add_argument('a',type=Path);a.add_argument('b',type=Path)
    a=sub.add_parser('package-edit');a.add_argument('--source',required=True,type=Path);a.add_argument('--plan',required=True,type=Path);a.add_argument('--output-dir',required=True,type=Path)
    a=sub.add_parser('models')
    a=sub.add_parser('identify');a.add_argument('inputs',nargs='+',type=Path)
    a=sub.add_parser('package-matrix');a.add_argument('--base',required=True,type=Path);a.add_argument('--donor',required=True,type=Path)
    a=sub.add_parser('package-interactive');a.add_argument('--base',required=True,type=Path);a.add_argument('--donor',required=True,type=Path);a.add_argument('--output',required=True,type=Path)
    a=sub.add_parser('package-build');a.add_argument('--base',required=True,type=Path);a.add_argument('--donor',required=True,type=Path);a.add_argument('--profile',required=True,type=Path);a.add_argument('--output-dir',required=True,type=Path);a.add_argument('--no-verify',action='store_true')
    args=p.parse_args(argv)
    try:
        # The package-* group and models/identify depend on the package_edit,
        # package_profile and models modules. Those are not part of this source
        # tree, so the commands are registered for CLI-shape stability but are
        # refused explicitly instead of raising ImportError.
        needs=[]
        if args.command in ('models','identify'): needs=['models']
        elif args.command.startswith('package-'):
            needs=['package_edit']
            if args.command in ('package-matrix','package-interactive','package-build'): needs.append('package_profile')
        missing=[m for m in needs if not Path(__file__).with_name(m+'.py').is_file()]
        if missing:
            raise ValueError('This source tree omits '+', '.join(m+'.py' for m in missing)+
                             ', so `'+args.command+'` is unavailable. Read-only package inspection '
                             'is available in codec/: `node codec/package_builder.mjs inspect FILE` '
                             'and `node codec/package_builder.mjs diff ORIGINAL REFERENCE`.')
        if args.command in ('models','identify'):
            from . import models
            result=models.registry() if args.command=='models' else models.identify_many(args.inputs)
            print(json.dumps(result,ensure_ascii=False,indent=2));return 0
        if args.command.startswith('package-'):
            from . import package_edit as package
            if args.command in ('package-matrix','package-interactive','package-build'):
                from . import package_profile as profile
                if args.command=='package-matrix': result=profile.matrix(args.base,args.donor)
                elif args.command=='package-interactive':
                    value=profile.interactive(args.base,args.donor)
                    if args.output.exists(): raise ValueError('refusing to overwrite: %s'%args.output)
                    args.output.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
                    result={'profile':str(args.output),'changes':len(value['components'])}
                else:
                    result=profile.build(args.base,args.donor,profile.load_profile(args.profile),args.output_dir,verify=not args.no_verify)
                print(json.dumps(result,ensure_ascii=False,indent=2))
                return 0 if result.get('verification',{}).get('passed',True) else 3
            if args.command=='package-info': result=package.info(args.input)
            elif args.command=='package-list': result=package.listing(args.input)
            elif args.command=='package-extract': result=package.extract(args.input,args.component,args.output)
            elif args.command=='package-diff': result=package.diff(args.a,args.b)
            else: result=package.edit(args.source,args.plan,args.output_dir)
            print(json.dumps(result,ensure_ascii=False,indent=2))
            return 0 if result.get('verification',{}).get('passed',True) else 3
        if args.command in ('native-default-config','validate-native','extract-native','build-native','verify-native','reverse-native'):
            from .native import portable as native
            if args.command=='native-default-config': result=native.default_config()
            elif args.command=='validate-native': result=native.config_file(args.config)
            elif args.command=='extract-native': result=native.extract(args.official_fw,args.output_dir,args.asc_fw)
            elif args.command=='build-native': result=native.build(args.inputs,args.config,args.output_dir)
            elif args.command=='verify-native': result=native.verify(args.inputs,args.input_dir)
            else: result=native.reverse(args.inputs,args.input_dir,args.output_dir)
            print(json.dumps(result,ensure_ascii=False,indent=2));return 0
        if args.command=='serve':
            from .server import serve
            serve(args.port);return 0
        if args.command=='default-config':
            if args.output:
                with args.output.open('xb') as f:f.write(canonical(default_config()))
                result={'created':str(args.output)}
            else: result=default_config()
        elif args.command=='validate': result=load(args.config)
        elif args.command=='build-source': result=build(load(args.config),args.output)
        elif args.command=='build-firmware': load(args.config);refuse_firmware()
        elif args.command=='verify-official':result=verify_official(args.input)
        elif args.command=='compile-sources':
            from .reference import compile_sources
            result=compile_sources(args.build_dir)
        elif args.command=='run-reference':
            from .reference import run
            rgb=[int(v) for v in args.rgb.split(',')]
            result=run(load(args.config),args.build_dir,gamma=args.gamma,asc_resource=args.asc_resource,original_serial=args.original_serial,next_reported_serial=args.next_reported_serial,rgb=rgb)
        elif args.command=='source-eval':
            from .reference import source_eval
            result=source_eval(args.style,[int(v) for v in args.rgb.split(',')],args.gamma)
        elif args.command=='prepare-asc':
            from .reference import prepare_asc
            result=prepare_asc(args.ordinary_b2y,args.asc_b2y,args.output)
        elif args.command in ('rebuild-reference','extract-asc'):
            if args.command=='rebuild-reference':
                verify_official(args.input)
                command=['node',str(ROOT/'codec/package_builder.mjs'),'rebuild-reference',str(args.input),str(args.output_dir)]
            else:
                verify_official(args.official_fw)
                command=['node',str(ROOT/'codec/extract_asc.mjs'),str(args.official_fw),str(args.asc_fw),str(args.output_dir)]
            return subprocess.run(command).returncode
        print(json.dumps(result,ensure_ascii=False,indent=2));return 0
    except (ValueError,OSError,TypeError,KeyError) as e:
        print(json.dumps({'error':str(e),'firmware_available':False},ensure_ascii=False),file=sys.stderr);return 2

if __name__=='__main__': sys.exit(main())
