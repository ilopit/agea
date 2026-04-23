// Webview frame receiver + WebGL fullscreen-quad texture uploader.
//
// Uses WebGL rather than 2D ctx.putImageData: at 4K60, putImageData costs
// enough JS to starve every other callback in the webview. A single RGBA
// texture and a fullscreen triangle is comparatively free.

(function () {
  const vscode = acquireVsCodeApi();
  const canvas = document.getElementById("viewport");
  const statusEl = document.getElementById("status");

  const gl = canvas.getContext("webgl2") || canvas.getContext("webgl");
  if (!gl) {
    statusEl.textContent = "webgl unavailable";
    statusEl.className = "disconnected";
    return;
  }

  // Fullscreen triangle — cheaper than a quad (no shared edge, half the
  // fragment invocations near-clipped on typical GPUs).
  const vs = `
    attribute vec2 a_pos;
    varying vec2 v_uv;
    void main() {
      v_uv = a_pos * 0.5 + 0.5;
      // Flip Y so the top of the image maps to the top of the canvas.
      v_uv.y = 1.0 - v_uv.y;
      gl_Position = vec4(a_pos, 0.0, 1.0);
    }`;
  const fs = `
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_tex;
    void main() {
      gl_FragColor = texture2D(u_tex, v_uv);
    }`;

  function compile(type, src) {
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
      console.error(gl.getShaderInfoLog(s));
      throw new Error("shader compile failed");
    }
    return s;
  }

  const prog = gl.createProgram();
  gl.attachShader(prog, compile(gl.VERTEX_SHADER, vs));
  gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, fs));
  gl.linkProgram(prog);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
    console.error(gl.getProgramInfoLog(prog));
    throw new Error("program link failed");
  }
  gl.useProgram(prog);

  const posLoc = gl.getAttribLocation(prog, "a_pos");
  const texLoc = gl.getUniformLocation(prog, "u_tex");

  const buf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(
    gl.ARRAY_BUFFER,
    new Float32Array([-1, -1, 3, -1, -1, 3]),
    gl.STATIC_DRAW,
  );
  gl.enableVertexAttribArray(posLoc);
  gl.vertexAttribPointer(posLoc, 2, gl.FLOAT, false, 0, 0);

  const tex = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.uniform1i(texLoc, 0);
  gl.activeTexture(gl.TEXTURE0);

  let lastWidth = 0;
  let lastHeight = 0;

  function resizeCanvas() {
    const w = canvas.clientWidth;
    const h = canvas.clientHeight;
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
      gl.viewport(0, 0, w, h);
    }
  }
  window.addEventListener("resize", resizeCanvas);
  resizeCanvas();

  function drawFrame(width, height, pixelFormat, pixelsBuffer) {
    // node Buffer arrives as a Uint8Array-compatible view after structured
    // clone. Normalize to Uint8Array for gl.texImage2D.
    const pixels = new Uint8Array(
      pixelsBuffer.buffer,
      pixelsBuffer.byteOffset,
      pixelsBuffer.byteLength,
    );

    // PixelFormat: 0 = RGBA8, 1 = BGRA8. WebGL accepts GL_RGBA only — if the
    // publisher ever ships BGRA we swizzle in a shader path. For Phase 1 the
    // publisher always produces RGBA.
    if (width !== lastWidth || height !== lastHeight) {
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGBA,
        width,
        height,
        0,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        pixels,
      );
      lastWidth = width;
      lastHeight = height;
    } else {
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.texSubImage2D(
        gl.TEXTURE_2D,
        0,
        0,
        0,
        width,
        height,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        pixels,
      );
    }

    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }

  window.addEventListener("message", (event) => {
    const msg = event.data;
    if (msg.type === "frame") {
      drawFrame(msg.width, msg.height, msg.pixelFormat, msg.pixels);
    } else if (msg.type === "status") {
      statusEl.textContent = msg.state;
      statusEl.className = msg.state;
    }
  });

  // --------------------------------------------------------------------
  // Input forwarding: canvas events → postMessage → extension.
  // --------------------------------------------------------------------

  // Canvas-pixel coordinates (not CSS) so the engine can map to its own
  // viewport without knowing the webview devicePixelRatio.
  function toCanvasCoords(e) {
    const rect = canvas.getBoundingClientRect();
    const sx = canvas.width / rect.width;
    const sy = canvas.height / rect.height;
    return {
      x: Math.round((e.clientX - rect.left) * sx),
      y: Math.round((e.clientY - rect.top) * sy),
    };
  }

  let lastPos = null;
  canvas.addEventListener("pointermove", (e) => {
    const p = toCanvasCoords(e);
    const dx = lastPos ? p.x - lastPos.x : 0;
    const dy = lastPos ? p.y - lastPos.y : 0;
    lastPos = p;
    vscode.postMessage({
      type: "input.mouseMove",
      x: p.x,
      y: p.y,
      dx: dx,
      dy: dy,
    });
  });

  canvas.addEventListener("pointerdown", (e) => {
    canvas.setPointerCapture(e.pointerId);
    vscode.postMessage({
      type: "input.mouseButton",
      button: e.button,
      down: true,
    });
  });
  canvas.addEventListener("pointerup", (e) => {
    canvas.releasePointerCapture(e.pointerId);
    vscode.postMessage({
      type: "input.mouseButton",
      button: e.button,
      down: false,
    });
  });

  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    vscode.postMessage({
      type: "input.mouseWheel",
      deltaY: e.deltaY,
      deltaX: e.deltaX,
    });
  }, { passive: false });

  function keyEventToMods(e) {
    let m = 0;
    if (e.shiftKey) m |= 1;
    if (e.ctrlKey) m |= 2;
    if (e.altKey) m |= 4;
    if (e.metaKey) m |= 8;
    return m;
  }

  window.addEventListener("keydown", (e) => {
    vscode.postMessage({
      type: "input.key",
      keyCode: e.keyCode,
      down: true,
      mods: keyEventToMods(e),
    });
  });
  window.addEventListener("keyup", (e) => {
    vscode.postMessage({
      type: "input.key",
      keyCode: e.keyCode,
      down: false,
      mods: keyEventToMods(e),
    });
  });

  // Inform the extension we're ready to receive.
  vscode.postMessage({ type: "ready" });
})();
