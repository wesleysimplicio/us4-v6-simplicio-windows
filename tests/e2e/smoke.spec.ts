import {expect, test} from '@playwright/test';
import type {TestInfo} from '@playwright/test';
import {execFile} from 'node:child_process';
import {existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync} from 'node:fs';
import path from 'node:path';
import {promisify} from 'node:util';

const execFileAsync = promisify(execFile);
const packageVersion = (JSON.parse(readFileSync(path.resolve(process.cwd(), 'package.json'), 'utf8')) as
{
    version: string;
}).version;

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

async function runPowerShell(
    args: string[],
    env: NodeJS.ProcessEnv = process.env,
    ): Promise<{stdout : string; stderr : string; exitCode : number | null}>
{
    return runProcess(process.platform === 'win32' ? 'powershell.exe' : 'pwsh', args, env);
}

async function runProcess(
    executable: string,
    args: string[],
    env: NodeJS.ProcessEnv = process.env,
    ): Promise<{stdout : string; stderr : string; exitCode : number | null}>
{
    try
    {
        const result = await execFileAsync(executable, args, {
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

function escapePowerShellSingleQuoted(value: string): string
{
    return value.replaceAll('\'', '\'\'');
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
        expect(stdout).toContain('moe.preview:');
        expect(stdout).toContain('hot_hit_rate_pct:');
        expect(stdout).toContain('eviction_count:');
    });

    test('exports probe as json', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(cliPath, [ 'probe', '--format', 'json' ], {
            ...process.env,
            US4_HAS_DIRECTML : process.env.US4_HAS_DIRECTML ?? '1',
            US4_CPU_NAME : 'Playwright Json CPU',
            US4_GPU_NAME : 'Playwright Json GPU',
        });

        await attachProcessOutput(testInfo, 'probe-json', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stderr).toBe('');
        const payload = JSON.parse(stdout) as
        {
            execution: string;
            cpu: string;
            gpu: string;
            selected_backend: string;
            moe: {
                hot_hit_rate_pct: number;
                eviction_count: number;
                telemetry_events: number;
            };
        };
        expect(payload.execution).toBe('probe');
        expect(payload.cpu).toBe('Playwright Json CPU');
        expect(payload.gpu).toBe('Playwright Json GPU');
        expect(payload.selected_backend).toBe('directml');
        expect(payload.moe.hot_hit_rate_pct).toBeGreaterThan(0);
        expect(payload.moe.eviction_count).toBeGreaterThan(0);
        expect(payload.moe.telemetry_events).toBe(5);
    });

    test('reports nano mode for low-memory cpu-only hosts', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(cliPath, [ 'probe' ], {
            ...process.env,
            US4_HAS_CUDA : '',
            US4_HAS_DIRECTML : '',
            US4_HAS_VULKAN : '',
            US4_HAS_NPU : '',
            US4_HOST_GIB : '8',
            US4_CPU_NAME : 'Playwright Low Memory CPU',
            US4_GPU_NAME : 'Playwright Low Memory GPU',
        });

        await attachProcessOutput(testInfo, 'probe-low-memory', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stdout).toContain('backend: cpu-avx2');
        expect(stdout).toContain('mode: NANO');
        expect(stdout).toContain('Playwright Low Memory CPU');
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
        expect(stdout).toContain('speculative.acceptance_rate_pct:');
        expect(stdout).toContain('speculative.step_1.delta_pct:');
    });

    test('exports the cpu-only scalar baseline as json', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'qwen-0.5b', '--prompt', 'hello from playwright', '--backend',
                'cpu', '--max-tokens', '5', '--format', 'json'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
            },
        );

        await attachProcessOutput(testInfo, 'run-json-cpu', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stderr).toBe('');
        const payload = JSON.parse(stdout) as
        {
            execution: string;
            status: string;
            plan_execution: string;
            backend: string;
            generated_text: string;
            generated_tokens: number[];
            speculative: {
                active: boolean;
                decoder: string;
                acceptance_rate_pct: number;
                token_acceptance_trace: number[];
            };
        };
        expect(payload.execution).toBe('run');
        expect(payload.status).toBe('completed');
        expect(payload.plan_execution).toBe('cpu-scalar');
        expect(payload.backend).toBe('cpu-avx2');
        expect(Array.isArray(payload.generated_tokens)).toBeTruthy();
        expect(typeof payload.generated_text).toBe('string');
        expect(payload.speculative.active).toBeTruthy();
        expect(payload.speculative.decoder).toBe('peagle');
        expect(payload.speculative.acceptance_rate_pct).toBeGreaterThan(0);
        expect(Array.isArray(payload.speculative.token_acceptance_trace)).toBeTruthy();
    });

    test('shows MoE prefetch and sparsity telemetry in text mode', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'deepseek-r1-distill', '--prompt', 'moe telemetry from playwright',
                '--backend', 'cpu', '--max-tokens', '5'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
            },
        );

        await attachProcessOutput(testInfo, 'run-moe-telemetry', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stdout).toContain('supports_moe: yes');
        expect(stdout).toContain('moe.prefetch_hit_ratio_pct:');
        expect(stdout).toContain('moe.sparsity_hit_ratio_pct:');
    });

    test('exports MoE prefetch and sparsity telemetry as json', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'deepseek-r1-distill', '--prompt', 'moe telemetry from playwright',
                '--backend', 'cpu', '--max-tokens', '5', '--format', 'json'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
            },
        );

        await attachProcessOutput(testInfo, 'run-moe-telemetry-json', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stderr).toBe('');
        const payload = JSON.parse(stdout) as
        {
            moe: {
                route_count: number;
                prefetch: {
                    prediction_count: number;
                    hit_ratio_pct: number;
                    predicted_experts: number[];
                };
                sparsity_cache: {
                    entry_count: number;
                    hit_ratio_pct: number;
                };
            };
        };
        expect(payload.moe.route_count).toBeGreaterThan(0);
        expect(payload.moe.prefetch.prediction_count).toBeGreaterThan(0);
        expect(payload.moe.prefetch.hit_ratio_pct).toBeGreaterThan(0);
        expect(Array.isArray(payload.moe.prefetch.predicted_experts)).toBeTruthy();
        expect(payload.moe.sparsity_cache.entry_count).toBeGreaterThan(0);
        expect(payload.moe.sparsity_cache.hit_ratio_pct).toBeGreaterThan(0);
    });

    test('shows MiniMax multimodal cache telemetry in text mode', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'minimax-m2', '--prompt', 'multimodal cache from playwright',
                '--backend', 'cpu', '--max-tokens', '5'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
            },
        );

        await attachProcessOutput(testInfo, 'run-minimax-multimodal-cache', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stdout).toContain('family: minimax');
        expect(stdout).toContain('multimodal_cache.entry_count:');
        expect(stdout).toContain('multimodal_cache.audio_entries:');
    });

    test('exports MiniMax multimodal cache telemetry as json', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'minimax-m2', '--prompt', 'multimodal cache from playwright',
                '--backend', 'cpu', '--max-tokens', '5', '--format', 'json'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
            },
        );

        await attachProcessOutput(testInfo, 'run-minimax-multimodal-cache-json', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stderr).toBe('');
        const payload = JSON.parse(stdout) as
        {
            multimodal_cache: {
                entry_count: number;
                hit_count: number;
                miss_count: number;
                image_entries: number;
                audio_entries: number;
            };
        };
        expect(payload.multimodal_cache.entry_count).toBeGreaterThan(0);
        expect(payload.multimodal_cache.hit_count).toBeGreaterThan(0);
        expect(payload.multimodal_cache.miss_count).toBeGreaterThan(0);
        expect(payload.multimodal_cache.image_entries).toBeGreaterThan(0);
        expect(payload.multimodal_cache.audio_entries).toBeGreaterThan(0);
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

    test('exports a DirectML dry-run plan as json', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'run', '--model', 'qwen-0.5b', '--prompt', 'hello directml', '--backend',
                'directml', '--format', 'json'
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

        await attachProcessOutput(testInfo, 'directml-dry-run-json', stdout, stderr);

        expect(exitCode).toBe(2);
        const payload = JSON.parse(stdout) as
        {
            execution: string;
            status: string;
            plan_execution: string;
            backend: string;
            report_text: string;
        };
        expect(payload.execution).toBe('run');
        expect(payload.status).toBe('dry-run');
        expect(payload.plan_execution).toBe('directml-dry-run');
        expect(payload.backend).toBe('directml');
        expect(payload.report_text).toContain('directml.graph_state: ready');
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
        expect(stdout).toContain('windows_ml.compile_target: npu');
        expect(stdout).toContain('windows_ml.graph_reusable: yes');
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
        expect(stdout).toContain('windows_ml.compile_target: cpu-fallback');
        expect(stdout).toContain('windows_ml.fallback_reason: opt-in-required');
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

    test('falls back gracefully when Windows ML opt-in is requested without an NPU',
         async ({}, testInfo) => {
             const cliPath = await requireCliBinary(testInfo);

             const {stdout, stderr, exitCode} = await runCli(
                 cliPath,
                 [
                     'run', '--model', 'qwen-0.5b', '--prompt', 'hello no npu', '--backend',
                     'windows-ml', '--npu'
                 ],
                 {
                     ...process.env,
                     US4_HAS_CUDA : '',
                     US4_HAS_DIRECTML : '',
                     US4_HAS_VULKAN : '1',
                     US4_HAS_NPU : '',
                     US4_GPU_NAME : 'Radeon RX Test',
                     US4_GPU_VENDOR : 'amd',
                     US4_GPU_CLASS : 'discrete',
                     US4_DEVICE_GIB : '8',
                     US4_POWER_SOURCE : 'ac',
                     US4_BATTERY_PERCENT : '100',
                     US4_BATTERY_SAVER : '0',
                     US4_THERMAL_STATE : 'nominal',
                     US4_ETW_THROTTLED : '0',
                 },
             );

             await attachProcessOutput(testInfo, 'windows-ml-no-npu-fallback', stdout, stderr);

             expect(exitCode).toBe(2);
             expect(stdout).toContain('backend: windows-ml');
             expect(stdout).toContain('execution: windows-ml-dry-run');
             expect(stdout).toContain('windows_ml.adapter_state: compiled');
             expect(stdout).toContain('windows_ml.compile_target: cpu-fallback');
             expect(stdout).toContain('windows_ml.fallback_reason: npu-unavailable');
             expect(stdout).toContain('windows_ml.cpu_fallback_armed: yes');
             expect(stdout).toContain('windows_ml.npu_partitions: 0');
             expect(stdout).toContain('windows_ml.mixed_dispatch_active: yes');
             expect(stdout).toContain('windows_ml.mixed_dispatch_npu_dense: no');
             expect(stdout).toContain('windows_ml.mixed_dispatch_cpu_fallback: yes');
             expect(stdout).toContain('windows_ml.mixed_dispatch_policy_degraded: no');
             expect(stderr).toContain('not implemented yet');
         });

    test('tunes and persists a cpu-only profile selection', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);
        const storePath = testInfo.outputPath('profiles.json');

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [ 'tune', '--model', 'qwen-0.5b', '--backend', 'cpu', '--mode', 'cpu-only' ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
                US4_PROFILE_STORE_PATH : storePath,
            },
        );

        await attachProcessOutput(testInfo, 'tune', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stdout).toContain('execution: tune');
        expect(stdout).toContain('profile: cpu-only');
        expect(stdout).toContain('tune.selected_profile: cpu-only');
        expect(stdout).toContain('tune.persisted: yes');
        expect(stdout).toContain(`tune.store_path: ${storePath}`);
        expect(stdout).toContain('tune_status: completed');
        expect(existsSync(storePath)).toBeTruthy();
        expect(readFileSync(storePath, 'utf8')).toContain('"profile_id": "cpu-only"');
    });

    test('exports tune as json and persists a cpu-only profile selection', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);
        const storePath = testInfo.outputPath('tune-json-profiles.json');

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [
                'tune', '--model', 'qwen-0.5b', '--backend', 'cpu', '--mode', 'cpu-only',
                '--format', 'json'
            ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '',
                US4_HAS_NPU : '',
                US4_PROFILE_STORE_PATH : storePath,
            },
        );

        await attachProcessOutput(testInfo, 'tune-json', stdout, stderr);

        expect(exitCode).toBe(0);
        expect(stderr).toBe('');
        const payload = JSON.parse(stdout) as
        {
            execution: string;
            selected_profile: string;
            persisted: boolean;
        };
        expect(payload.execution).toBe('tune');
        expect(payload.selected_profile).toBe('cpu-only');
        expect(payload.persisted).toBeTruthy();
        expect(existsSync(storePath)).toBeTruthy();
    });

    test('exports the current bench matrix as json without persisting a profile',
         async ({}, testInfo) => {
             const cliPath = await requireCliBinary(testInfo);
             const storePath = testInfo.outputPath('bench-profiles.json');

             const {stdout, stderr, exitCode} = await runCli(
                 cliPath,
                 [
                     'bench', '--model', 'qwen-0.5b', '--backend', 'cpu', '--mode', 'cpu-only',
                     '--format', 'json'
                 ],
                 {
                     ...process.env,
                     US4_HAS_CUDA : '',
                     US4_HAS_DIRECTML : '',
                     US4_HAS_VULKAN : '',
                     US4_HAS_NPU : '',
                     US4_PROFILE_STORE_PATH : storePath,
                 },
             );

             await attachProcessOutput(testInfo, 'bench-json', stdout, stderr);

             expect(exitCode).toBe(0);
             expect(stderr).toBe('');
             const payload = JSON.parse(stdout) as
             {
                 execution: string;
                 selected_profile: string;
                 selected_backend: string;
                 persisted: boolean;
                 decisions: Array<{key : string; value : string; rationale : string}>;
                 samples: Array<{
                     benchmark : string; backend : string; profile : string; supported : boolean;
                     regression_critical : boolean;
                     score : number;
                     rationale : string;
                 }>;
             };
             expect(payload.execution).toBe('bench');
             expect(payload.selected_profile).toBe('cpu-only');
             expect(payload.selected_backend).toBe('cpu');
             expect(payload.persisted).toBeFalsy();
             expect(Array.isArray(payload.decisions)).toBeTruthy();
             expect(Array.isArray(payload.samples)).toBeTruthy();
             expect(payload.decisions.length).toBeGreaterThan(0);
             expect(payload.samples.length).toBeGreaterThan(0);
             expect(payload.samples[0]?.benchmark).toBe('dense_baseline_qwen_cpu_only');
             expect(payload.samples[0]?.backend).toBe('cpu');
             expect(typeof payload.samples[0]?.score).toBe('number');
             expect(existsSync(storePath)).toBeFalsy();
         });

    test('renders the current bench matrix as text without persisting a profile',
         async ({}, testInfo) => {
             const cliPath = await requireCliBinary(testInfo);
             const storePath = testInfo.outputPath('bench-text-profiles.json');

             const {stdout, stderr, exitCode} = await runCli(
                 cliPath,
                 [ 'bench', '--model', 'qwen-0.5b', '--backend', 'cpu', '--mode', 'cpu-only' ],
                 {
                     ...process.env,
                     US4_HAS_CUDA : '',
                     US4_HAS_DIRECTML : '',
                     US4_HAS_VULKAN : '',
                     US4_HAS_NPU : '',
                     US4_PROFILE_STORE_PATH : storePath,
                 },
             );

             await attachProcessOutput(testInfo, 'bench-text', stdout, stderr);

             expect(exitCode).toBe(0);
             expect(stderr).toBe('');
             expect(stdout).toContain('execution: bench');
             expect(stdout).toContain('bench.selected_profile: cpu-only');
             expect(stdout).toContain('bench.persisted: no');
             expect(stdout).toContain('bench_status: completed');
             expect(existsSync(storePath)).toBeFalsy();
         });

    test('renders the serve scaffold as json', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [ 'serve', '--model', 'qwen-0.5b', '--backend', 'windows-ml', '--format', 'json' ],
            {
                ...process.env,
                US4_HAS_CUDA : '',
                US4_HAS_DIRECTML : '',
                US4_HAS_VULKAN : '1',
                US4_HAS_NPU : '',
                US4_GPU_NAME : 'Serve Test GPU',
                US4_GPU_VENDOR : 'amd',
                US4_GPU_CLASS : 'discrete',
                US4_DEVICE_GIB : '8',
            },
        );

        await attachProcessOutput(testInfo, 'serve-json', stdout, stderr);

        expect(exitCode).toBe(2);
        const payload = JSON.parse(stdout) as
        {
            execution: string;
            status: string;
            backend: string;
        };
        expect(payload.execution).toBe('serve');
        expect(payload.status).toBe('scaffold-only');
        expect(payload.backend).toBe('windows-ml');
        expect(stderr).toContain('Serve pipeline scaffolding is ready');
    });

    test('rejects invalid bench output formats', async ({}, testInfo) => {
        const cliPath = await requireCliBinary(testInfo);

        const {stdout, stderr, exitCode} = await runCli(
            cliPath,
            [ 'bench', '--model', 'qwen-0.5b', '--format', 'xml' ],
            process.env,
        );

        await attachProcessOutput(testInfo, 'bench-invalid-format', stdout, stderr);

        expect(exitCode).toBe(1);
        expect(stdout).toContain('Usage:');
        expect(stderr).toContain('Unknown value for --format.');
    });

    test('reuses a tuned cpu-only profile in a later bench matrix on the same host',
         async ({}, testInfo) => {
             const cliPath = await requireCliBinary(testInfo);
             const storePath = testInfo.outputPath('shared-profiles.json');
             const env = {
                 ...process.env,
                 US4_HAS_CUDA : '',
                 US4_HAS_DIRECTML : '1',
                 US4_HAS_VULKAN : '',
                 US4_HAS_NPU : '',
                 US4_GPU_NAME : 'Intel Arc Test',
                 US4_GPU_VENDOR : 'intel',
                 US4_GPU_CLASS : 'integrated',
                 US4_DEVICE_GIB : '8',
                 US4_PROFILE_STORE_PATH : storePath,
             };

             const tune = await runCli(
                 cliPath,
                 [ 'tune', '--model', 'qwen-0.5b', '--backend', 'cpu', '--mode', 'cpu-only' ],
                 env,
             );
             await attachProcessOutput(testInfo, 'tune-reuse', tune.stdout, tune.stderr);

             const bench = await runCli(
                 cliPath,
                 [ 'bench', '--model', 'qwen-0.5b', '--format', 'json' ],
                 env,
             );
             await attachProcessOutput(testInfo, 'bench-reuse', bench.stdout, bench.stderr);

             expect(tune.exitCode).toBe(0);
             expect(bench.exitCode).toBe(0);
             const payload = JSON.parse(bench.stdout) as
             {
                 recommended_profile: string;
                 selected_profile: string;
                 selected_backend: string;
                 persisted: boolean;
             };
             expect(payload.recommended_profile).toBe('cpu-only');
             expect(payload.selected_profile).toBe('cpu-only');
             expect(payload.selected_backend).toBe('auto');
             expect(payload.persisted).toBeFalsy();
         });

    test('builds a portable zip release artifact', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('portable-dist');
        const zipPath = path.join(outputDir, `us4-v6-windows-${packageVersion}-portable.zip`);
        const scriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
        const cliBuildDir = path.resolve(process.cwd(), 'build');

        mkdirSync(outputDir, {recursive : true});

        const build = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            scriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
        ]);

        await attachProcessOutput(testInfo, 'portable-zip', build.stdout, build.stderr);

        expect(build.exitCode).toBe(0);
        expect(existsSync(zipPath)).toBeTruthy();

        const listEntries = await runPowerShell([
            '-NoProfile',
            '-Command',
            [
                'Add-Type -AssemblyName System.IO.Compression.FileSystem;',
                `$zip = [System.IO.Compression.ZipFile]::OpenRead('${escapePowerShellSingleQuoted(zipPath)}');`,
                '$zip.Entries | ForEach-Object { $_.FullName };',
                '$zip.Dispose();'
            ].join(' '),
        ]);

        await attachProcessOutput(testInfo, 'portable-zip-entries', listEntries.stdout, listEntries.stderr);

        expect(listEntries.exitCode).toBe(0);
        expect(listEntries.stdout).toContain('us4-cli.exe');
        expect(listEntries.stdout).toContain('README.md');
        expect(listEntries.stdout).toContain('README.pt-BR.md');
        expect(listEntries.stdout).toContain('CHANGELOG.md');
    });

    test('installs PowerShell completions idempotently into a temporary profile',
         async ({}, testInfo) => {
             const profilePath = testInfo.outputPath('Microsoft.PowerShell_profile.ps1');
             const scriptPath = path.resolve(process.cwd(), 'scripts', 'install-completions.ps1');
             const completionPath = path.resolve(process.cwd(), 'scripts', 'completions', 'us4-cli.ps1');

             mkdirSync(path.dirname(profilePath), {recursive : true});

             const install = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-Command',
                 [
                     `$PROFILE = '${escapePowerShellSingleQuoted(profilePath)}';`,
                     `& '${escapePowerShellSingleQuoted(scriptPath)}';`,
                     `& '${escapePowerShellSingleQuoted(scriptPath)}';`,
                     'Get-Content $PROFILE -Raw'
                 ].join(' '),
             ]);

             await attachProcessOutput(testInfo, 'install-completions', install.stdout, install.stderr);

             expect(install.exitCode).toBe(0);
             expect(existsSync(profilePath)).toBeTruthy();
             const profileContent = readFileSync(profilePath, 'utf8');
             expect(profileContent).toContain(completionPath);
             expect(profileContent.split(completionPath).length - 1).toBe(1);
         });

    test('builds an msix package when MakeAppx is present or fails with a clear prerequisite',
         async ({}, testInfo) => {
             const outputDir = testInfo.outputPath('msix-dist');
             const scriptPath = path.resolve(process.cwd(), 'scripts', 'build-msix.ps1');
             const cliBuildDir = path.resolve(process.cwd(), 'build');

             mkdirSync(outputDir, {recursive : true});

             const result = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 scriptPath,
                 '-BuildDir',
                 cliBuildDir,
                 '-OutputDir',
                 outputDir,
             ]);

             await attachProcessOutput(testInfo, 'build-msix', result.stdout, result.stderr);

             if (result.exitCode === 0)
             {
                 expect(result.stdout).toContain('Unsigned MSIX created at');
                 expect(result.stdout).toContain('.msix');
                 return;
             }

             expect(`${result.stdout}\n${result.stderr}`).toContain('MakeAppx.exe not found');
         });

    test('generates SHA256 checksums for release artifacts', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('checksum-dist');
        const buildScriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
        const checksumScriptPath = path.resolve(process.cwd(), 'scripts', 'generate-checksums.ps1');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const checksumsPath = path.join(outputDir, 'SHA256SUMS.txt');

        mkdirSync(outputDir, {recursive : true});

        const build = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            buildScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'checksum-build-portable', build.stdout, build.stderr);
        expect(build.exitCode).toBe(0);

        const checksum = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            checksumScriptPath,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'checksum-generate', checksum.stdout, checksum.stderr);

        expect(checksum.exitCode).toBe(0);
        expect(existsSync(checksumsPath)).toBeTruthy();
        const contents = readFileSync(checksumsPath, 'utf8');
        expect(contents).toContain(`us4-v6-windows-${packageVersion}-portable.zip`);
    });

    test('runs post-publish smoke against the portable zip artifact', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('post-publish-dist');
        const buildScriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
        const smokeScriptPath = path.resolve(process.cwd(), 'scripts', 'post-publish-smoke.ps1');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const zipPath = path.join(outputDir, `us4-v6-windows-${packageVersion}-portable.zip`);
        const workingDir = path.join(outputDir, 'smoke-work');

        mkdirSync(outputDir, {recursive : true});

        const build = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            buildScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'post-publish-build-portable', build.stdout, build.stderr);
        expect(build.exitCode).toBe(0);

        const smoke = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            smokeScriptPath,
            '-ArtifactPath',
            zipPath,
            '-WorkingDir',
            workingDir,
        ]);
        await attachProcessOutput(testInfo, 'post-publish-smoke', smoke.stdout, smoke.stderr);

        expect(smoke.exitCode).toBe(0);
        expect(smoke.stdout).toContain('Portable zip smoke passed');
    });

    test('runs post-publish smoke against an MSIX artifact in dev-only mode or fails with a clear local prerequisite',
         async ({}, testInfo) => {
             const outputDir = testInfo.outputPath('post-publish-msix-dist');
             const buildScriptPath = path.resolve(process.cwd(), 'scripts', 'build-msix.ps1');
             const smokeScriptPath = path.resolve(process.cwd(), 'scripts', 'post-publish-smoke.ps1');
             const cliBuildDir = path.resolve(process.cwd(), 'build');
             const workingDir = path.join(outputDir, 'msix-smoke-work');

             mkdirSync(outputDir, {recursive : true});

             const build = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 buildScriptPath,
                 '-BuildDir',
                 cliBuildDir,
                 '-OutputDir',
                 outputDir,
             ]);
             await attachProcessOutput(testInfo, 'post-publish-build-msix', build.stdout, build.stderr);

             if (build.exitCode !== 0)
             {
                 expect(`${build.stdout}\n${build.stderr}`).toContain('MakeAppx.exe not found');
                 return;
             }

             const msixEntry = readdirSync(outputDir).find((entry) => entry.toLowerCase().endsWith('.msix'));
             if (!msixEntry)
             {
                 expect(build.stdout).toContain('.msix');
                 return;
             }
             const msixPath = path.join(outputDir, msixEntry);

             const smoke = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 smokeScriptPath,
                 '-ArtifactPath',
                 msixPath,
                 '-WorkingDir',
                 workingDir,
                 '-EnableDevMsixSmoke',
                 '-DevCertificatePassword',
                 'us4-dev-pass',
             ]);
             await attachProcessOutput(testInfo, 'post-publish-msix-smoke', smoke.stdout, smoke.stderr);

             if (smoke.exitCode === 0)
             {
                 expect(smoke.stdout).toContain('Dev MSIX smoke passed');
                 return;
             }

             expect(`${smoke.stdout}\n${smoke.stderr}`).toMatch(
                 /signtool\.exe not found|Add-AppxPackage is not available|Get-AppxPackage is not available|Local dev MSIX signing prerequisites are unavailable|MSIX package is not signed with a trusted certificate/
             );
         });

    test('renders and validates publishable winget manifests from repository templates', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('winget-manifests');
        const renderScriptPath = path.resolve(process.cwd(), 'scripts', 'render-winget-manifests.ps1');
        const validateScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-winget-manifests.ps1');
        const portableUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}-portable.zip`;
        const msixUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}.0.msix`;

        mkdirSync(outputDir, {recursive : true});

        const renderResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderScriptPath,
            '-Version',
            packageVersion,
            '-PortableUrl',
            portableUrl,
            '-MsixUrl',
            msixUrl,
            '-OutputDir',
            outputDir,
        ]);

        await attachProcessOutput(testInfo, 'winget-render', renderResult.stdout, renderResult.stderr);

        expect(renderResult.exitCode).toBe(0);
        expect(existsSync(path.join(outputDir, 'default.yaml'))).toBeTruthy();
        expect(existsSync(path.join(outputDir, 'installer.yaml'))).toBeTruthy();
        expect(existsSync(path.join(outputDir, 'locale.en-US.yaml'))).toBeTruthy();
        expect(readFileSync(path.join(outputDir, 'installer.yaml'), 'utf8')).toContain(portableUrl);

        const validateResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            validateScriptPath,
            '-ManifestDir',
            outputDir,
            '-ExpectedVersion',
            packageVersion,
            '-RequirePublishableUrls',
            '-Format',
            'json',
        ]);

        await attachProcessOutput(testInfo, 'winget-validate', validateResult.stdout, validateResult.stderr);

        expect(validateResult.exitCode).toBe(0);
        const payload = JSON.parse(validateResult.stdout) as
        {
            execution: string;
            status: string;
            installer_urls: string[];
            issue_codes: string[];
        };
        expect(payload.execution).toBe('validate-winget-manifests');
        expect(payload.status).toBe('ready');
        expect(payload.installer_urls).toContain(portableUrl);
        expect(payload.issue_codes).toHaveLength(0);
    });

    test('fails winget manifest validation when placeholder URLs remain', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('winget-placeholder-manifests');
        const renderScriptPath = path.resolve(process.cwd(), 'scripts', 'render-winget-manifests.ps1');
        const validateScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-winget-manifests.ps1');

        mkdirSync(outputDir, {recursive : true});

        const renderResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderScriptPath,
            '-Version',
            packageVersion,
            '-PortableUrl',
            `https://example.invalid/us4-v6-windows-${packageVersion}-portable.zip`,
            '-MsixUrl',
            `https://example.invalid/us4-v6-windows-${packageVersion}.0.msix`,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'winget-render-placeholder', renderResult.stdout, renderResult.stderr);

        expect(renderResult.exitCode).toBe(0);

        const validateResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            validateScriptPath,
            '-ManifestDir',
            outputDir,
            '-ExpectedVersion',
            packageVersion,
            '-RequirePublishableUrls',
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'winget-validate-placeholder', validateResult.stdout, validateResult.stderr);

        expect(validateResult.exitCode).toBe(1);
        const payload = JSON.parse(validateResult.stdout) as
        {
            status: string;
            issue_codes: string[];
        };
        expect(payload.status).toBe('blocked');
        expect(payload.issue_codes).toContain('installer_url_not_publishable');
    });

    test('validates the generated release asset set against checksums and winget manifests',
         async ({}, testInfo) => {
             const outputDir = testInfo.outputPath('release-assets-dist');
             const manifestDir = testInfo.outputPath('release-assets-winget');
             const cliBuildDir = path.resolve(process.cwd(), 'build');
             const portableScriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
             const checksumScriptPath = path.resolve(process.cwd(), 'scripts', 'generate-checksums.ps1');
             const renderScriptPath = path.resolve(process.cwd(), 'scripts', 'render-winget-manifests.ps1');
             const validateScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-release-assets.ps1');
             const portableUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}-portable.zip`;
             const msixUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}.0.msix`;

             mkdirSync(outputDir, {recursive : true});
             mkdirSync(manifestDir, {recursive : true});

             const build = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 portableScriptPath,
                 '-BuildDir',
                 cliBuildDir,
                 '-OutputDir',
                 outputDir,
             ]);
             await attachProcessOutput(testInfo, 'release-assets-build-portable', build.stdout, build.stderr);
             expect(build.exitCode).toBe(0);

             const checksum = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 checksumScriptPath,
                 '-OutputDir',
                 outputDir,
             ]);
             await attachProcessOutput(testInfo, 'release-assets-checksums', checksum.stdout, checksum.stderr);
             expect(checksum.exitCode).toBe(0);

             const render = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 renderScriptPath,
                 '-Version',
                 packageVersion,
                 '-PortableUrl',
                 portableUrl,
                 '-MsixUrl',
                 msixUrl,
                 '-OutputDir',
                 manifestDir,
             ]);
             await attachProcessOutput(testInfo, 'release-assets-winget-render', render.stdout, render.stderr);
             expect(render.exitCode).toBe(0);

             const validate = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 validateScriptPath,
                 '-OutputDir',
                 outputDir,
                 '-ManifestDir',
                 manifestDir,
                 '-ExpectedVersion',
                 packageVersion,
                 '-Format',
                 'json',
             ]);
             await attachProcessOutput(testInfo, 'release-assets-validate', validate.stdout, validate.stderr);

             expect(validate.exitCode).toBe(0);
             const payload = JSON.parse(validate.stdout) as
             {
                 execution: string;
                 status: string;
                 artifacts: string[];
                 issue_codes: string[];
             };
             expect(payload.execution).toBe('validate-release-assets');
             expect(payload.status).toBe('ready');
             expect(payload.artifacts).toContain(`us4-v6-windows-${packageVersion}-portable.zip`);
             expect(payload.artifacts).toContain('SHA256SUMS.txt');
             expect(payload.issue_codes).toHaveLength(0);
         });

    test('validates a publishable release layout without staging directories', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('publish-layout-clean');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const portableScriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
        const checksumScriptPath = path.resolve(process.cwd(), 'scripts', 'generate-checksums.ps1');
        const layoutScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-publish-layout.ps1');

        mkdirSync(outputDir, {recursive : true});

        const build = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            portableScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'publish-layout-build-portable', build.stdout, build.stderr);
        expect(build.exitCode).toBe(0);

        const checksum = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            checksumScriptPath,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'publish-layout-checksums', checksum.stdout, checksum.stderr);
        expect(checksum.exitCode).toBe(0);

        writeFileSync(path.join(outputDir, 'release-manifest.json'), '{}', 'utf8');
        writeFileSync(path.join(outputDir, 'release-notes.md'), '# notes', 'utf8');

        const validate = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            layoutScriptPath,
            '-OutputDir',
            outputDir,
            '-ExpectedVersion',
            packageVersion,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'publish-layout-clean-validate', validate.stdout, validate.stderr);

        expect(validate.exitCode).toBe(0);
        const payload = JSON.parse(validate.stdout) as
        {
            execution: string;
            status: string;
            directories: string[];
            issue_codes: string[];
        };
        expect(payload.execution).toBe('validate-publish-layout');
        expect(payload.status).toBe('ready');
        expect(payload.directories).toHaveLength(0);
        expect(payload.issue_codes).toHaveLength(0);
    });

    test('fails publish layout validation when staging directories leak into the release output', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('publish-layout-dirty');
        const layoutScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-publish-layout.ps1');

        mkdirSync(path.join(outputDir, 'msix-staging'), {recursive : true});
        writeFileSync(path.join(outputDir, `us4-v6-windows-${packageVersion}-portable.zip`), 'zip', 'utf8');
        writeFileSync(path.join(outputDir, 'SHA256SUMS.txt'), 'checksum', 'utf8');

        const validate = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            layoutScriptPath,
            '-OutputDir',
            outputDir,
            '-ExpectedVersion',
            packageVersion,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'publish-layout-dirty-validate', validate.stdout, validate.stderr);

        expect(validate.exitCode).toBe(1);
        const payload = JSON.parse(validate.stdout) as
        {
            status: string;
            directories: string[];
            issue_codes: string[];
        };
        expect(payload.status).toBe('blocked');
        expect(payload.directories).toContain('msix-staging');
        expect(payload.issue_codes).toContain('unexpected_directory_present');
    });

    test('fails release asset validation when checksums are missing', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('release-assets-missing-checksums');
        const manifestDir = testInfo.outputPath('release-assets-missing-checksums-winget');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const portableScriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
        const renderScriptPath = path.resolve(process.cwd(), 'scripts', 'render-winget-manifests.ps1');
        const validateScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-release-assets.ps1');
        const portableUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}-portable.zip`;
        const msixUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}.0.msix`;

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(manifestDir, {recursive : true});

        const build = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            portableScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'release-assets-missing-checksums-build', build.stdout, build.stderr);
        expect(build.exitCode).toBe(0);

        const render = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderScriptPath,
            '-Version',
            packageVersion,
            '-PortableUrl',
            portableUrl,
            '-MsixUrl',
            msixUrl,
            '-OutputDir',
            manifestDir,
        ]);
        await attachProcessOutput(testInfo, 'release-assets-missing-checksums-render', render.stdout, render.stderr);
        expect(render.exitCode).toBe(0);

        const validate = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            validateScriptPath,
            '-OutputDir',
            outputDir,
            '-ManifestDir',
            manifestDir,
            '-ExpectedVersion',
            packageVersion,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'release-assets-missing-checksums-validate', validate.stdout, validate.stderr);

        expect(validate.exitCode).toBe(1);
        const payload = JSON.parse(validate.stdout) as
        {
            status: string;
            issue_codes: string[];
        };
        expect(payload.status).toBe('blocked');
        expect(payload.issue_codes).toContain('checksums_missing');
    });

    test('renders a release manifest from artifacts, checksums, and winget urls', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('release-manifest-dist');
        const manifestDir = testInfo.outputPath('release-manifest-winget');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const portableScriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
        const checksumScriptPath = path.resolve(process.cwd(), 'scripts', 'generate-checksums.ps1');
        const renderWingetScriptPath = path.resolve(process.cwd(), 'scripts', 'render-winget-manifests.ps1');
        const renderManifestScriptPath = path.resolve(process.cwd(), 'scripts', 'render-release-manifest.ps1');
        const portableUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}-portable.zip`;
        const msixUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}.0.msix`;
        const releaseManifestPath = path.join(outputDir, 'release-manifest.json');

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(manifestDir, {recursive : true});

        const build = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            portableScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'release-manifest-build-portable', build.stdout, build.stderr);
        expect(build.exitCode).toBe(0);

        const checksum = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            checksumScriptPath,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'release-manifest-checksums', checksum.stdout, checksum.stderr);
        expect(checksum.exitCode).toBe(0);

        const renderWinget = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderWingetScriptPath,
            '-Version',
            packageVersion,
            '-PortableUrl',
            portableUrl,
            '-MsixUrl',
            msixUrl,
            '-OutputDir',
            manifestDir,
        ]);
        await attachProcessOutput(testInfo, 'release-manifest-winget-render', renderWinget.stdout, renderWinget.stderr);
        expect(renderWinget.exitCode).toBe(0);

        const renderManifest = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderManifestScriptPath,
            '-OutputDir',
            outputDir,
            '-ManifestDir',
            manifestDir,
            '-ExpectedVersion',
            packageVersion,
            '-OutputPath',
            releaseManifestPath,
        ]);
        await attachProcessOutput(testInfo, 'release-manifest-render', renderManifest.stdout, renderManifest.stderr);
        expect(renderManifest.exitCode).toBe(0);
        expect(existsSync(releaseManifestPath)).toBeTruthy();

        const payload = JSON.parse(readFileSync(releaseManifestPath, 'utf8')) as
        {
            version: string;
            installer_urls: {
                portable: string | null;
                msix: string | null;
            };
            artifacts: Array<{
                name: string;
                type: string;
                sha256: string;
                url: string | null;
            }>;
        };
        expect(payload.version).toBe(packageVersion);
        expect(payload.installer_urls.portable).toBe(portableUrl);
        expect(payload.installer_urls.msix).toBe(msixUrl);
        expect(payload.artifacts.some((artifact) => artifact.name === `us4-v6-windows-${packageVersion}-portable.zip` &&
                                                     artifact.type === 'portable-zip' &&
                                                     artifact.url === portableUrl &&
                                                     artifact.sha256.length === 64)).toBeTruthy();
    });

    test('renders release notes from changelog and release manifest', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('release-notes-dist');
        const manifestDir = testInfo.outputPath('release-notes-winget');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const portableScriptPath = path.resolve(process.cwd(), 'scripts', 'build-portable-zip.ps1');
        const checksumScriptPath = path.resolve(process.cwd(), 'scripts', 'generate-checksums.ps1');
        const renderWingetScriptPath = path.resolve(process.cwd(), 'scripts', 'render-winget-manifests.ps1');
        const renderManifestScriptPath = path.resolve(process.cwd(), 'scripts', 'render-release-manifest.ps1');
        const renderNotesScriptPath = path.resolve(process.cwd(), 'scripts', 'render-release-notes.ps1');
        const portableUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}-portable.zip`;
        const msixUrl = `https://github.com/wesleysimplicio/us4-v6-simplicio-windows/releases/download/v${packageVersion}/us4-v6-windows-${packageVersion}.0.msix`;
        const releaseManifestPath = path.join(outputDir, 'release-manifest.json');
        const releaseNotesPath = path.join(outputDir, 'release-notes.md');

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(manifestDir, {recursive : true});

        const build = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            portableScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'release-notes-build-portable', build.stdout, build.stderr);
        expect(build.exitCode).toBe(0);

        const checksum = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            checksumScriptPath,
            '-OutputDir',
            outputDir,
        ]);
        await attachProcessOutput(testInfo, 'release-notes-checksums', checksum.stdout, checksum.stderr);
        expect(checksum.exitCode).toBe(0);

        const renderWinget = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderWingetScriptPath,
            '-Version',
            packageVersion,
            '-PortableUrl',
            portableUrl,
            '-MsixUrl',
            msixUrl,
            '-OutputDir',
            manifestDir,
        ]);
        await attachProcessOutput(testInfo, 'release-notes-winget-render', renderWinget.stdout, renderWinget.stderr);
        expect(renderWinget.exitCode).toBe(0);

        const renderManifest = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderManifestScriptPath,
            '-OutputDir',
            outputDir,
            '-ManifestDir',
            manifestDir,
            '-ExpectedVersion',
            packageVersion,
            '-OutputPath',
            releaseManifestPath,
        ]);
        await attachProcessOutput(testInfo, 'release-notes-release-manifest', renderManifest.stdout, renderManifest.stderr);
        expect(renderManifest.exitCode).toBe(0);

        const renderNotes = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            renderNotesScriptPath,
            '-Version',
            packageVersion,
            '-ChangelogPath',
            path.resolve(process.cwd(), 'CHANGELOG.md'),
            '-ReleaseManifestPath',
            releaseManifestPath,
            '-OutputPath',
            releaseNotesPath,
        ]);
        await attachProcessOutput(testInfo, 'release-notes-render', renderNotes.stdout, renderNotes.stderr);
        expect(renderNotes.exitCode).toBe(0);
        expect(existsSync(releaseNotesPath)).toBeTruthy();

        const content = readFileSync(releaseNotesPath, 'utf8');
        expect(content).toContain(`# Release ${packageVersion}`);
        expect(content).toContain('## Highlights');
        expect(content).toContain('## Artifacts');
        expect(content).toContain(`us4-v6-windows-${packageVersion}-portable.zip`);
    });

    test('executes the local release dry-run and exports a structured summary', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('release-dry-run-dist');
        const manifestDir = testInfo.outputPath('release-dry-run-winget');
        const workingDir = testInfo.outputPath('release-dry-run-working');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const releaseDryRunScriptPath = path.resolve(process.cwd(), 'scripts', 'release-dry-run.ps1');

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(manifestDir, {recursive : true});
        mkdirSync(workingDir, {recursive : true});

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            releaseDryRunScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
            '-ManifestDir',
            manifestDir,
            '-WorkingDir',
            workingDir,
            '-Version',
            packageVersion,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'release-dry-run', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        const payload = JSON.parse(result.stdout) as
        {
            execution: string;
            status: string;
            version: string;
            tag: string;
            dev_msix_requested: boolean;
            dev_msix_preflight: {
                execution: string;
                status: string;
            };
            artifact_names: string[];
            issue_codes: string[];
            steps: Array<{
                name: string;
                status: string;
                detail: string;
            }>;
        };
        expect(payload.execution).toBe('release-dry-run');
        expect(payload.status).toBe('ready');
        expect(payload.version).toBe(packageVersion);
        expect(payload.tag).toBe(`v${packageVersion}`);
        expect(payload.dev_msix_requested).toBeFalsy();
        expect(payload.dev_msix_preflight.execution).toBe('dev-msix-smoke');
        expect(['ready', 'blocked']).toContain(payload.dev_msix_preflight.status);
        expect(payload.artifact_names).toContain(`us4-v6-windows-${packageVersion}-portable.zip`);
        expect(payload.artifact_names).toContain('SHA256SUMS.txt');
        expect(payload.issue_codes).toHaveLength(0);
        expect(payload.steps.some((step) => step.name === 'preflight' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'validate-release-tag' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'build-portable-zip' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'generate-checksums' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'render-winget-manifests' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'validate-release-assets' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'validate-publish-layout' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'render-release-notes' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'portable-smoke' &&
                                            step.status === 'passed')).toBeTruthy();
        expect(payload.steps.some((step) => step.name === 'build-msix' &&
                                            (step.status === 'passed' || step.status === 'skipped'))).toBeTruthy();
        expect(payload.steps.some((step) => step.status === 'failed')).toBeFalsy();
    });

    test('blocks the local release dry-run when the release tag does not match the version', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('release-dry-run-tag-mismatch-dist');
        const manifestDir = testInfo.outputPath('release-dry-run-tag-mismatch-winget');
        const workingDir = testInfo.outputPath('release-dry-run-tag-mismatch-working');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const releaseDryRunScriptPath = path.resolve(process.cwd(), 'scripts', 'release-dry-run.ps1');

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(manifestDir, {recursive : true});
        mkdirSync(workingDir, {recursive : true});

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            releaseDryRunScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
            '-ManifestDir',
            manifestDir,
            '-WorkingDir',
            workingDir,
            '-Version',
            packageVersion,
            '-Tag',
            'v0.0.0',
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'release-dry-run-tag-mismatch', result.stdout, result.stderr);

        expect(result.exitCode).not.toBe(0);
        const payload = JSON.parse(result.stdout) as
        {
            execution: string;
            status: string;
            issue_codes: string[];
            steps: Array<{
                name: string;
                status: string;
                detail: string;
            }>;
        };
        expect(payload.execution).toBe('release-dry-run');
        expect(payload.status).toBe('blocked');
        expect(payload.issue_codes).toContain('validate-release-tag:Release tag validation failed.');
        expect(payload.steps.some((step) => step.name === 'validate-release-tag' &&
                                            step.status === 'failed' &&
                                            step.detail === 'Release tag validation failed.')).toBeTruthy();
    });

    test('keeps release dry-run manifests ephemeral when no manifest directory is provided', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('release-dry-run-default-manifests-dist');
        const workingDir = testInfo.outputPath('release-dry-run-default-manifests-working');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const releaseDryRunScriptPath = path.resolve(process.cwd(), 'scripts', 'release-dry-run.ps1');
        const repoManifestDir = path.resolve(process.cwd(), 'packaging', 'winget', 'manifests');

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(workingDir, {recursive : true});

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            releaseDryRunScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
            '-WorkingDir',
            workingDir,
            '-Version',
            packageVersion,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'release-dry-run-default-manifests', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        const payload = JSON.parse(result.stdout) as
        {
            status: string;
            manifest_dir: string;
        };
        expect(payload.status).toBe('ready');
        expect(payload.manifest_dir).toBe(path.join(workingDir, 'winget-manifests'));
        expect(existsSync(path.join(workingDir, 'winget-manifests', 'installer.yaml'))).toBeTruthy();
        expect(existsSync(repoManifestDir)).toBeFalsy();
    });

    test('renders consolidated project status from planning, release, and evidence signals', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('project-status-dist');
        const manifestDir = testInfo.outputPath('project-status-winget');
        const reportDir = testInfo.outputPath('project-status-playwright-report');
        const testResultsDir = testInfo.outputPath('project-status-test-results');
        const markdownPath = testInfo.outputPath('project-status.md');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const projectStatusScriptPath = path.resolve(process.cwd(), 'scripts', 'render-project-status.ps1');

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(manifestDir, {recursive : true});
        mkdirSync(reportDir, {recursive : true});
        mkdirSync(testResultsDir, {recursive : true});
        writeFileSync(path.join(reportDir, 'index.html'), '<html><body>report</body></html>', 'utf8');
        mkdirSync(path.join(testResultsDir, 'spec-a'), {recursive : true});
        writeFileSync(path.join(testResultsDir, 'spec-a', 'trace.zip'), 'trace', 'utf8');
        writeFileSync(path.join(testResultsDir, 'spec-a', 'shot.png'), 'png', 'utf8');
        writeFileSync(path.join(testResultsDir, 'spec-a', 'video.webm'), 'webm', 'utf8');

        const jsonResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            projectStatusScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
            '-ManifestDir',
            manifestDir,
            '-PlaywrightReportDir',
            reportDir,
            '-TestResultsDir',
            testResultsDir,
            '-RequireEvidence',
            '-IncludeReleaseDryRun',
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'project-status-json', jsonResult.stdout, jsonResult.stderr);

        expect(jsonResult.exitCode).toBe(0);
        const payload = JSON.parse(jsonResult.stdout) as
        {
            execution: string;
            status: string;
            planning: {
                sprint_count: number;
                total_tasks: number;
                done_tasks: number;
                remaining_tasks: number;
                sprints: Array<{sprint: string; total_tasks: number; done_tasks: number; remaining_tasks: number;}>;
            };
            release: {
                preflight: {
                    execution: string;
                    status: string;
                    package_version: string;
                    cmake_version: string;
                    has_cli_binary: boolean;
                };
                dev_msix_preflight: {
                    execution: string;
                    status: string;
                };
                dry_run: {
                    execution: string;
                    status: string;
                    steps: Array<{status: string;}>;
                } | null;
            };
            evidence: {
                has_html_report: boolean;
                trace_count: number;
                screenshot_count: number;
                video_count: number;
            };
            issue_codes: string[];
        };
        expect(payload.execution).toBe('project-status');
        expect(payload.status).not.toBe('blocked');
        expect(payload.planning.sprint_count).toBe(payload.planning.sprints.length);
        expect(payload.planning.total_tasks).toBe(payload.planning.done_tasks + payload.planning.remaining_tasks);
        expect(payload.release.preflight.execution).toBe('release-preflight');
        expect(payload.release.preflight.package_version).toBe(packageVersion);
        expect(payload.release.preflight.cmake_version).toBe(packageVersion);
        expect(payload.release.preflight.has_cli_binary).toBeTruthy();
        expect(payload.release.dev_msix_preflight.execution).toBe('dev-msix-smoke');
        expect(['ready', 'blocked']).toContain(payload.release.dev_msix_preflight.status);
        expect(payload.release.dry_run?.execution).toBe('release-dry-run');
        expect(payload.release.dry_run?.status).toBe('ready');
        expect(payload.release.dry_run?.steps.some((step) => step.status === 'failed')).toBeFalsy();
        expect(payload.evidence.has_html_report).toBeTruthy();
        expect(payload.evidence.trace_count).toBeGreaterThan(0);
        expect(payload.evidence.screenshot_count).toBeGreaterThan(0);
        expect(payload.evidence.video_count).toBeGreaterThan(0);
        expect(Array.isArray(payload.issue_codes)).toBeTruthy();

        const markdownResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            projectStatusScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-PlaywrightReportDir',
            reportDir,
            '-TestResultsDir',
            testResultsDir,
            '-OutputPath',
            markdownPath,
            '-Format',
            'markdown',
        ]);
        await attachProcessOutput(testInfo, 'project-status-markdown', markdownResult.stdout, markdownResult.stderr);

        expect(markdownResult.exitCode).toBe(0);
        expect(existsSync(markdownPath)).toBeTruthy();
        const markdown = readFileSync(markdownPath, 'utf8');
        expect(markdown).toContain('# Project Status');
        expect(markdown).toContain('## Planning');
        expect(markdown).toContain('## Release');
        expect(markdown).toContain('## Evidence');
        expect(markdown).toContain('## Remaining Work');
        expect(markdown).toContain('Dev MSIX preflight:');
    });

    test('keeps project-status release dry-run manifests ephemeral by default', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('project-status-default-manifests-dist');
        const reportDir = testInfo.outputPath('project-status-default-manifests-report');
        const testResultsDir = testInfo.outputPath('project-status-default-manifests-results');
        const cliBuildDir = path.resolve(process.cwd(), 'build');
        const projectStatusScriptPath = path.resolve(process.cwd(), 'scripts', 'render-project-status.ps1');
        const repoManifestDir = path.resolve(process.cwd(), 'packaging', 'winget', 'manifests');

        mkdirSync(outputDir, {recursive : true});
        mkdirSync(reportDir, {recursive : true});
        mkdirSync(testResultsDir, {recursive : true});
        writeFileSync(path.join(reportDir, 'index.html'), '<html><body>report</body></html>', 'utf8');
        mkdirSync(path.join(testResultsDir, 'spec-a'), {recursive : true});
        writeFileSync(path.join(testResultsDir, 'spec-a', 'trace.zip'), 'trace', 'utf8');
        writeFileSync(path.join(testResultsDir, 'spec-a', 'shot.png'), 'png', 'utf8');
        writeFileSync(path.join(testResultsDir, 'spec-a', 'video.webm'), 'webm', 'utf8');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            projectStatusScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-OutputDir',
            outputDir,
            '-PlaywrightReportDir',
            reportDir,
            '-TestResultsDir',
            testResultsDir,
            '-IncludeReleaseDryRun',
            '-RequireEvidence',
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'project-status-default-manifests', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        const payload = JSON.parse(result.stdout) as
        {
            release: {
                dry_run: {
                    manifest_dir: string;
                } | null;
            };
        };
        expect(payload.release.dry_run?.manifest_dir).not.toBe(repoManifestDir);
        expect(payload.release.dry_run?.manifest_dir).toContain('winget-manifests');
        expect(existsSync(repoManifestDir)).toBeFalsy();
    });

    test('validates release tag against the current package version', async ({}, testInfo) => {
        const validateTagScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-release-tag.ps1');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            validateTagScriptPath,
            '-Tag',
            `v${packageVersion}`,
            '-ExpectedVersion',
            packageVersion,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'release-tag-validate', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        const payload = JSON.parse(result.stdout) as
        {
            execution: string;
            status: string;
            issue_codes: string[];
        };
        expect(payload.execution).toBe('validate-release-tag');
        expect(payload.status).toBe('ready');
        expect(payload.issue_codes).toHaveLength(0);
    });

    test('fails release tag validation when the tag mismatches the package version', async ({}, testInfo) => {
        const validateTagScriptPath = path.resolve(process.cwd(), 'scripts', 'validate-release-tag.ps1');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            validateTagScriptPath,
            '-Tag',
            'v9.9.9',
            '-ExpectedVersion',
            packageVersion,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'release-tag-validate-mismatch', result.stdout, result.stderr);

        expect(result.exitCode).toBe(1);
        const payload = JSON.parse(result.stdout) as
        {
            status: string;
            issue_codes: string[];
        };
        expect(payload.status).toBe('blocked');
        expect(payload.issue_codes).toContain('tag_version_mismatch');
    });

    test('exports planning status as json from versioned sprint files', async ({}, testInfo) => {
        const planningStatusScriptPath = path.resolve(process.cwd(), 'scripts', 'render-planning-status.ps1');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            planningStatusScriptPath,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'planning-status-json', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        const payload = JSON.parse(result.stdout) as
        {
            sprint_count: number;
            total_tasks: number;
            done_tasks: number;
            remaining_tasks: number;
            sprints: Array<{
                sprint: string;
                status: string;
                total_tasks: number;
                done_tasks: number;
                remaining_tasks: number;
            }>;
        };
        expect(payload.sprint_count).toBe(12);
        expect(payload.total_tasks).toBe(88);
        expect(payload.done_tasks).toBe(6);
        expect(payload.remaining_tasks).toBe(82);
        expect(payload.sprints.some((entry) => entry.sprint === 'sprint-12' &&
                                               entry.status === 'in_progress' &&
                                               entry.done_tasks === 6 &&
                                               entry.remaining_tasks === 2)).toBeTruthy();
    });

    test('renders planning status markdown artifact', async ({}, testInfo) => {
        const planningStatusScriptPath = path.resolve(process.cwd(), 'scripts', 'render-planning-status.ps1');
        const outputPath = testInfo.outputPath('planning-status.md');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            planningStatusScriptPath,
            '-Format',
            'markdown',
            '-OutputPath',
            outputPath,
        ]);
        await attachProcessOutput(testInfo, 'planning-status-markdown', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        expect(existsSync(outputPath)).toBeTruthy();

        const content = readFileSync(outputPath, 'utf8');
        expect(content).toContain('# Planning Status');
        expect(content).toContain('Generated from `sprint-XX/SPRINT.md` frontmatter and versioned task checkboxes.');
        expect(content).toContain('- Sprints: 12');
        expect(content).toContain('- Total tasks: 88');
        expect(content).toContain('- Done tasks: 6');
        expect(content).toContain('- Remaining tasks: 82');
        expect(content).toContain('| sprint-12 | in_progress | 6 | 2 | 8 |');
    });

    test('fails MSIX signing with a clear certificate prerequisite message', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('signing');
        const packagePath = path.join(outputDir, `unsigned-${packageVersion}.msix`);
        const signScriptPath = path.resolve(process.cwd(), 'scripts', 'sign-msix.ps1');

        mkdirSync(outputDir, {recursive : true});
        writeFileSync(packagePath, 'placeholder-msix', 'utf8');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            signScriptPath,
            '-PackagePath',
            packagePath,
        ], {
            ...process.env,
            US4_SIGN_CERT_PATH : '',
            US4_SIGN_CERT_BASE64 : '',
            US4_SIGN_CERT_PASSWORD : '',
        });

        await attachProcessOutput(testInfo, 'sign-msix-prereq', result.stdout, result.stderr);

        expect(result.exitCode).toBe(1);
        expect(`${result.stdout}\n${result.stderr}`).toContain('MSIX signing requires certificate configuration');
    });

    test('creates and removes a dev signing certificate for local MSIX smoke flows', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('dev-signing-cert');
        const createScriptPath = path.resolve(process.cwd(), 'scripts', 'create-dev-signing-cert.ps1');
        const removeScriptPath = path.resolve(process.cwd(), 'scripts', 'remove-dev-signing-cert.ps1');

        mkdirSync(outputDir, {recursive : true});

        const createResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            createScriptPath,
            '-OutputDir',
            outputDir,
            '-CertificatePassword',
            'us4-dev-pass',
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'create-dev-signing-cert', createResult.stdout, createResult.stderr);

        expect(createResult.exitCode).toBe(0);
        const createPayload = JSON.parse(createResult.stdout) as
        {
            execution: string;
            status: string;
            subject: string;
            thumbprint: string;
            pfx_path: string;
            cer_path: string;
        };
        expect(createPayload.execution).toBe('create-dev-signing-cert');
        expect(createPayload.status).toBe('ready');
        expect(createPayload.subject).toBe('CN=US4 Dev');
        expect(existsSync(createPayload.pfx_path)).toBeTruthy();
        expect(existsSync(createPayload.cer_path)).toBeTruthy();

        const removeResult = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            removeScriptPath,
            '-Thumbprint',
            createPayload.thumbprint,
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'remove-dev-signing-cert', removeResult.stdout, removeResult.stderr);

        expect(removeResult.exitCode).toBe(0);
        const removePayload = JSON.parse(removeResult.stdout) as
        {
            execution: string;
            status: string;
            thumbprint: string;
        };
        expect(removePayload.execution).toBe('remove-dev-signing-cert');
        expect(removePayload.status).toBe('ready');
        expect(removePayload.thumbprint).toBe(createPayload.thumbprint);
    });

    test('exports release preflight as json for the current local state', async ({}, testInfo) => {
        const preflightScriptPath = path.resolve(process.cwd(), 'scripts', 'preflight-release.ps1');
        const cliBuildDir = path.resolve(process.cwd(), 'build');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            preflightScriptPath,
            '-BuildDir',
            cliBuildDir,
            '-Format',
            'json',
        ]);

        await attachProcessOutput(testInfo, 'release-preflight-json', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        const payload = JSON.parse(result.stdout) as
        {
            execution: string;
            status: string;
            package_version: string;
            cmake_version: string;
            changelog_has_current_version: boolean;
            has_cli_binary: boolean;
            issue_codes: string[];
        };
        expect(payload.execution).toBe('release-preflight');
        expect(payload.status).toBe('ready');
        expect(payload.package_version).toBe(packageVersion);
        expect(payload.cmake_version).toBe(packageVersion);
        expect(payload.changelog_has_current_version).toBeTruthy();
        expect(payload.has_cli_binary).toBeTruthy();
        expect(payload.issue_codes).toHaveLength(0);
    });

    test('fails release preflight when signing is required without certificate configuration',
         async ({}, testInfo) => {
             const preflightScriptPath = path.resolve(process.cwd(), 'scripts', 'preflight-release.ps1');
             const cliBuildDir = path.resolve(process.cwd(), 'build');

             const result = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 preflightScriptPath,
                 '-BuildDir',
                 cliBuildDir,
                 '-Format',
                 'json',
                 '-RequireSigning',
             ], {
                 ...process.env,
                 US4_SIGN_CERT_PATH : '',
                 US4_SIGN_CERT_BASE64 : '',
                 US4_SIGN_CERT_PASSWORD : '',
             });

             await attachProcessOutput(testInfo, 'release-preflight-signing-blocked', result.stdout, result.stderr);

             expect(result.exitCode).toBe(1);
             const payload = JSON.parse(result.stdout) as
             {
                 status: string;
                 issue_codes: string[];
             };
             expect(payload.status).toBe('blocked');
             expect(payload.issue_codes).toContain('signing_config_missing');
         });

    test('exports dev MSIX smoke preflight readiness or clear local blockers', async ({}, testInfo) => {
        const devMsixSmokeScriptPath = path.resolve(process.cwd(), 'scripts', 'dev-msix-smoke.ps1');

        const result = await runPowerShell([
            '-NoProfile',
            '-ExecutionPolicy',
            'Bypass',
            '-File',
            devMsixSmokeScriptPath,
            '-PreflightOnly',
            '-Format',
            'json',
        ]);
        await attachProcessOutput(testInfo, 'dev-msix-smoke-preflight', result.stdout, result.stderr);

        const payload = JSON.parse(result.stdout) as
        {
            execution: string;
            status: string;
            preflight_only?: boolean;
            issue_codes: string[];
        };
        expect(payload.execution).toBe('dev-msix-smoke');

        if (result.exitCode === 0) {
            expect(payload.status).toBe('ready');
            expect(payload.preflight_only).toBeTruthy();
            expect(payload.issue_codes).toHaveLength(0);
        } else {
            expect(payload.status).toBe('blocked');
            expect(payload.issue_codes.some((code) => code.endsWith('_missing'))).toBeTruthy();
        }
    });

    test('fails MSIX install smoke with a clear signing/trust prerequisite message',
         async ({}, testInfo) => {
             const outputDir = testInfo.outputPath('install-msix-smoke');
             const packagePath = path.join(outputDir, `unsigned-${packageVersion}.msix`);
             const smokeScriptPath = path.resolve(process.cwd(), 'scripts', 'install-msix-smoke.ps1');

             mkdirSync(outputDir, {recursive : true});
             writeFileSync(packagePath, 'placeholder-msix', 'utf8');

             const result = await runPowerShell([
                 '-NoProfile',
                 '-ExecutionPolicy',
                 'Bypass',
                 '-File',
                 smokeScriptPath,
                 '-PackagePath',
                 packagePath,
             ]);

             await attachProcessOutput(testInfo, 'install-msix-smoke-prereq', result.stdout, result.stderr);

             expect(result.exitCode).toBe(1);
             expect(`${result.stdout}\n${result.stderr}`).toContain('MSIX package is not signed with a trusted certificate');
         });
});
