#!/usr/bin/env python3
"""Login OAuth DEDICADO ao dispositivo (sessao independente do seu Claude Code).

Fluxo PKCE do Claude Code em DOIS passos (nao-interativo, funciona via `!`):

    python device_login.py            # passo 1: imprime a URL de autorizacao
    # ... abra a URL, aprove, copie o CODIGO da pagina final ...
    python device_login.py <codigo>   # passo 2: troca o codigo por tokens

Como e uma autorizacao separada, renovar esse token na ESP32 NAO afeta o login
do Claude Code no seu Mac. Tokens salvos em .device_credentials.json (gitignored).
"""
from __future__ import annotations

import base64
import hashlib
import json
import secrets
import sys
import webbrowser
from pathlib import Path
from urllib.parse import urlencode

import httpx

CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
AUTH_URL = "https://claude.ai/oauth/authorize"
TOKEN_URL = "https://console.anthropic.com/v1/oauth/token"
REDIRECT_URI = "https://console.anthropic.com/oauth/code/callback"
SCOPES = "org:create_api_key user:profile user:inference"
HERE = Path(__file__).parent
OUT = HERE / ".device_credentials.json"
STATE_FILE = HERE / ".device_login_state.json"


def _pkce() -> tuple[str, str]:
    verifier = base64.urlsafe_b64encode(secrets.token_bytes(32)).rstrip(b"=").decode()
    challenge = base64.urlsafe_b64encode(
        hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
    return verifier, challenge


def step1() -> None:
    verifier, challenge = _pkce()
    state = secrets.token_urlsafe(16)
    STATE_FILE.write_text(json.dumps({"verifier": verifier, "state": state}))
    params = {
        "code": "true",  # exigido pelo fluxo do Claude Code
        "response_type": "code",
        "client_id": CLIENT_ID,
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
        "state": state,
    }
    url = AUTH_URL + "?" + urlencode(params)
    print("\nPASSO 1 — autorizar a placa (sessao separada do seu Mac).")
    print("\nAbra esta URL no navegador, aprove, e copie o CODIGO da pagina final:\n")
    print("  ", url, "\n")
    print("Depois rode:  python device_login.py <codigo>\n")
    try:
        webbrowser.open(url)
    except Exception:
        pass


def step2(code_in: str) -> None:
    if not STATE_FILE.exists():
        print("Rode o passo 1 primeiro (python device_login.py).")
        return
    st = json.loads(STATE_FILE.read_text())
    verifier = st["verifier"]
    code = code_in.split("#")[0]
    returned_state = code_in.split("#")[1] if "#" in code_in else st["state"]

    body = {
        "grant_type": "authorization_code",
        "code": code,
        "state": returned_state,
        "client_id": CLIENT_ID,
        "redirect_uri": REDIRECT_URI,
        "code_verifier": verifier,
    }
    with httpx.Client(timeout=30) as c:
        r = c.post(TOKEN_URL, json=body,
                   headers={"Content-Type": "application/json",
                            "User-Agent": "anthropic"})
    if r.status_code != 200:
        print(f"FALHOU na troca de token: HTTP {r.status_code}\n{r.text[:400]}")
        return
    tok = r.json()
    access, refresh = tok.get("access_token"), tok.get("refresh_token")
    if not access or not refresh:
        print("Resposta sem access/refresh token:", tok)
        return
    OUT.write_text(json.dumps({
        "access_token": access,
        "refresh_token": refresh,
        "expires_in": tok.get("expires_in"),
    }, indent=2))
    STATE_FILE.unlink(missing_ok=True)
    print(f"OK! Tokens do dispositivo salvos em {OUT.name} "
          f"(access len={len(access)}, refresh len={len(refresh)}, "
          f"expira_em={tok.get('expires_in')}s).")

    with httpx.Client(timeout=20) as c:
        vr = c.post("https://api.anthropic.com/v1/messages",
            headers={"anthropic-version": "2023-06-01",
                     "anthropic-beta": "oauth-2025-04-20",
                     "Content-Type": "application/json",
                     "Authorization": f"Bearer {access}"},
            json={"model": "claude-haiku-4-5-20251001", "max_tokens": 1,
                  "messages": [{"role": "user", "content": "hi"}]})
    print(f"Validacao /v1/messages -> HTTP {vr.status_code}; "
          f"5h={vr.headers.get('anthropic-ratelimit-unified-5h-utilization')} "
          f"7d={vr.headers.get('anthropic-ratelimit-unified-7d-utilization')}")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        step2(sys.argv[1])
    else:
        step1()
