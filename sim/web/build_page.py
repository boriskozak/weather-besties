#!/usr/bin/env python3
"""Builds the two simulator pages from the current emulator build.

  site/index.html                       the Weather Besties page (simulator, how to update,
                                        run it on a PC, screens). Loads emu/ and img/ next to it.
  site/Weather-Besties-Simulator.html   one standalone file: double-click it on a PC. The
                                        emulator is embedded, works offline, and adds a
                                        Live weather button (needs internet).

Run after `make web/weather-besties.wasm` and `make shots`:  python3 web/build_page.py
"""
import base64
import hashlib
import html
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.dirname(HERE)
ROOT = os.path.dirname(SIM)
SITE = os.path.join(HERE, "site")
VERSION = "1.4.0"
UPDATE_BIN = f"weather-besties-v{VERSION}-update.bin"
# Upper Montclair, NJ (what the board's town lookup returned)
LAT, LON = 40.8462, -74.2013


def read(p, mode="r"):
    with open(p, mode) as f:
        return f.read()


EMU_JS = read(os.path.join(HERE, "emu.js"))

STYLE = r"""
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Fredoka:wght@500;600&family=Figtree:wght@400;500;600&family=JetBrains+Mono:wght@400;500&display=swap">
<style>
/* Layout: one calm column. The live simulator sits in a bezel with the two printed cats on top, controls under it, guides below. */
:root {
  --bg: #F2F7FC;
  --surface: #FFFFFF;
  --ink: #1B2542;
  --muted: #4E5B79;
  --accent: #D63D73;
  --accent-ink: #FFFFFF;
  --sky: #D7EAF8;
  --line: #DCE5F0;
  --bezel: #1B2542;
  --code-bg: #EEF3FA;
  --display: "Fredoka", "Trebuchet MS", system-ui, sans-serif;
  --body: "Figtree", system-ui, -apple-system, "Segoe UI", sans-serif;
  --mono: "JetBrains Mono", ui-monospace, "SFMono-Regular", Menlo, Consolas, monospace;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #0E1530; --surface: #172148; --ink: #EEF2FF; --muted: #A9B5D5; --accent: #FF8DB8; --accent-ink: #1B2542;
    --sky: #22315E; --line: #2A3866; --bezel: #070B1C; --code-bg: #111A3B; color-scheme: dark;
  }
}
:root[data-theme="dark"] {
  --bg: #0E1530; --surface: #172148; --ink: #EEF2FF; --muted: #A9B5D5; --accent: #FF8DB8; --accent-ink: #1B2542;
  --sky: #22315E; --line: #2A3866; --bezel: #070B1C; --code-bg: #111A3B; color-scheme: dark;
}

body { margin: 0; background: var(--bg); color: var(--ink); font-family: var(--body); font-size: 1rem; line-height: 1.55; }
.wrap {
  max-width: 920px;
  margin: 0 auto;
  padding-block: 28px 64px;
  padding-left: max(16px, env(safe-area-inset-left, 0px));
  padding-right: max(16px, env(safe-area-inset-right, 0px));
  display: grid;
  gap: 56px;
}
h1, h2, h3 { font-family: var(--display); font-weight: 600; line-height: 1.15; margin: 0; text-wrap: balance; }
h1 { font-size: clamp(2.2rem, 6vw, 3.2rem); }
h2 { font-size: 1.7rem; }
h3 { font-size: 1.15rem; font-weight: 500; }
p { margin: 0; }
a { color: var(--accent); text-underline-offset: 3px; }
a:focus-visible, button:focus-visible, input:focus-visible + .switch { outline: 3px solid var(--accent); outline-offset: 3px; }
.lede { color: var(--muted); font-size: 1.1rem; max-width: 62ch; }
.eyebrow { font-family: var(--mono); font-size: 0.78rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); }

.hero { display: grid; gap: 22px; }
.hero-text { display: grid; gap: 12px; }
.device { position: relative; max-width: 780px; width: 100%; margin: 52px auto 0; }
.cats { position: absolute; top: -58px; left: 0; right: 0; display: flex; justify-content: space-between; padding: 0 9%; pointer-events: none; }
.cats img { width: clamp(64px, 14vw, 110px); height: auto; }
.bezel { background: var(--bezel); border-radius: 26px; padding: clamp(10px, 2.2vw, 18px); box-shadow: 0 18px 40px rgba(27, 37, 66, 0.22); position: relative; z-index: 1; }
.glass { position: relative; border-radius: 10px; overflow: hidden; aspect-ratio: 800 / 480; max-width: 100%; background: #0B1022; }
.glass img, .glass canvas { position: absolute; inset: 0; width: 100%; height: 100%; display: block; }
.glass canvas { touch-action: pan-y; cursor: pointer; }
.glass .status { position: absolute; left: 12px; bottom: 12px; right: 12px; width: fit-content; font: 600 0.8rem var(--body); background: rgba(11, 16, 34, 0.82); color: #F4F6FF; border-radius: 999px; padding: 6px 12px; }

.controls { display: grid; gap: 14px; max-width: 780px; width: 100%; margin: 0 auto; }
.ctl-row { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
.ctl-label { font-family: var(--mono); font-size: 0.75rem; letter-spacing: 0.06em; text-transform: uppercase; color: var(--muted); width: 100%; }
.chip-btn { font: 500 0.92rem var(--body); border: 1px solid var(--line); background: var(--surface); color: var(--ink); border-radius: 999px; padding: 7px 14px; cursor: pointer; }
.chip-btn[aria-pressed="true"] { background: var(--accent); border-color: var(--accent); color: var(--accent-ink); }
.chip-btn.live { border-color: var(--accent); color: var(--accent); font-weight: 600; }
.chip-btn.live[aria-pressed="true"] { color: var(--accent-ink); }
.chip-btn:disabled { opacity: 0.5; cursor: default; }
.toggle { display: inline-flex; align-items: center; gap: 10px; font-weight: 500; cursor: pointer; }
.toggle input { position: absolute; opacity: 0; width: 1px; height: 1px; }
.switch { width: 42px; height: 24px; border-radius: 999px; background: var(--line); position: relative; transition: background .2s; flex: none; }
.switch::after { content: ""; position: absolute; top: 3px; left: 3px; width: 18px; height: 18px; border-radius: 50%; background: var(--surface); box-shadow: 0 1px 3px rgba(0,0,0,.25); transition: transform .2s; }
.toggle input:checked + .switch { background: var(--accent); }
.toggle input:checked + .switch::after { transform: translateX(18px); }
.hint { color: var(--muted); font-size: 0.92rem; }
.hint b { color: var(--ink); font-weight: 600; }

.section { display: grid; gap: 20px; }
.steps { list-style: none; margin: 0; padding: 0; display: grid; gap: 14px; counter-reset: s; }
.steps li { counter-increment: s; display: grid; grid-template-columns: 2.2rem 1fr; gap: 14px; align-items: start; }
.steps li::before { content: counter(s); font-family: var(--display); font-weight: 600; font-size: 1.1rem; width: 2.2rem; height: 2.2rem; border-radius: 50%; display: grid; place-items: center; background: var(--accent); color: var(--accent-ink); }
.steps li > div { min-width: 0; padding-top: 4px; display: grid; gap: 4px; }
.steps b { font-weight: 600; }
.steps span { color: var(--muted); }
.note { background: var(--surface); border: 1px solid var(--line); border-radius: 16px; padding: 18px 20px; display: grid; grid-template-columns: minmax(0, 1fr); gap: 10px; min-width: 0; align-content: start; }
.note p, .note li { color: var(--muted); }
.note ul { margin: 0; padding-left: 1.2rem; display: grid; gap: 6px; }
code, .mono { font-family: var(--mono); font-size: 0.88em; }
code { background: var(--code-bg); border-radius: 6px; padding: 1px 6px; overflow-wrap: anywhere; }
.cmd { display: flex; gap: 10px; align-items: stretch; flex-wrap: wrap; min-width: 0; }
.cmd pre { flex: 1 1 320px; min-width: 0; margin: 0; overflow-x: auto; background: var(--code-bg); border-radius: 12px; padding: 14px 16px; font-family: var(--mono); font-size: 0.85rem; line-height: 1.6; }
.btn { font: 600 0.95rem var(--body); border: 0; border-radius: 999px; padding: 10px 18px; background: var(--sky); color: var(--ink); cursor: pointer; align-self: flex-start; }
.two { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 16px; }
.hash { word-break: break-all; color: var(--muted); font-size: 0.8rem; }
.shot { width: 100%; height: auto; border-radius: 14px; border: 1px solid var(--line); aspect-ratio: 800 / 480; max-width: 100%; display: block; }
.gallery { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 22px 18px; }
.gallery figure { margin: 0; display: grid; gap: 8px; }
.gallery figcaption { font-size: 0.92rem; color: var(--muted); }
.gallery figcaption b { color: var(--ink); font-weight: 600; }
@media (max-width: 620px) { .gallery { grid-template-columns: 1fr; } }
footer { color: var(--muted); font-size: 0.88rem; }
@media (prefers-reduced-motion: no-preference) {
  .cats img { transition: transform .3s ease; }
  .device:hover .cats img:first-child { transform: translateY(-4px) rotate(-4deg); }
  .device:hover .cats img:last-child { transform: translateY(-4px) rotate(4deg); }
}
</style>
"""


