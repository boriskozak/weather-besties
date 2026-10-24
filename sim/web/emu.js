/* Loads weather-besties.wasm and gives back a small API. Works in browsers and Node. */
(function (root) {
  "use strict";

  function wasiShim(getMemory) {
    var log = "";
    return {
      fd_write: function (fd, iovs, iovsLen, nwritten) {
        var mem = new DataView(getMemory().buffer);
        var bytes = new Uint8Array(getMemory().buffer);
        var total = 0;
        for (var i = 0; i < iovsLen; i++) {
          var ptr = mem.getUint32(iovs + i * 8, true);
          var len = mem.getUint32(iovs + i * 8 + 4, true);
          for (var k = 0; k < len; k++) log += String.fromCharCode(bytes[ptr + k]);
          total += len;
        }
        var nl;
        while ((nl = log.indexOf("\n")) >= 0) {
          if (typeof console !== "undefined") console.log("[board]", log.slice(0, nl));
          log = log.slice(nl + 1);
        }
        mem.setUint32(nwritten, total, true);
        return 0;
      },
      fd_close: function () { return 0; },
      fd_seek: function () { return 70; },
      fd_fdstat_get: function (fd, out) {
        var mem = new DataView(getMemory().buffer);
        mem.setUint8(out, 2); /* character device */
        return 0;
      },
    };
  }

  function wrap(ex, getMem) {
    if (ex._initialize) ex._initialize();
    ex.emu_init();
    var W = 800, H = 480;
    function cstr(ptr) {
      var b = new Uint8Array(getMem().buffer, ptr);
      var s = [];
      for (var i = 0; b[i]; i++) s.push(b[i]);
      return new TextDecoder().decode(new Uint8Array(s));
    }
    return {
      width: W,
      height: H,
      tick: function (ms) { return ex.emu_tick(ms >>> 0) !== 0; },
      pointer: function (x, y, down) { ex.emu_pointer(x | 0, y | 0, down ? 1 : 0); },
      load: function (i) { ex.emu_load(i); },
      screen: function (i) { ex.emu_screen(i); },
      bikeBus: function (on) { ex.emu_set_bike_bus(on ? 1 : 0); },
      /* Live weather: the forecast address the board would use, then hand back the reply. */
      forecastUrl: function (lat, lon) { return cstr(ex.emu_forecast_url(lat, lon)); },
      loadJson: function (text, nowUtcSeconds) {
        var bytes = new TextEncoder().encode(text);
        var ptr = ex.emu_alloc(bytes.length);
        new Uint8Array(getMem().buffer, ptr, bytes.length + 1).set(bytes);
        return ex.emu_load_json(ptr, bytes.length, nowUtcSeconds) !== 0;
      },
      scenarios: function () {
        var out = [];
        for (var i = 0; i < ex.emu_count(); i++) out.push(cstr(ex.emu_name(i)));
        return out;
      },
      /* Copies the RGB565 frame into an RGBA byte array (e.g. ImageData.data). */
      frame: function (rgba) {
        var px = new Uint16Array(getMem().buffer, ex.emu_fb(), W * H);
        for (var i = 0, j = 0; i < px.length; i++, j += 4) {
          var c = px[i];
          var r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
          rgba[j] = (r << 3) | (r >> 2);
          rgba[j + 1] = (g << 2) | (g >> 4);
          rgba[j + 2] = (b << 3) | (b >> 2);
          rgba[j + 3] = 255;
        }
      },
    };
  }

  /* WebAssembly build. */
  function create(wasmBytes) {
    var memory = null;
    var getMem = function () { return memory; };
    var imports = { wasi_snapshot_preview1: wasiShim(getMem) };
    return WebAssembly.instantiate(wasmBytes, imports).then(function (res) {
      memory = res.instance.exports.memory;
      return wrap(res.instance.exports, getMem);
    });
  }

  /* Plain-JavaScript build (weather-besties.asm.js), for places that block WebAssembly. */
  function createFromAsm(asmFunc) {
    var ex = null;
    var getMem = function () { return ex.memory; };
    ex = asmFunc({ wasi_snapshot_preview1: wasiShim(getMem) });
    return wrap(ex, getMem);
  }

  var api = { create: create, createFromAsm: createFromAsm };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  root.WeatherBestiesEmu = api;
})(typeof globalThis !== "undefined" ? globalThis : this);
