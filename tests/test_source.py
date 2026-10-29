import copy
import hashlib
import http.client
import io
import json
import math
import shutil
import os
from pathlib import Path
import re
import tempfile
import threading
import unittest
import zipfile
import subprocess
import sys
from leica_source.config import DEFAULT, STYLES, ROOT, ConfigError, canonical, strict_json, validate, selected_modules, capabilities, default_config
from leica_source.export import archive_bytes, build, refuse_firmware, FirmwareUnavailable
from leica_source.reference import run, compile_sources, MissingInput, prepare_asc, gamma_file, import_file
from leica_source.server import handler

def selection(style='cinestill800t'):
    c=copy.deepcopy(DEFAULT);f=c['features'];f.update(touch=True,reported_serial='7654321')
    f['frame_guides']={'enabled':True,'preset':0};f['wb']={'enabled':True,'gains':['2','1','1/2']}
    f['styles']={'enabled':list(STYLES),'default':style};return c

class Configuration(unittest.TestCase):
    def test_strict_invalid_inputs(self):
        changes=[lambda c:c.update(qualified=True),lambda c:c.update(baseline='M10-P-5.26.35.51'),lambda c:c['features'].update(touch=1),lambda c:c['features'].update(reported_serial='１２３４５６７'),lambda c:c['features']['frame_guides'].update(preset=True),lambda c:c['features']['wb'].update(gains=['0','1','1']),lambda c:c['features']['wb'].update(gains=['1/0','1','1']),lambda c:c['features']['wb'].update(gains=['NaN','1','1']),lambda c:c['features']['styles'].update(default='unknown'),lambda c:c['features']['styles'].update(enabled=['normal','normal'])]
        for mutate in changes:
            c=copy.deepcopy(DEFAULT);mutate(c)
            with self.subTest(c=c),self.assertRaises(ConfigError):validate(c)
        with self.assertRaises(ConfigError):strict_json('{"a":1,"a":2}')
        with self.assertRaises(ConfigError):strict_json('{"a":NaN}')
    @unittest.skipUnless(all((ROOT/'modules'/m).exists() for m in ('touch','identity','framing','wb','asc','film')),'requires full source capability set')
    def test_capability_set_and_frozen_default(self):
        c=selection();v=validate(c);c['features']['reported_serial']='1234567'
        self.assertEqual(v['features']['reported_serial'],'7654321')
        self.assertEqual(selected_modules(v),['asc','controller','film','framing','identity','touch','wb'])
        v['features']['styles']['enabled'].reverse()
        self.assertEqual(validate(v)['features']['styles']['enabled'],list(STYLES))
    def test_firmware_command_never_bypassed(self):
        with self.assertRaises(FirmwareUnavailable):refuse_firmware({'qualified':True})
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as temp:
            p=Path(temp)/'custom.FW'
            with self.assertRaises(FirmwareUnavailable):build(DEFAULT,p)
            self.assertFalse(p.exists())

