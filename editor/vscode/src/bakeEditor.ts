import * as vscode from "vscode";
import { RpcClient } from "./rpc";

let currentPanel: vscode.WebviewPanel | undefined;

export function openBakeEditor(client: RpcClient): void {
  if (currentPanel) {
    currentPanel.reveal();
    return;
  }

  currentPanel = vscode.window.createWebviewPanel(
    "kryga.bakeEditor",
    "Lightmap Baker",
    vscode.ViewColumn.One,
    { enableScripts: true, retainContextWhenHidden: true },
  );

  currentPanel.webview.html = html();

  currentPanel.webview.onDidReceiveMessage(async (msg) => {
    switch (msg?.type) {
      case "ready":
        refresh(client);
        break;
      case "setConfig":
        try {
          await client.request("tools.bake.setConfig", msg.patch);
        } catch (e) {
          vscode.window.showErrorMessage(`Kryga: bake.setConfig failed — ${e}`);
        }
        break;
      case "applyPreset":
        try {
          await client.request("tools.bake.applyPreset", { preset: msg.preset });
          refresh(client);
        } catch (e) {
          vscode.window.showErrorMessage(`Kryga: bake.applyPreset failed — ${e}`);
        }
        break;
      case "bake":
        try {
          await client.request("tools.bake.start");
        } catch (e) {
          vscode.window.showErrorMessage(`Kryga: bake.start failed — ${e}`);
        }
        break;
      case "refresh":
        refresh(client);
        break;
    }
  });

  client.onNotification("tools.action.started", () => {
    currentPanel?.webview.postMessage({ type: "busyChanged", busy: true });
  });
  client.onNotification("tools.action.completed", () => {
    currentPanel?.webview.postMessage({ type: "busyChanged", busy: false });
    refresh(client);
  });

  currentPanel.onDidDispose(() => {
    currentPanel = undefined;
  });
}

async function refresh(client: RpcClient): Promise<void> {
  try {
    const [config, scene] = await Promise.all([
      client.request("tools.bake.getConfig"),
      client.request("tools.bake.getSceneInfo"),
    ]);
    currentPanel?.webview.postMessage({ type: "load", config, scene });
  } catch (e) {
    currentPanel?.webview.postMessage({ type: "error", message: String(e) });
  }
}

