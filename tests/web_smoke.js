const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const { chromium } = require("playwright");

const site = path.resolve(process.argv[2] || "site");
const requiredFiles = [
  "index.html",
  "jwpqt.js",
  "jwpqt.data",
  "jwpqt.wasm",
  "qtloader.js",
  "qtlogo.svg",
  "licenses/NotoSansJP-OFL.txt",
  "licenses/_cpright.txt",
  "licenses/gnugpl.txt",
  "share/doc/jwpqt/RELEASE_NOTES.md",
  "share/doc/jwpqt/handbook/manifest.json",
];

for (const relativePath of requiredFiles) {
  const filename = path.join(site, relativePath);
  const stat = fs.statSync(filename);
  if (!stat.isFile() || stat.size === 0) {
    throw new Error(`Missing or empty Web artifact: ${relativePath}`);
  }
}

const handbookDirectory = path.join(site, "share/doc/jwpqt/handbook");
const handbookTopics = fs.readdirSync(path.join(handbookDirectory, "topics"))
    .filter((filename) => filename.endsWith(".md"));
const handbookManifest = JSON.parse(
    fs.readFileSync(path.join(handbookDirectory, "manifest.json"), "utf8"));
if (handbookTopics.length !== 125 || handbookManifest.topic_count !== 125 ||
    handbookManifest.topics.length !== 125) {
  throw new Error("Incomplete Web handbook topic inventory");
}

const contentTypes = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".svg": "image/svg+xml",
  ".txt": "text/plain; charset=utf-8",
  ".wasm": "application/wasm",
};

const server = http.createServer((request, response) => {
  const requested = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
  const relativePath = requested === "/" ? "index.html" : requested.slice(1);
  const filename = path.resolve(site, relativePath);
  if (filename !== site && !filename.startsWith(`${site}${path.sep}`)) {
    response.writeHead(403).end();
    return;
  }

  fs.readFile(filename, (error, bytes) => {
    if (error) {
      response.writeHead(404).end();
      return;
    }
    response.writeHead(200, {
      "Cache-Control": "no-store",
      "Content-Type": contentTypes[path.extname(filename)] || "application/octet-stream",
    });
    response.end(bytes);
  });
});

async function main() {
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const address = server.address();
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  await page.emulateMedia({ colorScheme: "dark" });
  const errors = [];
  page.on("console", (message) => {
    if (message.type() === "error" && message.text() !== "unwind") {
      errors.push(`console: ${message.text()}`);
    }
  });
  page.on("pageerror", (error) => {
    if (error.message !== "unwind") {
      errors.push(`page: ${error.message}`);
    }
  });

  try {
    await page.goto(`http://127.0.0.1:${address.port}/`, { waitUntil: "load" });
    const canvas = page.locator("canvas").first();
    await canvas.waitFor({ state: "visible", timeout: 30000 });
    await page.waitForTimeout(1000);
    const waitForScheme = async (expected) => {
      for (let attempt = 0; attempt < 40; ++attempt) {
        const scheme = await page.evaluate(() => document.documentElement.dataset.jwpqtColorScheme);
        if (scheme === expected) return;
        await page.waitForTimeout(50);
      }
      throw new Error(`Web color scheme did not become ${expected}`);
    };
    await waitForScheme("dark");
    await page.emulateMedia({ colorScheme: "light" });
    await waitForScheme("light");
    await page.emulateMedia({ colorScheme: "dark" });
    await waitForScheme("dark");
    const bounds = await canvas.boundingBox();
    if (!bounds || bounds.width < 800 || bounds.height < 500) {
      throw new Error(`Web canvas is not full-sized: ${JSON.stringify(bounds)}`);
    }

    await page.mouse.click(300, 160);
    await page.evaluate(() => {
      window.__jwpqtOriginalCanvas =
        document.querySelector("#qt-shadow-container")?.shadowRoot?.querySelector("canvas") ??
        document.querySelector("canvas");
    });
    const baseCanvas = await canvas.screenshot();
    for (const closeKey of ["Escape", "Enter"]) {
      await page.keyboard.press("Alt+t");
      await page.waitForTimeout(100);
      await page.keyboard.press("Enter");
      await page.waitForTimeout(300);
      const optionsCanvas = await canvas.screenshot();
      if (optionsCanvas.equals(baseCanvas)) {
        throw new Error("Options did not open in the Web application");
      }
      await page.keyboard.press(closeKey);
      await page.waitForTimeout(500);
      const canvasSurvived = await page.evaluate(() => {
        const currentCanvas =
          document.querySelector("#qt-shadow-container")?.shadowRoot?.querySelector("canvas") ??
          document.querySelector("canvas");
        return window.__jwpqtOriginalCanvas === currentCanvas && currentCanvas?.isConnected === true;
      });
      if (!canvasSurvived) {
        throw new Error(`Web canvas was destroyed after closing Options with ${closeKey}`);
      }
    }

    await page.keyboard.press("Control+O");
    const picker = page.locator("#jwpqt-open-file");
    await picker.waitFor({ state: "attached" });
    const original = Buffer.from("web smoke 日本\n", "utf8");
    await picker.setInputFiles({ name: "web-smoke.txt", mimeType: "text/plain", buffer: original });
    await page.waitForTimeout(500);

    await page.keyboard.press("Control+End");
    for (const key of "konnichiha") {
      await page.keyboard.press(key);
    }
    await page.waitForTimeout(250);

    await page.evaluate(() => {
      window.__jwpqtSavedDownload = { bytes: null, error: null };
      const originalCreateObjectUrl = URL.createObjectURL;
      URL.createObjectURL = function createObjectURL(object) {
        const url = originalCreateObjectUrl.call(this, object);
        if (object instanceof Blob) {
          object.arrayBuffer()
            .then((buffer) => {
              window.__jwpqtSavedDownload.bytes = Array.from(new Uint8Array(buffer));
            })
            .catch((error) => {
              window.__jwpqtSavedDownload.error = String(error);
            });
        }
        return url;
      };
    });
    await page.mouse.click(20, 12);
    await page.waitForTimeout(250);
    await page.mouse.click(80, 236);
    let download = null;
    for (let attempt = 0; attempt < 40 && download?.bytes == null && !download?.error; ++attempt) {
      await page.waitForTimeout(250);
      download = await page.evaluate(() => window.__jwpqtSavedDownload);
    }
    if (download === null || download.error || download.bytes === null) {
      throw new Error(`Web download was not produced: ${download?.error ?? "timed out"}`);
    }
    const expected = Buffer.from("web smoke 日本\nこんにちは", "utf8");
    const actual = Buffer.from(download.bytes);
    if (!actual.equals(expected)) {
      throw new Error(`Web round trip mismatch: ${actual.toString("utf8")}`);
    }
    if (errors.length !== 0) {
      throw new Error(`Web console errors: ${JSON.stringify(errors)}`);
    }
  } finally {
    await browser.close();
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
