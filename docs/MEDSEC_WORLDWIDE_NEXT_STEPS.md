# GrokLink OS — MedSec worldwide next steps

**Status:** Strategic plan (2026-07-22)  
**Platform:** GrokLink OS (native) + legacy v2 healthcare packs on GrokLink-Firmware  
**Audience:** Operator / product owner (Knock)

> **NOT A MEDICAL DEVICE.** GrokLink must not be marketed for diagnosis, treatment,
> monitoring of care, closed-loop therapy, alarms, or patient-connected use.
> Target role: **authorized medical security (MedSec) research & facility RF instrument**.

---

## 1. The honest product thesis

### What “MedSec device used in hospitals worldwide” can mean

| Interpretation | Feasible on Flipper/WB55? | Path |
|----------------|---------------------------|------|
| **Clinical care device** (patient monitoring, therapy, EHR bridge) | **No** | Abandon on this silicon; Phase 3 medical-grade hardware only if ever |
| **Hospital security / BME instrument** (authorized RF survey, training, lab assessment) | **Yes, with process** | Primary path |
| **OEM / vendor security lab tool** (device makers test wireless surfaces) | **Yes** | Parallel B2B path |
| **Academic cyber-physical curriculum** | **Yes** | Fast credibility path |

**Worldwide hospital adoption** for interpretation #2 requires:

1. **Trust architecture** already started (policy, audit, HITL) — double down  
2. **Procurement language** (not a gadget: engagement tooling + evidence chain)  
3. **Institutional process** (RoE, dual control, no PHI on device)  
4. **Eventually: non-Flipper form factor** for anything that leaves the Faraday cage or touches production floors  

GrokLink’s differentiator is **safety-by-construction + LLM-assisted observation + exportable audit**, not “another Flipper firmware.”

### What already exists (do not rebuild)

From **GrokLink-Firmware** Phase 1–2 (`docs/HEALTHCARE_MEDSEC_PLAN.md`):

| Asset | Location |
|-------|----------|
| Passive skills | `medsec_passive_ism_watch`, `fac_rf_baseline_watch`, `med_asset_uid_inventory` |
| Passive missions | `medsec_lab_passive_ism`, `fac_rf_snapshot_passive` |
| TX-deny blacklist templates | `sd_card/groklink/healthcare/blacklist/*` |
| Operator runbook | `HEALTHCARE_OPERATOR_RUNBOOK.md` |
| Engagement / casefile / anomaly / research FHIR export | PC `lab *` CLI (v2.1.2+) |

**GrokLink OS 3.7** strengths to leverage:

- USB-stable GrokRPC, observe tools, Signal Cognition  
- Policy engine, confirm tokens, default-deny TX  
- Passive mission catalog + RAM vault + plug-sync  
- PC bridge observation narrative (LLM-ready)  

**Gap:** Healthcare packs and engagement CLI are **v2-first**; OS native needs a **port + productization** pass.

---

## 2. Positioning (market language)

**One-liner (public):**  
*GrokLink is a policy-gated RF research instrument for authorized MedSec labs and clinical engineering training — not a medical device.*

**Buyers (worldwide):**

1. Hospital **clinical engineering / HTM** RF awareness programs  
2. Hospital / health-system **cybersecurity** red/purple teams (written RoE)  
3. Medical device **manufacturers** (pre-market wireless security labs)  
4. **Universities** teaching healthcare cyber-physical security  
5. Specialized **MedSec consultancies**  

**Non-buyers / anti-positioning:**

- Bedside care, implant workflows, medication cabinet attacks, jamming, “ward TX”  
- Anything that sounds like pirate Flipper “hospital hacks”

---

## 3. Capability roadmap (what to build next)

### Horizon 0 — Now (1–2 weeks): OS port of Phase 1 + hard disclaimers

Goal: **one install path on GrokLink OS** that a lab can run without v2 overlay.

