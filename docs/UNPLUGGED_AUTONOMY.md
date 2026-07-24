# Unplugged autonomy — what works without USB

**GrokLink OS 3.6+**

## Short answer

| Who | Unplugged USB? |
|-----|----------------|
| **PC / LLM Grok** (bridge, observe tools, chat) | **No** — needs a link (USB CDC today; host-sim is PC-only) |
| **On-device GrokAgent** (ROM missions, light RX, vault) | **Yes** — after field arm; passive only, no TX |

Unplugging the cable does **not** stop the RTOS main loop. If the offline explorer is armed, the device keeps taking passive RX steps (~600 ms cadence) and logging to the **RAM vault**.

---

## Two agents, one device

```
┌─────────────────────┐         USB CDC          ┌──────────────────────┐
│  PC LLM ("Grok")    │◄───────────────────────►│  GrokLink OS         │
│  tools / CLI / skill│     only while plugged   │  policy · radio      │
└─────────────────────┘                          │  GrokAgent missions  │
                                                 │  RAM vault · GUI     │
         no link ────────────────────────────────►│  offline ticks ──┐   │
                                                 └──────────────────│───┘
                                                                   │
                                              unplugged: agent still│ticks
                                              passive RX only       ▼
```

---

## How to enable unplugged explore

### A — From PC (before unplug)

```powershell
groklink-os prepare-unplugged --id lab_passive_watch
# Then unplug USB. Device continues passive missions.
# Later: plug in → groklink-os vault-tail / agent-status
```

Tool: `prepare_unplugged_explore` (edu_ack + agent_auto + offline + arm).

### B — From device GUI (no PC)

1. Navigate to **SAFETY** page (Left/Right).  
2. **Hold OK ~2 seconds**.  
3. Device field-edu-acks, arms `lab_passive_watch`, enables offline.  
4. Display shows **FIELD ACTIVE** / mission id.  
5. USB may stay unplugged; RADIO page shows last RX / agent line.

### C — Permanent field research unit (default on personal builds)

`glk_config.h` for this product line:

```c
#define GLK_BOOT_FIELD_EXPLORE 1
#define GLK_FIELD_EXPLORE_STICKY 1
```

- Power-on: edu (passive) + all ROM passive missions autonomous + offline on  
- Sticky: offline stays armed even if host tries to disable  
- Still **no auto-TX** · no third-party decode · no rolling-code prediction  

Firmware **v3.6.1** ships this as the personal field-research profile.

---

## Safety (unchanged)

- **Passive RX only** on autonomous path  
- **No agent TX** without human confirm (policy still denies)  
- Field edu ack is for **owned/authorized** research; still audited  
- Vault is **RAM** — lost on power cycle until SD persistence lands  

---

## What PC Grok cannot do unplugged

- `observe_rx` / multi-LLM tools  
- Live packaging / schema narratives to the chat  
- Lab beacon encode (host-only) while disconnected  

**Workflow:** arm field explore → unplug → device listens → **replug → `groklink-os plug-sync`** (auto research journal) → optional live `observe_rx` → LLM reads lessons.

See [PLUG_SYNC_RESEARCH.md](PLUG_SYNC_RESEARCH.md).

---

## Related

- ROM mission `lab_passive_watch`  
- Tools: `start_offline_agent`, `prepare_unplugged_explore`, `get_vault_tail`  
- Field report: [docs/lab/FIELD_REPORT_v3.6.0.md](lab/FIELD_REPORT_v3.6.0.md)  
