#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import gzip
import http.server
import io
import json
import os.path
import signal
import sys
import tarfile
import tempfile
import urllib.parse

DBROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'db')
AUR_SERVER_VERSION = 5


class FakeAurHandler(http.server.BaseHTTPRequestHandler):

    def do_GET(self):
        handlers = {
            '/rpc': self.handle_rpc,
            '/cgit/aur.git/snapshot': self.handle_download,
            '/cgit/aur.git/plain/': self.handle_source_file,
        }

        url = urllib.parse.urlparse(self.path)

        for path, handler in handlers.items():
            if url.path.startswith(path):
                return handler('GET', url)

        return self.respond(status_code=404)

    def do_POST(self):
        handlers = {
            '/rpc': self.handle_rpc,
        }
        url = urllib.parse.urlparse(self.path)

        for path, handler in handlers.items():
            if url.path.startswith(path):
                return handler('POST', url)

        return self.respond(status_code=404)

    @staticmethod
    def last_of(l):
        return l[-1] if l else None

    def make_json_reply(self, querytype, results=[], error=None):
        response = {
            'version': AUR_SERVER_VERSION,
            'type': querytype,
            'resultcount': len(results),
            'results': results,
        }

        if error:
            response['error'] = error

        return json.dumps(response).encode()

    def make_package_reply(self, querytype, results):
        return self.make_json_reply(querytype, results)

    def make_error_reply(self, error_message):
        return self.make_json_reply('error', [], error_message)

    def make_pkgbuild(self, pkgname):
        return f'pkgname={pkgname}\npkgver=1.2.3\n'.encode()

    def lookup_response(self, querytype, fragment):
        path = os.path.join(DBROOT, querytype, fragment)
        try:
            with open(path) as f:
                return f.read().strip().encode()
        except FileNotFoundError:
            return self.make_package_reply(querytype, [])

    def handle_rpc_info(self, args):
        results = []

        if len(args) == 1:
            try:
                status_code = int(next(iter(args)))
                return self.respond(
                    status_code=status_code,
                    response=f'{status_code}: fridge too loud\n'.encode())
            except ValueError:
                pass

        for arg in args:
            reply = self.lookup_response('info', arg)
            # extract the results from each DB file
            results.extend(json.loads(reply)['results'])

        return self.respond(
            response=self.make_package_reply('multiinfo', results))

    def handle_rpc_search(self, arg, by):
        if len(arg) < 2:
            return self.respond(
                response=self.make_error_reply('Query arg too small.'))

        reply = self.lookup_response('search', f'{by}|{arg}' if by else arg)

        return self.respond(response=reply)

    def handle_rpc(self, command, url):
        if command == 'GET':
            queryparams = urllib.parse.parse_qs(url.query)
        else:
            content_len = int(self.headers.get('Content-Length'))
            queryparams = urllib.parse.parse_qs(
                self.rfile.read(content_len).decode())

        if url.path.startswith('/rpc/v5/search'):
            return self.handle_rpc_search(
                url.path.rsplit('/')[-1], self.last_of(queryparams.get('by')))
        elif url.path.startswith('/rpc/v5/info'):
            return self.handle_rpc_info(set(queryparams.get('arg[]', [])))
        else:
            return self.respond(response=self.make_error_reply(
                'Incorrect request type specified.'))

    def handle_download(self, command, url):
        pkgname = os.path.basename(url.path).split('.')[0]

        # error injection for specific package name
        if pkgname == 'yaourt':
            return self.respond(response=b'you should use a better AUR helper')

        with tempfile.NamedTemporaryFile() as f:
            with tarfile.open(f.name, mode='w') as tar:
                b = self.make_pkgbuild(pkgname)

                t = tarfile.TarInfo(f'{pkgname}/PKGBUILD')
                t.size = len(b)
                tar.addfile(t, io.BytesIO(b))

            headers = {
                'content-disposition': f'inline, filename={pkgname}.tar.gz'
            }

            response = gzip.compress(f.read())

            return self.respond(headers=headers, response=response)

    def handle_source_file(self, command, url):
        queryparams = urllib.parse.parse_qs(url.query)
        pkgname = self.last_of(queryparams.get('h'))

        source_file = os.path.basename(url.path)
        if source_file == 'PKGBUILD':
            return self.respond(response=self.make_pkgbuild(pkgname))
        else:
            return self.respond(status_code=404)

    def respond(self, status_code=200, headers=[], response=None):
        self.send_response(status_code)

        for k, v in headers:
            self.send_header(k, v)
        self.end_headers()

        if response:
            self.wfile.write(response)

    def log_message(self, format, *args):
        # TODO: would be nice to make these visible:
        #  1) when the server is being run outside of a test context
        #  2) at the end of a failing test
        pass


def Serve(queue=None, port=0):

    class FakeAurServer(http.server.HTTPServer):

        def handle_error(self, request, client_address):
            raise

    with FakeAurServer(('localhost', port), FakeAurHandler) as server:
        host, port = server.socket.getsockname()[:2]
        endpoint = f'http://{host}:{port}'

        if queue:
            queue.put(endpoint)
        else:
            print(f'serving on {endpoint}')

        signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(0))

        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    port = 9001
    if len(sys.argv) >= 2:
        port = int(sys.argv[1])
    Serve(port=port)
