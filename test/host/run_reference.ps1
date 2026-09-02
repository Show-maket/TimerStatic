$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildDir = Join-Path $repoRoot 'build\host-torture'
$exePath = Join-Path $buildDir 'timerstatic_torture.exe'
$knownCompiler = 'C:\MinGW\bin\c++.exe'
$compiler = if (Test-Path -LiteralPath $knownCompiler) {
    $knownCompiler
} else {
    (Get-Command g++ -ErrorAction Stop).Source
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$compileArgs = @(
    '-std=gnu++11',
    '-O2',
    '-Wall',
    '-Wextra',
    '-Werror',
    '-Wno-comment',
    '-DTIMERSTATIC_HOST_REFERENCE',
    "-I$(Join-Path $repoRoot 'test\host')",
    "-I$(Join-Path $repoRoot 'src')",
    '-x', 'c++',
    (Join-Path $repoRoot 'examples\HardwareTorture\HardwareTorture.ino'),
    (Join-Path $repoRoot 'src\TimerStatic.cpp'),
    (Join-Path $repoRoot 'test\host\Arduino.cpp'),
    (Join-Path $repoRoot 'test\host\main.cpp'),
    '-o', $exePath
)

& $compiler @compileArgs
if ($LASTEXITCODE -ne 0) {
    Write-Output "TS,HOST,FAIL,stage=compile,exit=$LASTEXITCODE"
    exit $LASTEXITCODE
}

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $exePath
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $startInfo
[void]$process.Start()

if (-not $process.WaitForExit(15000)) {
    $process.Kill()
    $process.WaitForExit()
    Write-Output 'TS,HOST,FAIL,stage=run,reason=timeout'
    exit 124
}

$stdout = $process.StandardOutput.ReadToEnd()
$stderr = $process.StandardError.ReadToEnd()
if ($stdout) { Write-Output $stdout.TrimEnd() }
if ($stderr) { [Console]::Error.WriteLine($stderr.TrimEnd()) }

$passCases = @($stdout -split "`r?`n" | Where-Object { $_ -match '^TS,CASE,[^,]+,PASS,' }).Count
$failCases = @($stdout -split "`r?`n" | Where-Object { $_ -match '^TS,CASE,[^,]+,FAIL,' }).Count
$summaryOk = $stdout -match '(?m)^TS,SUMMARY,PASS,passed=19,failed=0,'
$stopOk = $stdout -match '(?m)^TS,SUITE,STOP\r?$'

if ($process.ExitCode -ne 0 -or $passCases -ne 19 -or $failCases -ne 0 -or
    -not $summaryOk -or -not $stopOk) {
    Write-Output "TS,HOST,FAIL,stage=verify,exit=$($process.ExitCode),pass_cases=$passCases,fail_cases=$failCases,summary=$summaryOk,stop=$stopOk"
    exit 1
}

Write-Output 'TS,HOST,PASS,cases=19,timeout_ms=15000'
