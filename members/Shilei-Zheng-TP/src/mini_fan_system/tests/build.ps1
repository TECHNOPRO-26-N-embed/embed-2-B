# build.ps1 -- PC coverage test for Mini Fan System
#
# Usage:
#   PS> cd src\mini_fan_system\tests
#   PS> .\build.ps1
#
# Requires gcc (MinGW/MSYS2) in PATH.
# Exit code: 0 = all branches covered, 1 = compilation error or missing branches.

$scriptDir = $PSScriptRoot
if (-not $scriptDir) { $scriptDir = Split-Path -Parent $PSCommandPath -ErrorAction SilentlyContinue }
if (-not $scriptDir) { $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition }

Set-Location $scriptDir
[System.IO.Directory]::SetCurrentDirectory($scriptDir)

Write-Host "=== Compiling with gcc ===" -ForegroundColor Cyan

$gccArgs = @("-std=c99", "-Wall", "-Wextra", "-DPC_TEST", "-I..", "-I.", "..\system_context.c", "..\button_input.c", "..\pot_speed.c", "..\fault_detector.c", "..\state_machine.c", "..\motor_output.c", "..\led_output.c", "..\debug_output.c", "mock_c_port.c", "coverage_tracker.c", "test_main.c", "-o", "coverage_test.exe")

$proc = Start-Process -FilePath "gcc" -ArgumentList $gccArgs -NoNewWindow -Wait -PassThru -WorkingDirectory $scriptDir

if ($proc.ExitCode -ne 0) {
    Write-Host "[NG] Compilation failed (exit=$($proc.ExitCode))." -ForegroundColor Red
    exit 1
}
Write-Host "Compilation OK." -ForegroundColor Green

Write-Host "`n=== Running coverage test ===" -ForegroundColor Cyan
# Run via cmd so we can both display on console AND save to file as plain ASCII
cmd /c ".\coverage_test.exe > coverage_result.txt 2>&1"
$exitCode = $LASTEXITCODE

# Display the saved file content on console
Get-Content coverage_result.txt | ForEach-Object { Write-Host $_ }

Write-Host "`nResult saved to: $(Join-Path $scriptDir 'coverage_result.txt')" -ForegroundColor Yellow

if ($exitCode -eq 0) {
    Write-Host "`n[OK] Coverage test PASSED." -ForegroundColor Green
} else {
    Write-Host "`n[NG] Coverage test FAILED (exit=$exitCode)." -ForegroundColor Red
}

exit $exitCode
