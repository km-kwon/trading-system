# Operations Runbook

Mini ATS Trading Console은 Cloudflare Pages에 전부 올라가는 앱이 아닙니다.
현재 배포 구조는 정적 UI와 Pages Functions는 Cloudflare에 두고, 실제 matching engine은 로컬 WSL에서 실행한 뒤 Cloudflare Tunnel로 연결하는 방식입니다.

## 구성

```text
Browser
  -> Cloudflare Pages
  -> Pages Functions (/api/*, /events)
  -> Cloudflare Tunnel (*.trycloudflare.com)
  -> local web/bridge.py
  -> local C++ mini_ats TCP gateway
```

각 구성요소의 역할:

- `web/index.html`, `web/app.js`, `web/styles.css`: Trading Console UI
- `functions/api/[[path]].js`: Cloudflare Pages에서 `/api/*` 요청을 bridge로 proxy
- `functions/events.js`: Cloudflare Pages에서 `/events` SSE stream을 bridge로 proxy
- `web/bridge.py`: browser API를 C++ TCP gateway로 전달하고 market data를 SSE로 streaming
- `build/mini_ats`: C++ matching engine과 TCP/UDP gateway
- `ATS_BRIDGE_ORIGIN`: Pages Functions가 호출할 bridge HTTPS origin

## 평소 실행 순서

아침에 한 번에 켜려면 다음 스크립트를 사용합니다.

```bash
./scripts/start_cloudflare_preview.sh
```

이 스크립트는 기존 local bridge/tunnel 프로세스를 정리한 뒤 다음 작업을 자동으로 수행합니다.

- `web/bridge.py --start-engine` 실행
- Cloudflare quick tunnel 실행
- 새 `https://...trycloudflare.com` 주소 추출
- Pages preview secret `ATS_BRIDGE_ORIGIN` 갱신
- 임시 Wrangler workspace에 `ATS_BRIDGE_PROXY_ORIGIN` fallback var 주입
- Cloudflare Pages `master` preview branch 재배포로 Functions 환경변수 반영
- `https://master.mini-ats-trading-console.pages.dev/api/health` 확인

종료할 때는:

```bash
./scripts/start_cloudflare_preview.sh --stop-only
```

bridge/tunnel은 살아 있는데 Cloudflare secret 또는 deploy 단계에서만 실패했다면:

```bash
./scripts/start_cloudflare_preview.sh --deploy-only
```

수동으로 실행하려면 아래 절차를 따릅니다.

터미널 1에서 local bridge와 C++ engine을 실행합니다.

```bash
python3 web/bridge.py --host 127.0.0.1 --port 8080 --start-engine
```

터미널 2에서 Cloudflare Tunnel을 실행합니다.

```bash
~/.local/bin/cloudflared tunnel --url http://127.0.0.1:8080
```

출력된 `https://...trycloudflare.com` 주소를 기억합니다.
이 주소는 quick tunnel을 다시 켤 때마다 바뀔 수 있습니다.

## Tunnel 주소가 바뀐 경우

현재 접속 주소는 preview branch alias입니다.

```text
https://master.mini-ats-trading-console.pages.dev
```

이 주소는 preview deployment입니다.
현재 repo의 `wrangler.toml`에 `pages_build_output_dir`가 있어서 Cloudflare Pages는 Wrangler config를 배포 설정의 source of truth로 사용합니다.
그래서 secret을 넣은 뒤 새 배포까지 해야 Function 런타임 환경변수에 반영됩니다.

```bash
./scripts/start_cloudflare_preview.sh
```

스크립트는 현재 quick tunnel URL을 Pages secret `ATS_BRIDGE_ORIGIN`에 넣고, `.runtime/cloudflare-preview/deploy-cwd/`에 `web`, `functions`, `wrangler.toml` 배포 사본을 만든 뒤 그 임시 workspace에서 재배포합니다.
Pages Functions는 `ATS_BRIDGE_ORIGIN`을 우선 읽고, direct upload preview에서 secret이 런타임에 비어 있으면 임시 config의 `ATS_BRIDGE_PROXY_ORIGIN` fallback을 읽습니다.
quick tunnel 주소는 매번 바뀌므로 repo의 `wrangler.toml`에는 넣지 않습니다.

## 재배포

UI 또는 Pages Functions 파일을 수정한 뒤:

```bash
./scripts/start_cloudflare_preview.sh
```

배포 후 주로 확인할 주소:

```text
https://master.mini-ats-trading-console.pages.dev
https://master.mini-ats-trading-console.pages.dev/api/health
```

`/api/health`가 다음처럼 나오면 연결이 정상입니다.

```json
{"bridge":"OK","engineConnected":true}
```

## C++ 또는 bridge 수정 시

C++ 소스 수정 후:

```bash
cmake -S . -B build
cmake --build build
```

그 다음 `web/bridge.py` 터미널을 `Ctrl+C`로 종료하고 다시 실행합니다.

```bash
python3 web/bridge.py --host 127.0.0.1 --port 8080 --start-engine
```

`web/bridge.py`만 수정한 경우에는 빌드 없이 bridge만 재시작하면 됩니다.

## 종료

터미널 1, 터미널 2에서 각각 `Ctrl+C`를 누릅니다.

프로세스가 남았는지 확인:

```bash
pgrep -af 'web/bridge.py|mini_ats'
pgrep -af cloudflared
```

필요하면 종료:

```bash
pkill -f 'python3 web/bridge.py'
pkill -f 'build/mini_ats --tcp'
pkill -f 'cloudflared tunnel --url http://127.0.0.1:8080'
```

## 자주 나는 문제

`ATS_BRIDGE_ORIGIN is not configured`

- Pages Functions는 배포됐지만 environment secret이 없는 상태입니다.
- `master...pages.dev`는 preview 환경이므로 `--env preview`에 secret을 넣어야 합니다.

`Cloudflare Tunnel error 1033`

- `cloudflared`가 꺼졌거나 quick tunnel 주소가 더 이상 유효하지 않습니다.
- tunnel을 다시 켜고 새 주소를 `ATS_BRIDGE_ORIGIN` preview secret에 넣습니다.

`Gateway Offline`

- Cloudflare Pages와 tunnel은 살아 있지만 local bridge 또는 C++ engine 연결이 안 된 상태입니다.
- `python3 web/bridge.py --host 127.0.0.1 --port 8080 --start-engine`을 확인합니다.

`mini-ats-trading-console.pages.dev`에 Nothing is here yet

- production deployment가 없거나 production alias를 쓰고 있는 상태입니다.
- 현재 운영 확인은 `https://master.mini-ats-trading-console.pages.dev`를 기준으로 합니다.

## 현재 UI 범위

- Order Ticket: limit/market 주문 제출, cancel
- Order Book: best bid/ask, bid/ask pressure, resting buy/sell 주문 현황
- Session Stats: command, accept/reject, trades, quantity, notional, VWAP
- Trade Tape: 체결 로그 scroll view
- Open Orders: 취소 가능한 resting order scroll view
- Gateway Stream: gateway response scroll view

Raw command 입력은 console UI에서 제거했습니다.
