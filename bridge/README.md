# GrokLink OS PC Bridge

Python package for **GrokLink OS 3.2** (native RTOS) — JSON RPC, multi-LLM signal observability, skill craft.

```powershell
cd bridge
py -3 -m pip install -e ".[dev]"
# Optional USB CDC support for real hardware:
# py -3 -m pip install -e ".[serial]"

# Host OS sim (TCP):
$env:GLK_RPC_HOST = "127.0.0.1"
$env:GLK_RPC_PORT = "7341"
groklink-os ping

# Or real device (USB CDC 230400):
# $env:GLK_SERIAL_PORT = "COM5"
# groklink-os list-ports
# groklink-os ping

groklink-os edu-ack
groklink-os status
groklink-os rx --freq 433920000 --ms 500
groklink-os observe-session --freqs 433920000
```

## Signal observability (LLM tools)

```powershell
groklink-os tools-schema
groklink-os observe-rx --freq 433920000 --ms 400
groklink-os observe-spectrum
groklink-os observe-serve --port 8741
groklink-os tool-call observe_rx --args "{\"freq_hz\":433920000,\"ms\":200}"
```

Python:

```python
from groklink_os.observe.tools import ToolDispatcher, tools_openai_format

tools = tools_openai_format()  # OpenAI Chat Completions `tools=`
d = ToolDispatcher()
print(d.dispatch("observe_rx", {"freq_hz": 433920000, "ms": 400})["result"]["narrative"])
d.close()
```

Docs: [../docs/SIGNAL_OBSERVABILITY.md](../docs/SIGNAL_OBSERVABILITY.md).

Tests:

```powershell
py -3 -m pytest tests -q
```

Legal: authorized research only. TX requires confirm tokens. Observation paths are passive-only.
