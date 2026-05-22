import * as vscode from "vscode";
import { RpcClient } from "./rpc";

let panel: vscode.WebviewPanel | undefined;

export function openMaterialBrowser(
  client: RpcClient,
  ownerId: string,
  currentMaterial: string,
): void {
  if (panel) {
    panel.reveal();
    panel.webview.postMessage({
      type: "init",
      owner_id: ownerId,
      current_material: currentMaterial,
    });
    return;
  }

  panel = vscode.window.createWebviewPanel(
    "kryga.materialBrowser",
    "Kryga: Materials",
    vscode.ViewColumn.Beside,
    { enableScripts: true },
  );

  panel.onDidDispose(() => {
    panel = undefined;
  });

  panel.webview.html = html();

  panel.webview.onDidReceiveMessage(async (msg) => {
    if (msg.type === "ready" || msg.type === "refresh") {
      try {
        const res = await client.request<{
          items: { id: string; type_name?: string }[];
        }>("model.list", { source: "all", kind: "material" });
        const materials = (res.items ?? []).map((m) => ({
          id: m.id,
          type: m.type_name ?? "material",
        }));
        panel?.webview.postMessage({
          type: "materials",
          materials,
          owner_id: msg.owner_id,
          current_material: msg.current_material,
        });
      } catch (e) {
        panel?.webview.postMessage({
          type: "error",
          message: String(e),
        });
      }
    } else if (msg.type === "requestPreview") {
      try {
        const res = await client.request<{ image: string }>(
          "editor.material.preview",
          { id: msg.material_id, size: msg.size || 128 },
        );
        panel?.webview.postMessage({
          type: "previewResult",
          material_id: msg.material_id,
          image: res.image,
          target: msg.target || "grid",
        });
      } catch {
        // preview unavailable
      }
    } else if (msg.type === "assign") {
      try {
        await client.request("model.object.property.set", {
          owner_id: msg.owner_id,
          name: "material",
          value: msg.material_id,
        });
        panel?.webview.postMessage({
          type: "assigned",
          material_id: msg.material_id,
        });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: assign failed — ${e}`);
      }
    } else if (msg.type === "editMaterial") {
      try {
        const res = await client.request<{ edit_id: string; material: unknown }>(
          "editor.material.edit",
          { id: msg.material_id },
        );
        panel?.webview.postMessage({
          type: "editResult",
          material_id: msg.material_id,
          edit_id: res.edit_id,
          material: res.material,
        });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: edit failed — ${e}`);
      }
    } else if (msg.type === "editField") {
      try {
        const res = await client.request<{ value: unknown }>(
          "model.object.property.set",
          { owner_id: msg.material_id, name: msg.field, value: msg.value },
        );
        panel?.webview.postMessage({
          type: "fieldUpdated",
          material_id: msg.material_id,
          field: msg.field,
          value: res.value,
        });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: setField failed — ${e}`);
      }
    } else if (msg.type === "saveEdit") {
      try {
        await client.request("editor.material.save", { id: msg.material_id });
        panel?.webview.postMessage({
          type: "editSaved",
          material_id: msg.material_id,
        });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: save failed — ${e}`);
      }
    } else if (msg.type === "discardEdit") {
      try {
        await client.request("editor.material.discard", { id: msg.material_id });
        panel?.webview.postMessage({
          type: "editDiscarded",
          material_id: msg.material_id,
        });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: discard failed — ${e}`);
      }
    }
  });

  panel.webview.postMessage({
    type: "init",
    owner_id: ownerId,
    current_material: currentMaterial,
  });
}

function html(): string {
  return /* html */ `
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy"
        content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:;">
  <style>
    :root { color-scheme: var(--vscode-color-scheme); }
    body {
      margin: 0; padding: 12px;
      font-family: var(--vscode-font-family);
      font-size: var(--vscode-font-size);
      color: var(--vscode-foreground);
      background: var(--vscode-editor-background);
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 12px;
      padding-bottom: 8px;
      border-bottom: 1px solid var(--vscode-sideBar-border);
    }
    .header h2 { margin: 0; font-size: 1.1em; }
    .current-label {
      font-size: 0.85em;
      color: var(--vscode-descriptionForeground);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
      gap: 8px;
    }
    .mat-card {
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 8px;
      border: 2px solid transparent;
      border-radius: 6px;
      cursor: pointer;
      transition: background 0.1s;
      position: relative;
    }
    .mat-card:hover {
      background: var(--vscode-list-hoverBackground);
    }
    .mat-card.active {
      border-color: var(--vscode-focusBorder);
      background: var(--vscode-list-activeSelectionBackground);
    }
    .mat-card .preview {
      width: 80px;
      height: 80px;
      border-radius: 50%;
      background: var(--vscode-editor-inactiveSelectionBackground);
      margin-bottom: 6px;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }
    .mat-card .preview img {
      width: 80px;
      height: 80px;
      image-rendering: auto;
    }
    .mat-card .preview .placeholder {
      font-size: 0.75em;
      color: var(--vscode-descriptionForeground);
    }
    .mat-card .label {
      font-size: 0.8em;
      text-align: center;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      max-width: 100%;
    }
    .mat-card .edit-btn {
      position: absolute;
      top: 4px;
      right: 4px;
      width: 22px;
      height: 22px;
      border-radius: 3px;
      border: none;
      background: var(--vscode-button-secondaryBackground);
      color: var(--vscode-button-secondaryForeground);
      cursor: pointer;
      font-size: 12px;
      display: flex;
      align-items: center;
      justify-content: center;
      opacity: 0;
      transition: opacity 0.15s;
    }
    .mat-card:hover .edit-btn { opacity: 0.8; }
    .mat-card .edit-btn:hover { opacity: 1; background: var(--vscode-button-background); color: var(--vscode-button-foreground); }

    /* ── Editor panel ────────────────────────────── */
    .editor-panel {
      margin-top: 12px;
      border: 1px solid var(--vscode-sideBar-border);
      border-radius: 6px;
      overflow: hidden;
    }
    .editor-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 8px 12px;
      background: var(--vscode-editor-inactiveSelectionBackground);
      border-bottom: 1px solid var(--vscode-sideBar-border);
    }
    .editor-header .editor-title {
      font-weight: 600;
      font-size: 0.95em;
    }
    .editor-header .editor-type {
      font-size: 0.8em;
      color: var(--vscode-descriptionForeground);
      margin-left: 8px;
    }
    .editor-actions {
      display: flex;
      gap: 6px;
    }
    .editor-actions button {
      padding: 3px 10px;
      border: none;
      border-radius: 3px;
      cursor: pointer;
      font-size: 0.85em;
      font-family: inherit;
    }
    .btn-save {
      background: var(--vscode-button-background);
      color: var(--vscode-button-foreground);
    }
    .btn-save:hover { background: var(--vscode-button-hoverBackground); }
    .btn-discard {
      background: var(--vscode-button-secondaryBackground);
      color: var(--vscode-button-secondaryForeground);
    }
    .btn-discard:hover { background: var(--vscode-button-secondaryHoverBackground); }
    .btn-close {
      background: none;
      color: var(--vscode-foreground);
      font-size: 1.1em;
      padding: 0 4px;
      opacity: 0.6;
    }
    .btn-close:hover { opacity: 1; }

    .editor-body {
      display: flex;
      gap: 16px;
      padding: 12px;
    }
    .editor-preview {
      flex-shrink: 0;
      width: 160px;
      height: 160px;
      border-radius: 50%;
      background: var(--vscode-editor-inactiveSelectionBackground);
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }
    .editor-preview img {
      width: 160px;
      height: 160px;
      image-rendering: auto;
    }
    .editor-preview .placeholder {
      font-size: 0.85em;
      color: var(--vscode-descriptionForeground);
    }
    .editor-fields {
      flex: 1;
      min-width: 0;
    }
    .editor-category { margin-bottom: 8px; }
    .editor-category-header {
      cursor: pointer;
      user-select: none;
      padding: 2px 4px;
      font-weight: 600;
      font-size: 0.9em;
      background: var(--vscode-editor-inactiveSelectionBackground);
      border-radius: 2px;
      margin-bottom: 4px;
    }
    .editor-category-header::before { content: "\\25BE "; }
    .editor-category.collapsed .editor-category-header::before { content: "\\25B8 "; }
    .editor-category.collapsed .cat-fields { display: none; }
    .cat-fields { padding: 2px 4px 2px 12px; }
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
      font-size: 0.9em;
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
    .field-value select {
      flex: 1;
      min-width: 0;
      padding: 1px 4px;
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, transparent);
      font-family: inherit;
      font-size: inherit;
    }
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
    .color-row {
      display: flex;
      gap: 4px;
      flex: 1;
      align-items: center;
    }
    .color-row .vec-input { flex: 1; }
    .color-swatch {
      width: 22px;
      height: 22px;
      border-radius: 3px;
      border: 1px solid var(--vscode-input-border, #444);
      cursor: pointer;
      flex-shrink: 0;
    }
    .color-swatch input[type="color"] {
      opacity: 0;
      width: 0;
      height: 0;
      position: absolute;
    }

    .err { color: var(--vscode-errorForeground); padding: 12px; }
    .loading { color: var(--vscode-descriptionForeground); padding: 12px; }
  </style>
</head>
<body>
  <div class="header">
    <h2>Materials</h2>
    <span class="current-label" id="current-label"></span>
  </div>
  <div id="root" class="loading">Loading materials...</div>
  <div id="editor"></div>
  <script>
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');
    const editorRoot = document.getElementById('editor');
    const currentLabel = document.getElementById('current-label');
    let ownerId = '';
    let currentMaterial = '';
    let editingId = null;
    let editingEditId = null;
    let editingData = null;
    let previewDebounce = null;

    window.addEventListener('message', (ev) => {
      const m = ev.data;
      if (m.type === 'init') {
        ownerId = m.owner_id;
        currentMaterial = m.current_material;
        currentLabel.textContent = 'Current: ' + currentMaterial;
        vscode.postMessage({ type: 'refresh', owner_id: ownerId, current_material: currentMaterial });
      } else if (m.type === 'materials') {
        renderGrid(m.materials, m.current_material);
        for (const mat of m.materials) {
          vscode.postMessage({ type: 'requestPreview', material_id: mat.id });
        }
      } else if (m.type === 'previewResult') {
        if (m.target === 'editor') {
          const img = document.getElementById('editor-preview-img');
          if (img) {
            img.src = m.image;
            img.style.display = '';
            const ph = img.parentElement.querySelector('.placeholder');
            if (ph) ph.style.display = 'none';
          }
          // Also update the grid card
          const gridImg = document.getElementById('preview-' + m.material_id);
          if (gridImg) {
            gridImg.src = m.image;
            gridImg.style.display = '';
            const gph = gridImg.parentElement.querySelector('.placeholder');
            if (gph) gph.style.display = 'none';
          }
        } else {
          const img = document.getElementById('preview-' + m.material_id);
          if (img) {
            img.src = m.image;
            img.style.display = '';
            const ph = img.parentElement.querySelector('.placeholder');
            if (ph) ph.style.display = 'none';
          }
        }
      } else if (m.type === 'assigned') {
        currentMaterial = m.material_id;
        currentLabel.textContent = 'Current: ' + currentMaterial;
        document.querySelectorAll('.mat-card').forEach(c => c.classList.remove('active'));
        const card = document.getElementById('card-' + m.material_id);
        if (card) card.classList.add('active');
      } else if (m.type === 'editResult') {
        editingId = m.material_id;
        editingEditId = m.edit_id || m.material_id;
        editingData = m.material;
        renderEditor(m.material_id, m.material);
        vscode.postMessage({ type: 'requestPreview', material_id: m.material_id, size: 256, target: 'editor' });
      } else if (m.type === 'fieldUpdated') {
        if (m.material_id === editingEditId || m.material_id === editingId) {
          updateFieldValue(m.field, m.value);
          schedulePreviewRefresh(editingId);
        }
      } else if (m.type === 'editSaved' || m.type === 'editDiscarded') {
        closeEditor();
        vscode.postMessage({ type: 'requestPreview', material_id: m.material_id });
      } else if (m.type === 'error') {
        root.className = 'err';
        root.textContent = 'Error: ' + m.message;
      }
    });

    function schedulePreviewRefresh(matId) {
      if (previewDebounce) clearTimeout(previewDebounce);
      previewDebounce = setTimeout(() => {
        vscode.postMessage({ type: 'requestPreview', material_id: matId, size: 256, target: 'editor' });
        previewDebounce = null;
      }, 200);
    }

    function renderGrid(materials, current) {
      root.className = 'grid';
      root.innerHTML = '';
      for (const mat of materials) {
        const card = document.createElement('div');
        card.className = 'mat-card';
        card.id = 'card-' + mat.id;
        if (mat.id === current) card.classList.add('active');

        const editBtn = document.createElement('button');
        editBtn.className = 'edit-btn';
        editBtn.textContent = '\\u270E';
        editBtn.title = 'Edit material';
        editBtn.addEventListener('click', (e) => {
          e.stopPropagation();
          vscode.postMessage({ type: 'editMaterial', material_id: mat.id });
        });
        card.appendChild(editBtn);

        const preview = document.createElement('div');
        preview.className = 'preview';
        const img = document.createElement('img');
        img.id = 'preview-' + mat.id;
        img.style.display = 'none';
        preview.appendChild(img);
        const ph = document.createElement('span');
        ph.className = 'placeholder';
        ph.textContent = '...';
        preview.appendChild(ph);
        card.appendChild(preview);

        const label = document.createElement('div');
        label.className = 'label';
        label.textContent = mat.id;
        label.title = mat.id + ' (' + mat.type + ')';
        card.appendChild(label);

        card.addEventListener('click', () => {
          vscode.postMessage({ type: 'assign', owner_id: ownerId, material_id: mat.id });
        });

        root.appendChild(card);
      }
    }

    // ── Editor panel ─────────────────────────────────────────────

    function renderEditor(matId, matData) {
      editorRoot.innerHTML = '';
      const panel = document.createElement('div');
      panel.className = 'editor-panel';

      // Header
      const header = document.createElement('div');
      header.className = 'editor-header';
      const titleArea = document.createElement('span');
      const title = document.createElement('span');
      title.className = 'editor-title';
      title.textContent = matId;
      titleArea.appendChild(title);
      if (matData.type) {
        const tp = document.createElement('span');
        tp.className = 'editor-type';
        tp.textContent = matData.type;
        titleArea.appendChild(tp);
      }
      header.appendChild(titleArea);

      const actions = document.createElement('div');
      actions.className = 'editor-actions';
      const saveBtn = document.createElement('button');
      saveBtn.className = 'btn-save';
      saveBtn.textContent = 'Save';
      saveBtn.addEventListener('click', () => {
        vscode.postMessage({ type: 'saveEdit', material_id: matId });
      });
      const discardBtn = document.createElement('button');
      discardBtn.className = 'btn-discard';
      discardBtn.textContent = 'Discard';
      discardBtn.addEventListener('click', () => {
        vscode.postMessage({ type: 'discardEdit', material_id: matId });
      });
      const closeBtn = document.createElement('button');
      closeBtn.className = 'btn-close';
      closeBtn.textContent = '\\u00D7';
      closeBtn.title = 'Close without saving';
      closeBtn.addEventListener('click', () => {
        vscode.postMessage({ type: 'discardEdit', material_id: matId });
      });
      actions.appendChild(saveBtn);
      actions.appendChild(discardBtn);
      actions.appendChild(closeBtn);
      header.appendChild(actions);
      panel.appendChild(header);

      // Body: preview + fields
      const body = document.createElement('div');
      body.className = 'editor-body';

      const previewWrap = document.createElement('div');
      previewWrap.className = 'editor-preview';
      const previewImg = document.createElement('img');
      previewImg.id = 'editor-preview-img';
      previewImg.style.display = 'none';
      previewWrap.appendChild(previewImg);
      const ph = document.createElement('span');
      ph.className = 'placeholder';
      ph.textContent = 'Rendering...';
      previewWrap.appendChild(ph);
      body.appendChild(previewWrap);

      const fieldsArea = document.createElement('div');
      fieldsArea.className = 'editor-fields';

      const cats = matData.categories || [];
      for (const cat of cats) {
        const editableFields = cat.fields.filter(f => !f.readonly && fieldEditable(f));
        if (editableFields.length === 0) continue;

        const catEl = document.createElement('div');
        catEl.className = 'editor-category';
        const ch = document.createElement('div');
        ch.className = 'editor-category-header';
        ch.textContent = cat.name;
        ch.addEventListener('click', () => catEl.classList.toggle('collapsed'));
        catEl.appendChild(ch);

        const fieldsEl = document.createElement('div');
        fieldsEl.className = 'cat-fields';
        for (const f of editableFields) {
          fieldsEl.appendChild(renderField(matId, f));
        }
        catEl.appendChild(fieldsEl);
        fieldsArea.appendChild(catEl);
      }

      body.appendChild(fieldsArea);
      panel.appendChild(body);
      editorRoot.appendChild(panel);
      editorRoot.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }

    function closeEditor() {
      editingId = null;
      editingEditId = null;
      editingData = null;
      editorRoot.innerHTML = '';
    }

    // ── Field rendering (shared logic with inspector) ────────────

    function renderField(matId, f) {
      const el = document.createElement('div');
      el.className = 'field';
      el.dataset.matId = matId;
      el.dataset.name = f.name;
      el.dataset.kind = f.kind;

      const label = document.createElement('div');
      label.className = 'field-label';
      label.textContent = f.name;
      el.appendChild(label);

      const val = document.createElement('div');
      val.className = 'field-value';

      if (f.enum_values && f.enum_values.length > 0) {
        const sel = document.createElement('select');
        for (const ev of f.enum_values) {
          const opt = document.createElement('option');
          opt.value = ev;
          opt.textContent = ev;
          if (String(f.value) === ev) opt.selected = true;
          sel.appendChild(opt);
        }
        sel.addEventListener('change', () => sendField(matId, f.name, sel.value));
        val.appendChild(sel);
      } else if (f.kind === 'bool') {
        const cb = document.createElement('input');
        cb.type = 'checkbox';
        cb.checked = !!f.value;
        cb.addEventListener('change', () => sendField(matId, f.name, cb.checked));
        val.appendChild(cb);
      } else if (isNumeric(f.kind)) {
        attachScrub(label, () => readNumeric(el), (v) => writeNumeric(el, v),
                    (v) => sendField(matId, f.name, v));
        const inp = numericInput(f.kind, f.value);
        inp.addEventListener('change', () => {
          const parsed = parseNumeric(f.kind, inp.value);
          inp.value = fmtNum(parsed, f.kind);
          sendField(matId, f.name, parsed);
        });
        inp.addEventListener('keydown', (e) => { if (e.key === 'Enter') inp.blur(); });
        val.appendChild(inp);
      } else if (isVec(f.kind)) {
        const cols = vecLen(f.kind);
        const useColor = (cols === 3 || cols === 4) && isColorHint(f);

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
            sendField(matId, f.name, arr);
            if (swatch) updateSwatch(swatch, arr);
          });
          inp.addEventListener('keydown', (e) => { if (e.key === 'Enter') inp.blur(); });
          inputs.push(inp);
          wrap.appendChild(inp);
        }
        attachScrub(label, () => inputs.map(x => parseFloat(x.value) || 0),
                    (vs) => { inputs.forEach((inp, i) => inp.value = fmtNum(vs[i], f.kind)); if (swatch) updateSwatch(swatch, vs); },
                    (vs) => sendField(matId, f.name, vs));

        let swatch = null;
        if (useColor) {
          const row = document.createElement('div');
          row.className = 'color-row';

          swatch = document.createElement('div');
          swatch.className = 'color-swatch';
          const arr = Array.isArray(f.value) ? f.value : [0,0,0];
          updateSwatch(swatch, arr);

          const picker = document.createElement('input');
          picker.type = 'color';
          picker.value = rgbToHex(arr);
          picker.addEventListener('input', () => {
            const rgb = hexToRgb(picker.value);
            inputs.forEach((inp, i) => { if (i < 3) inp.value = fmtNum(rgb[i], f.kind); });
            const result = inputs.map(x => parseFloat(x.value) || 0);
            updateSwatch(swatch, result);
            sendField(matId, f.name, result);
          });
          swatch.addEventListener('click', () => picker.click());
          swatch.appendChild(picker);

          row.appendChild(swatch);
          row.appendChild(wrap);
          val.appendChild(row);
        } else {
          val.appendChild(wrap);
        }
      } else if (f.kind === 'string' || f.kind === 'id') {
        const inp = document.createElement('input');
        inp.type = 'text';
        inp.value = String(f.value ?? '');
        inp.addEventListener('change', () => sendField(matId, f.name, inp.value));
        val.appendChild(inp);
      } else {
        const ro = document.createElement('span');
        ro.className = 'ro';
        ro.textContent = formatRo(f.value);
        val.appendChild(ro);
      }

      el.appendChild(val);
      return el;
    }

    function sendField(matId, name, value) {
      vscode.postMessage({ type: 'editField', material_id: editingEditId || matId, field: name, value });
    }

    function updateFieldValue(name, value) {
      const sel = '.field[data-name="' + cssEscape(name) + '"]';
      const el = editorRoot.querySelector(sel);
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
        const sw = el.querySelector('.color-swatch');
        if (sw) updateSwatch(sw, value);
      } else if (kind === 'string' || kind === 'id') {
        const inp = el.querySelector('input[type="text"]');
        if (inp && document.activeElement !== inp) inp.value = String(value ?? '');
      } else {
        const selEl = el.querySelector('select');
        if (selEl) selEl.value = String(value ?? '');
      }
    }

    // ── Shared helpers ──────────────────────────────────────────

    function fieldEditable(f) {
      if (f.enum_values && f.enum_values.length > 0) return true;
      return f.kind === 'bool' || f.kind === 'string' || f.kind === 'id'
        || isNumeric(f.kind) || isVec(f.kind);
    }
    function isNumeric(k) {
      return ['int8_t','int16_t','int32_t','int64_t',
              'uint8_t','uint16_t','uint32_t','uint64_t',
              'float','double'].includes(k);
    }
    function isFloat(k) { return k === 'float' || k === 'double' || isVec(k); }
    function isVec(k) { return k === 'vec2' || k === 'vec3' || k === 'vec4'; }
    function vecLen(k) { return k === 'vec2' ? 2 : k === 'vec3' ? 3 : 4; }

    function isColorHint(f) {
      if (f.hints && f.hints.includes('color')) return true;
      const n = f.name.toLowerCase();
      return n.includes('color') || n.includes('colour') || n === 'tint'
          || n === 'albedo' || n === 'emissive';
    }

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
    function fmtNum(n, kind) {
      if (n === null || n === undefined || !Number.isFinite(Number(n))) return '0';
      const num = Number(n);
      if (!isFloat(kind)) return String(Math.trunc(num));
      if (num === 0) return '0';
      return parseFloat(num.toPrecision(6)).toString();
    }
    function formatRo(v) {
      if (v === null || v === undefined) return '';
      if (Array.isArray(v)) return '[' + v.map(formatRo).join(', ') + ']';
      if (typeof v === 'number') {
        if (!Number.isFinite(v)) return String(v);
        if (Number.isInteger(v)) return String(v);
        return parseFloat(v.toPrecision(6)).toString();
      }
      return String(v);
    }

    function updateSwatch(swatch, arr) {
      const r = Math.round(clamp01(arr[0]) * 255);
      const g = Math.round(clamp01(arr[1]) * 255);
      const b = Math.round(clamp01(arr[2]) * 255);
      swatch.style.background = 'rgb(' + r + ',' + g + ',' + b + ')';
    }
    function clamp01(v) { return Math.max(0, Math.min(1, v)); }
    function rgbToHex(arr) {
      const r = Math.round(clamp01(arr[0]) * 255);
      const g = Math.round(clamp01(arr[1]) * 255);
      const b = Math.round(clamp01(arr[2]) * 255);
      return '#' + [r,g,b].map(c => c.toString(16).padStart(2,'0')).join('');
    }
    function hexToRgb(hex) {
      const m = hex.match(/^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i);
      if (!m) return [0,0,0];
      return [parseInt(m[1],16)/255, parseInt(m[2],16)/255, parseInt(m[3],16)/255];
    }

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

    function cssEscape(s) {
      return String(s).replace(/[^a-zA-Z0-9_-]/g, c => '\\\\' + c);
    }

    vscode.postMessage({ type: 'ready' });
  </script>
</body>
</html>`;
}
