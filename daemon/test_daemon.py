#!/usr/bin/env python3
"""Testes do daemon. Sem dependencia de framework: roda com o python do venv
(que ja tem httpx). Uso:

    python test_daemon.py

Sai com codigo != 0 se algum caso falhar.
"""
from __future__ import annotations

import asyncio

import daemon


# ---- Fakes para poll_usage (evita rede) -----------------------------------
class _FakeResp:
    def __init__(self, status_code: int, headers: dict):
        self.status_code = status_code
        self.headers = headers


class _FakeClient:
    """Substitui httpx.AsyncClient: context manager async que devolve uma
    resposta fixa em .post()."""

    def __init__(self, resp: _FakeResp):
        self._resp = resp

    async def __aenter__(self):
        return self

    async def __aexit__(self, *exc):
        return False

    async def post(self, *args, **kwargs):
        return self._resp


def _patch_httpx(resp: _FakeResp):
    daemon.httpx.AsyncClient = lambda *a, **k: _FakeClient(resp)


# ---- Casos ----------------------------------------------------------------
def test_pct():
    assert daemon._pct("0.51") == 51
    assert daemon._pct("0") == 0
    assert daemon._pct("1") == 100
    assert daemon._pct("1.5") == 100      # clamp
    assert daemon._pct(None) is None
    assert daemon._pct("abc") is None


def test_reset_secs():
    import time
    future = str(time.time() + 3600)
    assert 3590 <= daemon._reset_secs(future) <= 3600
    assert daemon._reset_secs(None) == 0
    assert daemon._reset_secs(str(time.time() - 100)) == 0  # passado -> 0


def test_poll_usage_auth_expired():
    _patch_httpx(_FakeResp(401, {}))
    out = asyncio.run(daemon.poll_usage("tok"))
    assert out is daemon.AUTH_EXPIRED, f"esperava AUTH_EXPIRED, veio {out!r}"

    _patch_httpx(_FakeResp(403, {}))
    out = asyncio.run(daemon.poll_usage("tok"))
    assert out is daemon.AUTH_EXPIRED


def test_poll_usage_ok():
    headers = {
        "anthropic-ratelimit-unified-5h-utilization": "0.36",
        "anthropic-ratelimit-unified-7d-utilization": "0.87",
    }
    _patch_httpx(_FakeResp(200, headers))
    out = asyncio.run(daemon.poll_usage("tok"))
    assert isinstance(out, dict), f"esperava dict, veio {out!r}"
    assert out["session"] == 36
    assert out["weekly"] == 87


def _run_all():
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"  ok   {t.__name__}")
        except AssertionError as e:
            failed += 1
            print(f"  FAIL {t.__name__}: {e}")
    print(f"\n{len(tests) - failed}/{len(tests)} passaram")
    return failed


if __name__ == "__main__":
    import sys
    sys.exit(1 if _run_all() else 0)
