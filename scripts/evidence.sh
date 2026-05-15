#!/usr/bin/env bash
set -euo pipefail

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

resolve_cli_path() {
  if [[ -n "${US4_CLI_PATH:-}" ]]; then
    printf '%s\n' "$US4_CLI_PATH"
    return
  fi

  printf '%s\n' "$(pwd)/build/us4-cli.exe"
}

show_build_guidance() {
  local cli_path
  cli_path="$(resolve_cli_path)"
  echo "CLI evidence is blocked because the executable was not found."
  echo "Expected binary: $cli_path"
  echo "A skipped Playwright run does not satisfy the CLI/UX evidence requirement."
  echo
  echo "Toolchain snapshot:"
  echo "  cmake     : $(command_exists cmake && echo true || echo false)"
  echo "  ninja     : $(command_exists ninja && echo true || echo false)"
  echo "  cl        : $(command_exists cl && echo true || echo false)"
  echo "  clang-cl  : $(command_exists clang-cl && echo true || echo false)"
  echo "  clang++   : $(command_exists clang++ && echo true || echo false)"
  echo
  echo "Build us4-cli first, for example:"
  echo 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release'
  echo 'cmake --build build --target us4-cli'
}

CLI_PATH="$(resolve_cli_path)"

if [[ ! -f "$CLI_PATH" ]]; then
  show_build_guidance
  exit 1
fi

echo "Capturing Playwright evidence for us4-cli"
npx playwright test --project=cli --reporter=list,html

if [[ ! -f playwright-report/index.html ]]; then
  echo "Playwright HTML report was not generated."
  exit 1
fi

if [[ ! -f test-results/results.json ]]; then
  echo "Playwright JSON results were not generated."
  exit 1
fi

SUMMARY_JSON="$(node - <<'EOF'
const fs = require('node:fs');
const report = JSON.parse(fs.readFileSync('test-results/results.json', 'utf8'));

function walkSuite(suite, totals) {
  for (const spec of suite.specs ?? []) {
    for (const test of spec.tests ?? []) {
      totals.total += 1;
      const status = test.status ?? 'unknown';
      totals[status] = (totals[status] ?? 0) + 1;
    }
  }

  for (const child of suite.suites ?? []) {
    walkSuite(child, totals);
  }
}

const totals = { total: 0, expected: 0, skipped: 0, unexpected: 0, flaky: 0, unknown: 0 };
for (const suite of report.suites ?? []) {
  walkSuite(suite, totals);
}

console.log(JSON.stringify(totals));

if (!totals.total || totals.total === totals.skipped) {
  process.exit(2);
}
EOF
)" || {
  status=$?
  if [[ $status -eq 2 ]]; then
    echo "Playwright only produced skipped tests. Evidence is not complete."
    exit 1
  fi
  exit "$status"
}

echo "Playwright evidence summary: $SUMMARY_JSON"
