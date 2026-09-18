# GrokLink Landing Hub Design (v3.8 research hub)

**Date:** 2026-07-31  
**Target:** https://groklink.jonbailey.xyz/  
**Source of truth:** Desktop prompt + GrokLink-OS docs (no overclaim)

## Phase 1 - Ranked concepts (impact / cost)

| Rank | Concept | Impact | Cost | Notes |
|------|---------|--------|------|-------|
| 1 | Safety Policy Simulator (modes + confirm mock) | Very high | Med | Makes default-deny visceral |
| 2 | Architecture Layer-Cake Explorer | Very high | Med | Core differentiator vs Flipper fork story |
| 3 | Skills + Agent Hub + vault hash-chain demo | Very high | Med-High | localStorage chain is educational, honest |
| 4 | Interactive Quickstart Wizard | High | Low-Med | Onboarding without wall of text |
| 5 | Sticky safety / edu banner always visible | High | Low | Legal framing non-negotiable |
| 6 | Dynamic Roadmap (shipped vs planned) | High | Low | Credibility + honesty on v3.9 |
| 7 | Docs accordion + filter from real docs | Med-High | Med | Deep dive without leaving site |
| 8 | Ecosystem constellation cards | Med | Low | Constellation aesthetic, cross-links |
| 9 | Canvas spectrum / planner mock (passive) | Med | Med | Educational only; not live RF |
| 10 | ST7567 GUI emulator mock | Med | Med | Nice polish; secondary |
| 11 | PWA offline shell of quickstart + safety | Med | Med | Progressive; optional later |
| 12 | Full Three.js device turntable | Low-Med | High | Budget risk; skip for v1 hub |

**Selected hybrid:** Living Lab Console = sticky safety + architecture explorer + policy simulator + skills/agent/vault demo + wizard + roadmap + docs + ecosystem + MedSec.

## Phase 2 - Information architecture

1. Sticky safety strip (edu phrase + legal one-liner)
2. Hero + badges + CTAs
3. Architecture explorer (`#architecture`)
4. Safety policy simulator (`#safety`)
5. Skills and Agent hub (`#skills`)
6. Quickstart wizard (`#start`)
7. Roadmap timeline (`#roadmap`)
8. Docs deep-dive (`#docs`)
9. MedSec / authorized research (`#medsec`)
10. Ecosystem constellation (`#ecosystem`)
11. Footer legal + GitHub + MIT

## Visual system

- Deep navy/black: `#070b14` / `#0c1220`
- Cyan primary: `#22d3ee`
- Amber safety warn / red system risk
- IBM Plex Sans + Mono (research lab, terminal cues)
- Mobile-first, reduced-motion respect, no trackers
- Self-hosted scripts only (CSP `script-src 'self'`)

## Accuracy rails

- Version **3.8.0** | MIT | Not a Flipper fork
- Passive-only demos; no TX enablement on the web
- Vault demo is **browser localStorage educational mock**, not device crypto
- MedSec: not a medical device; written RoE; passive only
