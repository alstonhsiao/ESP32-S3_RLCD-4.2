#!/usr/bin/env python3
"""Serve the AI usage analysis page and proxy its JSON data endpoint."""

from argparse import ArgumentParser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.request import Request, urlopen


DATA_URL = "https://aiusage-web.zeabur.app/data"
WEB_ROOT = Path(__file__).resolve().parent


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_ROOT), **kwargs)

    def do_GET(self):  # noqa: N802 - required by BaseHTTPRequestHandler
        if self.path.split("?", 1)[0] == "/data":
            self.proxy_data()
            return
        super().do_GET()

    def proxy_data(self):
        request = Request(DATA_URL, headers={"User-Agent": "ESP32-S3-RLCD-analysis/1.0"})
        try:
            with urlopen(request, timeout=20) as response:
                body = response.read()
                content_type = response.headers.get("Content-Type", "application/json")
        except Exception as error:  # pragma: no cover - exercised by a live network
            message = str(error).encode("utf-8", "replace")
            self.send_response(502)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(message)))
            self.end_headers()
            self.wfile.write(message)
            return

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    parser = ArgumentParser(description="Serve the AI usage analysis page.")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Open http://127.0.0.1:{args.port}/aiusage-analysis.html")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