class PublicSource(unittest.TestCase):
    def check_provenance_record(self,r):
        self.assertEqual(set(r),{'origin','path','license','original_sha256','public_sha256','transformation'})
        for key in ('origin','path','license','public_sha256','transformation'):
            self.assertIsInstance(r[key],str);self.assertTrue(r[key].strip(),key)
        self.assertFalse(Path(r['path']).is_absolute())
        self.assertNotIn('..',Path(r['path']).parts);self.assertNotIn('\\',r['path'])
        self.assertEqual(Path(r['path']).as_posix(),r['path'])
        self.assertRegex(r['public_sha256'],r'\A[a-f0-9]{64}\Z')
        # These existing declarations describe new source, not copied originals.
        # Null is legal only for the declared paths and their exact attribution.
        authored_paths={
            'NOTICE.md','README.en.md','README.md','ROADMAP.md','codec/__init__.py',
            'docs/native-development.md','docs/status.md','docs/status.en.md',
            'examples/native-legacy-v1.json',
            'examples/native-original.json','examples/native-synthetic-combined.json',
            'leica_source/__main__.py','leica_source/export.py',
            'leica_source/native/__init__.py','leica_source/native/portable.py',
            'leica_source/native/source-pins.json'}
        if r['path'] in authored_paths:
            self.assertIsNone(r['original_sha256']);self.assertEqual(r['license'],'MIT')
            self.assertEqual(r['origin'],'project-original portable native adaptation')
            self.assertEqual(r['transformation'],'New portable native CLI/source/recipe/documentation with exact stock-bound validation; no official resource content.')
        elif r['path']=='leica_source/native/recipes.json':
            recipe=json.loads((ROOT/'leica_source/native/source-pins.json').read_text())['origins']['recipes.json']
            self.assertIsNone(r['original_sha256']);self.assertEqual(r['license'],'MIT')
            self.assertEqual(r['origin'],recipe['source_identity'])
            self.assertEqual(r['transformation'],recipe['transformation'])
            self.assertEqual(r['public_sha256'],recipe['recipe_sha256'])
        else:
            self.assertFalse(Path(r['origin']).is_absolute())
            self.assertNotIn('..',Path(r['origin']).parts);self.assertNotIn('\\',r['origin'])
            self.assertNotEqual(r['origin'],'project-original portable native adaptation')
            self.assertIsInstance(r['original_sha256'],str)
            self.assertRegex(r['original_sha256'],r'\A[a-f0-9]{64}\Z')
            if r['transformation'] in ('Unmodified bytes; portable destination path only.','exact source bytes, portable destination only'):
                self.assertEqual(r['original_sha256'],r['public_sha256'])
        self.assertEqual(hashlib.sha256((ROOT/r['path']).read_bytes()).hexdigest(),r['public_sha256'],r['path'])

    def test_provenance_and_include_closure(self):
        m=strict_json((ROOT/'provenance.json').read_bytes())
        self.assertEqual(set(m),{'schema','policy','files'})
        self.assertEqual(m['schema'],'leica.source-provenance/v1')
        self.assertGreaterEqual(len(m['files']),5)
        self.assertEqual(len({r['path'] for r in m['files']}),len(m['files']))
        for r in m['files']:self.check_provenance_record(r)
        copied=next(r for r in m['files'] if r['original_sha256'] is not None)
        authored=next(r for r in m['files'] if r['origin']=='project-original portable native adaptation')
        recipe=next(r for r in m['files'] if r['path']=='leica_source/native/recipes.json')
        for row,change in (
            (copied,{'original_sha256':None}),(copied,{'original_sha256':'g'*64}),(copied,{'original_sha256':False}),
            (copied,{'original_sha256':copied['original_sha256']+'\n'}),
            (copied,{'original_sha256':'0'*64}),(copied,{'origin':'../original.c'}),
            (copied,{'extra':True}),(copied,{'path':'../outside.c'}),
            (authored,{'original_sha256':authored['public_sha256']}),
            (authored,{'origin':'undeclared project source'}),(authored,{'transformation':''}),
            (authored,{'license':'unknown'}),(authored,{'public_sha256':'g'*64}),
            (authored,{'public_sha256':authored['public_sha256']+'\n'}),
            (authored,{'path':copied['path']}),(recipe,{'original_sha256':recipe['public_sha256']}),
            (recipe,{'origin':authored['origin']}),(recipe,{'transformation':'copied recipe'})):
            with self.subTest(path=row['path'],change=change),self.assertRaises(AssertionError):
                self.check_provenance_record(dict(row,**change))
        for p in (ROOT/'modules').rglob('*'):
            if p.suffix in ('.c','.h'):
                for header in re.findall(r'#include\s+"([^"]+)"',p.read_text()):self.assertTrue((p.parent/header).is_file(),str(p))
    @unittest.skipUnless(all((ROOT/'modules'/m).exists() for m in ('touch','identity','framing','wb','asc','film')),'selected bundle is a smaller source closure')
    def test_deterministic_archive_and_selected_closure(self):
        c=selection();a=archive_bytes(c);c['features']['styles']['enabled'].reverse();b=archive_bytes(c)
        self.assertEqual(a,b)
        with zipfile.ZipFile(io.BytesIO(a)) as z:
            names=z.namelist();prefix='leica-m10-open-firmware/'
            manifest=json.loads(z.read(prefix+'SOURCE-HASHES.json'))
            for n,h in manifest.items():self.assertEqual(hashlib.sha256(z.read(prefix+n)).hexdigest(),h)
            for module in selected_modules(c):self.assertTrue(any(n.startswith(prefix+'modules/'+module+'/') for n in names))
            self.assertIn(prefix+'modules/framing/m10_gui_native.h',names)
            self.assertIn(b'uint16_t id;',z.read(prefix+'modules/framing/m10_gui_native.h'))
            self.assertIn(prefix+'modules/identity/m10_builder.c',names)
            self.assertFalse(any(n.endswith('m10_mirror.S') for n in names))
        with zipfile.ZipFile(io.BytesIO(archive_bytes(DEFAULT))) as z:
            self.assertFalse(any('/modules/' in n for n in z.namelist()))
    def test_archive_contains_no_private_or_firmware_data(self):
        c=selection() if all((ROOT/'modules'/m).exists() for m in ('touch','identity','framing','wb','asc','film')) else DEFAULT
        with zipfile.ZipFile(io.BytesIO(archive_bytes(c))) as z:
            for n in z.namelist():
                self.assertFalse(n.lower().endswith(('.fw','.img','.bin','.host-resource','.host-fixture','.o','.so','.dylib','.dll','.exe')))
                data=z.read(n);self.assertNotIn(bytes([47])+b'Users'+bytes([47]),data,n);self.assertNotIn(bytes([47])+b'home'+bytes([47]),data,n)
                self.assertNotIn(b'base64'+b'_body',data,n)
                self.assertNotIn(b'BEGIN '+b'PRIVATE KEY',data,n)
                if n.endswith('.json') and '/effects/' not in n:self.assertNotIn(b'"base_gamma": [',data,n)

