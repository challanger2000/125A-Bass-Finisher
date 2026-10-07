# Baseline provenance

Imported from the authoritative High Gain Guitar Finisher V3 engineering source.

- Source repository: `challanger2000/125A-High-Gain-Guitar-Finisher-Final`
- Source branch: `v3.0.0-engineering`
- Source commit: `59c46c98aa499185a9e1ad34bdfb8ed7cbff95b2`

## Reused as engineering infrastructure
- VST3 Processor/Controller architecture
- Tone Match analyzer/capture/profile/message/state pipeline
- WAV/FLAC/MP3 reference decoding
- bypass crossfade
- Auto-Level infrastructure
- adaptive band/resonance infrastructure
- Biquad/filter utilities
- custom VSTGUI views
- dr_libs decoder dependency and license

## Deliberately not imported
- IndustrialRoom
- LeadDelay
- DelayDivisionMapping
- room/delay tests

## Must be redesigned for Bass
- FINISH voicing and nonlinear behavior
- MASS and low-end control
- MIX FIT frequency ranges and resonance behavior
- parameter/product IDs and state contract
- GUI layout/text/product identity
- build targets/artifact names
- bass-specific fixtures and acceptance measurements

No inherited Guitar DSP is assumed correct for bass merely because it compiles.
