# MedSec pilot in a box (15 minutes)

**NOT A MEDICAL DEVICE.** Written RoE required before any on-prem work.

## Hardware

| Item | Notes |
|------|--------|
| GrokLink OS device (WB55 / Flipper-class board) | USB CDC |
| USB cable | One serial owner |
| PC with Python 3.10+ | bridge `pip install -e ".[serial]"` |
| Optional Faraday cage | Required before any TX (default MedSec = no TX) |

## Pre-flight checklist

- [ ] Written authorization / RoE for the lab space  
- [ ] No PHI will be stored on device or in case titles  
- [ ] Profile = `medsec-strict` for hospital-adjacent posture  
- [ ] Operators trained: default-deny TX, edu-ack phrase  

## Demo script

1. **Connect** USB; close competing serial tools.  
2. `groklink-os ping` → `groklink-os edu-ack` → `groklink-os status`  
3. Confirm status shows research posture / `not_medical_device` when available.  
4. `groklink-os lab medsec-demo` (or `mission-run --id medsec_lab_passive_ism`)  
5. `groklink-os lab engagement-init --operator … --engagement … --roe-ack`  
6. `groklink-os lab casefile --dir cases/… --title … --hypothesis …`  
7. Optional unplug: `prepare-unplugged --id medsec_passive_watch` then later `plug-sync`  

## Success criteria

- Passive mission completes without TX  
- Casefile + engagement files exist with disclaimers  
- No clinical claims made in notes  

## Explicitly out of scope

- Live ward TX / spoof / jam  
- Patient monitoring or care decisions  
- Implant or medication-tag write/emulate  