@unittest.skipUnless(all((ROOT/'modules'/m).exists() for m in ('touch','identity','framing','wb','asc','film')),'selected bundle omits combined host integration modules')
class ActualHostIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/'build').mkdir(exist_ok=True);cls.temp=tempfile.TemporaryDirectory(dir=ROOT/'build');cls.build=Path(cls.temp.name)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def test_combined_configuration_calls_real_implementations(self):
        r=run(selection(),self.build,rgb=[120,80,40]);m=r['modules']
        self.assertEqual(set(m),{'controller','film','framing','identity','touch','wb'})
        self.assertEqual(m['touch']['submits'],1);self.assertEqual(m['touch']['outcome'],0)
        self.assertEqual(m['identity']['reported_software_label'],'7654321')
        self.assertEqual(m['identity']['preserved_original'],'1000001')
        self.assertEqual(m['identity']['checked_original_consumer'],'1000001')
        self.assertEqual(m['identity']['serialized_A431'],'7654321')
        self.assertEqual(m['identity']['old_capture_after_edit_clear_failure'],'7654321')
        self.assertTrue(m['identity']['descriptor_restored'])
        self.assertEqual(m['framing']['nominal_gate'],[0,90,720,300])
        self.assertEqual(m['framing']['line_apply_calls'],4);self.assertTrue(m['framing']['restored'])
        self.assertEqual(m['wb']['pairs'],[1,4,1,1,3,1])
        self.assertNotEqual(m['film']['rgb8'],[120,80,40]);self.assertFalse(r['firmware_available'])
    def test_touch_real_error_and_service_branches(self):
        c=copy.deepcopy(DEFAULT);c['features']['touch']=True
        cases=[(0,0x113,0,(0,1,0,0,0)),(0x13a,0x113,0,(1,0,1,1,0)),(0,0x113,-17,(2,1,1,0,0)),(0,0x113,-999,(3,1,0,0,1)),(0,0x211,0,(1,0,1,0,0))]
        for state,event,raw,want in cases:
            m=run(c,self.build,touch_state=state,touch_event=event,touch_raw=raw)['modules']['touch']
            self.assertEqual(tuple(m[k] for k in ('outcome','submits','payload_frees','debug_calls','retained_payload')),want)
    def test_all_eight_frames_and_selection_changes(self):
        c=copy.deepcopy(DEFAULT);f=c['features'];f['frame_guides']['enabled']=True;gates=[]
        for i in range(8):
            f['frame_guides']['preset']=i;m=run(c,self.build)['modules']['framing'];gates.append(tuple(m['nominal_gate']));self.assertTrue(m['restored'])
        self.assertEqual(len(set(gates)),8)
    def test_absent_or_wrong_inputs_do_not_fake_looks(self):
        with self.assertRaises(MissingInput):run(selection('velvia100'),self.build)
        with self.assertRaises(MissingInput):run(selection('asc_classic'),self.build)
        with tempfile.TemporaryDirectory(dir=self.build) as t:
            p=Path(t)/'gamma.json';p.write_text(json.dumps([0]*256))
            with self.assertRaises(MissingInput):gamma_file(p)
        c=selection();c['features']['jpeg_crop']=True
        with self.assertRaises(MissingInput):run(c,self.build)
    def test_downloaded_bundle_runs_its_own_source_and_refuses_FW(self):
        package=self.build/'download';package.mkdir()
        with zipfile.ZipFile(io.BytesIO(archive_bytes(selection()))) as z:z.extractall(package)
        checkout=package/'leica-m10-open-firmware'
        p=subprocess.run([sys.executable,'-m','leica_source','run-reference','--config','selection.json','--build-dir','build/host','--rgb','120,80,40'],cwd=checkout,capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr);r=json.loads(p.stdout)
        self.assertEqual(r['modules']['identity']['reported_software_label'],'7654321')
        p=subprocess.run([sys.executable,'-m','leica_source','build-firmware','--config','selection.json','--output','custom.FW'],cwd=checkout,capture_output=True,text=True)
        self.assertEqual(p.returncode,2);self.assertFalse((checkout/'custom.FW').exists())
    def test_all_C_sources_compile_without_fake_native_implementations(self):
        r=compile_sources(self.build/'objects');self.assertGreaterEqual(r['C_objects'],20);self.assertFalse(r['native_linked']);self.assertEqual(len(r['compile_only_ARM_objects']),1)
    @unittest.skipUnless(os.environ.get('LOCAL_ORDINARY_B2Y') and os.environ.get('LOCAL_ASC_B2Y'),'original local B2Y inputs not redistributed')
    def test_real_local_ASC_composition_and_both_plans(self):
        resource=self.build/'asc.host-resource';prepare_asc(os.environ['LOCAL_ORDINARY_B2Y'],os.environ['LOCAL_ASC_B2Y'],resource)
        plans=[]
        for style in ('asc_classic','asc_contemporary'):
            m=run(selection(style),self.build,asc_resource=resource)['modules']['asc']
            self.assertEqual(m['gamma_count'],8);self.assertEqual(m['production_closed_status'],8);self.assertFalse(m['pixel_rendered']);plans.append(m['cc1_fields'])
        self.assertNotEqual(plans[0],plans[1])


