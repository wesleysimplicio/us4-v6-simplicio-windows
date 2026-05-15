import {expect, test} from '@playwright/test';
import type {TestInfo} from '@playwright/test';
import {execFile} from 'node:child_process';
import {existsSync} from 'node:fs';
import path from 'node:path';
import {promisify} from 'node:util';

const execFileAsync = promisify(execFile);

function resolveCliPath(): string
{
    if (process.env.US4_CLI_PATH)
    {
        return process.env.US4_CLI_PATH;
    }

    const executableName = process.platform === 'win32' ? 'us4-cli.exe' : 'us4-cli';
    const candidates = [
        path.resolve(process.cwd(), 'build', executableName),
        path.resolve(process.cwd(), 'build', 'runtime', 'cli', executableName),
    ];

    const resolved = candidates.find((candidate) => existsSync(candidate));
    return resolved ?? candidates[0];
}

async function attachTextArtifact(testInfo: TestInfo, name: string, body: string): Promise<void>
{
    await testInfo.attach(name, {
        body : Buffer.from(body, 'utf8'),
        contentType : 'text/plain',
    });
}

async function attachDiagnostics(testInfo: TestInfo, cliPath: string): Promise<void>
{
    const diagnostics = {
        cliPath,
        cliExists : existsSync(cliPath),
        cwd : process.cwd(),
        platform : process.platform,
        ci : !!process.env.CI,
        env : {
            US4_CLI_PATH : process.env.US4_CLI_PATH ?? null,
            US4_HAS_CUDA : process.env.US4_HAS_CUDA ?? null,
            US4_HAS_DIRECTML : process.env.US4_HAS_DIRECTML ?? null,
            US4_HAS_VULKAN : process.env.US4_HAS_VULKAN ?? null,
            US4_HAS_NPU : process.env.US4_HAS_NPU ?? null,
            US4_CPU_NAME : process.env.US4_CPU_NAME ?? null,
            US4_GPU_NAME : process.env.US4_GPU_NAME ?? null,
        },
    };

    await testInfo.attach('cli-diagnostics', {
        body : Buffer.from(JSON.stringify(diagnostics, null, 2), 'utf8'),
        contentType : 'application/json',
    });
}

