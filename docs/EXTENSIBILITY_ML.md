# On-device ML — GrokLink OS 3.0

## Non-goals

- No LLM on STM32WB55.
- No automatic TX/GPIO from model outputs.

## Goals

- Feature extractors on pulse / RSSI streams.
- Optional TinyML models ≤ **20–40 KB** weights.
- Arena ≤ **~24 KB** (configurable `GLK_ML_ARENA_SIZE`).
- CMSIS-NN / TFLite Micro hooks in `glk_ml_load_model` / `glk_ml_infer`.

## Pipeline

```
radio capture → features → glk_ml_infer → mission vars / audit
                              │
                              └── NEVER opens driver TX/GPIO
                                    │
                              separate mission steps must call policy
```

## Heuristic default

Without a model blob, `glk_ml_infer` classifies quiet / normal / busy from
feature[0] (mean pulses). Good enough for lab demos and unit tests.

## PC side

Heavy training and skill synthesis stay on the PC bridge (`craft`, research
notebooks). On-device models are optional accelerators for offline autonomy.