| # | Action | Done when |
|---|--------|-----------|
| H0.1 | Port healthcare skill/mission JSON into `groklink-os` ROM/SD catalog (`medsec_*`, `fac_*`) as **passive-only** | **Done** — ROM + `sd_card/groklink/` |
| H0.2 | Port healthcare TX-deny blacklist templates into OS policy defaults (opt-in profile `medsec-strict`) | **Done** — `config/profile.json` + policy forbid TX |
| H0.3 | Write `docs/HEALTHCARE_OPERATOR_RUNBOOK.md` for **OS** | **Done** |
| H0.4 | Add MedSec mission allowlist to LLM tools (`run_passive_mission` only) | **Done** — bridge tools allowlist |
| H0.5 | Ship `NOT A MEDICAL DEVICE` on GUI, RPC `status`, bridge, README | **Done** |

**Success metric:** Lab demo in 15 minutes: plug → edu-ack → passive ISM mission → vault-tail → plug-sync → case notes.  
**Command:** `groklink-os lab medsec-demo`

### Horizon 1 — Credibility (1–2 months): Evidence product

Goal: something a CISO / clinical engineer can **file**, not just play with.

| # | Action | Done when |
|---|--------|-----------|
| H1.1 | Port v2 **engagement** model to OS bridge: engagement-id, operator-id, RoE ack, audit stamp | **Done** — `groklink-os lab engagement-*` |
| H1.2 | **Casefile export** (Markdown + JSON): hypothesis, bands, narrative, capture hash | **Done** — `lab casefile` |
| H1.3 | **Anomaly vs baseline** (PC-side only; never auto-TX) using observation history | **Done** — `lab anomaly` |
| H1.4 | PHI hygiene + PC vault seal for case exports | **Done** — `lab phi-check`, reject on engagement/casefile; `lab vault-seal` |
| H1.5 | **Threat model + ISO 14971-style risk file** (lightweight) for MedSec instrument intended use | **Done** — `docs/MEDSEC_RISK_FILE.md` |
| H1.6 | Independent **security review** of policy engine (confirm tokens, blacklist, audit chain) | **Internal checklist done** — `docs/MEDSEC_SECURITY_SELF_REVIEW.md`; external still open |

**Success metric:** First **paid or MoU pilot** with a university hospital lab or MedSec consultancy under written RoE.

### Horizon 2 — Hospital-floor-adjacent (3–6 months): Process + partners

Goal: enter hospitals as **authorized security instrumentation**, not care devices.

| # | Action | Done when |
|---|--------|-----------|
| H2.1 | **Facility RF baseline playbook** (passive only): engineering closets, training wings — never ICU TX | **Done** — `docs/FACILITY_RF_BASELINE_PLAYBOOK.md` |
| H2.2 | Hospital **RoE template** + dual-control checklist (legal review in 1–2 jurisdictions) | **Template done** — `docs/MEDSEC_ROE_TEMPLATE.md` (counsel review still open) |
| H2.3 | **EMC / spectrum etiquette** guide aligned with hospital RF policies | Partner clinical eng sign-off |
| H2.4 | Integration hooks: SIEM-friendly JSON, optional research bundle (explicit non-clinical) | **SIEM NDJSON done** — `lab export-siem` |
| H2.5 | Training curriculum: 1-day “Healthcare RF literacy + MedSec gates” | Delivered once externally |
| H2.6 | Partner program: 3 design partners (1 academic, 1 manufacturer lab, 1 health-system cyber) | Signed LOIs |

**Success metric:** ≥1 health-system cyber team runs passive baseline in **non-clinical** space with audit export accepted as evidence artifact.

### Horizon 3 — Worldwide product (6–18 months): Leave Flipper where care touches

Goal: scale without lying about hardware class.

