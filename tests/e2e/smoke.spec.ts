import { execFile } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { promisify } from 'node:util';

import { expect, test } from '@playwright/test';
import type { TestInfo } from '@playwright/test';

const execFileAsync = promisify(execFile);

function resolveCliPath(): string {
  if (process.env.US4_CLI_PATH) {
    return process.env.US4_CLI_PATH;
  }

  const executableName = process.platform === 'win32' ? 'us4-cli.exe' : 'us4-cli';
  return path.resolve(process.cwd(), 'build', executableName);
}

async function attachTextArtifact(testInfo: TestInfo, name: string, body: string): Promise<void> {
  await testInfo.attach(name, {
    body: Buffer.from(body, 'utf8'),
    contentType: 'text/plain',
  });
}

async function attachDiagnostics(testInfo: TestInfo, cliPath: string): Promise<void> {
  const diagnostics = {
    cliPath,
    cliExists: existsSync(cliPath),
    cwd: process.cwd(),
    platform: process.platform,
    ci: !!process.env.CI,
    env: {
      US4_CLI_PATH: process.env.US4_CLI_PATH ?? null,
      US4_HAS_CUDA: process.env.US4_HAS_CUDA ?? null,
      US4_HAS_DIRECTML: process.env.US4_HAS_DIRECTML ?? null,
      US4_HAS_VULKAN: process.env.US4_HAS_VULKAN ?? null,
      US4_HAS_NPU: process.env.US4_HAS_NPU ?? null,
      US4_CPU_NAME: process.env.US4_CPU_NAME ?? null,
      US4_GPU_NAME: process.env.US4_GPU_NAME ?? null,
    },
  };

  await testInfo.attach('cli-diagnostics', {
    body: Buffer.from(JSON.stringify(diagnostics, null, 2), 'utf8'),
    contentType: 'application/json',
  });
}

async function requireCliBinary(testInfo: TestInfo): Promise<string> {
  const cliPath = resolveCliPath();
  await attachDiagnostics(testInfo, cliPath);

  if (!existsSync(cliPath)) {
    const message =
      `CLI binary not found at ${cliPath}. ` +
      'Build us4-cli before running E2E evidence. ' +
      'A skipped smoke run does not satisfy the CLI/UX evidence requirement.';
    testInfo.annotations.push({
      type: 'toolchain-blocker',
      description: message,
    });
    test.skip(true, message);
  }

  return cliPath;
}

async function attachProcessOutput(
  testInfo: TestInfo,
  prefix: string,
  stdout: string,
  stderr: string,
): Promise<void> {
  await attachTextArtifact(testInfo, `${prefix}-stdout`, stdout);
  if (stderr) {
    await attachTextArtifact(testInfo, `${prefix}-stderr`, stderr);
  }
}

test.describe('us4-cli smoke', () => {
  test('prints help output', async ({}, testInfo) => {
    const cliPath = await requireCliBinary(testInfo);

    const { stdout, stderr } = await execFileAsync(cliPath, ['--help'], {
      windowsHide: true,
    });

    await attachProcessOutput(testInfo, 'help', stdout, stderr);

    expect(stdout).toContain('us4-cli');
    expect(stdout).toContain('--probe');
  });

  test('prints probe summary', async ({}, testInfo) => {
    const cliPath = await requireCliBinary(testInfo);

    const { stdout, stderr } = await execFileAsync(cliPath, ['--probe'], {
      env: {
        ...process.env,
        US4_HAS_DIRECTML: process.env.US4_HAS_DIRECTML ?? '1',
        US4_CPU_NAME: process.env.US4_CPU_NAME ?? 'Playwright Test CPU',
        US4_GPU_NAME: process.env.US4_GPU_NAME ?? 'Playwright Test GPU',
      },
      windowsHide: true,
    });

    await attachProcessOutput(testInfo, 'probe', stdout, stderr);

    expect(stdout).toContain('US4 Windows Edition Probe');
    expect(stdout).toContain('backend:');
    expect(stdout).toContain('Playwright Test GPU');
  });

  test('keeps run scaffold observable for evidence capture', async ({}, testInfo) => {
    const cliPath = await requireCliBinary(testInfo);

    const { stdout, stderr } = await execFileAsync(
      cliPath,
      ['run', '--model', 'qwen-0.5b', '--prompt', 'hello from playwright'],
      {
        env: {
          ...process.env,
          US4_HAS_DIRECTML: process.env.US4_HAS_DIRECTML ?? '1',
        },
        windowsHide: true,
      },
    );

    await attachProcessOutput(testInfo, 'run', stdout, stderr);

    expect(stdout).toContain('run');
    expect(stdout).toContain('qwen-0.5b');
  });
});
