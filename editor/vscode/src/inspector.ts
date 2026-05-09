// Property inspector webview. Subscribes to selection.changed → fetches
// properties.get → renders categorized fields. Two-way: edits inside the
// webview send setProperty messages back to the extension, which calls
// properties.set on the engine. Engine echoes via properties.changed, which
// the extension forwards to the webview for reconciliation.
//
// Webview-side script + HTML are inlined as a single string for simplicity;
// move into media/*.html if this grows.

import * as vscode from "vscode";
import { RpcClient } from "./rpc";

interface Field {
  name: string;
  kind: string;
  value: unknown;
  readonly?: boolean;
}

interface Category {
  name: string;
  fields: Field[];
}

interface Owner {
  id: string;
  type: string;
  categories: Category[];
}

interface PropertiesPayload {
  id: string;
  owners: Owner[];
}

interface PropertyChanged {
  owner_id: string;
  name: string;
  value: unknown;
}

export class InspectorProvider implements vscode.WebviewViewProvider {
  private view?: vscode.WebviewView;
  private currentId: string | undefined;

  constructor(private readonly client: RpcClient) {}

  resolveWebviewView(view: vscode.WebviewView): void {
    this.view = view;
    view.webview.options = { enableScripts: true };
    view.webview.html = this.html();

    view.webview.onDidReceiveMessage(async (msg) => {
      if (msg?.type === "setProperty") {
        try {
          await this.client.request("properties.set", {
            owner_id: msg.owner_id,
            name: msg.name,
            value: msg.value,
          });
          // Engine echoes via properties.changed → reconciliation handled
          // there. No further action here.
        } catch (e) {
          vscode.window.showErrorMessage(`Kryga: set failed — ${e}`);
        }
      }
    });

    if (this.currentId) {
      this.refreshFor(this.currentId);
    } else {
      this.post({ type: "empty" });
    }
  }

  // Called by extension when selection changes.
  setSelection(id: string | undefined): void {
    this.currentId = id;
    if (!this.view) return;
    if (!id) {
      this.post({ type: "empty" });
      return;
    }
    this.refreshFor(id);
  }

  // Called by extension on properties.changed notification.
  reconcile(change: PropertyChanged): void {
    this.post({ type: "reconcile", change });
  }

  private async refreshFor(id: string): Promise<void> {
    try {
      const payload = await this.client.request<PropertiesPayload>(
        "properties.get",
        { id },
      );
      this.post({ type: "load", payload });
    } catch (e) {
      this.post({ type: "error", message: String(e) });
    }
  }

  private post(msg: unknown): void {
    this.view?.webview.postMessage(msg);
  }

