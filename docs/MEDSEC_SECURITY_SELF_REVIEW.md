# MedSec security self-review checklist (policy engine)

**Date:** ________  
**Reviewer:** ________  
**Build / version:** ________  

This is an **internal** checklist pending independent third-party review (H1.6).

## Policy / safety

| # | Check | Pass? | Notes |
|---|-------|-------|-------|
| 1 | Default-deny TX without edu-ack | | |
| 2 | Default-deny TX without confirm | | |
| 3 | Confirm tokens single-use + TTL | | |
| 4 | medsec-strict blocks TX even with confirm | | |
| 5 | Blacklist fail-closed for TX when corrupt/missing (as designed) | | |
| 6 | No RPC wipe of blacklist | | |
| 7 | LLM tool allowlist rejects non-passive mission ids | | |
| 8 | Offline agent only allowlisted passive missions | | |
| 9 | Audit path records denies for elevated actions | | |
| 10 | Status exposes not_medical_device / profile | | |

## MedSec data path

| # | Check | Pass? | Notes |
|---|-------|-------|-------|
| 11 | Engagement requires --roe-ack | | |
| 12 | PHI-like patterns rejected in engagement/casefile fields | | |
| 13 | Casefile carries disclaimer + not_medical_device | | |
| 14 | Anomaly export sets never_auto_tx | | |
| 15 | Research export states clinical_use=false | | |
| 16 | Vault seal encrypts exports at rest (PC) | | |

## Residual risks (accept / track)

- Physical debug can dump RAM vault  
- Host sim is not RF security boundary  
- Unsigned skills in dev trust operator FS  
- Third-party review still open  

## Sign-off

Internal reviewer: ________________ Date: ________  
Next external review target date: ________  
