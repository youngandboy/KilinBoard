#!/usr/bin/env python3
"""Local dashboard + TCP collector for kyLin-V3.1 RT-Thread firmware.

Board -> PC: TCP 9000, newline-delimited JSON
Browser:     http://localhost:8080

Uplink frame:
  {"v":1,"t":"d","seq":1,"up":123,"temp":28.6,"humi":71.4,"light":1593}

Downlink command:
  {"t":"c","led":0,"on":1}
  {"t":"c","beep":100}
  {"t":"c","period":5}
"""

import json
import socket
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from socketserver import StreamRequestHandler, ThreadingTCPServer

TCP_HOST = "0.0.0.0"
TCP_PORT = 9000
HTTP_HOST = "0.0.0.0"
HTTP_PORT = 8080

lock = threading.Lock()
board_conn = None
state = {
    "connected": False,
    "peer": None,
    "last": None,
    "history": deque(maxlen=120),
    "board_seen": 0.0,
    "cmd_status": "",
}


class BoardHandler(StreamRequestHandler):
    def handle(self):
        global board_conn
        peer = f"{self.client_address[0]}:{self.client_address[1]}"
        with lock:
            board_conn = self.connection
            state["connected"] = True
            state["peer"] = peer
            state["board_seen"] = time.time()
        print(f"[board] connected {peer}", flush=True)

        try:
            for raw in self.rfile:
                line = raw.decode("utf-8", "replace").strip()
                if not line:
                    continue
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    print(f"[board] bad json: {line[:120]}", flush=True)
                    continue
                if data.get("t") != "d":
                    continue
                data["_recv"] = time.time()
                with lock:
                    state["last"] = data
                    state["history"].append(data)
                    state["board_seen"] = time.time()
                print(f"[board] {data}", flush=True)
        except (ConnectionError, OSError):
            pass
        finally:
            with lock:
                state["connected"] = False
                state["peer"] = None
                board_conn = None
            print(f"[board] disconnected {peer}", flush=True)


