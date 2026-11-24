# Bug scan: run the project's static + runtime analysis tools and report findings.
#
# Steps (see -Step):
#   static   - clang-tidy clang-analyzer-* over the project's translation units
#   asan     - build x64-asan, run unit tests under AddressSanitizer
#   hardened - build x64-hardened (/guard:cf + /sdl), run unit tests
#   all      - run all of the above in order
#
# _CRTDBG is intentionally not part of the auto scan: OpenCV's process-lifetime
# singletons make its exit/checkpoint leak report a wall of false positives.
# Leak checking stays a manual demo (see docs/RUNTIME_CHECK_TOOLS.md).
#
# Works both locally and in GitHub Actions CI. Exits non-zero when any finding is
# detected, so it can be wired into a scheduler (Task Scheduler, cron, CI).
#
# Local usage:
#   powershell -ExecutionPolicy Bypass -File scripts/scan-bugs.ps1 -Step asan
#
# The CppRuntimeCheck demo commands (asan-heap-overflow, crtdbg-leak, ...) are
# intentionally NOT run here: they are smoke tests that trigger known errors to
# prove each tool is wired up, not scans for real bugs. See docs/RUNTIME_CHECK_TOOLS.md.

param(
    [ValidateSet('all', 'static', 'asan', 'hardened')]
    [string]$Step = 'all'
)

$ErrorActionPreference = 'Stop'

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot   = Split-Path -Parent $ScriptRoot
$InCI       = ($env:GITHUB_ACTIONS -eq 'true')

# Local MSVC toolchains: 14.52 (debug family) and 14.44 (release / clang-tidy 19).
# Ignored on CI, where setup-msvc-dev already prepares the environment.
$VcvarsDebug    = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$VcvarsRelease  = 'D:\Program Files\visual_studio\VC\Auxiliary\Build\vcvars64.bat'
$ClangTidyLocal = 'D:\Program Files\visual_studio\VC\Tools\Llvm\x64\bin\clang-tidy.exe'

$Script:Findings = [System.Collections.Generic.List[string]]::new()

function Add-Finding {
    param([string]$Message)
    $Script:Findings.Add($Message)
    Write-Host "[FINDING] $Message" -ForegroundColor Red
}

# Run a command line. Locally it is wrapped in a vcvars environment; on CI the
# environment is already loaded by setup-msvc-dev.
function Invoke-Cmd {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [string]$Vcvars = ''
    )
    $full = $Command
    if (-not $InCI -and $Vcvars) {
        $full = "call `"$Vcvars`" && $Command"
    }
    Write-Host ''
    Write-Host ">>> $full"
    cmd /c $full
    if ($LASTEXITCODE -ne 0) {
        throw "command failed (exit $LASTEXITCODE): $full"
    }
}

# Run a command and capture its output lines, returning an object with Output
# and ExitCode. Same local/CI vcvars handling as Invoke-Cmd.
function Invoke-Capture {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [string]$Vcvars = ''
    )
    $full = $Command
    if (-not $InCI -and $Vcvars) {
        $full = "call `"$Vcvars`" && $Command"
    }
    Write-Host ''
    Write-Host ">>> $full"
    # Redirect stderr inside cmd (not via PowerShell's 2>&1) so PowerShell sees a
    # single stream and $ErrorActionPreference='Stop' cannot trip on stderr lines.
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $raw = cmd /c "$full 2>&1"
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $prev
    $output = @($raw | ForEach-Object { $_.ToString() })
    return @{ Output = $output; ExitCode = $exitCode }
}

# On CI the presets hardcode local compiler paths that do not exist on the
# runner, so override them with the runner's cl.exe (derived from
# VCToolsInstallDir, which setup-msvc-dev sets). Locally we keep the preset's
# absolute paths, which already cure STL1001.
function Get-CMakeCompilerArgs {
    if (-not $InCI) { return @() }
    $cl = $null
    if ($env:VCToolsInstallDir) {
        $candidate = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\cl.exe'
        if (Test-Path -LiteralPath $candidate) { $cl = $candidate }
    }
    if (-not $cl) {
        $cmd = Get-Command cl.exe -ErrorAction SilentlyContinue
        if ($cmd) { $cl = $cmd.Source }
    }
    if (-not $cl) { throw 'cannot locate cl.exe to override the preset compiler path' }
    $cl = $cl -replace '\\', '/'
    return @("-DCMAKE_CXX_COMPILER=$cl", "-DCMAKE_C_COMPILER=$cl")
}

# Configure + build the test target for a preset.
function Invoke-Build {
    param(
        [Parameter(Mandatory = $true)][string]$Preset,
        [string]$Vcvars = $VcvarsDebug
    )
    $extra = Get-CMakeCompilerArgs
    Invoke-Cmd -Command "cmake --preset $Preset $($extra -join ' ')" -Vcvars $Vcvars
    Invoke-Cmd -Command "cmake --build --preset $Preset --target camera_utils_tests" -Vcvars $Vcvars
}