def device(cat_black, cat_pink, poster, live):
    live_btn = ('<button class="chip-btn live" type="button" id="live" disabled>Live weather</button>'
                if live else "")
    return f"""
    <div class="device">
      <div class="cats" aria-hidden="true">
        <img src="{cat_black}" alt="">
        <img src="{cat_pink}" alt="">
      </div>
      <div class="bezel">
        <div class="glass">
          {poster}
          <canvas id="screen" width="800" height="480" role="img" aria-label="Live Weather Besties screen. Drag sideways to change pages."></canvas>
          <span class="status" id="status">Starting the simulator...</span>
        </div>
      </div>
    </div>
    <div class="controls">
      <div class="ctl-row" id="days"><span class="ctl-label">Weather</span>{live_btn}</div>
      <div class="ctl-row" id="screens">
        <span class="ctl-label">Jump to</span>
        <button class="chip-btn" type="button" data-screen="0" disabled>Today</button>
        <button class="chip-btn" type="button" data-screen="1" disabled>Next days</button>
        <button class="chip-btn" type="button" data-screen="2" disabled>Settings</button>
        <button class="chip-btn" type="button" data-screen="3" disabled>Setup</button>
        <button class="chip-btn" type="button" data-screen="4" disabled>Start-up</button>
        <button class="chip-btn" type="button" data-screen="5" disabled>WiFi problem</button>
      </div>
      <div class="ctl-row">
        <label class="toggle" for="bikebus"><input type="checkbox" id="bikebus" checked disabled><span class="switch"></span>Bike bus on Fridays</label>
      </div>
      <p class="hint">Use the mouse like a finger. <b>Swipe sideways</b> (a quick drag) to flip to the next days. <b>Click the cat</b> and it hops. <b>Hold the greeting</b> for 1.5 seconds for Settings. <b>Click &ldquo;Updated&rdquo;</b> at the bottom right for the speed test (here it measures your computer, not the board).</p>
    </div>"""


