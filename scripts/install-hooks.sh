#!/usr/bin/env bash
# Points git at the repo's hooks (.githooks): the pre-commit hook builds and runs the tests.
set -euo pipefail
cd "$(dirname "$0")/.."
git config core.hooksPath .githooks
echo "git hooks: .githooks"