async function requireCliBinary(testInfo: TestInfo): Promise<string>
{
    const cliPath = resolveCliPath();
    await attachDiagnostics(testInfo, cliPath);

    if (!existsSync(cliPath))
    {
        const message = `CLI binary not found at ${cliPath}. ` +
                        'Build us4-cli before running E2E evidence. ' +
                        'A skipped smoke run does not satisfy the CLI/UX evidence requirement.';
        testInfo.annotations.push({
            type : 'toolchain-blocker',
            description : message,
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
    ): Promise<void>
{
    await attachTextArtifact(testInfo, `${prefix}-stdout`, stdout);
    if (stderr)
    {
        await attachTextArtifact(testInfo, `${prefix}-stderr`, stderr);
    }
}

async function runCli(
    cliPath: string,
    args: string[],
    env: NodeJS.ProcessEnv = process.env,
    ): Promise<{stdout : string; stderr : string; exitCode : number | null}>
{
    try
    {
        const result = await execFileAsync(cliPath, args, {
            env,
            windowsHide : true,
        });
        return {
            stdout : result.stdout,
            stderr : result.stderr,
            exitCode : 0,
        };
    }
    catch (error)
    {
        const execError = error as NodeJS.ErrnoException &
        {
            code?: number|string;
            stdout?: string;
            stderr?: string;
        };
        return {
            stdout : execError.stdout ?? '',
            stderr : execError.stderr ?? '',
            exitCode : typeof execError.code === 'number' ? execError.code : null,
        };
    }
}

test.describe('us4-cli smoke', () => {
    test('prints help output', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(cliPath, [ '--help' ]);

        await attachProcessOutput(testInfo, 'help', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stdout).toContain('us4-cli');
        expect(stdout).toContain('--probe');
    });

    test('prints probe summary', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(cliPath, [ '--probe' ], {
            ...process.env,
            US4_HAS_DIRECTML : process.env.US4_HAS_DIRECTML ?? '1',
            US4_CPU_NAME : process.env.US4_CPU_NAME ?? 'Playwright Test CPU',
            US4_GPU_NAME : process.env.US4_GPU_NAME ?? 'Playwright Test GPU',
        });

        await attachProcessOutput(testInfo, 'probe', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stdout).toContain('US4 Windows Edition Probe');
        expect(stdout).toContain('backend:');
        expect(stdout).toContain('Playwright Test GPU');
    });

    test('runs the cpu-only scalar baseline for evidence capture', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'qwen-0.5b', '--prompt', 'hello from playwright', '--backend',
                'cpu', '--max-tokens', '5'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
            },
        );

        await attachProcessOutput(testInfo, 'run', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stdout).toContain('backend: cpu-avx2');
        expect(stdout).toContain('mode: CPU_ONLY');
        expect(stdout).toContain('run_status: completed');
        expect(stdout).toContain('generated_tokens:');
    });

    test('renders a DirectML dry-run plan', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'qwen-0.5b', '--prompt', 'hello directml', '--backend', 'directml'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '1',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
                US4_CPU_NAME : 'Playwright Test CPU',
                US4_GPU_NAME : 'Intel Arc Test',
                US4_GPU_VENDOR : 'intel',
                US4_GPU_CLASS : 'integrated',
                US4_DEVICE_GIB : '8',
            },
        );

        await attachProcessOutput(testInfo, 'directml-dry-run', stdout, stderr);

        expect(exitCode).toBe(2);
        expect(stdout).toContain('backend: directml');
        expect(stdout).toContain('execution: directml-dry-run');
        expect(stdout).toContain('directml.graph_state: ready');
        expect(stdout).toContain('directml.dispatch_ok: yes');
        expect(stderr).toContain('not implemented yet');
    });

    test('renders a CUDA dry-run plan', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [ 'run', '--model', 'qwen-0.5b', '--prompt', 'hello cuda', '--backend', 'cuda' ],
            {
                ...process.env,
                US4_HAS_CUDA : '1',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
                US4_GPU_NAME : 'NVIDIA RTX 4090',
                US4_GPU_VENDOR : 'nvidia',
                US4_GPU_CLASS : 'discrete',
                US4_DEVICE_GIB : '24',
                US4_DISCRETE_GPU_COUNT : '1',
            },
        );

        await attachProcessOutput(testInfo, 'cuda-dry-run', stdout, stderr);

        expect(exitCode).toBe(2);
        expect(stdout).toContain('backend: cuda');
        expect(stdout).toContain('execution: cuda-dry-run');
        expect(stdout).toContain('cuda.execution_flavor:');
        expect(stdout).toContain('cuda.prefill_chunk_tokens:');
        expect(stderr).toContain('not implemented yet');
    });

    test('renders a Vulkan dry-run plan', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [ 'run', '--model', 'qwen-0.5b', '--prompt', 'hello vulkan', '--backend', 'vulkan' ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '1',
                US4_HAS_NPU : '',
                US4_GPU_NAME : 'Radeon RX Test',
                US4_GPU_VENDOR : 'amd',
                US4_GPU_CLASS : 'discrete',
                US4_DEVICE_GIB : '12',
            },
        );

        await attachProcessOutput(testInfo, 'vulkan-dry-run', stdout, stderr);

        expect(exitCode).toBe(2);
        expect(stdout).toContain('backend: vulkan');
        expect(stdout).toContain('execution: vulkan-dry-run');
        expect(stdout).toContain('vulkan.context_state: bound');
        expect(stdout).toContain('vulkan.step_count:');
        expect(stdout).toContain('vulkan.descriptor_sets:');
        expect(stdout).toContain('vulkan.kernel_manifest_loaded: yes');
        expect(stdout).toContain('vulkan.kernel_count:');
        expect(stdout).toContain('vulkan.required_kernel_count:');
        expect(stdout).toContain('vulkan.timeline_semaphores:');
        expect(stderr).toContain('not implemented yet');
    });

    test('renders a Windows ML dry-run plan with NPU opt-in', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'qwen-0.5b', '--prompt', 'hello npu', '--backend', 'windows-ml',
                '--npu'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '1',
                US4_HAS_NPU : '1',
                US4_GPU_NAME : 'Radeon RX Test',
                US4_GPU_VENDOR : 'amd',
                US4_GPU_CLASS : 'discrete',
                US4_NPU_NAME : 'Ryzen AI Test',
                US4_NPU_VENDOR : 'microsoft',
                US4_DEVICE_GIB : '8',
                US4_POWER_SOURCE : 'ac',
                US4_BATTERY_PERCENT : '100',
                US4_BATTERY_SAVER : '0',
                US4_THERMAL_STATE : 'nominal',
                US4_ETW_THROTTLED : '0',
            },
        );

        await attachProcessOutput(testInfo, 'windows-ml-dry-run', stdout, stderr);

        expect(exitCode).toBe(2);
        expect(stdout).toContain('backend: windows-ml');
        expect(stdout).toContain('execution: windows-ml-dry-run');
        expect(stdout).toContain('windows_ml.adapter_state: compiled');
        expect(stdout).toContain('windows_ml.opt_in_satisfied: yes');
        expect(stdout).toContain('windows_ml.npu_partitions:');
        expect(stdout).toContain('windows_ml.dispatch_table_size: 5');
        expect(stdout).toContain('windows_ml.first_dispatch_target: npu');
        expect(stdout).toContain('windows_ml.power_policy: nominal');
        expect(stdout).toContain('windows_ml.synthetic_power_telemetry: yes');
        expect(stdout).toContain('windows_ml.mixed_dispatch_active: yes');
        expect(stdout).toContain('windows_ml.mixed_dispatch_slice_count: 5');
        expect(stdout).toContain('windows_ml.mixed_dispatch_gpu_primary: yes');
        expect(stdout).toContain('windows_ml.mixed_dispatch_npu_dense: yes');
        expect(stdout).toContain('windows_ml.mixed_dispatch_policy_degraded: no');
        expect(stdout).toContain('windows_ml.partition_count:');
        expect(stderr).toContain('not implemented yet');
    });

    test('renders a Windows ML fallback plan when opt-in is missing', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'qwen-0.5b', '--prompt', 'hello fallback', '--backend',
                'windows-ml'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '1',
                US4_HAS_NPU : '1',
                US4_GPU_NAME : 'Radeon RX Test',
                US4_GPU_VENDOR : 'amd',
                US4_GPU_CLASS : 'discrete',
                US4_NPU_NAME : 'Ryzen AI Test',
                US4_NPU_VENDOR : 'microsoft',
                US4_DEVICE_GIB : '8',
                US4_POWER_SOURCE : 'ac',
                US4_BATTERY_PERCENT : '100',
                US4_BATTERY_SAVER : '0',
                US4_THERMAL_STATE : 'nominal',
                US4_ETW_THROTTLED : '0',
            },
        );

        await attachProcessOutput(testInfo, 'windows-ml-fallback', stdout, stderr);

        expect(exitCode).toBe(2);
        expect(stdout).toContain('backend: windows-ml');
        expect(stdout).toContain('execution: windows-ml-dry-run');
        expect(stdout).toContain('windows_ml.adapter_state: compiled');
        expect(stdout).toContain('windows_ml.opt_in_satisfied: no');
        expect(stdout).toContain('windows_ml.cpu_fallback_partitions: 1');
        expect(stdout).toContain('windows_ml.last_dispatch_target: host-assist');
        expect(stdout).toContain('windows_ml.power_policy: nominal');
        expect(stdout).toContain('windows_ml.mixed_dispatch_active: yes');
        expect(stdout).toContain('windows_ml.mixed_dispatch_cpu_fallback: yes');
        expect(stdout).toContain('windows_ml.issue_codes: windows_ml.opt_in_required');
        expect(stderr).toContain('not implemented yet');
    });

    test('downgrades Windows ML mixed dispatch under thermal pressure', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'qwen-0.5b', '--prompt', 'hello thermal', '--backend',
                'windows-ml', '--npu'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '1',
                US4_HAS_NPU : '1',
                US4_GPU_NAME : 'Radeon RX Test',
                US4_GPU_VENDOR : 'amd',
                US4_GPU_CLASS : 'discrete',
                US4_NPU_NAME : 'Ryzen AI Test',
                US4_NPU_VENDOR : 'microsoft',
                US4_DEVICE_GIB : '8',
                US4_POWER_SOURCE : 'battery',
                US4_BATTERY_PERCENT : '18',
                US4_BATTERY_SAVER : '1',
                US4_THERMAL_STATE : 'throttled',
                US4_ETW_THROTTLED : '1',
            },
        );

        await attachProcessOutput(testInfo, 'windows-ml-thermal-throttle', stdout, stderr);

        expect(exitCode).toBe(2);
        expect(stdout).toContain('windows_ml.power_policy: thermal-throttle');
        expect(stdout).toContain('windows_ml.synthetic_power_telemetry: yes');
        expect(stdout).toContain(
            'windows_ml.power.issue_codes: windows_ml.power.thermal_throttle,windows_ml.power.etw_signal',
        );
        expect(stdout).toContain('windows_ml.mixed_dispatch_npu_dense: no');
        expect(stdout).toContain('windows_ml.mixed_dispatch_policy_degraded: yes');
        expect(stdout).toContain('windows_ml.mixed_dispatch_npu_demotions: 3');
        expect(stderr).toContain('not implemented yet');
    });
});
