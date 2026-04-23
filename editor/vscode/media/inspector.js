// Property inspector webview script.
//
// Parses "key=value" records coming from the extension, renders a
// drag-scrub Vector3 for the camera position, and sends `setProperty`
// records back on edit.

(function () {
  const vscode = acquireVsCodeApi();

  const entityEl = document.getElementById("entity");
  const componentsEl = document.getElementById("components");
  const statusEl = document.getElementById("status");
  const refreshBtn = document.getElementById("refresh");

  refreshBtn.addEventListener("click", () => {
    vscode.postMessage({ type: "postMessageIn", payload: "requestSelection" });
  });

  // Parse space-separated key=value tokens. First token is the record verb.
  function parseRecord(line) {
    const tokens = line.trim().split(/\s+/);
    if (tokens.length === 0) return null;
    const out = { verb: tokens[0] };
    for (let i = 1; i < tokens.length; ++i) {
      const eq = tokens[i].indexOf("=");
      if (eq > 0) {
        out[tokens[i].slice(0, eq)] = tokens[i].slice(eq + 1);
      }
    }
    return out;
  }

  // --- Drag-scrub number input ----------------------------------------
  //
  // Mouse-drag horizontally on the input = change value. Shift = fine,
  // Ctrl = coarse. Focus + type lets you enter values manually. `onCommit`
  // fires once per "gesture" (mouseup or blur), so the engine gets the
  // final value for undo grouping (engine-side undo stack is Phase 4+).
  function makeScrub({ axisClass, initial, step, onPreview, onCommit }) {
    const el = document.createElement("input");
    el.type = "text";
    el.className = "scrub " + axisClass;
    el.value = formatValue(initial);

    let dragging = false;
    let startX = 0;
    let startVal = 0;
    let preview = initial;

    function formatValue(v) {
      return (Math.fround(v)).toFixed(3);
    }

    el.addEventListener("mousedown", (e) => {
      if (document.activeElement === el) return;  // let the user type
      dragging = true;
      startX = e.clientX;
      startVal = parseFloat(el.value) || 0;
      preview = startVal;
      e.preventDefault();
    });
    window.addEventListener("mousemove", (e) => {
      if (!dragging) return;
      const dx = e.clientX - startX;
      const mult = e.shiftKey ? 0.1 : e.ctrlKey ? 10.0 : 1.0;
      preview = startVal + dx * step * mult;
      el.value = formatValue(preview);
      onPreview(preview);
    });
    window.addEventListener("mouseup", () => {
      if (!dragging) return;
      dragging = false;
      onCommit(preview);
    });
    el.addEventListener("keydown", (e) => {
      if (e.key === "Enter") el.blur();
    });
    el.addEventListener("blur", () => {
      const v = parseFloat(el.value);
      if (!isNaN(v)) onCommit(v);
    });

    return el;
  }

  // --- Render ---------------------------------------------------------

  let selection = null;

  function render() {
    componentsEl.innerHTML = "";
    if (!selection) {
      entityEl.textContent = "(no selection)";
      return;
    }
    entityEl.textContent = selection.entity ?? "(unknown)";

    const transform = document.createElement("div");
    transform.className = "component";
    transform.innerHTML = "<h3>Transform</h3>";

    const field = document.createElement("div");
    field.className = "field";
    const label = document.createElement("span");
    label.className = "label";
    label.textContent = "Position";
    const vec = document.createElement("div");
    vec.className = "vec";
    for (const axis of ["x", "y", "z"]) {
      const path = `camera.position.${axis}`;
      const el = makeScrub({
        axisClass: `axis-${axis}`,
        initial: selection[`pos_${axis}`] ?? 0,
        step: 0.01,
        onPreview: (value) => {
          vscode.postMessage({
            type: "postMessageIn",
            payload: `setProperty path=${path} value=${Math.fround(value)}`,
          });
        },
        onCommit: (value) => {
          vscode.postMessage({
            type: "postMessageIn",
            payload: `setProperty path=${path} value=${Math.fround(value)}`,
          });
        },
      });
      vec.appendChild(el);
    }
    field.appendChild(label);
    field.appendChild(vec);
    transform.appendChild(field);
    componentsEl.appendChild(transform);
  }

  // Phase 5: schema cache, keyed by type name. Populated on `schema`
  // records. Each field: { name, typeId, offset }. A later pass will map
  // typeId → UI widget (float, vec3, bool, enum); today we stash the data
  // so the mechanism is provable end-to-end.
  const schemas = new Map();

  window.addEventListener("message", (event) => {
    const msg = event.data;
    if (msg.type === "status") {
      statusEl.textContent = msg.state;
      statusEl.className = msg.state;
      return;
    }
    if (msg.type === "engineMessage") {
      // Re-parse schema records at the raw-string level so we can
      // preserve multiple `f=` tokens (the key-value collapse in
      // parseRecord would otherwise keep only the last one).
      if (msg.payload.startsWith("schema ")) {
        const tokens = msg.payload.trim().split(/\s+/);
        let type = null;
        const fields = [];
        for (let i = 1; i < tokens.length; ++i) {
          const t = tokens[i];
          if (t.startsWith("type=")) type = t.slice(5);
          else if (t.startsWith("f=")) {
            const [name, typeId, offset] = t.slice(2).split(":");
            fields.push({ name, typeId: parseInt(typeId, 10), offset: parseInt(offset, 10) });
          }
        }
        if (type) schemas.set(type, fields);
        return;
      }

      const rec = parseRecord(msg.payload);
      if (!rec) return;
      if (rec.verb === "selection") {
        selection = {
          entity: rec.entity,
          pos_x: parseFloat(rec.pos_x),
          pos_y: parseFloat(rec.pos_y),
          pos_z: parseFloat(rec.pos_z),
        };
        render();
      } else if (rec.verb === "propertyChanged") {
        // Engine echoed a value; keep the UI in sync by updating the
        // relevant scrub input without re-rendering the whole panel.
        // Phase 4 is simple enough that re-rendering works; revisit in
        // Phase 5 once there are many more fields.
        const path = rec.path;
        const v = parseFloat(rec.value);
        if (!selection) return;
        if (path === "camera.position.x") selection.pos_x = v;
        else if (path === "camera.position.y") selection.pos_y = v;
        else if (path === "camera.position.z") selection.pos_z = v;
        // Leave the DOM alone during an active drag to avoid visual
        // snap-back (see editor/README.md's "engine state authoritative"
        // note for the rationale).
      }
    }
  });
})();
