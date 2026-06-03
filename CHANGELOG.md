# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-06-03

### Fixed
- Parse ENTSO-E's sparse price curve correctly. ENTSO-E omits a quarter-hour
  `Point` whenever its price equals the previous position's, expecting the
  consumer to carry the last value forward until the next given position. The
  parser now backfills those skipped positions and pads to the period's full
  length (derived from its `timeInterval`), so a day yields all 96 quarter-hour
  slots instead of dropping the duplicated ones. Previously a flat stretch such
  as 11:15–12:15 collapsed to a single 11:15 entry, leaving only 89 slots. DST
  days (92/100 slots) now reconstruct correctly too.

## [1.0.0]

- Initial release.
