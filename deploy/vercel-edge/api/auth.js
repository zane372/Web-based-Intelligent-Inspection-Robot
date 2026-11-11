export const config = { runtime: "edge" };

const enc = new TextEncoder();
const secret = () => process.env.AUTH_SECRET ?? "dev-secret";

const b64url = (bytes) =>
  btoa(String.fromCharCode(...new Uint8Array(bytes))).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");

const unb64url = (s) =>
  Uint8Array.from(atob(s.replace(/-/g, "+").replace(/_/g, "/")), (c) => c.charCodeAt(0));

async function sign(data) {
  const key = await crypto.subtle.importKey("raw", enc.encode(secret()), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  return new Uint8Array(await crypto.subtle.sign("HMAC", key, enc.encode(data)));
}

async function verify(data, sigB64) {
  const expected = await sign(data);
  const given = unb64url(sigB64);
  if (given.length !== expected.length) return false;
  let diff = 0;
  for (let i = 0; i < expected.length; i++) diff |= expected[i] ^ given[i];
  return diff === 0;
}

export default async function handler(request) {
  const url = new URL(request.url);

  if (request.method === "POST") {
    const { sub = "anonymous", ttl = 3600 } = await request.json().catch(() => ({}));
    const payload = b64url(enc.encode(JSON.stringify({ sub, exp: Date.now() + ttl * 1000 })));
    const token = `${payload}.${b64url(await sign(payload))}`;
    return Response.json({ token }, { status: 201 });
  }

  const token = url.searchParams.get("token") ?? "";
  const [payload, sig] = token.split(".");
  if (!payload || !sig || !(await verify(payload, sig))) {
    return Response.json({ error: "invalid token" }, { status: 401 });
  }
  const claims = JSON.parse(new TextDecoder().decode(unb64url(payload)));
  if (claims.exp < Date.now()) return Response.json({ error: "expired" }, { status: 401 });
  return Response.json({ ok: true, sub: claims.sub });
}