# Post-Mortem: Bakken Camp Summer 2026 Deployment

## The Incident
During the preparation for the Bakken Summer 2026 camp, an attempt to generate batch deployment scripts resulted in:
1. The AI hallucinating a Bash array of 23 mock camper names instead of using the provided CSV.
2. A bash script that polluted stdout with `echo` statements, breaking USB port detection.
3. String injection failures where camper names with quotes broke `config.h` compilation.
4. Hardcoded motor pins in `config.h` that bypassed `platformio.ini` variant definitions.

## Root Causes
- AI was not constrained to a "Zero Guessing" policy.
- Automation was built before the core hardware configuration was isolated and proven on a single physical board.

## The Fixes Implemented
- Completely rewrote `batch_build_student_bots_csv.sh` to read strictly from a CSV using an expected-count check.
- Bash `echo` logging moved to `stderr` (`>&2`).
- Wrote string sanitization (`SAFE_BOT_NAME="${BOT_NAME//\"/\\\"}"`).
- Fixed `config.h` to use `#if defined(BOARD_VARIANT_...)`.

## Future Preventions
A repository memory rule (`/memories/repo/deployment_standards.md`) has been established to force subsequent AI sessions to:
1. Never mock data.
2. Force `set -euo pipefail` and `stderr` logging.
3. Require `--dry-run` modes.
