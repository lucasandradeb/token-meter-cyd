#!/usr/bin/env python3
"""Daemon do Token Meter (macOS).

Le o uso da conta Claude Code e transmite para o dispositivo ESP32 por BLE.

Fluxo:
  1. Le o token OAuth do Keychain (servico "Claude Code-credentials").
  2. Faz uma chamada minima a api.anthropic.com/v1/messages (max_tokens=1).
  3. Extrai o uso dos headers de rate-limit da resposta.
  4. Escreve {"session":NN,"weekly":NN} na caracteristica BLE do dispositivo.

O token NUNCA sai da maquina nem e registrado em log.

Inspirado no projeto Clawdmeter (https://github.com/HermannBjorgvin/Clawdmeter),
do qual vem a tecnica de ler o uso pelos headers de rate-limit. Codigo proprio.
"""
from __future__ import annotations

import asyncio
import getpass
import json
import re
import subprocess
import sys
import time
from pathlib import Path

import httpx
from bleak import BleakClient, BleakScanner

# ---- Configuracao ---------------------------------------------------------
KEYCHAIN_SERVICE = "Claude Code-credentials"
API_URL = "https://api.anthropic.com/v1/messages"
API_HEADERS = {
    "anthropic-version": "2023-06-01",
    "anthropic-beta": "oauth-2025-04-20",
    "Content-Type": "application/json",
    "User-Agent": "claude-code/2.1.5",
}
API_BODY = {
    "model": "claude-haiku-4-5-20251001",
    "max_tokens": 1,
    "messages": [{"role": "user", "content": "hi"}],
}

DEVICE_NAME = "TokenMeter"
CHR_UUID = "c0de0002-feed-4d61-9a11-0123456789ab"
POLL_INTERVAL = 30.0   # segundos entre leituras


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


# ---- Token (Keychain) -----------------------------------------------------
def _extract_access_token(blob: str) -> str | None:
    """Extrai o accessToken de um blob de credenciais (JSON direto, aninhado
    em claudeAiOauth, ou via regex de fallback)."""
    blob = blob.strip()
    try:
        data = json.loads(blob)
        if isinstance(data, dict):
            if isinstance(data.get("accessToken"), str):
                return data["accessToken"]
            for v in data.values():
                if isinstance(v, dict) and isinstance(v.get("accessToken"), str):
                    return v["accessToken"]
    except json.JSONDecodeError:
        pass
    m = re.search(r'"accessToken"\s*:\s*"([^"]+)"', blob)
    return m.group(1) if m else None


def _decode_keychain_blob(raw: str) -> str:
    """O `security ... -w` pode devolver o segredo como hex dump; decodifica de
    volta para texto quando for o caso (JSON nunca e hex valido)."""
    raw = raw.strip()
    if raw and len(raw) % 2 == 0 and re.fullmatch(r"[0-9a-fA-F]+", raw):
        try:
            return bytes.fromhex(raw).decode("utf-8")
        except (ValueError, UnicodeDecodeError):
            return raw
    return raw


def _read_token_file() -> str | None:
    """Le o token do arquivo de credenciais (Linux/Raspberry Pi e afins)."""
    cred = Path.home() / ".claude" / ".credentials.json"
    if not cred.exists():
        return None
    try:
        return _extract_access_token(cred.read_text())
    except OSError as e:
        log(f"Erro lendo {cred}: {e}")
        return None


def _read_token_keychain() -> str | None:
    """Le o token OAuth do Keychain do macOS, ou None."""
    try:
        out = subprocess.run(
            ["security", "find-generic-password", "-s", KEYCHAIN_SERVICE,
             "-a", getpass.getuser(), "-w"],
            check=True, capture_output=True, text=True, timeout=10,
        )
    except subprocess.CalledProcessError as e:
        log(f"Falha ao ler o Keychain (rc={e.returncode}). "
            f"Faca login no Claude Code primeiro.")
        return None
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        log(f"Erro de acesso ao Keychain: {e}")
        return None
    return _extract_access_token(_decode_keychain_blob(out.stdout))


