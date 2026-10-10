#!/usr/bin/env bash
# Points git at the repo's hooks (.githooks): pre-commit and pre-push build and run the tests (.githooks/gate.sh).
set -euo pipefail
cd "$(dirname "$0")/.."
git config core.hooksPath .githooks
echo "git hooks: .githooks"
