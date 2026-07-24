# Rolling Codes — Educational Overview (No Attack Tooling)

**Audience:** Students and authorized researchers using GrokLink OS.  
**This document does not provide prediction, cracking, or brand remote decode.**

## 1. Fixed code and replay

If a wireless remote always emits the same bitstream, anyone who records one transmission can **replay** that recording later. On a system that accepts the fixed code, replay opens the same lock without the original button.

GrokLink’s **lab beacon (GLK1)** deliberately uses a **plain counter** so that:

- Two beacons with the same lab_id, counter, and message produce **identical hex**.  
- Students can call `lab-replay-demo` and see that replay is trivial for fixed formats.  
- The lesson is about **why better designs exist**, not how to attack products in the field.

## 2. What rolling / hopping codes change

Rolling codes change an authenticating value on each press (receiver and transmitter stay synchronized via shared secret state and cryptography). A pure RF **replay of an old capture** should be rejected.

Common design goals (conceptual):

- Resist casual replay of recorded RF  
- Limit usefulness of a single sniffed frame  
- Require possession of the legitimate keying material for new valid frames  

## 3. What GrokLink will not ship

| Item | Reason |
|------|--------|
| Rolling-code predictors | Dual-use for unauthorized access |
| KeeLoq / seed recovery guides as product features | Same |
| Third-party protocol auto-decode of car/garage remotes | Unauthorized access risk |
| “Clone this remote” workflows | Explicitly out of policy |

## 4. Allowed practice on GrokLink

1. Encode/decode **GLK1** on owned lab frequencies  
2. Replay **your own** lab captures to learn the fixed-code failure mode  
3. Edge-timing statistics without claiming a commercial protocol  
4. Threat modeling and policy design for default-deny systems  

## 5. Agent rules

When asked to “decode that remote” or “predict the next rolling code”:

1. Refuse third-party decode and prediction.  
2. Offer `explain_rolling_codes` + GLK1 lab demo instead.  
3. Redirect to authorized owned-equipment experiments only.

## Related

- [LAB_CODEC.md](LAB_CODEC.md)  
- [SAFETY.md](SAFETY.md)  
- Tool: `explain_rolling_codes`  
