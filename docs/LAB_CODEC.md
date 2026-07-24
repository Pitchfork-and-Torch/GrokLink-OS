# GrokLink Lab Codec (GLK1) — Educational Owned-Lab Protocol

**Version:** GrokLink OS 3.6+  
**Protocol id:** `groklink.lab_beacon.v1`  
**Scope:** Encode/decode **only** GrokLink lab beacons for **owned / authorized** equipment.

## What this is

A **fixed educational frame format** so students and multi-LLM agents can:

1. Encode a known lab message + plain counter  
2. Decode it from hex or didactic OOK edges  
3. Observe that **identical frames replay** (same counter → same bytes)  
4. Connect that failure mode to **why commercial systems use rolling codes**

## What this is not

| Capability | Status |
|------------|--------|
| Third-party remote decode | **Not supported** |
| Rolling-code / hopping-code prediction | **Not implemented** |
| Vehicle / garage / access-control clone | **Forbidden** |
| Brand protocol libraries | **Out of scope** |
| Automatic TX | **Forbidden** — human confirm only |

## Frame layout

| Field | Size | Notes |
|-------|------|--------|
| magic | 4 | `GLK1` (`47 4c 4b 31`) |
| version | 1 | `1` |
| flags | 1 | bit0 = educational demo |
| lab_id | 2 LE | Operator-chosen 0…65535 |
| counter | 4 LE | **Plain** counter (replayable) |
| msg_len | 1 | 0…32 |
| message | N | UTF-8 lab text |
| crc16 | 2 LE | CRC-16/CCITT-FALSE over prior bytes |

## Host CLI

```powershell
groklink-os lab-beacon-encode --lab-id 1 --counter 3 --message HELLO
groklink-os lab-beacon-decode --hex <hex>
groklink-os lab-replay-demo --counter 7
groklink-os explain-rolling-codes
groklink-os lab-beacon-tx-plan --freq 433920000
```

## LLM tools

- `lab_beacon_encode` / `lab_beacon_decode`  
- `lab_replay_demo`  
- `explain_rolling_codes`  
- `analyze_edge_timing` (stats only, no protocol ID)

## TX policy

`lab-beacon-tx-plan` **never transmits**. Real RF TX remains:

1. `edu_ack`  
2. `confirm_issue --action subghz_tx`  
3. Human-supplied confirm token  
4. Device `subghz_tx` path as supported by the build  

Firmware may only support carrier TX on some builds; **host encode/decode works fully offline**.

## Python

```python
from groklink_os.lab_codec import LabBeacon, encode_beacon, decode_beacon_bytes

raw = encode_beacon(LabBeacon(lab_id=1, counter=1, message="LAB"))
print(decode_beacon_bytes(raw)["narrative"])
```
