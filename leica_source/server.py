"""Loopback-only local UI. No camera connections, uploads or external requests."""
import hmac
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import secrets
from .config import CATALOG, OFFICIAL, ROOT, capabilities, default_config, strict_json, validate
from .export import archive_bytes

def handler(port,token):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass  # no private local input paths in logs
        def reply(self,status,data,kind='application/json; charset=utf-8',attachment=None):
            self.send_response(status);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(data)))
            self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
            self.send_header('Content-Security-Policy',"default-src 'self'; style-src 'self'; script-src 'self'; connect-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'")
            if attachment:self.send_header('Content-Disposition','attachment; filename="'+attachment+'"')
            self.end_headers();self.wfile.write(data)
        def trusted(self):
            return self.headers.get('Host') in ('127.0.0.1:'+str(port),'localhost:'+str(port))
        def do_GET(self):
            if not self.trusted():self.reply(403,b'{}');return
            files={'/':'index.html','/app.js':'app.js','/style.css':'style.css'}
            if self.path in files:
                p=ROOT/'web'/files[self.path];data=p.read_bytes()
                if self.path=='/':data=data.replace(b'__CSRF__',token.encode())
                kind='text/html; charset=utf-8' if self.path=='/' else 'text/javascript; charset=utf-8' if self.path.endswith('.js') else 'text/css; charset=utf-8'
                self.reply(200,data,kind)
            elif self.path=='/api/catalog':self.reply(200,json.dumps({'default':default_config(),'available':capabilities(),'catalog':CATALOG,'official':OFFICIAL,'custom_firmware_available':False}).encode())
            else:self.reply(404,b'{}')
        def do_POST(self):
            origin=self.headers.get('Origin')
            if not self.trusted() or origin not in (None,'http://127.0.0.1:'+str(port),'http://localhost:'+str(port)) or not hmac.compare_digest(self.headers.get('X-Local-Token',''),token):self.reply(403,b'{}');return
            try:
                length=int(self.headers.get('Content-Length','0'))
                if not 0<length<=65536 or self.headers.get_content_type()!='application/json':raise ValueError('bounded JSON request required')
                body=strict_json(self.rfile.read(length))
                if self.path=='/api/export':self.reply(200,archive_bytes(body),'application/zip','leica-m10-development-source.zip')
                elif self.path=='/api/validate':self.reply(200,json.dumps(validate(body)).encode())
                elif self.path=='/api/verify-official':
                    from .__main__ import verify_official
                    if type(body) is not dict or set(body)!={'path'} or type(body['path']) is not str:raise ValueError('local path required')
                    self.reply(200,json.dumps(verify_official(Path(body['path']))).encode())
                else:self.reply(404,b'{}')
            except (ValueError,OSError,TypeError,KeyError) as e:self.reply(400,json.dumps({'error':str(e)}).encode())
    return Handler

def make_server(port=8780):
    # A remote address is never accepted as an argument or environment override.
    server=HTTPServer(('127.0.0.1',port),BaseHTTPRequestHandler)
    server.RequestHandlerClass=handler(server.server_port,secrets.token_urlsafe(32))
    return server

def serve(port):
    server=make_server(port)
    print('Open http://127.0.0.1:%d — development source download only. Ctrl-C to stop.'%server.server_port,flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
