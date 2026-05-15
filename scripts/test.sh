#!/usr/bin/env bash
set -euo pipefail

echo "Running current repository validation"

npm run test:cli
npm run pack:dry

if [[ -f CMakeLists.txt && -d build ]]; then
  if command -v ctest >/dev/null 2>&1; then
    echo "Detected runtime scaffold/build. Running CTest as additional validation."
    ctest --test-dir build --output-on-failure
  else
    echo "Build directory found, but ctest is not available in PATH. Skipping runtime test invocation."
  fi
fi

if [[ -d build ]] && command -v npx >/dev/null 2>&1; then
  CLI_PATH="${US4_CLI_PATH:-$(pwd)/build/us4-cli.exe}"
  if [[ -f "$CLI_PATH" ]]; then
    echo "Detected us4-cli binary. Running CLI Playwright smoke validation."
    npx playwright test --project=cli --reporter=list
  else
    echo "Build directory found, but us4-cli binary is missing. Skipping CLI Playwright smoke."
  fi
fi