SLIDE_IDS=('velvia100','e100','kodachrome64')
RGB_SAMPLES=((0,0,0),(255,255,255),(1,1,1),(16,16,16),(64,64,64),(128,128,128),(192,192,192),(254,254,254),(255,0,0),(0,255,0),(0,0,255),(255,255,0),(255,0,255),(0,255,255),(120,80,40),(24,96,216),(240,160,48))

@unittest.skipUnless((ROOT/'modules/film/grkit.py').is_file(),'selected source kit omits film')
class SlideRecipes(unittest.TestCase):
    def test_synthetic_gamma_boundary_arithmetic_and_rejection(self):
        # Synthetic monotonic gamma exercises arithmetic/branches only. It is not
        # accepted by gamma_file and makes no exact GRKit or camera-output claim.
        m=import_file(ROOT/'modules/film/grkit.py','synthetic_grkit_test')
        gamma=[round(i*16384/255) for i in range(256)]
        results=[]
        for look in ('velvia-100-v1','ektachrome-e100-v1','kodachrome-64-v1'):
            effect=json.loads((ROOT/'modules/film/effects'/(look+'.json')).read_text());model=m.Model(effect,gamma)
            samples=[model.render(tuple(m.srgb_decode(v/255) for v in rgb)) for rgb in RGB_SAMPLES]
            self.assertLess(sum(samples[0]),sum(samples[1]))
            self.assertTrue(all(math.isfinite(v) and 0<=v<=1 for rgb in samples for v in rgb))
            results.append(tuple(samples))
            for linear in ((float('nan'),0,0),(float('inf'),0,0),(0,0)):
                with self.subTest(look=look,linear=linear),self.assertRaises(ValueError):model.render(linear)
            for bad in (None,[],[0]*256,list(reversed(gamma)),[False]+gamma[1:]):
                with self.subTest(look=look),self.assertRaises(ValueError):m.Model(effect,bad)
        self.assertEqual(len(set(results)),3)
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as temp:
            path=Path(temp)/'synthetic-gamma.json';path.write_text(json.dumps(gamma))
            with self.assertRaises(MissingInput):gamma_file(path)

    @unittest.skipUnless(os.environ.get('LOCAL_GRKIT_GAMMA'),'exact local gamma not redistributed; exact recipe test not executed')
    def test_three_exact_gamma_boundary_neutral_saturation_recipes(self):
        outputs=[]
        for style in SLIDE_IDS:
            config=copy.deepcopy(DEFAULT);config['features']['styles']={'enabled':['normal',style],'default':style}
            samples=[]
            for rgb in RGB_SAMPLES:
                result=run(config,ROOT/'build/exact-gamma',gamma=os.environ['LOCAL_GRKIT_GAMMA'],rgb=list(rgb))['modules']['film']
                self.assertTrue(all(math.isfinite(v) and 0<=v<=1 for v in result['display_rgb']))
                self.assertEqual(result['input_rgb8'],list(rgb));samples.append(tuple(result['display_rgb']))
            self.assertLess(sum(samples[0]),sum(samples[1]));outputs.append(tuple(samples))
        self.assertEqual(len(set(outputs)),3)

    @unittest.skipUnless(os.environ.get('LOCAL_GRKIT_GAMMA') and os.environ.get('LOCAL_GRKIT_REFERENCE'),'exact gamma/reference omitted; original parity comparison not executed')
    def test_exact_gamma_matches_accepted_original_for_all_samples(self):
        original_path=Path(os.environ['LOCAL_GRKIT_REFERENCE'])
        original=import_file(original_path,'accepted_grkit_test');gamma=gamma_file(os.environ['LOCAL_GRKIT_GAMMA'])
        for style,look in zip(SLIDE_IDS,('velvia-100-v1','ektachrome-e100-v1','kodachrome-64-v1')):
            effect=json.loads((original_path.parent/'targets'/('grkit-'+look)/'original-effect.json').read_text())
            model=original.Model(effect,gamma)
            config=copy.deepcopy(DEFAULT);config['features']['styles']={'enabled':['normal',style],'default':style}
            for rgb in RGB_SAMPLES:
                with self.subTest(style=style,rgb=rgb):
                    expected=model.render(tuple(original.srgb_decode(v/255) for v in rgb))
                    actual=run(config,ROOT/'build/exact-gamma',gamma=os.environ['LOCAL_GRKIT_GAMMA'],rgb=list(rgb))['modules']['film']['display_rgb']
                    self.assertEqual(actual,expected)

