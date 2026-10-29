"""Focused public integration checks; accepted 227-sample recipe suites reused."""
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile
import unittest
import sys
from leica_source.config import ROOT, DEFAULT, STYLES, STYLE_FILMS, STYLE_ALIASES, validate, selected_modules
from leica_source.reference import import_file, gamma_file, source_eval, controller_run, prepare_asc, run

@unittest.skipUnless(all((ROOT/'modules'/m).is_dir() for m in ('film','controller','asc','framing')),'reduced bundle omits additive sources')
class AdditiveSource(unittest.TestCase):
    def test_reference_import_never_creates_a_cache(self):
        (ROOT/'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as t:
            module=Path(t)/'frozen_probe.py';module.write_text('VALUE = 42\n')
            previous=sys.dont_write_bytecode
            loaded=import_file(module,'no_cache_probe')
            self.assertEqual(loaded.VALUE,42);self.assertEqual(sys.dont_write_bytecode,previous)
            self.assertFalse((Path(t)/'__pycache__').exists())
    def test_22_styles_and_original_ID_compatibility(self):
        self.assertEqual(len(STYLES),22);self.assertEqual(len(STYLE_FILMS),19)
        for old in ('normal','asc_classic','asc_contemporary','cinestill800t','velvia100','e100','kodachrome64'):
            c=copy.deepcopy(DEFAULT);c['features']['styles']={'enabled':['normal'] if old=='normal' else ['normal',old],'default':old};self.assertEqual(validate(c)['features']['styles']['default'],old)
        self.assertEqual(STYLE_ALIASES['velvia100'],'velvia-100-v1')
        c=copy.deepcopy(DEFAULT);c['features']['frame_guides']['enabled']=True
        self.assertEqual(selected_modules(c),['asc','controller','framing'])
    def test_recipe_licenses_and_optional_monochrome_stages(self):
        records=json.loads((ROOT/'modules/film/recipe_catalog.json').read_text())
        self.assertEqual(len(records),18)
        self.assertEqual(sum(r['licensing']['recipe_data']=='CC-BY-SA-4.0' for r in records),11)
        self.assertEqual(sum(r['licensing']['recipe_data']=='MIT' for r in records),7)
        self.assertEqual(sum(r['licensing']['source_family']=='spectral_film_lut' for r in records),6)
        self.assertEqual(sum(r['licensing']['source_family']=='t3mujinpack' for r in records),1)
        engine=import_file(ROOT/'modules/film/recipe_engine.py','additive_engine')
        self.assertEqual(engine.INV601,engine.inverse(engine.BT601))
        self.assertEqual(engine.FLAT601,tuple(x for row in engine.BT601 for x in row))
        # This is a declared synthetic mathematical probe, not a real recipe or
        # substitute for the exact local gamma required by the public host runner.
        gamma=[round(i*16384/255) for i in range(256)]
        for r in records:
            effect=json.loads((ROOT/'modules/film'/r['recipe']).read_text())
            self.assertEqual(effect['attribution']['license'],r['licensing']['recipe_data'])
            if effect['flags']:
                self.assertNotIn('post_matrix',effect);self.assertNotIn('hue_sectors',effect)
                model=engine.Model(effect,gamma);self.assertIsNone(model.hue)
                self.assertEqual(model.post,(1,0,0,0,1,0,0,0,1))
                output=model.render((.1,.2,.3));self.assertEqual(output[0],output[1]);self.assertEqual(output[1],output[2])
    def test_actual_knots_and_selective_engine_branches(self):
        engine=import_file(ROOT/'modules/film/recipe_engine.py','additive_extensions')
        effect=json.loads((ROOT/'modules/film/deltas/velvia-100-v1.json').read_text())
        effect.pop('curves');effect['curve_knots']=[[[0,0],[128,7000],[255,16383]]]*3
        effect['selective_color']={'hue':30,'lower':[10,20],'higher':[12,25],'chroma':[10,100]}
        model=engine.Model(effect,[round(i*16384/255) for i in range(256)])
        self.assertEqual(model.curves[0][128],7000);self.assertEqual(len(model.selective),16384)
        result=model.render((.3,.1,.01));self.assertTrue(all(math.isfinite(v) and 0<=v<=1 for v in result))
    def test_real_SFC_guides_independent_of_closed_native_film(self):
        c=copy.deepcopy(DEFAULT);c['features']['styles']={'enabled':['normal','cinestill800t'],'default':'cinestill800t'};c['features']['frame_guides']={'enabled':True,'preset':7}
        result=controller_run(c,ROOT/'build/additive-tests')
        self.assertEqual((result['native_style_status'],result['native_style_stage']),(8,0))
        self.assertEqual(result['independent_guides_status'],0)
        self.assertEqual(result['framing']['nominal_gate'],[147,80,426,320])
        self.assertTrue(result['framing']['restored']);self.assertEqual(result['needs_restore_after_close'],0)
        self.assertTrue(result['production_imports_closed'])
    @unittest.skipUnless(os.environ.get('LOCAL_GRKIT_GAMMA'),'exact local gamma omitted')
    def test_all_18_actual_recipes_through_one_host_entry(self):
        outputs=source_eval('all',[120,80,40],os.environ['LOCAL_GRKIT_GAMMA'])
        self.assertEqual(len(outputs),19)
        for style,output in outputs.items():
            if style=='cinestill800t':continue
            self.assertIn(output['effect_license'],('MIT','CC-BY-SA-4.0'))
            self.assertTrue(all(math.isfinite(v) and 0<=v<=1 for v in output['display_rgb']))
        self.assertEqual(sum(o.get('monochrome',False) for o in outputs.values()),4)
    @unittest.skipUnless(os.environ.get('LOCAL_ORDINARY_B2Y') and os.environ.get('LOCAL_ASC_B2Y'),'original local B2Y inputs omitted')
    def test_real_SFC_ASC_partial_and_restore(self):
        (ROOT/'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as t:
            resource=Path(t)/'asc.host-resource';prepare_asc(os.environ['LOCAL_ORDINARY_B2Y'],os.environ['LOCAL_ASC_B2Y'],resource)
            c=copy.deepcopy(DEFAULT);c['features']['styles']={'enabled':['normal','asc_classic'],'default':'asc_classic'};c['features']['frame_guides']['enabled']=True
            good=controller_run(c,t,resource.read_bytes());self.assertEqual(good['completed'],3);self.assertEqual(good['restore_status'],0)
            self.assertEqual(good['host_consumed_bytes_apply'],147456);self.assertEqual(good['host_consumed_bytes_after_restore'],36864)
            partial=controller_run(c,t,resource.read_bytes(),True)
            self.assertEqual(partial['style_status'],5);self.assertEqual(partial['completed'],1)
            self.assertEqual(partial['needs_restore_after_apply'],3)
            self.assertEqual(partial['needs_restore_after_close'],0);self.assertTrue(partial['normal_after_restore'])
