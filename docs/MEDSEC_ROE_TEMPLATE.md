# Rules of Engagement (RoE) template — GrokLink MedSec

**NOT A MEDICAL DEVICE.** This template is for **authorized RF research / facility MedSec**
work only. It is not legal advice. Have institutional counsel review before use.

---

## 1. Parties

| Role | Name / org | Contact |
|------|------------|---------|
| Sponsor / client | | |
| Operator org | | |
| Lead operator | | |
| Second operator (dual control) | | |
| Site owner / clinical engineering | | |
| Cybersecurity sponsor | | |

**Engagement ID:** `ENG-________`  
**Dates:** from __________ to __________  
**Sites / spaces in scope:** (list **non-clinical** preferred; no patient rooms unless explicitly authorized)

---

## 2. Intended use (locked)

In-scope:

- [ ] Passive SubGHz observation / occupancy baselining  
- [ ] Lab Faraday TX only (if checked, complete §5)  
- [ ] Owned-tag NFC UID **read** study  
- [ ] Training / curriculum exercises  
- [ ] Evidence export (casefile, audit stamp) for sponsor  

Out of scope (always):

- Diagnosis, treatment, care monitoring, closed-loop control  
- Patient-connected use or EHR clinical integration  
- Jamming, spoofing, or TX on live wards without dual written approval  
- Write/emulate on in-use medication or implant-related tags  
- Collection or storage of PHI / patient identifiers  

---

## 3. Authorization

I confirm:

1. The sponsor authorizes this engagement under applicable law and policy.  
2. Operators will use only listed spaces and assets.  
3. GrokLink will run **`medsec-strict`** (TX forbidden) unless §5 is fully completed.  
4. No PHI will be entered into case titles, notes, engagement fields, or device storage.  
5. Unexpected clinical device behavior → **stop**, document, escalate to site owner.  

| Role | Signature | Date |
|------|-----------|------|
| Sponsor | | |
| Lead operator | | |
| Site owner | | |

---

## 4. Technical controls (required)

| Control | Required setting |
|---------|------------------|
| Profile | `medsec-strict` (default for this engagement) |
| Edu phrase | `I_WILL_USE_ONLY_AUTHORIZED_TARGETS` before elevated ops |
| Autonomous TX | Forbidden |
| LLM tools | Passive allowlisted missions only |
| Exports | Casefile + stamped audit; redacted before any public share |
| Serial owner | Single host process owns CDC |

---

## 5. TX exception (optional — Faraday only)

Only complete if TX is required for **owned lab gear** in a controlled chamber.

| Item | Value |
|------|--------|
| Cage / chamber ID | |
| Dual operators present | Yes / No |
| Profile temporarily | `default` (then restore `medsec-strict`) |
| Confirm tokens used | Yes |
| Target assets (owned only) | |
| Frequencies (Hz) | |
| Max TX duration / duty | |

Signatures for TX exception: ________________ / ________________ Date: ________

---

## 6. Data handling

| Data | Handling |
|------|----------|
| Casefiles | Private vault; retention ______ days |
| Raw captures | No PHI; hash in CASEFILE |
| Public release | Redacted examples only |
| PHI discovery | Stop; purge path; notify sponsor |

---

## 7. Stop conditions

- Clinical interference suspicion  
- Audit integrity failure  
- RoE scope violation request  
- Device lost / stolen  

---

## 8. Acknowledgement

Operator ID (lab label only): ______________  
`groklink-os lab engagement-init --operator … --engagement ENG-… --roe-ack`

**Disclaimer:** GrokLink OS is not a medical device and is not for clinical care.