@unittest.skipUnless(shutil.which('node'),'Node is required for the browser-script DOM-model regression')
class ExtractedBundles(unittest.TestCase):
    def probe(self,checkout,value):
        p=subprocess.run([sys.executable,'tests/bundle_probe.py'],input=json.dumps(value),cwd=checkout,capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr);return json.loads(p.stdout)
    def check_bundle(self,config,modules):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as temp:
            with zipfile.ZipFile(io.BytesIO(archive_bytes(config))) as z:z.extractall(temp)
            checkout=Path(temp)/'leica-m10-open-firmware'
            state=self.probe(checkout,{'command':'state'})
            self.assertEqual(state['catalog']['default'],validate(config))
            self.assertEqual(state['catalog']['available']['modules'],modules)
            p=subprocess.run(['node','tests/ui_probe.mjs'],input=json.dumps(state),cwd=checkout,capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stderr);ui=json.loads(p.stdout)
            self.assertEqual(ui['configuration'],validate(config));self.assertEqual(ui['exported'],validate(config))
            self.assertEqual([x['value'] for x in ui['styles'] if x['checked']],config['features']['styles']['enabled'])
            for option in ui['default_options']:self.assertEqual(option['disabled'],option['value'] not in config['features']['styles']['enabled'])
            for style in ui['styles']:
                self.assertEqual(style['disabled'],style['value']=='normal' or style['value'] not in state['catalog']['available']['styles'])
            for feature,control in [('touch','touch'),('reported_serial','serial'),('frame_guides','guides'),('wb','wb')]:
                self.assertEqual(ui['disabled'][control],not state['catalog']['available']['features'][feature])
            result=self.probe(checkout,{'command':'roundtrip','config':ui['exported']})
            self.assertEqual((result['validate_status'],result['export_status']),(200,200));self.assertEqual(result['selection'],validate(config))
            self.assertEqual(result['run']['shot_config'],validate(config));self.assertFalse(result['run']['firmware_available'])
            self.assertNotIn('leica-m10-open-firmware/examples/all-host.json',result['members'])
            if not modules:self.assertFalse(any('/modules/' in n for n in result['members']))
            bad=copy.deepcopy(config);bad['features']['touch']=True
            denied=self.probe(checkout,{'command':'roundtrip','config':bad})
            self.assertEqual((denied['validate_status'],denied['export_status']),(400,400));self.assertIn('unavailable',denied['error']['error'])
            p=subprocess.run([sys.executable,'-m','leica_source','default-config'],cwd=checkout,capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stderr);self.assertEqual(json.loads(p.stdout),validate(config))
            from leica_source.native import portable as native
            validators={DEFAULT['schema']:('validate',validate),
                        native.SCHEMA:('validate-native',native.normalize),
                        native.OLD_SCHEMA:('validate-native',native.normalize)}
            for example in sorted((checkout/'examples').glob('*.json')):
                value=strict_json(example.read_bytes())
                self.assertIn(value.get('schema'),validators,example.name)
                command,normalize=validators[value['schema']]
                p=subprocess.run([sys.executable,'-B','-m','leica_source',command,str(example)],cwd=checkout,capture_output=True,text=True)
                self.assertEqual(p.returncode,0,p.stderr);self.assertEqual(json.loads(p.stdout),normalize(value))
                other='validate-native' if command=='validate' else 'validate'
                p=subprocess.run([sys.executable,'-B','-m','leica_source',other,str(example)],cwd=checkout,capture_output=True,text=True)
                self.assertEqual(p.returncode,2,p.stdout);self.assertFalse(json.loads(p.stderr)['firmware_available'])
            # Both object shapes retain explicit schema admission after relocation.
            for value in (dict(config,schema='unknown'),{'schema':'unknown','components':[]}):
                unknown=checkout/'unknown-schema.json';unknown.write_bytes(canonical(value))
                self.assertNotIn(value['schema'],validators)
                for command in ('validate','validate-native'):
                    p=subprocess.run([sys.executable,'-B','-m','leica_source',command,str(unknown)],cwd=checkout,capture_output=True,text=True)
                    self.assertEqual(p.returncode,2,p.stdout);self.assertFalse(json.loads(p.stderr)['firmware_available'])
    def test_extracted_minimal_ui_defaults_validate_export_and_run(self):self.check_bundle(copy.deepcopy(DEFAULT),[])
    @unittest.skipUnless(all((ROOT/'modules'/m).is_dir() for m in ('asc','film')),'all-style regression requires ASC and film source')
    def test_extracted_all_style_ui_preserves_frozen_selection(self):
        config=copy.deepcopy(DEFAULT);config['features']['styles']={'enabled':list(STYLES),'default':'cinestill800t'}
        self.check_bundle(config,['asc','controller','film','framing'])
    def test_checkout_defaults_use_all_available_styles(self):
        if (ROOT/'selection.json').is_file():self.skipTest('downloaded kit uses its frozen defaults')
        self.assertEqual(default_config()['features']['styles']['enabled'],capabilities()['styles'])

