function bridgeOrigin(env) {
  const origin = env.ATS_BRIDGE_ORIGIN || env.ATS_BRIDGE_PROXY_ORIGIN;
  if (!origin) {
    return null;
  }
  return origin.endsWith("/") ? origin.slice(0, -1) : origin;
}

function unavailableEventStream() {
  const payload = JSON.stringify({
    error: "ATS_BRIDGE_ORIGIN is not configured for this Cloudflare Pages deployment",
  });
  return new Response(`event: error\ndata: ${payload}\n\n`, {
    status: 503,
    headers: {
      "content-type": "text/event-stream; charset=utf-8",
      "cache-control": "no-store",
    },
  });
}

export async function onRequestGet(context) {
  const origin = bridgeOrigin(context.env);
  if (!origin) {
    return unavailableEventStream();
  }

  const requestUrl = new URL(context.request.url);
  const headers = new Headers(context.request.headers);
  headers.delete("host");

  return fetch(`${origin}${requestUrl.pathname}${requestUrl.search}`, {
    headers,
    redirect: "manual",
  });
}
