Register-ArgumentCompleter -Native -CommandName us4-cli, us4-cli.exe -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)

    $commandElements = @($commandAst.CommandElements | ForEach-Object { $_.Extent.Text })
    if ($commandElements.Count -le 1) {
        $candidates = @("help", "version", "probe", "serve", "run", "bench", "tune")
    } else {
        $verb = $commandElements[1]
        $previous = if ($commandElements.Count -ge 2) { $commandElements[-1] } else { "" }
        switch ($previous) {
            "--backend" {
                $candidates = @("auto", "cpu", "cuda", "directml", "vulkan", "windows-ml", "npu")
                break
            }
            "--mode" {
                $candidates = @("auto", "full", "balanced", "degraded", "ultra_low", "micro", "nano", "cpu_only")
                break
            }
            "--format" {
                $candidates = @("text", "json")
                break
            }
            default {
                $baseFlags = @("--help", "--version")
                $modelFlags = @("--model", "--backend", "--mode", "--format", "--npu")
                $runFlags = @("--prompt", "--model-path", "--max-tokens")
                $benchFlags = @("--max-tokens")
                $tuneFlags = @("--max-tokens")
                switch ($verb) {
                    "probe" { $candidates = $baseFlags + @("--format") }
                    "serve" { $candidates = $baseFlags + $modelFlags }
                    "run" { $candidates = $baseFlags + $modelFlags + $runFlags }
                    "bench" { $candidates = $baseFlags + $modelFlags + $benchFlags }
                    "tune" { $candidates = $baseFlags + $modelFlags + $tuneFlags }
                    default { $candidates = @("help", "version", "probe", "serve", "run", "bench", "tune") }
                }
                break
            }
        }
    }

    $candidates |
        Where-Object { $_ -like "$wordToComplete*" } |
        Sort-Object -Unique |
        ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, "ParameterValue", $_)
        }
}