class BoardServer(ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class WebHandler(BaseHTTPRequestHandler):
    server_version = "KylinDashboard/1.0"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        return

    def _json(self, obj, status=200):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _html(self, text):
        body = text.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            self._html(INDEX_HTML)
        elif self.path == "/api/data":
            with lock:
                payload = {
                    "connected": state["connected"],
                    "peer": state["peer"],
                    "last": state["last"],
                    "history": list(state["history"])[-30:],
                    "cmd_status": state["cmd_status"],
                }
            self._json(payload)
        else:
            self._json({"error": "not found"}, 404)

    def do_POST(self):
        if self.path != "/api/cmd":
            self._json({"error": "not found"}, 404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(length)
            cmd = json.loads(body.decode("utf-8"))
        except (ValueError, json.JSONDecodeError):
            self._json({"ok": False, "error": "bad json"}, 400)
            return

        ok, message = send_command(cmd)
        with lock:
            state["cmd_status"] = message
        self._json({"ok": ok, "message": message}, 200 if ok else 409)


def send_command(cmd):
    global board_conn
    line = json.dumps(cmd, separators=(",", ":")) + "\n"
    with lock:
        conn = board_conn
        if conn is None:
            return False, "board not connected"
        try:
            conn.sendall(line.encode("utf-8"))
        except OSError as exc:
            return False, f"send failed: {exc}"
    print(f"[cmd] {line.strip()}", flush=True)
    return True, line.strip()


INDEX_HTML = r"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>kyLin-V3.1 实时数据</title>
<style>
  :root { color-scheme: dark; --bg:#0d1117; --card:#161b22; --line:#30363d; --fg:#e6edf3; --dim:#8b949e; --ok:#2ea043; --bad:#da3633; --accent:#58a6ff; }
  * { box-sizing: border-box; }
  body { margin:0; font-family: ui-sans-serif, system-ui, "Segoe UI", Arial, sans-serif; background:var(--bg); color:var(--fg); }
  header { padding:20px 28px; border-bottom:1px solid var(--line); display:flex; align-items:center; justify-content:space-between; gap:16px; }
  h1 { font-size:20px; margin:0; font-weight:600; }
  .badge { font-size:13px; padding:4px 10px; border-radius:999px; background:#21262d; color:var(--dim); }
  .badge.on { background:rgba(46,160,67,.15); color:#3fb950; }
  .badge.off { background:rgba(218,54,51,.15); color:#f85149; }
  main { padding:24px 28px; max-width:1200px; margin:0 auto; }
  .grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(220px,1fr)); gap:16px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:18px; }
  .label { color:var(--dim); font-size:13px; margin-bottom:8px; }
  .value { font-size:32px; font-weight:650; letter-spacing:-.02em; }
  .unit { font-size:14px; color:var(--dim); font-weight:400; margin-left:4px; }
  .row { display:flex; gap:10px; flex-wrap:wrap; align-items:center; }
  button, select, input { background:#21262d; color:var(--fg); border:1px solid var(--line); border-radius:8px; padding:8px 12px; font-size:14px; }
  button { cursor:pointer; }
  button:hover { border-color:var(--accent); }
  button.primary { background:#1f6feb; border-color:#1f6feb; }
  table { width:100%; border-collapse:collapse; font-size:14px; }
  th, td { text-align:left; padding:8px 10px; border-bottom:1px solid var(--line); }
  th { color:var(--dim); font-weight:500; }
  .muted { color:var(--dim); font-size:13px; }
  .section-title { margin:26px 0 12px; font-size:15px; color:var(--dim); }
  .cmd { margin-top:10px; }
  code { background:#21262d; padding:2px 6px; border-radius:6px; }
</style>
</head>
<body>
<header>
  <h1>kyLin-V3.1 · RT-Thread 实时数据</h1>
  <div id="status" class="badge">连接中…</div>
</header>
<main>
  <div class="grid">
    <div class="card"><div class="label">温度</div><div><span id="temp" class="value">--</span><span class="unit">°C</span></div></div>
    <div class="card"><div class="label">湿度</div><div><span id="humi" class="value">--</span><span class="unit">%RH</span></div></div>
    <div class="card"><div class="label">光照</div><div><span id="light" class="value">--</span><span class="unit">mV</span></div></div>
    <div class="card"><div class="label">加速度 X / Y / Z</div><div class="value" style="font-size:22px"><span id="ax">--</span> <span class="unit">/</span> <span id="ay">--</span> <span class="unit">/</span> <span id="az">--</span><span class="unit">mg</span></div></div>
    <div class="card"><div class="label">序号 / 运行时间</div><div class="value" style="font-size:22px"><span id="seq">--</span> <span class="unit">/</span> <span id="uptime">--</span><span class="unit">s</span></div></div>
  </div>

  <div class="section-title">远程控制</div>
  <div class="card">
    <div class="row">
      <span class="muted">LED</span>
      <select id="ledId"><option value="0">LED1 红</option><option value="1">LED2 绿</option><option value="2">LED3 黄</option><option value="3">LED4 蓝</option></select>
      <button onclick="led(1)">开</button>
      <button onclick="led(0)">关</button>
      <span style="width:12px"></span>
      <span class="muted">蜂鸣器</span>
      <input id="beepMs" type="number" value="100" min="1" max="2000" style="width:90px">
      <button onclick="beep()">响</button>
      <span style="width:12px"></span>
      <span class="muted">上传周期</span>
      <input id="period" type="number" value="5" min="1" max="3600" style="width:90px">
      <button onclick="period()">设置</button>
    </div>
    <div id="cmdStatus" class="muted cmd">指令状态：--</div>
  </div>

  <div class="section-title">最近数据</div>
  <div class="card">
    <table>
      <thead><tr><th>时间</th><th>seq</th><th>温度</th><th>湿度</th><th>光照</th><th>ax</th><th>ay</th><th>az</th></tr></thead>
      <tbody id="history"><tr><td colspan="8" class="muted">等待数据…</td></tr></tbody>
    </table>
  </div>

  <p class="muted">数据协议：NDJSON over TCP 9000；网页服务：8080。板端每 1 s 上报一帧，可用“上传周期”修改。</p>
</main>
<script>
const $ = (id) => document.getElementById(id);
let lastRecv = 0;

async function sendCmd(cmd) {
  try {
    const r = await fetch('/api/cmd', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(cmd)});
    const j = await r.json();
    $('cmdStatus').textContent = '指令状态：' + (j.ok ? '已发送 ' + j.message : j.message);
  } catch (e) {
    $('cmdStatus').textContent = '指令状态：发送失败';
  }
}
function led(on) { sendCmd({t:'c', led: Number($('ledId').value), on: Number(on)}); }
function beep() { sendCmd({t:'c', beep: Number($('beepMs').value)}); }
function period() { sendCmd({t:'c', period: Number($('period').value)}); }

function fmt(x, digits=1) { return (x === null || x === undefined) ? '--' : Number(x).toFixed(digits); }

async function poll() {
  try {
    const r = await fetch('/api/data', {cache:'no-store'});
    const j = await r.json();
    const s = $('status');
    s.className = 'badge ' + (j.connected ? 'on' : 'off');
    s.textContent = j.connected ? ('板端在线 · ' + (j.peer || '')) : '板端离线';
    const d = j.last;
    if (d) {
      $('temp').textContent = fmt(d.temp);
      $('humi').textContent = fmt(d.humi);
      $('light').textContent = d.light ?? '--';
      $('ax').textContent = d.ax ?? '--';
      $('ay').textContent = d.ay ?? '--';
      $('az').textContent = d.az ?? '--';
      $('seq').textContent = d.seq ?? '--';
      $('uptime').textContent = d.up ?? '--';
      lastRecv = Date.now();
    }
    $('cmdStatus').textContent = '指令状态：' + (j.cmd_status || '--');

    const rows = (j.history || []).slice().reverse().slice(0, 20).map(x => {
      const t = x._recv ? new Date(x._recv * 1000).toLocaleTimeString() : '--';
      return `<tr><td>${t}</td><td>${x.seq ?? '--'}</td><td>${fmt(x.temp)} °C</td><td>${fmt(x.humi)} %</td><td>${x.light ?? '--'} mV</td><td>${x.ax ?? '--'}</td><td>${x.ay ?? '--'}</td><td>${x.az ?? '--'}</td></tr>`;
    });
    $('history').innerHTML = rows.length ? rows.join('') : '<tr><td colspan="8" class="muted">等待数据…</td></tr>';
  } catch (e) {
    $('status').className = 'badge off';
    $('status').textContent = '网页服务未连接';
  }
}
setInterval(poll, 1000);
poll();
</script>
</body>
</html>
"""


def main():
    board_server = BoardServer((TCP_HOST, TCP_PORT), BoardHandler)
    web_server = ThreadingHTTPServer((HTTP_HOST, HTTP_PORT), WebHandler)
    web_server.daemon_threads = True

    threading.Thread(target=board_server.serve_forever, daemon=True).start()
    threading.Thread(target=web_server.serve_forever, daemon=True).start()

    print(f"Board TCP : {TCP_HOST}:{TCP_PORT}", flush=True)
    print(f"Web page  : http://localhost:{HTTP_PORT}", flush=True)
    print("Open the web page and reset the board.", flush=True)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        board_server.shutdown()
        web_server.shutdown()


if __name__ == "__main__":
    main()