| # | Action | Done when |
|---|--------|-----------|
| H3.1 | **MedSec appliance** design: same software patterns on industrial/medical-adjacent SBC + controlled radio module (60601-minded supply chain consult) | Hardware brief + BOM |
| H3.2 | Cert track **as IT/security tool** (not SaMD): e.g. ISO 27001 process, SBOM, signed firmware, secure boot where feasible | Release process docs |
| H3.3 | Multi-region RF policy packs (US ISM vs EU vs JP band rules) as **policy profiles**, not attack packs | Profile catalog |
| H3.4 | Channel: MedSec consultancies + HTM societies + DEF CON Biohacking / BSides Health track demos | Conference presence |
| H3.5 | If any software ever touches regulated clinical workflows — **separate product**, separate repo, regulatory counsel | Firewall from GrokLink research line |

**Success metric:** Commercial SKU sold as **MedSec research appliance** with support contract; Flipper remains **lab/education SKU**.

---

## 4. What *not* to do (kills trust worldwide)

1. Market Flipper GrokLink as FDA/MDR medical equipment  
2. TX / spoof / jam on live wards or implant-adjacent links  
3. Store PHI on SD or unencrypted PC captures  
4. Auto-TX on anomaly (even “for research”)  
5. Public dumps of hospital RF captures without redaction + authorization  
6. Promise “works with every hospital wireless device” (decode arms race + liability)  
7. Merge “abliterated / weaponized” RF tooling into the MedSec product line  

---

## 5. Immediate ordered backlog (execute next)

Priority order for the **next engineering sprint**:

1. **H0.1–H0.5** — Port Phase 1 packs + MedSec profile + disclaimers onto GrokLink OS  
2. **H1.1–H1.2** — Engagement + casefile on OS bridge (reuse v2 CLI ideas)  
3. **H1.5** — MedSec risk file + update `docs/SAFETY.md` Healthcare section to link this plan  
4. **Pilot script** — 1-page “Lab pilot in a box” (hardware list, RoE stub, demo script)  
5. **Outreach** — 3 target design partners (do not cold-demo on a live hospital floor)  

Defer until after first pilot:

- Full NFC inventory FAP  
- BLE status channel for hospital use  
- Any TX skill in healthcare profile (keep Faraday-only, dual control)

---

## 6. KPIs (how you know it’s working)

| KPI | 90-day target | 12-month target |
|-----|---------------|-----------------|
| External pilots under RoE | 1 | 5+ |
| Passive-only MedSec missions in ROM/SD | 3+ | 10+ region packs |
| Casefiles accepted by partner as evidence | 1 | Standard deliverable |
| Public “hospital hack” framing incidents | 0 | 0 |
| Clinical care claims in marketing | 0 | 0 |
| Revenue from MedSec training / appliance | Optional | Support contracts |

---

## 7. Relationship to prior docs

| Doc | Role |
|-----|------|
| `GrokLink-Firmware/docs/HEALTHCARE_MEDSEC_PLAN.md` | Original assessment (v2) — still valid ethics |
| `GrokLink-Firmware/docs/HEALTHCARE_OPERATOR_RUNBOOK.md` | Phase 1 deploy how-to (v2 bridge) |
| `groklink-os/docs/SAFETY.md` | Core safety; Healthcare section stays passive-only |
| `groklink-os/docs/ROADMAP_3.7.md` | OS product train (SD skills, vault) — MedSec rides this |
| **This file** | Worldwide MedSec **next steps** and anti-scope |

---

## 8. One-paragraph executive answer

To become a **potential MedSec instrument used by hospitals worldwide**, GrokLink OS should not chase clinical certification on Flipper hardware. It should productize as a **policy-gated, audit-first RF research and facility-security tool**: port Phase 1 passive packs to OS, ship engagement/casefile evidence, pilot only under written RoE in labs and non-clinical spaces, then scale via partners and eventually a **medical-adjacent appliance** that reuses the software safety model. Worldwide trust is won by **what you refuse to do** (ward TX, PHI, care claims) as much as by spectrum features.
