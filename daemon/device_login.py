#!/usr/bin/env python3
"""Login OAuth DEDICADO ao dispositivo (sessao independente do seu Claude Code).

Faz o fluxo PKCE do Claude Code para obter um par access/refresh token proprio
da placa. Como e uma autorizacao separada, renovar esse token na ESP32 NAO
afeta o login do Claude Code no seu Mac.

Uso:
    python device_login.py

Abre o navegador, voce aprova, cola o codigo de volta. Salva os tokens em
.device_credentials.json (gitignored) para o provisionamento da placa.
"""
from __future__ import annotations

import base64
import hashlib
import json
import secrets
import webbrowser
from pathlib import Path

import httpx

CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
AUTH_URL = "https://claude.ai/oauth/authorize"
TOKEN_URL = "https://console.anthropic.com/v1/oauth/token"
REDIRECT_URI = "https://console.anthropic.com/oauth/code/callback"
SCOPES = "user:inference user:profile"
OUT = Path(__file__).parent / ".device_credentials.json"


def _pkce() -> tuple[str, str]:
    verifier = base64.urlsafe_b64encode(secrets.token_bytes(32)).rstrip(b"=").decode()
    challenge = base64.urlsafe_b64encode(
        hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
    return verifier, challenge


def main() -> None:
    verifier, challenge = _pkce()
    state = secrets.token_urlsafe(16)
    params = {
        "response_type": "code",
        "client_id": CLIENT_ID,
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
        "state": state,
    }
    url = AUTH_URL + "?" + "&".join(f"{k}={httpx.QueryParams({k: v})[k]}"
                                    for k, v in params.items())
    print("\n1) Abrindo o navegador para autorizar a PLACA (sessao separada).")
    print("   Se nao abrir, copie e cole esta URL:\n")
    print("  ", url, "\n")
    try:
        webbrowser.open(url)
    except Exception:
        pass
    print("2) Aprove o acesso. Voce sera levado a uma pagina com um CODIGO.")
    code_in = input("3) Cole o codigo aqui e Enter: ").strip()

    # O codigo pode vir como "code#state".
    code = code_in.split("#")[0]
    returned_state = code_in.split("#")[1] if "#" in code_in else state

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
                   headers={"Content-Type": "application/json"})
    if r.status_code != 200:
        print(f"\nFALHOU na troca de token: HTTP {r.status_code}\n{r.text[:400]}")
        return
    tok = r.json()
    access = tok.get("access_token")
    refresh = tok.get("refresh_token")
    expires_in = tok.get("expires_in")
    if not access or not refresh:
        print("\nResposta sem access/refresh token:", tok)
        return

    OUT.write_text(json.dumps({
        "access_token": access,
        "refresh_token": refresh,
        "expires_in": expires_in,
    }, indent=2))
    print(f"\nOK! Tokens do dispositivo salvos em {OUT.name} "
          f"(access len={len(access)}, refresh len={len(refresh)}, "
          f"expira_em={expires_in}s).")

    # Valida chamando a API e lendo os headers de uso.
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
    main()
