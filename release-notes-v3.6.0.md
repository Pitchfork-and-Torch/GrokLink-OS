# GrokLink OS v3.6.0

From-scratch research RTOS for multi-radio portable hardware with gated agent, skills, on-device GUI, and PC bridge.

## Install

```powershell
git clone https://github.com/Pitchfork-and-Torch/GrokLink-OS.git
cd GrokLink-OS/bridge
py -3 -m pip install -e ".[serial]"
```

DFU flash (device in STM32 DFU mode, VID 0483 PID DF11):

```powershell
# Use release asset GrokLink-OS-v3.6.0-radio.dfu
# From a clone with tools/flash_os_dfu_only.ps1 after placing the DFU under dist/dfu/
```

MIT License.
