// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Dubwise FM contributors
// The web sound editor (roadmap 5.13; tools/site/editor/), opened in headless Chromium without an FM-1: every channel's
// tab shows a slider for each of its fields (fields.js, made from the firmware), the page's CRC is the firmware's
// (CRC-16/CCITT-FALSE: "123456789" -> 0x29B1) and its 7-bit packing round-trips; no script errors.
// CI: .github/workflows/ci.yml, job "web".   node tests/editor_test.mjs
import { fileURLToPath } from "node:url";
import { createRequire } from "node:module";
import { readFileSync } from "node:fs";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const page_url = new URL("../tools/site/editor/index.html", import.meta.url).href;
const FIELDS = JSON.parse(readFileSync(fileURLToPath(new URL("../tools/site/editor/fields.js", import.meta.url)), "utf8")
  .split("= ")[1].trim().replace(/;$/, ""));

const browser = await chromium.launch();
const page = await browser.newPage();
const errors = [];
page.on("pageerror", (e) => errors.push(String(e)));
await page.goto(page_url);
let fail = 0;
for (let ch = 0; ch < 8; ch++) {
  await page.locator("#tabs button").nth(ch).click();
  const n = await page.locator('#fields input[type="range"]').count();
  const want = FIELDS.filter((x) => x.ch === ch).length;
  if (n !== want) { console.log(`channel ${ch + 1}: ${n} sliders, want ${want}`); fail = 1; }
}
const r = await page.evaluate(() => {
  const crc = crc16([..."123456789"].map((c) => c.charCodeAt(0)));
  const data = Array.from({ length: 48 }, (_, i) => (i * 37 + 11) & 0xFF);
  const back = unpack7(pack7(data)).slice(0, 48);
  return { crc, round: back.every((v, i) => v === data[i]) };
});
if (r.crc !== 0x29B1) { console.log(`crc16 check: 0x${r.crc.toString(16)}, want 0x29b1`); fail = 1; }
if (!r.round) { console.log("pack7 / unpack7 don't round-trip"); fail = 1; }
if (errors.length) { console.log("script errors:", errors); fail = 1; }
await browser.close();
console.log(fail ? "editor: FAIL" : `editor: ok (${FIELDS.length} fields, 8 tabs, crc, packing)`);
process.exit(fail);
