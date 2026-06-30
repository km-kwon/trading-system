const state = {
  snapshot: null,
  side: "BUY",
  orderType: "LIMIT",
  eventSource: null,
  settingsInitialized: false,
  renderScheduled: false,
  htmlCache: new Map(),
};

const els = {
  eventStatus: document.querySelector("#eventStatus"),
  engineStatus: document.querySelector("#engineStatus"),
  instrumentId: document.querySelector("#instrumentId"),
  referenceVersion: document.querySelector("#referenceVersion"),
  nextIds: document.querySelector("#nextIds"),
  orderForm: document.querySelector("#orderForm"),
  cancelForm: document.querySelector("#cancelForm"),
  timeInForce: document.querySelector("#timeInForce"),
  price: document.querySelector("#price"),
  quantity: document.querySelector("#quantity"),
  orderId: document.querySelector("#orderId"),
  cancelOrderId: document.querySelector("#cancelOrderId"),
  submitOrder: document.querySelector("#submitOrder"),
  message: document.querySelector("#message"),
  bestBid: document.querySelector("#bestBid"),
  bestAsk: document.querySelector("#bestAsk"),
  spread: document.querySelector("#spread"),
  bidOrderCount: document.querySelector("#bidOrderCount"),
  askOrderCount: document.querySelector("#askOrderCount"),
  bidDepthTotal: document.querySelector("#bidDepthTotal"),
  askDepthTotal: document.querySelector("#askDepthTotal"),
  bookPressure: document.querySelector("#bookPressure"),
  bookPressureDetail: document.querySelector("#bookPressureDetail"),
  referenceSymbol: document.querySelector("#referenceSymbol"),
  referenceGrid: document.querySelector("#referenceGrid"),
  asks: document.querySelector("#asks"),
  bids: document.querySelector("#bids"),
  activeBidOrders: document.querySelector("#activeBidOrders"),
  activeAskOrders: document.querySelector("#activeAskOrders"),
  statsGrid: document.querySelector("#statsGrid"),
  vwap: document.querySelector("#vwap"),
  tradeCount: document.querySelector("#tradeCount"),
  tradesBody: document.querySelector("#tradesBody"),
  openOrderCount: document.querySelector("#openOrderCount"),
  openOrders: document.querySelector("#openOrders"),
  responseStream: document.querySelector("#responseStream"),
  refreshSnapshot: document.querySelector("#refreshSnapshot"),
};

const numberFormat = new Intl.NumberFormat("en-US");
const scheduleFrame = window.requestAnimationFrame
  ? (callback) => window.requestAnimationFrame(callback)
  : (callback) => window.setTimeout(callback, 16);

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function formatNumber(value) {
  if (value === null || value === undefined || value === "") {
    return "-";
  }
  return numberFormat.format(Number(value));
}

function formatPrice(value) {
  return formatNumber(value);
}

function formatSide(side) {
  return side === "BUY" ? "Buy" : side === "SELL" ? "Sell" : "-";
}

function api(path, body) {
  return fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  }).then(async (response) => {
    const payload = await response.json();
    if (!response.ok || payload.ok === false) {
      throw new Error(payload.error || `HTTP ${response.status}`);
    }
    return payload;
  });
}

function setMessage(text, kind = "") {
  els.message.textContent = text;
  els.message.className = `message ${kind}`.trim();
}

function setText(element, text) {
  const next = String(text ?? "");
  if (element.textContent !== next) {
    element.textContent = next;
  }
}

function setClassName(element, className) {
  if (element.className !== className) {
    element.className = className;
  }
}

function setHtml(cacheKey, element, html) {
  if (state.htmlCache.get(cacheKey) !== html) {
    element.innerHTML = html;
    state.htmlCache.set(cacheKey, html);
  }
}

function getSettings() {
  return {
    instrumentId: Number(els.instrumentId.value),
    referenceVersion: Number(els.referenceVersion.value),
  };
}

function setControl(control, value) {
  state[control] = value;
  document.querySelectorAll(`[data-control="${control}"]`).forEach((button) => {
    button.classList.toggle("active", button.dataset.value === value);
  });
  syncTicketState();
}

