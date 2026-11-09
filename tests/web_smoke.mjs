// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Dubwise FM contributors
// The browser version, started as a player would (CI: .github/workflows/ci.yml, job "web"): app/build/emu/ served
// over HTTP, opened in headless Chromium, "Switch on" pressed; it must say "On.", show no error, and the screen must
// be drawn (the DUB page: not all black). After ./build.sh --web.   node tests/web_smoke.mjs
import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const ROOT = join(fileURLToPath(new URL("..", import.meta.url)), "app/build/emu");
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm",
                ".json": "application/json" };

const server = createServer(async (req, res) => {
  const path = normalize(join(ROOT, decodeURIComponent(new URL(req.url, "http://x").pathname)));
  try {
    const file = path.endsWith("/") ? join(path, "index.html") : path;
    const body = await readFile(file);
    res.writeHead(200, { "content-type": TYPES[extname(file)] || "application/octet-stream" });
    res.end(body);
  } catch {
    res.writeHead(404);
    res.end();
  }
});
await new Promise((r) => server.listen(0, r));
const url = `http://localhost:${server.address().port}/index.html`;

let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(56)} ${cond ? "ok" : "FAIL"}`); failed += !cond; };
const browser = await chromium.launch({ args: ["--autoplay-policy=no-user-gesture-required"] });
try {
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url);
  await page.click("#go");
  await page.waitForFunction(() => document.querySelector("#status").textContent.startsWith("On"), null, { timeout: 15000 })
    .catch(() => {});
  await page.waitForTimeout(1500);
  const status = await page.textContent("#status"), alert = await page.textContent("#alert");
  ok(status.startsWith("On"), `switched on (status: "${status}")`);
  ok(!alert, `no alert${alert ? ` ("${alert}")` : ""}`);
  ok(errors.length === 0, `no page errors${errors.length ? ` (${errors[0]})` : ""}`);
  const lit = await page.evaluate(() => {          // the screen drawn: some of its pixels not black
    const c = document.querySelector("#screen"), d = c.getContext("2d").getImageData(0, 0, c.width, c.height).data;
    let n = 0;
    for (let i = 0; i < d.length; i += 4) if (d[i] + d[i + 1] + d[i + 2] > 60) n++;
    return n;
  });
  ok(lit > 1000, `the screen drawn (${lit} lit pixels)`);
} finally {
  await browser.close();
  server.close();
}
console.log(failed ? `web smoke: ${failed} FAILED` : "web smoke: ok");
process.exit(failed ? 1 : 0);
