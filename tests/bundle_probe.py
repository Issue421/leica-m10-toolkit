"""Subprocess probe: import only the extracted kit, use its actual HTTP handler."""
import io
import json
from pathlib import Path
import sys
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from leica_source.config import canonical
from leica_source.reference import run
from leica_source.server import handler

class MemorySocket:
    def __init__(self,raw):self.raw=raw;self.output=io.BytesIO()
    def makefile(self,*args):return io.BytesIO(self.raw)
    def sendall(self,data):self.output.write(data)

def request(method,path,body=None):
    data=canonical(body) if body is not None else b''
    raw=(method+' '+path+' HTTP/1.0\r\nHost: 127.0.0.1:8780\r\nContent-Type: application/json\r\nX-Local-Token: probe-token\r\nContent-Length: '+str(len(data))+'\r\n\r\n').encode()+data
    socket=MemorySocket(raw);handler(8780,'probe-token')(socket,('127.0.0.1',1),None)
    headers,data=socket.output.getvalue().split(b'\r\n\r\n',1)
    return int(headers.split()[1]),data

def main():
    args=json.load(sys.stdin)
    if args['command']=='state':
        result={}
        for path,key in [('/','html'),('/app.js','script'),('/api/catalog','catalog')]:
            status,data=request('GET',path)
            if status!=200:raise ValueError('State request failed: '+path)
            result[key]=json.loads(data) if key=='catalog' else data.decode()
    else:
        config=args['config'];status,body=request('POST','/api/validate',config)
        export_status,data=request('POST','/api/export',config)
        result={'validate_status':status,'export_status':export_status}
        if status==200 and export_status==200:
            result['validated']=json.loads(body)
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                result['members']=z.namelist();result['selection']=json.loads(z.read('leica-m10-open-firmware/selection.json'))
            result['run']=run(config,Path('build/probe-host'),rgb=[120,80,40])
        else:result['error']=json.loads(data)
    print(json.dumps(result))

if __name__=='__main__':main()
