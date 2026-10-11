# What the build reads, for the hooks' check that the tree they build and test is the one committed or
# pushed. Sourced by pre-commit and pre-push; keep it to paths that exist.
# shellcheck disable=SC2034 # used by the hooks that source it
build_inputs=(engine plugin tests scripts cmake presets assets .githooks '*CMakeLists.txt')
