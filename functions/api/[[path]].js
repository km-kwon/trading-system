function bridgeOrigin(env) {
  const origin = env.ATS_BRIDGE_ORIGIN || env.ATS_BRIDGE_PROXY_ORIGIN;
  if (!origin) {
    return null;
  }
  return origin.endsWith("/") ? origin.slice(0, -1) : origin;
}

function unavailable() {
  return Response.json(
    {
      ok: false,
      error: "ATS_BRIDGE_ORIGIN is not configured for this Cloudflare Pages deployment",
    },
    {
      status: 503,
      headers: {
        "cache-control": "no-store",
      },
    },
  );
}

export async function onRequest(context) {
  const origin = bridgeOrigin(context.env);
  if (!origin) {
    return unavailable();
  }

  const requestUrl = new URL(context.request.url);
  const targetUrl = `${origin}${requestUrl.pathname}${requestUrl.search}`;
  const headers = new Headers(context.request.headers);
  headers.delete("host");

  return fetch(targetUrl, {
    method: context.request.method,
    headers,
    body: context.request.method === "GET" || context.request.method === "HEAD"
      ? undefined
      : context.request.body,
    redirect: "manual",
  });
}