  private html(): string {
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
      margin: 0; padding: 6px;
      font-family: var(--vscode-font-family);
      font-size: var(--vscode-font-size);
      color: var(--vscode-foreground);
      background: var(--vscode-sideBar-background);
    }
    .empty { color: var(--vscode-descriptionForeground); padding: 8px; }
    .toolbar {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 2px 0 6px 0;
      font-size: 0.9em;
      color: var(--vscode-descriptionForeground);
      border-bottom: 1px solid var(--vscode-sideBar-border);
      margin-bottom: 4px;
    }
    .toolbar label { display: flex; align-items: center; gap: 4px; cursor: pointer; }
    .object-header {
      font-weight: 600;
      padding: 4px 0;
      margin-bottom: 4px;
    }
    .owner { margin-bottom: 8px; }
    .owner-title {
      font-size: 0.9em;
      color: var(--vscode-descriptionForeground);
      padding: 2px 4px;
    }
    .category { margin: 4px 0; }
    .category-header {
      cursor: pointer;
      user-select: none;
      padding: 2px 4px;
      font-weight: 600;
      background: var(--vscode-editor-inactiveSelectionBackground);
      border-radius: 2px;
    }
    .category-header::before { content: "▾ "; }
    .category.collapsed .category-header::before { content: "▸ "; }
    .category.collapsed .fields { display: none; }
    .fields { padding: 4px 4px 4px 12px; }
    .field {
      display: grid;
      grid-template-columns: minmax(80px, 38%) 1fr;
      align-items: center;
      gap: 6px;
      padding: 1px 0;
      min-height: 22px;
    }
    .field-label {
      color: var(--vscode-foreground);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .field-label.scrub { cursor: ew-resize; }
    .field-value { display: flex; gap: 3px; align-items: center; }
    .field-value input[type="text"],
    .field-value input[type="number"] {
      flex: 1;
      min-width: 0;
      padding: 1px 4px;
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, transparent);
      font-family: inherit;
      font-size: inherit;
    }
    .field-value input[type="number"]::-webkit-inner-spin-button { -webkit-appearance: none; margin: 0; }
    .field-value input[type="checkbox"] { margin: 0; }
    .field-value .ro {
      font-family: var(--vscode-editor-font-family);
      color: var(--vscode-descriptionForeground);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .vec-input {
      display: grid;
      grid-template-columns: repeat(var(--cols, 3), 1fr);
      gap: 2px;
      flex: 1;
      min-width: 0;
    }
    .err { color: var(--vscode-errorForeground); padding: 8px; }
  </style>
</head>
<body>
  <div id="root" class="empty">No selection.</div>
  <script>
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');
    let currentPayload = null;
    let fullPrecision = false;

    // Track collapsed categories per (owner_id, category_name) so re-renders
    // don't reset open/close state. Stored in webview memory (not persisted).
    const collapsedKeys = new Set();
    const keyOf = (ownerId, catName) => ownerId + '\\u0000' + catName;

    window.addEventListener('message', (ev) => {
      const m = ev.data;
      if (m.type === 'empty') {
        currentPayload = null;
        root.className = 'empty';
        root.textContent = 'No selection.';
      } else if (m.type === 'error') {
        currentPayload = null;
        root.className = 'err';
        root.textContent = 'Error: ' + m.message;
      } else if (m.type === 'load') {
        currentPayload = m.payload;
        render(currentPayload);
      } else if (m.type === 'reconcile') {
        if (!currentPayload) return;
        // Patch in-memory payload then update the matching field's UI value
        // without rebuilding the whole DOM.
        for (const owner of currentPayload.owners) {
          if (owner.id !== m.change.owner_id) continue;
          for (const cat of owner.categories) {
            for (const f of cat.fields) {
              if (f.name === m.change.name) {
                f.value = m.change.value;
                applyFieldValue(owner.id, m.change.name, m.change.value);
                return;
              }
            }
          }
        }
      }
    });

    function render(p) {
      root.className = '';
      root.innerHTML = '';

      const tb = document.createElement('div');
      tb.className = 'toolbar';
      const lbl = document.createElement('label');
      const cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.checked = fullPrecision;
      cb.addEventListener('change', () => {
        fullPrecision = cb.checked;
        if (currentPayload) render(currentPayload);
      });
      lbl.appendChild(cb);
      lbl.appendChild(document.createTextNode('Full precision'));
      tb.appendChild(lbl);
      root.appendChild(tb);

      const header = document.createElement('div');
      header.className = 'object-header';
      header.textContent = p.id;
      root.appendChild(header);

      for (const owner of p.owners) {
        const empty = owner.categories.every(c => c.fields.length === 0);
        if (empty) continue;

        const ownerEl = document.createElement('div');
        ownerEl.className = 'owner';
        const t = document.createElement('div');
        t.className = 'owner-title';
        t.textContent = owner.type + (owner.id !== p.id ? '  —  ' + owner.id : '');
        ownerEl.appendChild(t);

        for (const cat of owner.categories) {
          if (cat.fields.length === 0) continue;
          const catEl = document.createElement('div');
          catEl.className = 'category';
          const k = keyOf(owner.id, cat.name);
          if (collapsedKeys.has(k)) catEl.classList.add('collapsed');
          const ch = document.createElement('div');
          ch.className = 'category-header';
          ch.textContent = cat.name;
          ch.addEventListener('click', () => {
            catEl.classList.toggle('collapsed');
            if (catEl.classList.contains('collapsed')) collapsedKeys.add(k); else collapsedKeys.delete(k);
          });
          catEl.appendChild(ch);

          const fields = document.createElement('div');
          fields.className = 'fields';
          for (const f of cat.fields) {
            fields.appendChild(renderField(owner.id, f));
          }
          catEl.appendChild(fields);
          ownerEl.appendChild(catEl);
        }

        root.appendChild(ownerEl);
      }
    }

    function renderField(ownerId, f) {
      const el = document.createElement('div');
      el.className = 'field';
      el.dataset.ownerId = ownerId;
      el.dataset.name = f.name;
      el.dataset.kind = f.kind;

      const label = document.createElement('div');
      label.className = 'field-label';
      label.textContent = f.name;
      el.appendChild(label);

      const val = document.createElement('div');
      val.className = 'field-value';

      if (f.readonly || !editable(f.kind)) {
        const ro = document.createElement('span');
        ro.className = 'ro';
        ro.textContent = formatRo(f.value);
        val.appendChild(ro);
      } else if (f.kind === 'bool') {
        const cb = document.createElement('input');
        cb.type = 'checkbox';
        cb.checked = !!f.value;
        cb.addEventListener('change', () =>
          send(ownerId, f.name, cb.checked));
        val.appendChild(cb);
      } else if (isNumeric(f.kind)) {
        attachScrub(label, () => readNumeric(el), (v) => writeNumeric(el, v),
                    (v) => send(ownerId, f.name, v));
        const inp = numericInput(f.kind, f.value);
        inp.addEventListener('change', () => {
          const parsed = parseNumeric(f.kind, inp.value);
          inp.value = fmtNum(parsed, f.kind);
          send(ownerId, f.name, parsed);
        });
        inp.addEventListener('keydown', (e) => {
          if (e.key === 'Enter') inp.blur();
        });
        val.appendChild(inp);
      } else if (isVec(f.kind)) {
        const cols = vecLen(f.kind);
        const wrap = document.createElement('div');
        wrap.className = 'vec-input';
        wrap.style.setProperty('--cols', String(cols));
        const inputs = [];
        const labels = ['x', 'y', 'z', 'w'];
        for (let i = 0; i < cols; ++i) {
          const inp = numericInput(f.kind, Array.isArray(f.value) ? f.value[i] : 0);
          inp.title = labels[i];
          inp.addEventListener('change', () => {
            const arr = inputs.map(x => parseFloat(x.value) || 0);
            inputs.forEach((x, j) => x.value = fmtNum(arr[j], f.kind));
            send(ownerId, f.name, arr);
          });
          inp.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') inp.blur();
          });
          inputs.push(inp);
          wrap.appendChild(inp);
        }
        attachScrub(label, () => inputs.map(x => parseFloat(x.value) || 0),
                    (vs) => inputs.forEach((inp, i) => inp.value = fmtNum(vs[i], f.kind)),
                    (vs) => send(ownerId, f.name, vs));
        val.appendChild(wrap);
      } else if (f.kind === 'string' || f.kind === 'id') {
        const inp = document.createElement('input');
        inp.type = 'text';
        inp.value = String(f.value ?? '');
        inp.addEventListener('change', () => send(ownerId, f.name, inp.value));
        val.appendChild(inp);
      }

