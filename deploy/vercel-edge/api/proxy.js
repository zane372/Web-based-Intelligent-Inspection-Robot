export const config = { runtime: "edge" };

export default async function handler(request) {
  const base = process.env.UPSTREAM_URL;
  if (!base) return Response.json({ error: "UPSTREAM_URL not set" }, { status: 500 });

  const url = new URL(request.url);
  const target = new URL(base);
  const p = url.searchParams.get("p");
  if (p) target.pathname = `/${p.replace(/^\/+/, "")}`;
  for (const [k, v] of url.searchParams) if (k !== "p") target.searchParams.set(k, v);

  const headers = new Headers();
  for (const [k, v] of request.headers) {
    if (["host", "connection", "content-length"].includes(k.toLowerCase())) continue;
    headers.set(k, v);
  }
  headers.set("x-forwarded-host", url.host);

  const init = { method: request.method, headers, signal: AbortSignal.timeout(8000) };
  if (!["GET", "HEAD"].includes(request.method)) init.body = await request.text();

  try {
    const upstream = await fetch(target.toString(), init);
    const out = new Headers(upstream.headers);
    out.set("x-proxied-by", "edge");
    out.set("cache-control", "s-maxage=10, stale-while-revalidate=30");
    return new Response(upstream.body, { status: upstream.status, headers: out });
  } catch (err) {
    return Response.json({ error: "upstream failed", detail: String(err?.name ?? err) }, { status: 502 });
  }
}