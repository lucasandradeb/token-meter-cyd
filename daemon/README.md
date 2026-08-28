# Daemon — Token Meter

Lê o uso da conta Claude Code (token OAuth do Keychain) e transmite por BLE
para a placa `TokenMeter`.

## Rodar manualmente

```bash
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python daemon.py
```

Ctrl+C encerra.

## Rodar em segundo plano (autostart via launchd)

1. Crie o venv e instale as dependências (acima).
2. Edite `com.tokenmeter.daemon.plist`: troque `__PYTHON__` pelo caminho do
   Python do venv e `__DAEMON__` pelo caminho de `daemon.py`. Ex:
   - `__PYTHON__` → `/Users/voce/personal_projects/esp32/daemon/venv/bin/python`
   - `__DAEMON__` → `/Users/voce/personal_projects/esp32/daemon/daemon.py`
3. Instale:
   ```bash
   cp com.tokenmeter.daemon.plist ~/Library/LaunchAgents/
   launchctl load ~/Library/LaunchAgents/com.tokenmeter.daemon.plist
   ```
4. Logs em `/tmp/tokenmeter.out` e `/tmp/tokenmeter.err`.

Parar / remover:
```bash
launchctl unload ~/Library/LaunchAgents/com.tokenmeter.daemon.plist
```

> Na primeira execução o macOS pode pedir permissão de **Bluetooth**. Conceda em
> Ajustes > Privacidade e Segurança > Bluetooth. Sob launchd, quem precisa da
> permissão é o binário Python do venv.

## Configuração

Ajuste no topo de `daemon.py`:
- `POLL_INTERVAL` — segundos entre leituras (padrão 30).
- `DEVICE_NAME` / `CHR_UUID` — devem casar com o firmware.

## Segurança

O token OAuth é lido do Keychain apenas em memória, usado no header
`Authorization` da chamada à API, e **nunca** é gravado em disco ou em log.