function syncTicketState() {
  const isBuy = state.side === "BUY";
  els.submitOrder.textContent = isBuy ? "Submit Buy" : "Submit Sell";
  els.submitOrder.classList.toggle("buy-action", isBuy);
  els.submitOrder.classList.toggle("sell-action", !isBuy);
  els.price.disabled = state.orderType === "MARKET";
}

function applySnapshot(snapshot) {
  state.snapshot = snapshot;
  if (!state.settingsInitialized && snapshot.defaults) {
    els.instrumentId.value = snapshot.defaults.instrumentId ?? 1001;
    els.referenceVersion.value = snapshot.defaults.referenceVersion ?? 7;
    state.settingsInitialized = true;
  }
  scheduleRender();
}

function scheduleRender() {
  if (state.renderScheduled) {
    return;
  }
  state.renderScheduled = true;
  scheduleFrame(() => {
    state.renderScheduled = false;
    render();
  });
}

function pollSnapshotWhenDisconnected() {
  if (!state.eventSource || state.eventSource.readyState !== EventSource.OPEN) {
    fetchSnapshot();
  }
}

function updateEventStatus(text, className) {
  setText(els.eventStatus, text);
  setClassName(els.eventStatus, className);
}

function render() {
  const snapshot = state.snapshot;
  if (!snapshot) {
    return;
  }
  renderStatus(snapshot);
  renderReferenceData(snapshot.referenceData || {}, snapshot.defaults || {});
  renderBook(snapshot.book || {}, snapshot.openOrders || []);
  renderStats(snapshot.stats || {});
  renderTrades(snapshot.trades || []);
  renderOpenOrders(snapshot.openOrders || []);
  renderResponses(snapshot.responses || []);
  setText(els.nextIds, `seq ${snapshot.nextSequence ?? "-"} / order ${snapshot.nextOrderId ?? "-"}`);
}

function renderStatus(snapshot) {
  const health = snapshot.health || {};
  const online = Boolean(health.engineConnected);
  setText(els.engineStatus, online ? "Gateway Online" : "Gateway Offline");
  setClassName(els.engineStatus, `status-pill ${online ? "on" : "off"}`);
}

function renderReferenceData(referenceData, defaults) {
  const instrumentId = referenceData.instrumentId ?? defaults.instrumentId;
  const referenceVersion = referenceData.referenceVersion ?? defaults.referenceVersion;
  const symbol = referenceData.symbol || "-";
  const priceBand = referenceData.lowerPriceLimit && referenceData.upperPriceLimit
    ? `${formatPrice(referenceData.lowerPriceLimit)} - ${formatPrice(referenceData.upperPriceLimit)}`
    : "-";
  const rows = [
    ["Instrument", instrumentId ? `#${instrumentId}` : "-"],
    ["Tick Size", referenceData.tickSize ?? "-"],
    ["Price Band", priceBand],
    ["Session", referenceData.session || "-"],
    ["Ref Version", referenceVersion ?? "-"],
  ];

  setText(els.referenceSymbol, symbol);
  setHtml(
    "referenceGrid",
    els.referenceGrid,
    rows
      .map(
        ([label, value]) => `
        <div class="reference-item">
          <span>${escapeHtml(label)}</span>
          <strong>${escapeHtml(value)}</strong>
        </div>
      `,
      )
      .join(""),
  );
}

function renderBook(book, openOrders) {
  const bestBid = book.bestBid;
  const bestAsk = book.bestAsk;
  setText(els.bestBid, bestBid ? formatPrice(bestBid.price) : "-");
  setText(els.bestAsk, bestAsk ? formatPrice(bestAsk.price) : "-");

  if (bestBid && bestAsk) {
    setText(els.spread, `spread ${formatNumber(Number(bestAsk.price) - Number(bestBid.price))}`);
  } else {
    setText(els.spread, "spread -");
  }

  const levels = [...(book.bids || []), ...(book.asks || [])];
  const maxQuantity = Math.max(1, ...levels.map((level) => Number(level.quantity || 0)));
  renderLevels(els.asks, book.asks || [], "ask", maxQuantity);
  renderLevels(els.bids, book.bids || [], "bid", maxQuantity);
  renderRestingBook(openOrders);
}

