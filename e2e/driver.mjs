// Long-lived driver server: holds the AppConnection to a running app instance
// and exposes it over local HTTP, so an agent (or a human with curl) can
// interact step by step across many shell invocations.
//
//   node driver.mjs [--port 4499] [--no-launch]
//
// Endpoints (all JSON):
//   GET  /status                       {running, pid, exit}
//   POST /launch      {}               launch app + wait for UI ready
//   POST /eval        {script, async?, timeoutMs?}   run JS in the webview
//   POST /screenshot  {name?}          save PNG under out/, return path
//   POST /new-project {}               dismiss startup modal via New Project
//   POST /open-project {path}          load .rcp without the file chooser
//   POST /select-device {kind, device, channel?}     kind: input|output
//   POST /list-devices {kind}
//   POST /command     {type, args}     raw juce-end-to-end command passthrough
//   POST /quit                         clean quit
//   POST /shutdown                     quit app (if running) and stop server

import {createServer} from 'http';
import {createHarness} from './lib.mjs';

const args = process.argv.slice(2);
const port = Number(args.includes('--port') ? args[args.indexOf('--port') + 1] : 4499);
const autoLaunch = !args.includes('--no-launch');

const h = createHarness();

async function handle(pathname, body) {
  switch (pathname) {
    case '/status':
      return {running: h.state.pid !== null && h.state.exit === null, pid: h.state.pid, exit: h.state.exit};
    case '/launch': {
      const pid = await h.launch();
      await h.waitForAppReady();
      return {pid};
    }
    case '/eval': {
      const result = body.async
        ? await h.evalJsAsync(body.script, body.timeoutMs ?? 30000)
        : await h.evalJs(body.script, body.timeoutMs ?? 15000);
      return {result};
    }
    case '/screenshot':
      return {path: await h.nativeScreenshot(body.name ?? `screenshot-${Date.now()}.png`)};
    case '/new-project':
      await h.newProject();
      return {ok: true};
    case '/open-project':
      return {projectName: await h.openProject(body.path)};
    case '/select-device':
      await h.selectDevice(body.kind, body.device, body.channel ?? null);
      return {ok: true};
    case '/list-devices':
      return {devices: await h.listDevices(body.kind ?? 'input')};
    case '/command':
      return {response: await h.app.sendCommand({type: body.type, args: body.args ?? {}})};
    case '/quit':
      await h.app.quit();
      return {ok: true};
    case '/shutdown':
      if (h.state.pid !== null && h.state.exit === null) await h.app.quit().catch(() => {});
      setTimeout(() => process.exit(0), 100);
      return {ok: true};
    default:
      throw Object.assign(new Error(`unknown endpoint ${pathname}`), {status: 404});
  }
}

const server = createServer((req, res) => {
  let raw = '';
  req.on('data', (chunk) => (raw += chunk));
  req.on('end', async () => {
    let status = 200;
    let payload;
    try {
      const body = raw ? JSON.parse(raw) : {};
      payload = await handle(new URL(req.url, 'http://localhost').pathname, body);
    } catch (err) {
      status = err.status ?? 500;
      payload = {error: String(err?.message ?? err)};
    }
    res.writeHead(status, {'content-type': 'application/json'});
    res.end(JSON.stringify(payload));
  });
});

server.listen(port, '127.0.0.1', async () => {
  console.log(`[driver] listening on http://127.0.0.1:${port}`);
  if (autoLaunch) {
    try {
      const pid = await h.launch();
      await h.waitForAppReady();
      console.log(`[driver] app launched and ready, pid=${pid}`);
    } catch (err) {
      console.error(`[driver] launch failed: ${err}`);
    }
  }
});
