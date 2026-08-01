# Windows recognition and qFlipper with GrokLink OS

## 1. Windows identity (v3.7.0+)

| Layer | Stock Flipper (typical) | GrokLink OS |
|-------|-------------------------|-------------|
| USB VID | `0483` (ST) | `0483` (ST) |
| USB PID (app CDC) | `5740` | **`5740`** (max `usbser` compatibility) |
| Product string | Flipper branding | **GrokLink OS** |
| Manufacturer | Flipper / ST | Pitchfork-and-Torch |
| Protocol | Flipper protobuf | **GrokRPC JSON** @ 230400 |
| DFU | `0483:DF11` | Same ROM ST DFU |

List ports:

```powershell
py -3 -c "from serial.tools import list_ports
for p in list_ports.comports():
  print(p.device, hex(p.vid or 0), hex(p.pid or 0), p.description)"
```

## 2. USB stability (v3.7.0)

Verified on hardware after isolation bisect:

- USB-first boot (CDC-parity)
- Light RPC answered in bulk RX callback
- Service init with continuous USB poll
- Field SPI deferred while host has DTR (COM session stays primary)

## 3. qFlipper usage

| Action | Works? |
|--------|--------|
| Flash `.dfu` in DFU mode | **Yes** |
| Manage apps / protobuf RPC while GrokLink runs | **No** |
| Official update channel | **No** |

```powershell
.\tools\flash_os_dfu_only.ps1
```

## 4. Recover Flipper firmware

```powershell
.\tools\recover_flipper.ps1
```

Authorized research only.