function renderLevels(container, levels, side, maxQuantity) {
  if (!levels.length) {
    setHtml(
      `levels-${side}`,
      container,
      `<div class="level-row empty"><span>No ${side}s</span><span>-</span></div>`,
    );
    return;
  }

  setHtml(
    `levels-${side}`,
    container,
    levels
      .map((level) => {
        const quantity = Number(level.quantity || 0);
        const depth = Math.max(4, Math.min(100, (quantity / maxQuantity) * 100));
        return `
        <div class="level-row">
          <div class="level-fill" style="--depth: ${depth}%"></div>
          <span>${formatNumber(quantity)}</span>
          <span class="level-price ${side}">${formatPrice(level.price)}</span>
        </div>
      `;
      })
      .join(""),
  );
}

function orderRemaining(order) {
  return Number(order.remainingQuantity ?? order.quantity ?? 0);
}

function orderPrice(order, fallback = 0) {
  if (order.orderType === "MARKET" || order.price === null || order.price === undefined) {
    return fallback;
  }
  return Number(order.price);
}

function sortRestingOrders(orders, side) {
  return [...orders].sort((a, b) => {
    const priceA = orderPrice(a, side === "BUY" ? Number.NEGATIVE_INFINITY : Number.POSITIVE_INFINITY);
    const priceB = orderPrice(b, side === "BUY" ? Number.NEGATIVE_INFINITY : Number.POSITIVE_INFINITY);
    if (priceA !== priceB) {
      return side === "BUY" ? priceB - priceA : priceA - priceB;
    }
    return Number(a.sequence || 0) - Number(b.sequence || 0);
  });
}

function sortOpenOrdersForPanel(orders) {
  return [...orders].sort((a, b) => {
    const sideA = a.side === "BUY" ? 0 : 1;
    const sideB = b.side === "BUY" ? 0 : 1;
    if (sideA !== sideB) {
      return sideA - sideB;
    }
    const priceA = orderPrice(a, a.side === "BUY" ? Number.NEGATIVE_INFINITY : Number.POSITIVE_INFINITY);
    const priceB = orderPrice(b, b.side === "BUY" ? Number.NEGATIVE_INFINITY : Number.POSITIVE_INFINITY);
    if (priceA !== priceB) {
      return a.side === "BUY" ? priceB - priceA : priceA - priceB;
    }
    return Number(a.sequence || 0) - Number(b.sequence || 0);
  });
}

function renderRestingBook(openOrders) {
  const activeOrders = (openOrders || []).filter((order) => orderRemaining(order) > 0);
  const buyOrders = sortRestingOrders(
    activeOrders.filter((order) => order.side === "BUY"),
    "BUY",
  );
  const sellOrders = sortRestingOrders(
    activeOrders.filter((order) => order.side === "SELL"),
    "SELL",
  );
  const bidQty = buyOrders.reduce((sum, order) => sum + orderRemaining(order), 0);
  const askQty = sellOrders.reduce((sum, order) => sum + orderRemaining(order), 0);
  const pressure = bidQty + askQty > 0 ? Math.round((bidQty / (bidQty + askQty)) * 100) : null;
  const maxOrderQty = Math.max(1, ...activeOrders.map(orderRemaining));

  setText(els.bidOrderCount, formatNumber(buyOrders.length));
  setText(els.askOrderCount, formatNumber(sellOrders.length));
  setText(els.bidDepthTotal, `qty ${formatNumber(bidQty)}`);
  setText(els.askDepthTotal, `qty ${formatNumber(askQty)}`);
  setText(els.bookPressure, pressure === null ? "-" : `${pressure}% bid`);
  setText(els.bookPressureDetail, `bid ${formatNumber(bidQty)} / ask ${formatNumber(askQty)}`);
  renderRestingOrders(els.activeBidOrders, buyOrders, "bid", maxOrderQty);
  renderRestingOrders(els.activeAskOrders, sellOrders, "ask", maxOrderQty);
}

