---
id: I023
kind: instrument
status: trusted
created: 2026-08-14
---

## Instrument

SDL3 directional-shading A/B and equal-ambient discriminator

## Validated by

Failure class first: the initial enhanced-vs-vanilla comparison passed even with the static normal attribute unbound, because uniform dimming changed pixels. The equal-ambient class produced 0 differences and exposed the lie. After binding Shadercross's compacted static normal at location 3, room M0001_00_00 differs from equal ambient in 3,675 pixels and skinned hero C0000_00 in 2,750; vanilla and enhanced PNGs must also be non-identical. Known limit: vanilla-only comparison cannot distinguish directional response from a brightness multiplier.

## Known failure modes

(none recorded yet)