# The page's own script. WASM_SOURCE is either a fetch of emu/ files or embedded base64.
APP_JS = r"""
(function () {
  var canvas = document.getElementById("screen");
  var ctx = canvas.getContext("2d");
  var image = ctx.createImageData(800, 480);
  var statusEl = document.getElementById("status");
  var poster = document.getElementById("poster");
  var liveBtn = document.getElementById("live");
  var emu = null;
  var statusTimer = 0;

  function setStatus(text, ms) {
    clearTimeout(statusTimer);
    statusEl.textContent = text || "";
    statusEl.hidden = !text;
    if (text && ms) statusTimer = setTimeout(function () { setStatus(""); }, ms);
  }

  function press(group, btn) {
    document.querySelectorAll("#days .chip-btn").forEach(function (o) { o.setAttribute("aria-pressed", o === btn ? "true" : "false"); });
  }

  function start(e) {
    emu = e;
    var days = document.getElementById("days");
    emu.scenarios().forEach(function (name, i) {
      var b = document.createElement("button");
      b.type = "button";
      b.className = "chip-btn";
      b.textContent = name;
      b.setAttribute("aria-pressed", i === 0 ? "true" : "false");
      b.addEventListener("click", function () { emu.load(i); press(days, b); setStatus("Sample day", 1500); });
      days.appendChild(b);
    });
    if (liveBtn) {
      liveBtn.disabled = false;
      liveBtn.addEventListener("click", function () {
        setStatus("Getting today's weather for Upper Montclair...");
        fetch(emu.forecastUrl(LAT, LON))
          .then(function (r) { if (!r.ok) throw new Error("HTTP " + r.status); return r.text(); })
          .then(function (txt) {
            if (!emu.loadJson(txt, Date.now() / 1000)) throw new Error("bad data");
            press(days, liveBtn);
            setStatus("Live weather, right now", 2500);
          })
          .catch(function () { setStatus("Couldn't reach the weather service. Check the internet connection, or pick a sample day.", 6000); });
      });
    }
    document.querySelectorAll("#screens .chip-btn").forEach(function (b) {
      b.disabled = false;
      b.addEventListener("click", function () { emu.screen(parseInt(b.getAttribute("data-screen"), 10)); });
    });
    var bike = document.getElementById("bikebus");
    bike.disabled = false;
    bike.addEventListener("change", function () { emu.bikeBus(bike.checked); });

    function point(ev, down) {
      var r = canvas.getBoundingClientRect();
      emu.pointer((ev.clientX - r.left) * 800 / r.width, (ev.clientY - r.top) * 480 / r.height, down);
    }
    var pressed = false;
    canvas.addEventListener("pointerdown", function (ev) {
      pressed = true;
      try { canvas.setPointerCapture(ev.pointerId); } catch (err) {}
      point(ev, true);
    });
    canvas.addEventListener("pointermove", function (ev) { if (pressed) point(ev, true); });
    ["pointerup", "pointercancel"].forEach(function (t) {
      canvas.addEventListener(t, function (ev) { pressed = false; point(ev, false); });
    });
    canvas.addEventListener("contextmenu", function (ev) { ev.preventDefault(); });

    var t0 = performance.now();
    var first = true;
    function frame(now) {
      if (emu.tick(now - t0) || first) {
        emu.frame(image.data);
        ctx.putImageData(image, 0, 0);
        if (first) { first = false; if (poster) poster.hidden = true; setStatus(""); }
      }
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
  }

  function fail() {
    setStatus("The simulator couldn't start in this browser. Try Chrome or Edge.");
  }

  WASM_SOURCE
})();
"""