function renderRestingOrders(container, orders, side, maxOrderQty) {
  if (!orders.length) {
    setHtml(
      `resting-${side}`,
      container,
      `<div class="resting-order-row empty"><span>No ${side === "bid" ? "buy" : "sell"} orders</span><span>-</span></div>`,
    );
    return;
  }

  setHtml(
    `resting-${side}`,
    container,
    orders
      .slice(0, 80)
      .map((order) => {
        const remaining = orderRemaining(order);
        const depth = Math.max(5, Math.min(100, (remaining / maxOrderQty) * 100));
        const price = order.orderType === "MARKET" ? "MKT" : formatPrice(order.price);
        return `
        <div class="resting-order-row">
          <div class="level-fill" style="--depth: ${depth}%"></div>
          <span class="resting-order-id">#${escapeHtml(order.orderId)}</span>
          <span class="resting-price ${side}">${escapeHtml(price)}</span>
          <strong>${formatNumber(remaining)}</strong>
        </div>
      `;
      })
      .join(""),
  );
}

function renderStats(stats) {
  const tradedQuantity = Number(stats.tradedQuantity || 0);
  const tradedNotional = Number(stats.tradedNotional || 0);
  const vwap = tradedQuantity > 0 ? Math.floor(tradedNotional / tradedQuantity) : null;
  setText(els.vwap, `VWAP ${formatPrice(vwap)}`);

  const rows = [
    ["Commands", stats.commands || 0],
    ["Accepted", stats.accepted || 0],
    ["Rejected", stats.rejected || 0],
    ["Trades", stats.trades || 0],
    ["Quantity", tradedQuantity],
    ["Notional", tradedNotional],
  ];

  setHtml(
    "statsGrid",
    els.statsGrid,
    rows
      .map(
        ([label, value]) => `
        <div class="stat">
          <span>${escapeHtml(label)}</span>
          <strong>${formatNumber(value)}</strong>
        </div>
      `,
      )
      .join(""),
  );
}

function renderTrades(trades) {
  setText(els.tradeCount, `${trades.length} trades`);
  if (!trades.length) {
    setHtml("tradesBody", els.tradesBody, `<tr><td colspan="5" class="empty-state">No trades</td></tr>`);
    return;
  }

  setHtml(
    "tradesBody",
    els.tradesBody,
    trades
      .slice(0, 80)
      .map((trade) => {
        const side = trade.aggressorSide;
        const sideClass = side === "BUY" ? "side-buy" : side === "SELL" ? "side-sell" : "";
        return `
        <tr>
          <td>${escapeHtml(trade.sequence ?? "-")}</td>
          <td class="${sideClass}">${escapeHtml(formatSide(side))}</td>
          <td>${formatPrice(trade.price)}</td>
          <td>${formatNumber(trade.quantity)}</td>
          <td>${escapeHtml(trade.incomingOrderId ?? "-")}</td>
        </tr>
      `;
      })
      .join(""),
  );
}

function renderOpenOrders(orders) {
  setText(els.openOrderCount, `${orders.length} open`);
  if (!orders.length) {
    setHtml("openOrders", els.openOrders, `<div class="empty-state">No open orders</div>`);
    return;
  }

  setHtml(
    "openOrders",
    els.openOrders,
    sortOpenOrdersForPanel(orders)
      .map((order) => {
        const sideClass = order.side === "BUY" ? "side-buy" : "side-sell";
        const price = order.orderType === "MARKET" ? "MKT" : formatPrice(order.price);
        return `
        <div class="order-row">
          <div class="order-main">
            <div class="order-title">
              <span class="${sideClass}">#${escapeHtml(order.orderId)}</span>
              <span>${escapeHtml(formatSide(order.side))}</span>
              <span>${escapeHtml(order.orderType || "-")}</span>
            </div>
            <div class="order-meta">
              ${price} / remaining ${formatNumber(order.remainingQuantity)}
            </div>
          </div>
          <button type="button" data-cancel-order="${escapeHtml(order.orderId)}">Cancel</button>
        </div>
      `;
      })
      .join(""),
  );
}