function html(): string {
  return /* html */ `
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy"
        content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline';">
  <style>
    :root { color-scheme: var(--vscode-color-scheme); }
    body {
      margin: 0; padding: 12px 20px;
      font-family: var(--vscode-font-family);
      font-size: var(--vscode-font-size);
      color: var(--vscode-foreground);
      background: var(--vscode-editor-background);
      max-width: 600px;
    }
    .msg { color: var(--vscode-descriptionForeground); padding: 8px; }
    .err { color: var(--vscode-errorForeground); padding: 8px; }

    .section { margin: 6px 0; }
    .section-header {
      cursor: pointer; user-select: none;
      padding: 4px 8px; font-weight: 600;
      background: var(--vscode-editor-inactiveSelectionBackground);
      border-radius: 3px; font-size: 1.05em;
    }
    .section-header::before { content: "\\25BE "; }
    .section.collapsed .section-header::before { content: "\\25B8 "; }
    .section.collapsed .section-body { display: none; }
    .section-body { padding: 6px 6px 6px 16px; }

    .field {
      display: grid;
      grid-template-columns: 160px 1fr;
      align-items: center; gap: 8px;
      padding: 3px 0; min-height: 26px;
    }
    .field-label {
      color: var(--vscode-foreground);
      overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
    }
    .field-value { display: flex; gap: 4px; align-items: center; }
    .field-value input[type="number"] {
      flex: 1; min-width: 0; max-width: 160px;
      padding: 2px 6px;
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, transparent);
      border-radius: 2px; font-family: inherit; font-size: inherit;
    }
    .field-value input[type="number"]::-webkit-inner-spin-button { -webkit-appearance: none; margin: 0; }
    .field-value input[type="checkbox"] { margin: 0; }
    .field-value select {
      flex: 1; min-width: 0; max-width: 160px; padding: 2px 6px;
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, transparent);
      border-radius: 2px; font-family: inherit; font-size: inherit;
    }

    .toolbar {
      display: flex; gap: 8px; padding: 6px 0;
      border-bottom: 1px solid var(--vscode-panel-border, var(--vscode-sideBar-border));
      margin-bottom: 8px;
    }
    .toolbar button, .bake-btn {
      padding: 3px 12px;
      color: var(--vscode-button-foreground);
      background: var(--vscode-button-background);
      border: none; border-radius: 3px;
      cursor: pointer; font-size: 0.9em;
    }
    .toolbar button:hover, .bake-btn:hover {
      background: var(--vscode-button-hoverBackground);
    }
    .bake-btn {
      width: 100%; padding: 8px; font-size: 1em; font-weight: 600;
      margin-top: 8px;
    }
    .bake-btn:disabled {
      opacity: 0.5; cursor: not-allowed;
    }

    .preset-bar { display: flex; gap: 6px; margin: 4px 0; }
    .preset-btn {
      padding: 3px 14px;
      color: var(--vscode-button-secondaryForeground);
      background: var(--vscode-button-secondaryBackground);
      border: none; border-radius: 3px; cursor: pointer;
    }
    .preset-btn:hover {
      background: var(--vscode-button-secondaryHoverBackground);
    }

    .scene-info {
      font-size: 0.9em;
      color: var(--vscode-descriptionForeground);
      padding: 4px 0;
    }
    .warn {
      color: var(--vscode-editorWarning-foreground, #cca700);
      font-size: 0.9em; padding: 2px 0;
    }
  </style>
</head>
<body>
  <div id="root" class="msg">Loading...</div>
  <script>
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');
    let cfg = null;
    let scene = null;
    let busy = false;
    const collapsedSections = new Set();

    vscode.postMessage({ type: 'ready' });

    const RESOLUTIONS = [256, 512, 1024, 2048, 4096];

    window.addEventListener('message', (ev) => {
      const m = ev.data;
      if (m.type === 'load') {
        cfg = m.config;
        scene = m.scene;
        render();
      } else if (m.type === 'error') {
        root.className = 'err';
        root.textContent = 'Error: ' + m.message;
      } else if (m.type === 'busyChanged') {
        busy = m.busy;
        render();
      }
    });

    function send(patch) { vscode.postMessage({ type: 'setConfig', patch }); }

    function render() {
      if (!cfg || !scene) return;
      root.className = '';
      root.innerHTML = '';

      // Toolbar
      const tb = document.createElement('div');
      tb.className = 'toolbar';
      const refreshBtn = document.createElement('button');
      refreshBtn.textContent = 'Refresh';
      refreshBtn.addEventListener('click', () => vscode.postMessage({ type: 'refresh' }));
      tb.appendChild(refreshBtn);
      root.appendChild(tb);

      // Presets
      renderSection('Presets', (body) => {
        const bar = document.createElement('div');
        bar.className = 'preset-bar';
        for (const p of ['low', 'medium', 'high', 'maximum']) {
          const btn = document.createElement('button');
          btn.className = 'preset-btn';
          btn.textContent = p.charAt(0).toUpperCase() + p.slice(1);
          btn.addEventListener('click', () => vscode.postMessage({ type: 'applyPreset', preset: p }));
          bar.appendChild(btn);
        }
        body.appendChild(bar);
      });

      // Settings
      renderSection('Bake Settings', (body) => {
        addSelect(body, 'Resolution', RESOLUTIONS.map(r => ({ value: String(r), label: String(r) })),
          String(cfg.resolution),
          (v) => { cfg.resolution = parseInt(v); send({ resolution: cfg.resolution }); });

        addInt(body, 'Samples/Texel', cfg.samples_per_texel, 1, 1024,
          (v) => { cfg.samples_per_texel = v; send({ samples_per_texel: v }); });
        addInt(body, 'Bounces', cfg.bounce_count, 0, 8,
          (v) => { cfg.bounce_count = v; send({ bounce_count: v }); });
        addInt(body, 'Denoise Passes', cfg.denoise_iterations, 0, 8,
          (v) => { cfg.denoise_iterations = v; send({ denoise_iterations: v }); });

        addBool(body, 'Direct', cfg.bake_direct,
          (v) => { cfg.bake_direct = v; send({ bake_direct: v }); });
        addBool(body, 'Indirect', cfg.bake_indirect,
          (v) => { cfg.bake_indirect = v; send({ bake_indirect: v }); });
        addBool(body, 'AO', cfg.bake_ao,
          (v) => { cfg.bake_ao = v; send({ bake_ao: v }); });
        addFloat(body, 'AO Radius', cfg.ao_radius, 0.1, 0.1, 50,
          (v) => { cfg.ao_radius = v; send({ ao_radius: v }); });
        addFloat(body, 'AO Intensity', cfg.ao_intensity, 0.05, 0, 5,
          (v) => { cfg.ao_intensity = v; send({ ao_intensity: v }); });

        addFloat(body, 'Texels/Unit', cfg.texels_per_unit, 0.5, 0.5, 32,
          (v) => { cfg.texels_per_unit = v; send({ texels_per_unit: v }); });
        addInt(body, 'Min Tile', cfg.min_tile, 4, 128,
          (v) => { cfg.min_tile = v; send({ min_tile: v }); });
        addInt(body, 'Max Tile', cfg.max_tile, 32, 512,
          (v) => { cfg.max_tile = v; send({ max_tile: v }); });

        addInt(body, 'Shadow Samples', cfg.shadow_samples, 1, 128,
          (v) => { cfg.shadow_samples = v; send({ shadow_samples: v }); });
        addFloat(body, 'Shadow Spread', cfg.shadow_spread, 0.001, 0, 0.05,
          (v) => { cfg.shadow_spread = v; send({ shadow_spread: v }); });
        addFloat(body, 'Shadow Bias', cfg.shadow_bias, 0.001, 0.001, 0.1,
          (v) => { cfg.shadow_bias = v; send({ shadow_bias: v }); });
        addInt(body, 'Dilate Passes', cfg.dilate_iterations, 0, 8,
          (v) => { cfg.dilate_iterations = v; send({ dilate_iterations: v }); });

        addBool(body, 'Save PNG previews', cfg.save_png,
          (v) => { cfg.save_png = v; send({ save_png: v }); });
      });

      // Scene info
      renderSection('Scene', (body) => {
        const info = document.createElement('div');
        info.className = 'scene-info';
        info.innerHTML =
          'Static meshes: <b>' + scene.static_count + '</b><br>' +
          'Directional lights: <b>' + scene.directional_count + '</b><br>' +
          'Local lights: <b>' + scene.local_light_count + '</b>';
        body.appendChild(info);

        if (!scene.level_loaded) {
          const w = document.createElement('div');
          w.className = 'warn';
          w.textContent = 'No level loaded';
          body.appendChild(w);
        } else if (scene.static_count === 0) {
          const w = document.createElement('div');
          w.className = 'warn';
          w.textContent = 'No static meshes in level';
          body.appendChild(w);
        } else if (scene.directional_count === 0) {
          const w = document.createElement('div');
          w.className = 'warn';
          w.textContent = 'No directional lights in level';
          body.appendChild(w);
        }
      });

      // Bake button
      const canBake = scene.level_loaded && scene.static_count > 0 && scene.directional_count > 0 && !busy;
      const bakeBtn = document.createElement('button');
      bakeBtn.className = 'bake-btn';
      bakeBtn.textContent = busy ? 'Baking...' : 'Bake Lightmaps';
      bakeBtn.disabled = !canBake;
      bakeBtn.addEventListener('click', () => vscode.postMessage({ type: 'bake' }));
      root.appendChild(bakeBtn);
    }

    function renderSection(title, buildFn) {
      const sec = document.createElement('div');
      sec.className = 'section';
      if (collapsedSections.has(title)) sec.classList.add('collapsed');
      const hdr = document.createElement('div');
      hdr.className = 'section-header';
      hdr.textContent = title;
      hdr.addEventListener('click', () => {
        sec.classList.toggle('collapsed');
        if (sec.classList.contains('collapsed')) collapsedSections.add(title);
        else collapsedSections.delete(title);
      });
      sec.appendChild(hdr);
      const body = document.createElement('div');
      body.className = 'section-body';
      buildFn(body);
      sec.appendChild(body);
      root.appendChild(sec);
    }

    function makeField(parent, label) {
      const el = document.createElement('div');
      el.className = 'field';
      const lbl = document.createElement('div');
      lbl.className = 'field-label';
      lbl.textContent = label;
      el.appendChild(lbl);
      const val = document.createElement('div');
      val.className = 'field-value';
      el.appendChild(val);
      parent.appendChild(el);
      return val;
    }

    function addBool(parent, label, value, onChange) {
      const val = makeField(parent, label);
      const cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.checked = !!value;
      cb.addEventListener('change', () => onChange(cb.checked));
      val.appendChild(cb);
    }

    function addFloat(parent, label, value, step, min, max, onChange) {
      const val = makeField(parent, label);
      const inp = document.createElement('input');
      inp.type = 'number';
      inp.step = String(step);
      inp.min = String(min);
      inp.max = String(max);
      inp.value = fmtFloat(value);
      inp.addEventListener('change', () => {
        let v = parseFloat(inp.value);
        if (!Number.isFinite(v)) v = value;
        v = Math.max(min, Math.min(max, v));
        inp.value = fmtFloat(v);
        onChange(v);
      });
      inp.addEventListener('keydown', (e) => { if (e.key === 'Enter') inp.blur(); });
      val.appendChild(inp);
    }

    function addInt(parent, label, value, min, max, onChange) {
      const val = makeField(parent, label);
      const inp = document.createElement('input');
      inp.type = 'number';
      inp.step = '1';
      inp.min = String(min);
      inp.max = String(max);
      inp.value = String(value);
      inp.addEventListener('change', () => {
        let v = parseInt(inp.value, 10);
        if (!Number.isFinite(v)) v = value;
        v = Math.max(min, Math.min(max, v));
        inp.value = String(v);
        onChange(v);
      });
      inp.addEventListener('keydown', (e) => { if (e.key === 'Enter') inp.blur(); });
      val.appendChild(inp);
    }

    function addSelect(parent, label, options, current, onChange) {
      const val = makeField(parent, label);
      const sel = document.createElement('select');
      for (const opt of options) {
        const o = document.createElement('option');
        o.value = opt.value;
        o.textContent = opt.label;
        if (opt.value === current) o.selected = true;
        sel.appendChild(o);
      }
      sel.addEventListener('change', () => onChange(sel.value));
      val.appendChild(sel);
    }

    function fmtFloat(v) {
      if (!Number.isFinite(Number(v))) return '0';
      const n = Number(v);
      if (n === 0) return '0';
      return parseFloat(n.toPrecision(6)).toString();
    }
  </script>
</body>
</html>`;
}
