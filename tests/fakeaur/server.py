#!/usr/bin/env python

import gzip
import http.server
import io
import json
import os.path
import socket
import sys
import tarfile
import tempfile
import time
import urllib.parse


DBROOT = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'db')

class FakeAurHandler(http.server.BaseHTTPRequestHandler):

    def log_request(self, unused):
        pass

    def do_GET(self):
        handlers = {
            '/rpc': self.handle_rpc,
            '/cgit/aur.git/snapshot': self.handle_download,
            '/cgit/aur.git/plain/PKGBUILD': self.handle_pkgbuild,
        }

        url = urllib.parse.urlparse(self.path)

        for path, handler in handlers.items():
            if url.path.startswith(path):
                return handler(url)

        self.respond(404)


    def collapse_params(self, params):
        if not params:
            return None

        return params[-1]


    def make_json_reply(self, querytype, results=[], version=5, error=None):
        return  json.dumps({
            'version': version,
            'type': querytype,
            'resultcount': len(results),
            'results': results,
            'error': error,
        }).encode()


    def make_pkgbuild(self, pkgname):
        return 'pkgname={}\npkgver=1.2.3\n'.format(pkgname).encode()



    def lookup_canned_response(self, querytype, fragment):
        f = os.path.join(DBROOT, querytype, fragment)
        if not os.path.exists(f):
            return None

        with open(f) as replyfile:
            return replyfile.read().strip().encode()


    def handle_rpc_info(self, args):
        results = []
        for arg in args:
            r = self.lookup_canned_response('info', arg)
            if r:
                # extract the results from each DB file
                results.extend(json.loads(r)['results'])

        self.respond(response=self.make_json_reply('multiinfo', results))


    def handle_rpc_search(self, arg, by):
        f = '{}|{}'.format(by, arg) if by else arg
        reply = self.lookup_canned_response('search', f)
        if reply is None:
            reply = self.make_json_reply('search')

        self.respond(response=reply)


    def handle_rpc(self, url):
        queryparams = urllib.parse.parse_qs(url.query)

        rpc_type = self.collapse_params(queryparams.get('type', None))
        if rpc_type == 'info':
            self.handle_rpc_info(set(queryparams.get('arg[]', [])))
        elif rpc_type == 'search':
            self.handle_rpc_search(self.collapse_params(queryparams.get('arg')),
                                   self.collapse_params(queryparams.get('by')))
        else:
            self.respond(status_code=400)


    def handle_download(self, url):
        pkgname = os.path.basename(url.path).split('.')[0]

        with tempfile.NamedTemporaryFile() as f:
            with tarfile.open(f.name, mode='w') as tar:
                b = self.make_pkgbuild(pkgname)

                t = tarfile.TarInfo('{}/PKGBUILD'.format(pkgname))
                t.size = len(b)
                tar.addfile(t, io.BytesIO(b))

            headers = [
                    (
                        'content-disposition',
                        'inline, filename={}.tar.gz'.format(pkgname),
                    )
            ]

            self.respond(headers=headers, response=gzip.compress(f.read()))


    def handle_pkgbuild(self, url):
        queryparams = urllib.parse.parse_qs(url.query)
        pkgname = self.collapse_params(queryparams.get('h'))
        self.respond(response=self.make_pkgbuild(pkgname))


    def respond(self, status_code=200, headers=[], response=None):
        self.send_response(status_code)

        for k, v in headers:
            self.send_header(k, v)
        self.end_headers()

        if response:
            self.wfile.write(response)


class AurServer(http.server.HTTPServer):

    def __init__(self, socket, server_address, handler_cls):
        http.server.HTTPServer.__init__(
                self, server_address, handler_cls, bind_and_activate=False)

        # Due to inheritance, we end up with an extra socket via
        # socketserver.TCPServer. I don't feel like fixing this properly, so
        # let's just close the extra socket that we end up with.
        self.socket.close()

        self.socket = socket
        self.socket.listen()


def Serve(queue=None, port=0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('', port))

    serve = AurServer(sock, sock.getsockname(), FakeAurHandler)
    port = sock.getsockname()[1]

    if queue:
        queue.put(port)
    else:
        print('serving on localhost:{}'.format(port))

    try:
        serve.serve_forever()
    except KeyboardInterrupt:
        pass
    serve.server_close()
    sock.close()


if __name__ == '__main__':
    port = 9001
    if len(sys.argv) >= 2:
        port = int(sys.argv[1])
    Serve(port=port)