function renderResponses(responses) {
  if (!responses.length) {
    setHtml("responseStream", els.responseStream, `<div class="empty-state">No gateway responses</div>`);
    return;
  }

  setHtml(
    "responseStream",
    els.responseStream,
    responses
      .slice(0, 80)
      .map((response) => {
        const accepted = response.status === "ACCEPTED";
        const label = accepted ? "accepted" : "rejected";
        const detail = response.detail && response.detail !== "NONE" ? response.detail : response.reason;
        return `
        <div class="stream-row ${label}">
          <div class="stream-main">
            <div class="stream-title">
              <span>${escapeHtml(response.status || "UNKNOWN")}</span>
              <span>seq ${escapeHtml(response.sequence ?? "-")}</span>
              <span>${escapeHtml(response.commandType || "-")}</span>
              <span>${escapeHtml(detail || "")}</span>
            </div>
            <div class="stream-raw">${escapeHtml(response.raw || "")}</div>
          </div>
        </div>
      `;
      })
      .join(""),
  );
}

async function submitOrder(event) {
  event.preventDefault();
  const settings = getSettings();
  const body = {
    ...settings,
    side: state.side,
    orderType: state.orderType,
    timeInForce: els.timeInForce.value,
    price: els.price.value,
    quantity: els.quantity.value,
    orderId: els.orderId.value || undefined,
  };

  if (state.orderType === "MARKET") {
    body.price = undefined;
  }

  await sendAction(() => api("/api/order", body));
  els.orderId.value = "";
}

async function cancelOrder(event) {
  event.preventDefault();
  await cancelOrderId(els.cancelOrderId.value);
}

async function cancelOrderId(orderId) {
  const settings = getSettings();
  await sendAction(() =>
    api("/api/cancel", {
      ...settings,
      orderId,
    }),
  );
  els.cancelOrderId.value = "";
}

async function sendAction(action) {
  setMessage("Sending...", "");
  setBusy(true);
  try {
    const payload = await action();
    if (payload.snapshot) {
      applySnapshot(payload.snapshot);
    }
    const response = payload.response || {};
    const ok = response.status === "ACCEPTED";
    setMessage(response.raw || "OK", ok ? "ok" : "error");
  } catch (error) {
    setMessage(error.message || String(error), "error");
  } finally {
    setBusy(false);
  }
}

function setBusy(isBusy) {
  els.submitOrder.disabled = isBusy;
  document.querySelectorAll(".secondary-action").forEach((button) => {
    button.disabled = isBusy;
  });
}

async function fetchSnapshot() {
  try {
    const response = await fetch("/api/snapshot");
    const snapshot = await response.json();
    applySnapshot(snapshot);
  } catch (error) {
    setText(els.engineStatus, "Bridge Offline");
    setClassName(els.engineStatus, "status-pill off");
  }
}

function connectEvents() {
  if (state.eventSource) {
    state.eventSource.close();
  }
  const source = new EventSource("/events");
  state.eventSource = source;

  source.onopen = () => {
    updateEventStatus("Live", "status-pill on");
  };

  source.onerror = () => {
    updateEventStatus("Reconnecting", "status-pill warn");
  };

  source.onmessage = (event) => {
    try {
      const payload = JSON.parse(event.data);
      if (payload.snapshot) {
        applySnapshot(payload.snapshot);
      }
    } catch (error) {
      setMessage(error.message || String(error), "error");
    }
  };
}

document.querySelectorAll("[data-control]").forEach((button) => {
  button.addEventListener("click", () => {
    setControl(button.dataset.control, button.dataset.value);
  });
});

els.orderForm.addEventListener("submit", submitOrder);
els.cancelForm.addEventListener("submit", cancelOrder);
els.refreshSnapshot.addEventListener("click", fetchSnapshot);

els.openOrders.addEventListener("click", (event) => {
  const button = event.target.closest("[data-cancel-order]");
  if (!button) {
    return;
  }
  cancelOrderId(button.dataset.cancelOrder);
});

setControl("side", "BUY");
setControl("orderType", "LIMIT");
fetchSnapshot();
connectEvents();
setInterval(pollSnapshotWhenDisconnected, 5000);