class LocalHTTP(unittest.TestCase):
    def test_browser_delivery_export_and_origin_protection(self):
        # Real BaseHTTPRequestHandler parser/response implementation, ordinary byte
        # streams. Does not bypass an environment that prohibits socket binding.
        class MemorySocket:
            def __init__(self,raw):self.raw=raw;self.output=io.BytesIO()
            def makefile(self,*args):return io.BytesIO(self.raw)
            def sendall(self,data):self.output.write(data)
        def request(method,path,body=b'',extra=''):
            raw=(method+' '+path+' HTTP/1.0\r\nHost: 127.0.0.1:8780\r\nContent-Type: application/json\r\nX-Local-Token: synthetic-local-test\r\nContent-Length: '+str(len(body))+'\r\n'+extra+'\r\n').encode()+body
            socket=MemorySocket(raw);handler(8780,'synthetic-local-test')(socket,('127.0.0.1',1),None)
            headers,data=socket.output.getvalue().split(b'\r\n\r\n',1);return int(headers.split()[1]),data
        status,data=request('GET','/');html=data.decode();self.assertEqual(status,200)
        self.assertIn('开发源码 ZIP',html);self.assertIn('disabled>自定义 .FW',html)
        self.assertIn('synthetic-local-test',html)
        status,data=request('GET','/api/catalog');self.assertFalse(json.loads(data)['custom_firmware_available'])
        status,data=request('POST','/api/export',canonical(DEFAULT));self.assertEqual(status,200);self.assertTrue(zipfile.is_zipfile(io.BytesIO(data)))
        status,data=request('POST','/api/export',canonical(DEFAULT),'Origin: https://external.invalid\r\n');self.assertEqual(status,403)
        bad=copy.deepcopy(DEFAULT);bad['qualified']=True
        status,data=request('POST','/api/export',canonical(bad));self.assertEqual(status,400)

def setUpModule(): (ROOT/'build').mkdir(exist_ok=True)
