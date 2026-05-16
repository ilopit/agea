import * as vscode from "vscode";
import { RpcClient } from "./rpc";

let currentPanel: vscode.WebviewPanel | undefined;

export function openRenderConfig(client: RpcClient): void {
  if (currentPanel) {
    currentPanel.reveal();
    return;
  }

  currentPanel = vscode.window.createWebviewPanel(
    "kryga.renderConfig",
    "Render Config",
    vscode.ViewColumn.One,
    { enableScripts: true, retainContextWhenHidden: true },
  );

  currentPanel.webview.html = html();

  currentPanel.webview.onDidReceiveMessage(async (msg) => {
    if (msg?.type === "set") {
      try {
        await client.request("render.config.set", msg.patch);
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: render.config.set failed — ${e}`);
      }
    } else if (msg?.type === "refresh" || msg?.type === "ready") {
      refresh(client);
    }
  });

  currentPanel.onDidDispose(() => {
    currentPanel = undefined;
  });
}

export function refreshRenderConfig(client: RpcClient): void {
  if (currentPanel) {
    refresh(client);
  }
}

async function refresh(client: RpcClient): Promise<void> {
  try {
    const cfg = await client.request("render.config.get");
    currentPanel?.webview.postMessage({ type: "load", config: cfg });
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
      cursor: pointer;
      user-select: none;
      padding: 4px 8px;
      font-weight: 600;
      background: var(--vscode-editor-inactiveSelectionBackground);
      border-radius: 3px;
      font-size: 1.05em;
    }
    .section-header::before { content: "\\25BE "; }
    .section.collapsed .section-header::before { content: "\\25B8 "; }
    .section.collapsed .section-body { display: none; }
    .section-body { padding: 6px 6px 6px 16px; }

    .field {
      display: grid;
      grid-template-columns: 180px 1fr;
      align-items: center;
      gap: 8px;
      padding: 3px 0;
      min-height: 26px;
    }
    .field-label {
      color: var(--vscode-foreground);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .field-value { display: flex; gap: 4px; align-items: center; }
    .field-value input[type="number"] {
      flex: 1; min-width: 0; max-width: 200px;
      padding: 2px 6px;
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, transparent);
      border-radius: 2px;
      font-family: inherit; font-size: inherit;
    }
    .field-value input[type="number"]::-webkit-inner-spin-button { -webkit-appearance: none; margin: 0; }
    .field-value input[type="checkbox"] { margin: 0; }
    .field-value select {
      flex: 1; min-width: 0; max-width: 200px;
      padding: 2px 6px;
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, transparent);
      border-radius: 2px;
      font-family: inherit; font-size: inherit;
    }

    .color-row {
      display: flex; gap: 6px; flex: 1; align-items: center;
    }
    .color-swatch {
      width: 24px; height: 24px;
      border-radius: 3px;
      border: 1px solid var(--vscode-input-border, #444);
      cursor: pointer; flex-shrink: 0;
      position: relative;
    }
    .color-swatch input[type="color"] {
      opacity: 0; width: 0; height: 0; position: absolute;
    }

    .toolbar {
      display: flex; gap: 8px; padding: 6px 0;
      border-bottom: 1px solid var(--vscode-panel-border, var(--vscode-sideBar-border));
      margin-bottom: 8px;
    }
    .toolbar button {
      padding: 3px 12px;
      color: var(--vscode-button-foreground);
      background: var(--vscode-button-background);
      border: none; border-radius: 3px;
      cursor: pointer; font-size: 0.9em;
    }
    .toolbar button:hover {
      background: var(--vscode-button-hoverBackground);
    }
  </style>
</head>
<body>
  <div id="root" class="msg">Loading…</div>
  <script>
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');
    let cfg = null;
    const collapsedSections = new Set();

    vscode.postMessage({ type: 'ready' });

    const PCF_MODES = [
      { value: 'pcf_3x3', label: '3x3 (9 taps)' },
      { value: 'pcf_5x5', label: '5x5 (25 taps)' },
      { value: 'pcf_7x7', label: '7x7 (49 taps)' },
      { value: 'poisson16', label: 'Poisson 16' },
      { value: 'poisson32', label: 'Poisson 32' },
    ];

    const MAP_SIZES = [256, 512, 1024, 2048, 4096, 8192];

    window.addEventListener('message', (ev) => {
      const m = ev.data;
      if (m.type === 'load') {
        cfg = m.config;
        render();
      } else if (m.type === 'error') {
        root.className = 'err';
        root.textContent = 'Error: ' + m.message;
      }
    });

    function send(patch) {
      vscode.postMessage({ type: 'set', patch });
    }

    function render() {
      if (!cfg) return;
      root.className = '';
      root.innerHTML = '';

      const tb = document.createElement('div');
      tb.className = 'toolbar';
      const refreshBtn = document.createElement('button');
      refreshBtn.textContent = 'Refresh';
      refreshBtn.addEventListener('click', () => vscode.postMessage({ type: 'refresh' }));
      tb.appendChild(refreshBtn);
      root.appendChild(tb);

      renderSection('Lighting', (body) => {
        addBool(body, 'Directional', cfg.lighting.directional_enabled,
          (v) => send({ lighting: { directional_enabled: v } }));
        addBool(body, 'Local (Point/Spot)', cfg.lighting.local_enabled,
          (v) => send({ lighting: { local_enabled: v } }));
        addBool(body, 'Baked (Lightmaps)', cfg.lighting.baked_enabled,
          (v) => send({ lighting: { baked_enabled: v } }));
      });

      renderSection('Shadows', (body) => {
        addBool(body, 'Enabled', cfg.shadows.enabled,
          (v) => send({ shadows: { enabled: v } }));
        addSelect(body, 'PCF Mode', PCF_MODES, cfg.shadows.pcf_name,
          (v) => send({ shadows: { pcf: v } }));
        addFloat(body, 'Bias', cfg.shadows.bias, 0.0001, 0, 0.1,
          (v) => send({ shadows: { bias: v } }));
        addFloat(body, 'Normal Bias', cfg.shadows.normal_bias, 0.001, 0, 0.5,
          (v) => send({ shadows: { normal_bias: v } }));
        addInt(body, 'Cascades', cfg.shadows.cascade_count, 1, 4,
          (v) => send({ shadows: { cascade_count: v } }));
        addFloat(body, 'Distance', cfg.shadows.distance, 1, 10, 2000,
          (v) => send({ shadows: { distance: v } }));
        addSelect(body, 'Map Size', MAP_SIZES.map(s => ({ value: String(s), label: String(s) })),
          String(cfg.shadows.map_size),
          (v) => send({ shadows: { map_size: parseInt(v) } }));
      });

      renderSection('Clusters', (body) => {
        addInt(body, 'Tile Size', cfg.clusters.tile_size, 16, 512,
          (v) => send({ clusters: { tile_size: v } }));
        addInt(body, 'Depth Slices', cfg.clusters.depth_slices, 1, 64,
          (v) => send({ clusters: { depth_slices: v } }));
        addInt(body, 'Max Lights/Cluster', cfg.clusters.max_lights_per_cluster, 1, 256,
          (v) => send({ clusters: { max_lights_per_cluster: v } }));
      });

      renderSection('Debug', (body) => {
        addBool(body, 'Editor Mode', cfg.debug.editor_mode,
          (v) => send({ debug: { editor_mode: v } }));
        addBool(body, 'Show Grid', cfg.debug.show_grid,
          (v) => send({ debug: { show_grid: v } }));
        addBool(body, 'Light Wireframe', cfg.debug.light_wireframe,
          (v) => send({ debug: { light_wireframe: v } }));
        addBool(body, 'Light Icons', cfg.debug.light_icons,
          (v) => send({ debug: { light_icons: v } }));
        addBool(body, 'Frustum Culling', cfg.debug.frustum_culling,
          (v) => send({ debug: { frustum_culling: v } }));
      });

      renderSection('Render Scale', (body) => {
        addBool(body, 'Enabled', cfg.render_scale.enabled,
          (v) => send({ render_scale: { enabled: v } }));
        addInt(body, 'Divisor', cfg.render_scale.divisor, 1, 10,
          (v) => send({ render_scale: { divisor: v } }));
      });

      renderSection('Outline', (body) => {
        addBool(body, 'Enabled', cfg.outline.enabled,
          (v) => send({ outline: { enabled: v } }));
        addColor(body, 'Color', cfg.outline.color,
          (v) => send({ outline: { color: v } }));
        addFloat(body, 'Depth Threshold', cfg.outline.depth_threshold, 0.01, 0, 1,
          (v) => send({ outline: { depth_threshold: v } }));
        addFloat(body, 'Normal Threshold', cfg.outline.normal_threshold, 0.01, 0, 1,
          (v) => send({ outline: { normal_threshold: v } }));
      });
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

    function addColor(parent, label, value, onChange) {
      const val = makeField(parent, label);
      const row = document.createElement('div');
      row.className = 'color-row';

      const swatch = document.createElement('div');
      swatch.className = 'color-swatch';
      const arr = Array.isArray(value) ? value : [0,0,0,1];
      updateSwatch(swatch, arr);

      const picker = document.createElement('input');
      picker.type = 'color';
      picker.value = rgbToHex(arr);
      picker.addEventListener('input', () => {
        const rgb = hexToRgb(picker.value);
        const result = [rgb[0], rgb[1], rgb[2], arr[3] || 1];
        updateSwatch(swatch, result);
        text.textContent = result.map(v => fmtFloat(v)).join(', ');
        onChange(result);
      });
      swatch.addEventListener('click', () => picker.click());
      swatch.appendChild(picker);

      row.appendChild(swatch);

      const text = document.createElement('span');
      text.style.fontSize = '0.85em';
      text.style.color = 'var(--vscode-descriptionForeground)';
      text.textContent = arr.map(v => fmtFloat(v)).join(', ');
      row.appendChild(text);

      val.appendChild(row);
    }

    function updateSwatch(swatch, arr) {
      const r = Math.round(clamp01(arr[0]) * 255);
      const g = Math.round(clamp01(arr[1]) * 255);
      const b = Math.round(clamp01(arr[2]) * 255);
      swatch.style.background = 'rgb(' + r + ',' + g + ',' + b + ')';
    }
    function clamp01(v) { return Math.max(0, Math.min(1, v)); }
    function rgbToHex(arr) {
      return '#' + [0,1,2].map(i =>
        Math.round(clamp01(arr[i]) * 255).toString(16).padStart(2, '0')).join('');
    }
    function hexToRgb(hex) {
      const m = hex.match(/^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i);
      if (!m) return [0,0,0];
      return [parseInt(m[1],16)/255, parseInt(m[2],16)/255, parseInt(m[3],16)/255];
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
