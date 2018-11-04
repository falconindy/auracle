#!/usr/bin/env python

import gzip
import http.server
import io
import json
import os.path
import sys
import tarfile
import tempfile
import time
import urllib.parse


DBROOT = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'db')
AUR_SERVER_VERSION = 5


class FakeAurHandler(http.server.BaseHTTPRequestHandler):

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


    @staticmethod
    def last_of(l):
        return l[-1] if l else None


    def make_json_reply(self, querytype, results=[], error=None):
        return json.dumps({
            'version': AUR_SERVER_VERSION,
            'type': querytype,
            'resultcount': len(results),
            'results': results,
            'error': error,
        }).encode()


    def make_pkgbuild(self, pkgname):
        return 'pkgname={}\npkgver=1.2.3\n'.format(pkgname).encode()


    def lookup_response(self, querytype, fragment):
        f = os.path.join(DBROOT, querytype, fragment)
        if not os.path.exists(f):
            # empty reply
            return self.make_json_reply(querytype)

        with open(f) as replyfile:
            return replyfile.read().strip().encode()


    def handle_rpc_info(self, args):
        results = []

        for arg in args:
            reply = self.lookup_response('info', arg)
            # extract the results from each DB file
            results.extend(json.loads(reply)['results'])

        self.respond(response=self.make_json_reply('multiinfo', results))


    def handle_rpc_search(self, arg, by):
        reply = self.lookup_response(
                'search', '{}|{}'.format(by, arg) if by else arg)

        self.respond(response=reply)


    def handle_rpc(self, url):
        queryparams = urllib.parse.parse_qs(url.query)

        rpc_type = self.last_of(queryparams.get('type', None))
        if rpc_type == 'info':
            self.handle_rpc_info(set(queryparams.get('arg[]', [])))
        elif rpc_type == 'search':
            self.handle_rpc_search(self.last_of(queryparams.get('arg')),
                                   self.last_of(queryparams.get('by')))
        else:
            self.respond(response=self.make_json_reply(
                'error', error='Incorrect request type specified.'))


    def handle_download(self, url):
        pkgname = os.path.basename(url.path).split('.')[0]

        # error injection for specific package name
        if pkgname == 'yaourt':
            self.respond(response=b'you should use a better AUR helper')
            return

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
        pkgname = self.last_of(queryparams.get('h'))
        self.respond(response=self.make_pkgbuild(pkgname))


    def respond(self, status_code=200, headers=[], response=None):
        self.send_response(status_code)

        for k, v in headers:
            self.send_header(k, v)
        self.end_headers()

        if response:
            self.wfile.write(response)


def Serve(queue=None, port=0):
    serve = http.server.HTTPServer(('localhost', port), FakeAurHandler)
    host, port = serve.socket.getsockname()

    if queue:
        queue.put(port)
    else:
        print('serving on http://{}:{}'.format(host, port))

    try:
        serve.serve_forever()
    except KeyboardInterrupt:
        pass
    serve.server_close()


if __name__ == '__main__':
    port = 9001
    if len(sys.argv) >= 2:
        port = int(sys.argv[1])
    Serve(port=port)
