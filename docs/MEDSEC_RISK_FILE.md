# MedSec instrument — lightweight risk file (ISO 14971-inspired)

**Intended use:** GrokLink OS as an **authorized RF research and facility MedSec
instrument** (passive survey, training, lab assessment under written RoE).

**Not intended use:** diagnosis, treatment, monitoring of care, closed-loop control,
alarms, EHR integration for care, patient-connected use.

**Product class:** Research / IT-security tool — **not** a medical device (FDA/MDR SaMD/SiMD).

## Hazardous situations (selected)

| ID | Hazard | Foreseeable sequence | Harm | Risk control |
|----|--------|----------------------|------|--------------|
| H1 | TX interferes with clinical wireless | Operator TX on ward | Device malfunction / delay | Default-deny TX; medsec-strict forbids TX; RoE; Faraday for lab TX only |
| H2 | Spoof/jamming of clinical links | Misuse of firmware | Patient harm via device | No spoof tools; policy; legal ban; refuse out-of-scope asks |
| H3 | PHI leakage | Operator stores patient IDs on SD | Privacy breach | No PHI schema; engagement labels only; vault process |
| H4 | False “threat” score drives action | Anomaly auto-TX | Interference | Anomaly PC-only; never auto-TX |
| H5 | Marketed as medical device | Mislabeling | Regulatory + care misuse | Hard disclaimers GUI/RPC/docs |
| H6 | Care decision from pulse edges | Misinterpretation | Clinical error | Explicit non-clinical claims; education |

## Residual risk statement

With controls above, residual risk for **authorized lab MedSec use** is acceptable for
research/education. Residual risk for **clinical care use** is **unacceptable** — that
use is prohibited.

## Review

- Review when intended use changes.  
- Any clinical product would require a **separate** product line, hardware, and regulatory process.
