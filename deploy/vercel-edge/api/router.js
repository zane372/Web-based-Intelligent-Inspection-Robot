export const config = { runtime: "edge" };

const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-methods": "GET,POST,OPTIONS",
  "access-control-allow-headers": "content-type,authorization",
};

export default async function handler(request) {
  const url = new URL(request.url);
  const path = url.searchParams.get("p") ?? url.pathname.replace(/^\/api\/router\/?/, "");

  if (request.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });

  if (path === "info" && request.method === "GET") {
    return Response.json(
      { path, region: (typeof process !== "undefined" && process.env?.VERCEL_REGION) || "local", edge: Boolean(globalThis.EdgeRuntime) },
      { headers: { ...CORS, "cache-control": "s-maxage=30, stale-while-revalidate=60" } },
    );
  }

  if (path === "echo" && request.method === "POST") {
    const data = await request.json().catch(() => null);
    if (data === null) return Response.json({ error: "invalid json" }, { status: 400, headers: CORS });
    return Response.json({ received: data }, { status: 201, headers: CORS });
  }

  if (path === "echo") {
    return Response.json({ error: "method not allowed" }, { status: 405, headers: { ...CORS, allow: "POST" } });
  }

  return Response.json({ error: "not found", path }, { status: 404, headers: CORS });
}