function Resolve-ClangTidy {
    if ($InCI) {
        $found = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'clang-tidy.exe' -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $found) { throw 'clang-tidy.exe not found on the CI runner' }
        return $found.FullName
    }
    if (-not (Test-Path -LiteralPath $ClangTidyLocal)) { throw "clang-tidy not found: $ClangTidyLocal" }
    return $ClangTidyLocal
}

# Step 1: static analysis via clang-tidy's clang-analyzer-* (hard gate).
function Invoke-StaticScan {
    $clangTidy = Resolve-ClangTidy

    # clang-tidy must read the 14.44 STL headers; configure x64-release for its
    # compile_commands.json (no full build needed, matching .github/workflows/tidy.yml).
    $extra = Get-CMakeCompilerArgs
    Invoke-Cmd -Command "cmake --preset x64-release $($extra -join ' ')" -Vcvars $VcvarsRelease

    $dbPath = Join-Path $RepoRoot 'out/build/x64-release/compile_commands.json'
    if (-not (Test-Path -LiteralPath $dbPath)) { throw "compile_commands.json missing: $dbPath" }

    $root = (Resolve-Path $RepoRoot).Path -replace '\\', '/'
    $db = Get-Content -LiteralPath $dbPath -Raw | ConvertFrom-Json
    $files = @($db |
        ForEach-Object { $_.file = $_.file -replace '\\', '/'; $_ } |
        Where-Object { $_.file -like "$root/src/*" -or $_.file -like "$root/tests/*" } |
        ForEach-Object { $_.file } |
        Sort-Object -Unique)

    Write-Host "clang-tidy: checking $($files.Count) translation units"

    $buildDir = (Join-Path $RepoRoot 'out/build/x64-release') -replace '\\', '/'
    foreach ($f in $files) {
        $fwd = $f -replace '\\', '/'
        # magic_enum v0.9.8 mis-reflects enums under clang-cl; relax its static_assert only.
        # Must never be used for a real build.
        $cmd = "`"$clangTidy`" -p `"$buildDir`" `"$fwd`" --quiet --warnings-as-errors=clang-analyzer-* --extra-arg=-DMAGIC_ENUM_NO_CHECK_REFLECTED_ENUM"
        $result = Invoke-Capture -Command $cmd -Vcvars $VcvarsRelease
        $result.Output | ForEach-Object { Write-Host $_ }
        if ($result.ExitCode -ne 0) {
            Add-Finding "clang-tidy analyzer finding in $fwd"
        }
    }

    if (-not ($Script:Findings | Where-Object { $_ -like 'clang-tidy*' })) {
        Write-Host 'clang-tidy: no analyzer findings.'
    }
}

# Step 2: AddressSanitizer over the unit tests.
function Invoke-AsanScan {
    Invoke-Build -Preset 'x64-asan' -Vcvars $VcvarsDebug

    $testDir = Join-Path $RepoRoot 'out\build\x64-asan'
    # detect_container_overflow is a known false-positive source with the MSVC STL
    # (the container size annotation fires on legitimate vector reallocation), and
    # here it also trips inside OpenCV's ORB code. Keep the high-value checks on.
    $asanEnv = "ASAN_OPTIONS=detect_container_overflow=0"
    $result = Invoke-Capture -Command "set `"$asanEnv`"&& ctest --test-dir `"$testDir`" -C Debug -L unit --output-on-failure"
    $result.Output | ForEach-Object { Write-Host $_ }
    if ($result.ExitCode -ne 0) {
        Add-Finding 'ASan: one or more unit tests reported a sanitizer error'
    } else {
        Write-Host 'ASan: no memory errors in unit tests.'
    }
}

# Step 3: hardened build (/guard:cf + /sdl + /GS) must compile and pass tests.
function Invoke-HardenedScan {
    Invoke-Build -Preset 'x64-hardened' -Vcvars $VcvarsDebug

    $testDir = Join-Path $RepoRoot 'out\build\x64-hardened'
    $result = Invoke-Capture -Command "ctest --test-dir `"$testDir`" -C Release -L unit --output-on-failure"
    $result.Output | ForEach-Object { Write-Host $_ }
    if ($result.ExitCode -ne 0) {
        Add-Finding 'hardened: build or unit tests failed'
    } else {
        Write-Host 'hardened: compiled with /guard:cf /sdl and unit tests pass.'
    }
}

$steps = if ($Step -eq 'all') { @('static', 'asan', 'hardened') } else { @($Step) }

foreach ($s in $steps) {
    Write-Host ''
    Write-Host "===== step: $s =====" -ForegroundColor Cyan
    try {
        switch ($s) {
            'static'   { Invoke-StaticScan }
            'asan'     { Invoke-AsanScan }
            'hardened' { Invoke-HardenedScan }
        }
    } catch {
        Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}

Write-Host ''
if ($Script:Findings.Count -gt 0) {
    Write-Host "Bug scan found $($Script:Findings.Count) finding(s):" -ForegroundColor Red
    $Script:Findings | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

Write-Host 'Bug scan complete: no findings.' -ForegroundColor Green
exit 0