FETCH_SOURCE = r"""
  function loadPlainJs() {
    var s = document.createElement("script");
    s.src = "emu/weather-besties.asm.js";
    s.onload = function () {
      try { start(WeatherBestiesEmu.createFromAsm(window.WeatherBestiesAsm)); } catch (err) { fail(); }
    };
    s.onerror = fail;
    document.head.appendChild(s);
  }
  if (typeof WebAssembly === "object") {
    fetch("emu/weather-besties.wasm")
      .then(function (r) { if (!r.ok) throw new Error("download failed"); return r.arrayBuffer(); })
      .then(function (bytes) { return WeatherBestiesEmu.create(bytes); })
      .then(start)
      .catch(function () { loadPlainJs(); });
  } else {
    loadPlainJs();
  }
"""

EMBED_SOURCE = r"""
  try {
    var b64 = document.getElementById("wasm").textContent.trim();
    var bin = atob(b64);
    var bytes = new Uint8Array(bin.length);
    for (var i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    WeatherBestiesEmu.create(bytes).then(start).catch(fail);
  } catch (err) { fail(); }
"""


def app_js(source):
    return APP_JS.replace("WASM_SOURCE", source).replace("LAT, LON", f"{LAT}, {LON}")


def data_uri(path, mime):
    return f"data:{mime};base64," + base64.b64encode(read(path, "rb")).decode()


