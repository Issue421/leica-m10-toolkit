"""Portable independent verification; no JavaScript codec import.

Global full-package/payload pins replace the private historical per-row index.
The accepted independent COMP and TOC decoder bodies remain separate source.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
from independent_decoder import regular_bytes, decode_comp, inspect_toc
FW_PIN='d6ef5b77b70451d2b6a31fc4a090bf8f664c1e07a8bebade657aff7da692423c'
PAYLOAD_PIN='7a50492e9987416531f788737f2cd9f6f4ecbe6e1fadc0fdb9bc1521c5b47e6a'
sha=lambda b:hashlib.sha256(b).hexdigest()
def need(ok,reason):
    if not ok: raise ValueError(reason)
def verify(p,fixture=False):
    raw=regular_bytes(p); need(len(raw)>=56,'outer bounds')
    u=lambda at:struct.unpack_from('<I',raw,at)[0]
    need(u(0)==0x1e1caf3e and u(52)==0xe1e350c1,'outer magic')
    need(u(36)==4 and u(40)==56 and u(44)==2,'outer format')
    payload,blocks=decode_comp(raw); rows=inspect_toc(payload)
    result=dict(file_sha256=sha(raw),bytes=len(raw),payload_sha256=sha(payload),components=len(rows),blocks=len(blocks),synthetic_codec_fixture_only=fixture,deployment_ready=False)
    if not fixture:
        need(len(raw)==127615696 and sha(raw)==FW_PIN,'unchanged official M10 reference required')
        need(raw[4:8]==bytes((4,26,36,10)) and u(8)==0x07e1ea5e and u(48)==0x141,'M10 identity')
        need(sha(payload)==PAYLOAD_PIN and len(rows)==277,'complete official payload identity')
        result['full_package_byte_identity']=True
    return result
if __name__=='__main__':
    try:
        args=sys.argv[1:]; fixture=bool(args and args[0]=='--fixture')
        if fixture: args=args[1:]
        need(len(args)==1,'usage: independent_verify.py [--fixture] FILE')
        print(json.dumps(verify(Path(args[0]),fixture),indent=2))
    except (ValueError,OSError,KeyError,TypeError,RecursionError) as e:
        print(json.dumps({'error':str(e),'accepted':False})); sys.exit(2)