      el.appendChild(val);
      return el;
    }

    function send(owner_id, name, value) {
      vscode.postMessage({ type: 'setProperty', owner_id, name, value });
    }

    function editable(kind) {
      return kind === 'bool' || kind === 'string' || kind === 'id'
        || isNumeric(kind) || isVec(kind);
    }
    function isNumeric(k) {
      return ['int8_t','int16_t','int32_t','int64_t',
              'uint8_t','uint16_t','uint32_t','uint64_t',
              'float','double'].includes(k);
    }
    function isFloat(k) { return k === 'float' || k === 'double' || isVec(k); }
    function isVec(k) { return k === 'vec2' || k === 'vec3' || k === 'vec4'; }
    function vecLen(k) { return k === 'vec2' ? 2 : k === 'vec3' ? 3 : 4; }

    function numericInput(kind, value) {
      const inp = document.createElement('input');
      inp.type = 'number';
      if (isFloat(kind)) inp.step = 'any';
      else inp.step = '1';
      inp.value = fmtNum(value ?? 0, kind);
      return inp;
    }
    function parseNumeric(kind, str) {
      const n = isFloat(kind) ? parseFloat(str) : parseInt(str, 10);
      return Number.isFinite(n) ? n : 0;
    }
    function readNumeric(fieldEl) {
      const inp = fieldEl.querySelector('input[type="number"]');
      return parseFloat(inp.value) || 0;
    }
    function writeNumeric(fieldEl, v) {
      const inp = fieldEl.querySelector('input[type="number"]');
      const kind = fieldEl.dataset.kind;
      inp.value = fmtNum(v, kind);
    }
    // Display rounding: floats get 6 significant digits, integers as-is.
    // Engine-side value is unaffected — this only shapes what we paint.
    // When fullPrecision is on, fall back to native toString().
    function fmtNum(n, kind) {
      if (n === null || n === undefined || !Number.isFinite(Number(n))) return '0';
      const num = Number(n);
      if (!isFloat(kind)) return String(Math.trunc(num));
      if (fullPrecision) return num.toString();
      if (num === 0) return '0';
      return parseFloat(num.toPrecision(6)).toString();
    }
    function formatRo(v) {
      if (v === null || v === undefined) return '';
      if (Array.isArray(v)) return '[' + v.map(formatRo).join(', ') + ']';
      if (typeof v === 'number') {
        if (!Number.isFinite(v)) return String(v);
        if (Number.isInteger(v)) return String(v);
        if (fullPrecision) return v.toString();
        return parseFloat(v.toPrecision(6)).toString();
      }
      if (typeof v === 'object') {
        // Vec maps {x,y,z[,w]} render bracketed in their natural order;
        // arbitrary maps (custom types) render key:value pairs.
        const vecKeys = ['x', 'y', 'z', 'w'].filter(k => k in v);
        if (vecKeys.length > 0 && Object.keys(v).every(k => vecKeys.includes(k))) {
          return '[' + vecKeys.map(k => formatRo(v[k])).join(', ') + ']';
        }
        return '{' + Object.entries(v).map(([k, x]) => k + ': ' + formatRo(x)).join(', ') + '}';
      }
      return String(v);
    }

    // Drag-scrub: mousedown on label, mousemove dx*step adjusts value(s),
    // mouseup commits. Scalar uses readNum/writeNum; vec uses array helpers.
    function attachScrub(label, read, write, commit) {
      label.classList.add('scrub');
      let dragging = false, startX = 0, startVal = null, last = null;
      label.addEventListener('mousedown', (e) => {
        dragging = true;
        startX = e.clientX;
        startVal = read();
        last = startVal;
        e.preventDefault();
      });
      window.addEventListener('mousemove', (e) => {
        if (!dragging) return;
        const dx = e.clientX - startX;
        // Adaptive step: ~1% of the field magnitude per pixel, with a floor of
        // 0.1 so zero/near-zero fields still scrub at a usable rate. Shift =
        // ×0.1 fine, Alt = ×10 coarse. Vec uses the largest component as the
        // magnitude reference so all three scale together.
        const ref = Array.isArray(startVal)
          ? Math.max(0.1, ...startVal.map(v => Math.abs(v)))
          : Math.max(0.1, Math.abs(startVal));
        let step = ref * 0.01;
        if (e.shiftKey) step *= 0.1;
        if (e.altKey) step *= 10;
        if (Array.isArray(startVal)) {
          last = startVal.map(v => v + dx * step);
          write(last);
        } else {
          last = startVal + dx * step;
          write(last);
        }
      });
      window.addEventListener('mouseup', () => {
        if (!dragging) return;
        dragging = false;
        if (last !== null) commit(last);
        last = null;
      });
    }

    function applyFieldValue(ownerId, name, value) {
      const sel = '.field[data-owner-id="' + cssEscape(ownerId) + '"][data-name="' + cssEscape(name) + '"]';
      const el = root.querySelector(sel);
      if (!el) return;
      const kind = el.dataset.kind;
      if (kind === 'bool') {
        const cb = el.querySelector('input[type="checkbox"]');
        if (cb) cb.checked = !!value;
      } else if (isNumeric(kind)) {
        const inp = el.querySelector('input[type="number"]');
        if (inp && document.activeElement !== inp) inp.value = fmtNum(value, kind);
      } else if (isVec(kind)) {
        const inputs = el.querySelectorAll('input[type="number"]');
        for (let i = 0; i < inputs.length; ++i) {
          if (document.activeElement !== inputs[i]) inputs[i].value = fmtNum(value[i], kind);
        }
      } else if (kind === 'string' || kind === 'id') {
        const inp = el.querySelector('input[type="text"]');
        if (inp && document.activeElement !== inp) inp.value = String(value ?? '');
      }
    }
    function cssEscape(s) {
      return String(s).replace(/[^a-zA-Z0-9_-]/g, c => '\\\\' + c);
    }
  </script>
</body>
</html>`;
  }
}
