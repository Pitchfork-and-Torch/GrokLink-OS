# Facility RF baseline playbook (passive only)

**NOT A MEDICAL DEVICE.** Engineering / cybersecurity baselining under written RoE.

## Purpose

Establish a **repeatable passive occupancy baseline** for facility ISM/SRD bands so
later surveys can detect gross changes (heuristic only — not protocol ID, not threat intel).

## Where to measure (preferred order)

1. Faraday lab / RF test room  
2. Engineering / BME workshops  
3. IT / telecom closets  
4. Training / simulation wings (non-clinical)  
5. **Avoid** ICU, OR, patient rooms unless RoE + clinical engineering explicitly require and approve  

## Bands (default)

| Band (Hz) | Notes |
|-----------|--------|
| 315000000 | Common SRD region (region-dependent) |
| 433920000 | Common ISM |
| 868350000 | EU SRD-ish center (confirm local rules) |
| 915000000 | US ISM-ish (confirm local rules) |

Do **not** treat these as complete hospital band plans. Confirm with clinical engineering / spectrum policy.

## Procedure

1. Written RoE + engagement-init (`medsec-strict`).  
2. `groklink-os edu-ack`  
3. Run `fac_rf_snapshot_passive` or `fac_rf_baseline_watch` skill path.  
4. Repeat at same locations / same dwell for N sessions (e.g. 3).  
5. Export history; `lab anomaly` only after a baseline history exists.  
6. Casefile: hypothesis = “baseline occupancy for site X under conditions Y”.  

```powershell
groklink-os lab engagement-init --operator lab-op1 --engagement ENG-BASE-001 --site eng-closet-a --roe-ack
groklink-os mission-run --id fac_rf_snapshot_passive --steps 10
groklink-os lab casefile --dir cases/ENG-BASE-001 --title "Facility passive baseline" `
  --hypothesis "Quiet multi-band occupancy in eng closet A" --freqs 315000000,433920000,868350000
```

## Interpretation rules

- Pulse edges ≠ protocol IDs  
- Hot/quiet flags are **heuristic**  
- Never auto-TX on anomaly  
- Escalate only through human process  

## Red lines

- No TX on hospital floors under this playbook  
- No PHI in filenames or notes  
- No public raw captures of identifiable facilities without redaction + authorization  