def main():
    shutil.rmtree(SITE, ignore_errors=True)
    os.makedirs(os.path.join(SITE, "emu"))
    os.makedirs(os.path.join(SITE, "img"))
    wasm = os.path.join(HERE, "weather-besties.wasm")
    asmjs = os.path.join(HERE, "weather-besties.asm.js")
    shutil.copy(wasm, os.path.join(SITE, "emu"))
    shutil.copy(asmjs, os.path.join(SITE, "emu"))
    pngs = os.path.join(ROOT, "tools", "build", "png")
    for n in ("img_cat_black.png", "img_cat_pink.png"):
        shutil.copy(os.path.join(pngs, n), os.path.join(SITE, "img", n))
    shots = ["03_today_oct_morning", "11_cat_hanukkah_night3", "12_cat_christmas_snow",
             "14_cat_christmas_and_hanukkah", "05_rainy_afternoon", "07_summer_hot", "13_cat_plain", "04_next_days",
             "02_setup", "15_storm", "08_evening_plans_tomorrow"]
    for n in shots:
        shutil.copy(os.path.join(SIM, "out", n + ".png"), os.path.join(SITE, "img", n + ".png"))

    upd = os.path.join(ROOT, "build", "weather_besties.bin")
    sha = hashlib.sha256(read(upd, "rb")).hexdigest() if os.path.exists(upd) else ""

    poster = ('<img id="poster" src="img/03_today_oct_morning.png" alt="Weather Besties screen: 48 degrees, a black cat '
              'in a witch hat next to a speech bubble that says chilly morning, bring a jacket.">')

    # ---------------------------------------------------------------- the Weather Besties page
    page = f"""<title>Weather Besties</title>
<meta name="claude:viewport-fit-sides" content="cover">
{STYLE}
<div class="wrap">

  <header class="hero">
    <div class="hero-text">
      <span class="eyebrow">Firmware {VERSION} &middot; Waveshare ESP32-S3-Touch-LCD-4.3</span>
      <h1>Weather Besties</h1>
      <p class="lede">The weather station for Cleo and Lila, running below. This is the board's own screen code compiled for the browser, so it looks and behaves like the real 800 &times; 480 touchscreen. Pick a sample day to see the cat through the year.</p>
    </div>
    {device("img/img_cat_black.png", "img/img_cat_pink.png", poster, live=False)}
  </header>

  <section class="section" id="update">
    <h2>Update the board</h2>
    <ol class="steps">
      <li><div><b>Save the update file from the chat.</b><span>It's called <code>{UPDATE_BIN}</code>.</span></div></li>
      <li><div><b>Plug the board into the computer.</b><span>Use the USB-C port labeled <b>UART</b>, with a cable that carries data. If no new port shows up later, the cable only carries power.</span></div></li>
      <li><div><b>Open Espressif's web flasher in Chrome or Edge.</b><span><a href="https://espressif.github.io/esptool-js/" target="_blank" rel="noopener">espressif.github.io/esptool-js</a>. Set Baudrate to 921600, click <b>Connect</b> and pick the board's port.</span></div></li>
      <li><div><b>Flash Address <code>0x10000</code>, choose the file, click Program.</b><span>About 20 seconds. This address only replaces the app, so the saved WiFi and town stay put.</span></div></li>
      <li><div><b>Click Disconnect, then press RESET.</b><span>While the flasher is connected it can hold the board in reset, which looks like a dark screen.</span></div></li>
    </ol>
    <div class="two">
      <div class="note">
        <h3>If it won't connect</h3>
        <ul>
          <li>Only COM1 or other devices in the list: swap to a data cable.</li>
          <li>Hold <b>BOOT</b>, tap <b>RESET</b>, let go of BOOT, and connect again.</li>
          <li>Safari and Firefox can't talk to USB devices. Use Chrome or Edge.</li>
          <li>Starting fresh: the full image <code>weather-besties-v1.1.1.bin</code> goes at <code>0x0</code>, but it wipes the saved WiFi, so setup runs again.</li>
        </ul>
      </div>
      <div class="note">
        <h3>Or from a terminal</h3>
        <div class="cmd">
          <pre id="cmd">pip install esptool
python -m esptool --chip esp32s3 write_flash 0x10000 {UPDATE_BIN}</pre>
          <button class="btn" id="copy" type="button">Copy</button>
        </div>
        <p class="mono hash">SHA-256 {sha}</p>
      </div>
    </div>
  </section>

  <section class="section" id="pc">
    <h2>Run it on your PC</h2>
    <div class="two">
      <ol class="steps">
        <li><div><b>Save <code>Weather-Besties-Simulator.html</code> from the chat.</b><span>It's one file with the simulator built in.</span></div></li>
        <li><div><b>Double-click it.</b><span>It opens in your browser and works offline. Chrome or Edge is best.</span></div></li>
        <li><div><b>Click Live weather for today's real forecast.</b><span>That one button needs internet. It asks Open-Meteo the same question the board does, for Upper Montclair.</span></div></li>
      </ol>
      <div class="note">
        <h3>Build it yourself</h3>
        <p>The source zip has everything. On Windows, use WSL (Ubuntu). <code>make shots</code> draws every screen to PNGs; <code>make web</code> rebuilds this simulator and needs <a href="https://github.com/WebAssembly/wasi-sdk" target="_blank" rel="noopener">wasi-sdk</a>.</p>
        <div class="cmd">
          <pre>sudo apt install build-essential python3-pil
cd weather-besties/sim
make shots     # PNGs in sim/out
make test      # request-size check</pre>
        </div>
      </div>
    </div>
  </section>

  <section class="section" id="screens-gallery">
    <h2>The cat through the year</h2>
    <div class="gallery">
      <figure><img class="shot" src="img/03_today_oct_morning.png" alt="October morning: the cat in a witch hat and cape with a jack-o'-lantern, autumn leaves falling." loading="lazy"><figcaption><b>October.</b> Witch hat, cape and a jack-o'-lantern, with autumn leaves tumbling down. The walk-to-school card fades once it's past.</figcaption></figure>
      <figure><img class="shot" src="img/11_cat_hanukkah_night3.png" alt="Third night of Hanukkah: the cat in a beanie and blue scarf next to a menorah with three candles and the shamash lit." loading="lazy"><figcaption><b>Hanukkah.</b> Blue scarf and a menorah, one more candle each night, lit at 4 PM.</figcaption></figure>
      <figure><img class="shot" src="img/12_cat_christmas_snow.png" alt="Snowy December morning: the cat in a Santa hat next to a present, snowflakes falling." loading="lazy"><figcaption><b>Christmas week, snowing.</b> Santa hat and a present, with snow drifting down behind everything.</figcaption></figure>
      <figure><img class="shot" src="img/14_cat_christmas_and_hanukkah.png" alt="Christmas evening 2027: the cat in a Santa hat with the menorah, second night." loading="lazy"><figcaption><b>Both at once.</b> Christmas 2027 is the second night of Hanukkah, so Santa hat plus the menorah.</figcaption></figure>
      <figure><img class="shot" src="img/05_rainy_afternoon.png" alt="Rainy afternoon: the cat in a yellow rain hat." loading="lazy"><figcaption><b>Rainy day.</b> Yellow rain hat whenever the bubble says umbrella.</figcaption></figure>
      <figure><img class="shot" src="img/07_summer_hot.png" alt="Hot summer afternoon: the cat in heart sunglasses." loading="lazy"><figcaption><b>Strong sun.</b> Heart sunglasses.</figcaption></figure>
      <figure><img class="shot" src="img/15_storm.png" alt="Thunderstorm: the cat in a rain hat, rain falling and a lightning bolt flickering." loading="lazy"><figcaption><b>Thunderstorm.</b> Rain falls behind everything and lightning flickers every few seconds.</figcaption></figure>
      <figure><img class="shot" src="img/08_evening_plans_tomorrow.png" alt="Clear October evening: stars twinkling around the moon, the cat in its witch costume." loading="lazy"><figcaption><b>Clear night.</b> Stars twinkle around the moon. After 7 PM the cat talks about tomorrow.</figcaption></figure>
      <figure><img class="shot" src="img/13_cat_plain.png" alt="Mild spring morning: the cat with nothing on." loading="lazy"><figcaption><b>Nice day.</b> Just the cat.</figcaption></figure>
      <figure><img class="shot" src="img/04_next_days.png" alt="Next three days page." loading="lazy"><figcaption><b>Swipe left: next three days.</b> Each one compared with the day before.</figcaption></figure>
    </div>
  </section>

  <footer>Weather data by Open-Meteo.com (CC BY 4.0). Font: Fredoka (SIL Open Font License). The simulator here uses sample days; the PC file can also show live weather.</footer>
</div>

<script>
{EMU_JS}
</script>
<script>
{app_js(FETCH_SOURCE)}
document.getElementById("copy").addEventListener("click", function () {{
  var btn = this;
  var pre = document.getElementById("cmd");
  function selectCmd() {{
    var r = document.createRange(); r.selectNodeContents(pre);
    var s = window.getSelection(); s.removeAllRanges(); s.addRange(r);
  }}
  function done(ok) {{ btn.textContent = ok ? "Copied" : "Selected"; setTimeout(function () {{ btn.textContent = "Copy"; }}, 1600); }}
  try {{
    navigator.clipboard.writeText(pre.textContent).then(function () {{ done(true); }}, function () {{ selectCmd(); done(false); }});
  }} catch (err) {{ selectCmd(); done(false); }}
}});
</script>
"""
    with open(os.path.join(SITE, "index.html"), "w") as f:
        f.write(page)

    # ---------------------------------------------------------------- standalone PC file
    cb = data_uri(os.path.join(pngs, "img_cat_black.png"), "image/png")
    cp = data_uri(os.path.join(pngs, "img_cat_pink.png"), "image/png")
    wasm_b64 = base64.b64encode(read(wasm, "rb")).decode()
    solo = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Weather Besties Simulator</title>
{STYLE}
</head>
<body>
<div class="wrap">
  <header class="hero">
    <div class="hero-text">
      <span class="eyebrow">Simulator &middot; firmware {VERSION}</span>
      <h1>Weather Besties</h1>
      <p class="lede">The board's own screen code, running on your PC. Pick a sample day, or click Live weather for today's real forecast in Upper Montclair.</p>
    </div>
    {device(cb, cp, "", live=True)}
  </header>
  <footer>Weather data by Open-Meteo.com (CC BY 4.0). Font: Fredoka (SIL Open Font License); without internet the page uses your system font, but the screen itself always looks like the board.</footer>
</div>
<script type="application/octet-stream" id="wasm">{wasm_b64}</script>
<script>
{EMU_JS}
</script>
<script>
{app_js(EMBED_SOURCE)}
</script>
</body>
</html>
"""
    with open(os.path.join(SITE, "Weather-Besties-Simulator.html"), "w") as f:
        f.write(solo)
    for n in ("index.html", "Weather-Besties-Simulator.html"):
        print(f"{n}: {os.path.getsize(os.path.join(SITE, n)) // 1024} KB")


if __name__ == "__main__":
    main()
