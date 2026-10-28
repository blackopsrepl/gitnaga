#!/usr/bin/env python3
"""git's smart HTTP protocol behind basic auth, for transport tests.

    git_http_auth_server.py <project-root> <port> <user:password>

Every request without the right Authorization header gets a 401, so git has
to ask for credentials; authorized ones are handed to git http-backend.
"""
import base64
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

root, port, credentials = sys.argv[1], int(sys.argv[2]), sys.argv[3]
expected = "Basic " + base64.b64encode(credentials.encode()).decode()


class Handler(BaseHTTPRequestHandler):
    def handle_request(self):
        if self.headers.get("Authorization") != expected:
            self.send_response(401)
            self.send_header("WWW-Authenticate", 'Basic realm="gitnaga test"')
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        url = urlsplit(self.path)
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length) if length else b""
        env = {
            "GIT_PROJECT_ROOT": root,
            "GIT_HTTP_EXPORT_ALL": "1",
            "PATH_INFO": url.path,
            "QUERY_STRING": url.query,
            "REQUEST_METHOD": self.command,
            "CONTENT_TYPE": self.headers.get("Content-Type", ""),
            "CONTENT_LENGTH": str(len(body)),
            "REMOTE_USER": credentials.split(":")[0],
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": "/dev/null",
            "HTTP_GIT_PROTOCOL": self.headers.get("Git-Protocol", ""),
        }
        result = subprocess.run(["git", "http-backend"], input=body, env=env, capture_output=True)
        head, _, payload = result.stdout.partition(b"\r\n\r\n")
        status = 200
        headers = []
        for line in head.decode().split("\r\n"):
            name, _, value = line.partition(":")
            if name.lower() == "status":
                status = int(value.strip().split()[0])
            elif name:
                headers.append((name, value.strip()))
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    do_GET = handle_request
    do_POST = handle_request

    def log_message(self, format, *args):
        pass


ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
