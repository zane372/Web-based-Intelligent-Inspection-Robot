import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
const APP = resolve(dirname(fileURLToPath(import.meta.url)), "..");
import { EdgeVM } from "@edge-runtime/vm";

const strip = (src) =>
  src
    .replace(/export const config = \{[\s\S]*?\};\s*/m, "")
    .replace(/export default async function handler/, "async function handler");

const MODULES = [
  {
    file: "api/router.js",
    prelude: "",
    cases: [
      { name: "GET p=info", url: "https://x.dev/api/router?p=info", init: {}, expect: { status: 200, header: ["cache-control", "s-maxage"], body: (b) => b.edge === true } },
      { name: "OPTIONS preflight", url: "https://x.dev/api/router?p=info", init: { method: "OPTIONS" }, expect: { status: 204 } },
      { name: "POST p=echo", url: "https://x.dev/api/router?p=echo", init: { method: "POST", body: JSON.stringify({ a: 1 }), headers: { "content-type": "application/json" } }, expect: { status: 201, body: (b) => b.received.a === 1 } },
      { name: "POST bad json", url: "https://x.dev/api/router?p=echo", init: { method: "POST", body: "{oops" }, expect: { status: 400 } },
      { name: "GET p=echo → 405", url: "https://x.dev/api/router?p=echo", init: {}, expect: { status: 405, header: ["allow", "POST"] } },
      { name: "unknown path → 404", url: "https://x.dev/api/router?p=nope", init: {}, expect: { status: 404 } },
    ],
  },
  {
    file: "api/auth.js",
    prelude: `globalThis.process = { env: { AUTH_SECRET: "test-secret" } };`,
    cases: [
      { name: "no token → 401", url: "https://x.dev/api/auth", init: {}, expect: { status: 401 } },
      { name: "tampered → 401", url: "https://x.dev/api/auth?token=abc.def", init: {}, expect: { status: 401 } },
    ],
    extra: `
      const issue = await handler(new Request("https://x.dev/api/auth", {
        method: "POST",
        body: JSON.stringify({ sub: "zane", ttl: 60 }),
        headers: { "content-type": "application/json" },
      }));
      const { token } = await issue.json();
      results.unshift({ name: "POST issue → 201", status: issue.status, headers: {}, body: { token } });
      await run([
        { name: "verify ok", url: "https://x.dev/api/auth?token=" + token, init: {} },
      ]);
    `,
    verify: async (results) => {
      const issued = results.find((r) => r.name === "POST issue → 201");
      return issued.status === 201 && typeof issued.body.token === "string" && issued.body.token.includes(".");
    },
  },
  {
    file: "api/proxy.js",
    prelude: `
      globalThis.process = { env: { UPSTREAM_URL: "https://upstream.test/base" } };
      globalThis.fetch = async (u, i) => {
        if (String(u).includes("/boom")) throw new TypeError("boom");
        return new Response(JSON.stringify({ upstream: String(u), method: i.method, body: i.body ?? null }),
          { status: 200, headers: { "x-upstream": "real" } });
      };
    `,
    cases: [
      { name: "GET p=users?a=1", url: "https://x.dev/api/proxy?p=users&a=1", init: {}, expect: { status: 200, header: ["x-proxied-by", "edge"], body: (b) => b.upstream === "https://upstream.test/users?a=1" } },
      { name: "POST p=orders", url: "https://x.dev/api/proxy?p=orders", init: { method: "POST", body: "{\"x\":1}" }, expect: { status: 200, body: (b) => b.body === "{\"x\":1}" && b.method === "POST" } },
      { name: "upstream throw → 502", url: "https://x.dev/api/proxy?p=boom", init: {}, expect: { status: 502 } },
    ],
  },
];

const runModule = async (m) => {
  const src = strip(readFileSync(resolve(APP, m.file), "utf8"));
  const vm = new EdgeVM({ initialCode: `${m.prelude}\n${src}` });

  const code = `(async () => {
    const results = [];
    const run = async (cases) => {
      for (const c of cases) {
        try {
          const res = await handler(new Request(c.url, c.init));
          const text = await res.text();
          let body;
          try { body = JSON.parse(text); } catch { body = text; }
          results.push({ name: c.name, status: res.status, headers: Object.fromEntries(res.headers), body });
        } catch (e) {
          results.push({ name: c.name, error: String(e) });
        }
      }
      return results;
    };
    await run(${JSON.stringify(m.cases)});
    ${m.extra ?? ""}
    return results;
  })()`;

  return vm.evaluate(code);
};

let pass = 0, fail = 0;
const lines = [];
for (const m of MODULES) {
  const results = await runModule(m);
  for (const r of results) {
    const c = m.cases.find((x) => x.name === r.name);
    let ok = !r.error;
    const notes = [];
    if (r.error) notes.push(r.error.slice(0, 90));
    if (ok && c?.expect) {
      const e = c.expect;
      if (e.status !== undefined && r.status !== e.status) { ok = false; notes.push(`status ${r.status} != ${e.status}`); }
      if (ok && e.header) {
        const v = r.headers[e.header[0]] ?? "";
        if (!String(v).includes(e.header[1])) { ok = false; notes.push(`header ${e.header[0]}="${v}"`); }
      }
      if (ok && e.body && !e.body(r.body)) { ok = false; notes.push(`body ${JSON.stringify(r.body).slice(0, 80)}`); }
    }
    if (ok && r.name === "POST issue → 201" && m.verify) {
      const v = await m.verify(results);
      if (!v) { ok = false; notes.push("token malformed"); }
    }
    ok ? pass++ : fail++;
    lines.push(`${ok ? "PASS" : "FAIL"}  ${m.file.padEnd(14)} ${r.name.padEnd(24)} ${r.error ? "" : String(r.status).padEnd(4)} ${notes.join(" | ")}`);
  }
}
console.log(lines.join("\n"));
console.log(`\n${pass} passed, ${fail} failed`);
process.exitCode = fail ? 1 : 0;