# 125A Bass Finisher V1 — Release Audit

Audit basis: `challanger2000/125A-Engineering/START-HERE.md`, `QA/RELEASE-QA.md`, `STANDARDS/PROFESSIONAL-PLUGIN-BEHAVIOR.md`, `STANDARDS/DSP-GUIDELINES.md` and `QA/AUDIO-FIXTURES.md`.

Development branch: `v1.0.0-engineering`.

## Minimum 125A release gate

| Gate | Evidence in Bass Finisher | Status before current-head CI |
| --- | --- | --- |
| Steinberg VST3 Validator | Build workflow and 125A Plugin Tester | PASS on prior audited build; current-head confirmation pending |
| 125A Plugin Tester | Pinned v0.2.9 full release workflow | PASS on prior audited build (48 PASS / 0 WARNING / 0 FAIL); current-head confirmation pending |
| Editor lifecycle | Controller contract open/attach/detach/reopen + Plugin Tester lifecycle probe | PASS on prior audited build; current-head confirmation pending |
| I/O and event probe | Processor contract + Plugin Tester isolated I/O/Event probe | PASS on prior audited build |
| Offline/lifecycle/audio torture | Processor active-path matrix + repeated activate/process/deactivate + Plugin Tester worker | PASS on prior audited build |
| State save/restore | State serialization/atomicity tests | PASS |
| State restores audible result | New current-head processor render equality regression | PENDING current-head CI |
| Automation | Automation math + sample-accurate processor automation | PASS |
| Ctrl + left-click defaults | New current-head real VSTGUI Ctrl-click dispatch test on MATCH, FINISH, LOW CONTROL, MASS, OUTPUT custom knobs | PENDING current-head CI |
| Bypass | Crossfade contract + Plugin Tester real bypass stress | PASS on prior audited build |
| Mono/stereo | Processor matrix + Plugin Tester | PASS |
| Sample rates | 44.1/48/88.2/96/192 kHz covered across internal QA and Plugin Tester | PASS |
| Block sizes | 1 through 8192 samples covered across internal QA and Plugin Tester | PASS |
| NaN/Inf | DSP recovery + Plugin Tester | PASS |
| Denormal/subnormal | Processor torture + Plugin Tester | PASS |
| No unexpected callback allocations | Existing DSP allocation test plus new full `Processor::process()` allocation gate | PENDING current-head CI |
| Deterministic DSP regression | exact block-partition/realtime-offline equality + MATCH quality + mode-separation regressions | PASS / current mode hardening pending current-head CI |
| Latency/tail | Explicit 0-sample latency and 0-sample tail processor contract | PENDING current-head CI for explicit latency assertion |

## Measurement-specific Bass evidence

The release-session real programme material is retained outside the public repository and identified by SHA-256 so measurements refer to immutable files rather than filenames alone.

| File role | SHA-256 | Format |
| --- | --- | --- |
| Bass target / Bypass | `fafc740b314c9342a5b9ff94a2712ecea2ab2706fc4e16ba7114904e7b516ee6` | FLAC, 44.1 kHz stereo, 16.0 s |
| Bass target / MATCH only | `eee1a272bf4161e4c76d2f3278ad487bd2f85cc9f83cb3a521c40062a2286824` | FLAC, 44.1 kHz stereo, 16.0 s |
| Bass target / full chain | `fe8ba6ad511b923af2b9df365dbefadfde434e0e29d076c6802c197bf73e4809` | FLAC, 44.1 kHz stereo, 16.0 s |
| Reference | `0a1fba200422ece27e77c72b69687ebeadcbea5b1ee99fda6c03670de954265d` | FLAC, 44.1 kHz stereo, 274.0 s |

Measured mono RMS for the same files:
- Bypass: -28.21 dBFS
- MATCH only: -27.67 dBFS
- full chain: -24.04 dBFS
- reference: -26.16 dBFS

For a 16-band log-spaced 35 Hz–10 kHz Welch spectral-shape comparison with broadband offset removed:
- Bypass -> Reference RMS band error: **4.323 dB**
- MATCH -> Reference RMS band error: **3.339 dB**

Therefore MATCH measurably moves this real bass programme material toward the reference spectral shape. These hashed files are release-session evidence; they are not redistributed by the public repository because provenance/redistribution rights are not established.

## Nonlinear / DSP evidence

- FINISH harmonic generation is measured and bounded.
- MASS harmonic generation is measured and bounded.
- FINISH alias-risk regression is below the documented release guard; whole-plugin oversampling is therefore not justified by current measurements.
- DC is bounded on symmetric nonlinear probes.
- CLEAN/PUNCH/DENSE have different DSP target weights and nonlinear density; the current-head mode-separation QA requires measurable waveform, spectral, transient/crest and H3 separation.
- LOW CONTROL adaptive sub containment has a measured regression against the same static high-pass boundary.
- MASS response changes with LOW CONTROL and is guarded by measured frequency-response tests.

## Remaining release decision

Do not label the current branch RELEASE READY until:
1. the current-head normal CI is green;
2. the current-head **Bass Finisher Full Release QA** workflow is green, including internal CTests and 125A Plugin Tester v0.2.9;
3. the exact VST3 artifact handed to the user is taken from that full release QA run.

No subjective hearing result is required to substitute for any of the objective gates above.