def read_token() -> str | None:
    """Le o token OAuth do Claude Code, ou None.

    Ordem: arquivo ~/.claude/.credentials.json (Linux/Pi) e, se ausente, o
    Keychain do macOS. Assim o mesmo daemon roda no Mac ou num host Linux
    sempre-ligado."""
    tok = _read_token_file()
    if tok:
        return tok
    if sys.platform == "darwin":
        return _read_token_keychain()
    log("Sem credenciais: rode 'claude login' nesta maquina.")
    return None


# ---- API ------------------------------------------------------------------
def _pct(value: str | None) -> int | None:
    # O header de utilizacao vem como fracao 0..1 (ex: 0.51 = 51%).
    if not value:
        return None
    try:
        return max(0, min(100, round(float(value) * 100)))
    except ValueError:
        return None


def _reset_secs(value: str | None) -> int:
    """Header de reset e um epoch (segundos). Retorna segundos ate o reset."""
    if not value:
        return 0
    try:
        return max(0, int(float(value) - time.time()))
    except ValueError:
        return 0


async def poll_usage(token: str) -> dict | None:
    """Retorna {session, weekly, s_reset, w_reset} ou None se token expirou/erro.
    *_reset em segundos ate o proximo reset da janela."""
    headers = dict(API_HEADERS)
    headers["Authorization"] = f"Bearer {token}"
    try:
        async with httpx.AsyncClient(timeout=15) as http:
            resp = await http.post(API_URL, headers=headers, json=API_BODY)
    except httpx.HTTPError as e:
        log(f"Erro de rede na API: {e}")
        return None

    if resp.status_code in (401, 403):
        log("Token expirado/invalido. Rode um comando no Claude Code para "
            "renovar e tente de novo.")
        return None

    h = resp.headers
    session = _pct(h.get("anthropic-ratelimit-unified-5h-utilization"))
    weekly = _pct(h.get("anthropic-ratelimit-unified-7d-utilization"))
    # Contas em overage usam um unico modelo de limite.
    if session is None:
        session = _pct(h.get("anthropic-ratelimit-unified-overage-utilization"))
    if session is None and weekly is None:
        log(f"Sem headers de uso na resposta (HTTP {resp.status_code}).")
        return None
    return {
        "session": session or 0,
        "weekly": weekly or 0,
        "s_reset": _reset_secs(h.get("anthropic-ratelimit-unified-5h-reset")),
        "w_reset": _reset_secs(h.get("anthropic-ratelimit-unified-7d-reset")),
    }


# ---- BLE ------------------------------------------------------------------
async def find_device():
    log(f"Procurando '{DEVICE_NAME}' por BLE...")
    dev = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=15.0)
    if not dev:
        log("Dispositivo nao encontrado. Ligue a placa e verifique o BLE.")
    return dev


async def run() -> None:
    while True:
        dev = await find_device()
        if not dev:
            await asyncio.sleep(5)
            continue
        try:
            async with BleakClient(dev) as client:
                log(f"Conectado ao {DEVICE_NAME}.")
                while client.is_connected:
                    token = read_token()
                    if not token:
                        await asyncio.sleep(POLL_INTERVAL)
                        continue
                    usage = await poll_usage(token)
                    if usage:
                        payload = json.dumps(usage).encode()
                        await client.write_gatt_char(CHR_UUID, payload,
                                                     response=False)
                        log(f"Enviado: sessao={usage['session']}% "
                            f"semana={usage['weekly']}% "
                            f"(reset {usage['s_reset']}s/{usage['w_reset']}s)")
                    await asyncio.sleep(POLL_INTERVAL)
        except Exception as e:  # reconecta em qualquer falha de BLE
            log(f"Conexao BLE perdida ({e}); tentando de novo...")
            await asyncio.sleep(3)


if __name__ == "__main__":
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        log("Encerrado.")
        sys.exit(0)
