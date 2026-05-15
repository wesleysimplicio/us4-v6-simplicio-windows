import {expect, test} from '@playwright/test';
import type {TestInfo} from '@playwright/test';
import {execFile} from 'node:child_process';
import {existsSync, mkdirSync, readFileSync} from 'node:fs';
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
        };
        expect(payload.execution).toBe('probe');
        expect(payload.cpu).toBe('Playwright Json CPU');
        expect(payload.gpu).toBe('Playwright Json GPU');
        expect(payload.selected_backend).toBe('directml');
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
        };
        expect(payload.execution).toBe('run');
        expect(payload.status).toBe('completed');
        expect(payload.plan_execution).toBe('cpu-scalar');
        expect(payload.backend).toBe('cpu-avx2');
        expect(Array.isArray(payload.generated_tokens)).toBeTruthy();
        expect(typeof payload.generated_text).toBe('string');
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

    test('renders winget manifests from repository templates', async ({}, testInfo) => {
        const outputDir = testInfo.outputPath('winget-manifests');
        const renderScriptPath = path.resolve(process.cwd(), 'scripts', 'render-winget-manifests.ps1');

        mkdirSync(outputDir, {recursive : true});

        const result = await runPowerShell([
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
            `https://example.invalid/us4-v6-windows-${packageVersion}.msix`,
            '-OutputDir',
            outputDir,
        ]);

        await attachProcessOutput(testInfo, 'winget-render', result.stdout, result.stderr);

        expect(result.exitCode).toBe(0);
        expect(existsSync(path.join(outputDir, 'default.yaml'))).toBeTruthy();
        expect(existsSync(path.join(outputDir, 'installer.yaml'))).toBeTruthy();
        expect(existsSync(path.join(outputDir, 'locale.en-US.yaml'))).toBeTruthy();
        expect(readFileSync(path.join(outputDir, 'installer.yaml'), 'utf8')).toContain(
            `https://example.invalid/us4-v6-windows-${packageVersion}-portable.zip`
        );
    });
});
