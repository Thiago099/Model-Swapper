param(
    [string]$DependencyInclude = "",
    [string]$DeveloperShell = ""
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $DependencyInclude) {
    $DependencyInclude = Join-Path $projectRoot 'build/debug/vcpkg_installed/x64-windows-static/include'
}
if (-not (Test-Path -LiteralPath (Join-Path $DependencyInclude 'nlohmann/json.hpp'))) {
    throw 'Pass -DependencyInclude with an installed nlohmann/json.hpp include directory.'
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    if (-not $DeveloperShell) {
        foreach ($cacheRelativePath in @('build/debug/CMakeCache.txt', 'build/validation/CMakeCache.txt')) {
            $cachePath = Join-Path $projectRoot $cacheRelativePath
            if (-not (Test-Path -LiteralPath $cachePath)) { continue }
            $compilerLine = Get-Content -LiteralPath $cachePath | Select-String '^CMAKE_CXX_COMPILER:.*=(.+)$' | Select-Object -First 1
            if ($compilerLine) {
                $compilerPath = $compilerLine.Matches[0].Groups[1].Value
                $match = [regex]::Match($compilerPath, '^(.*)/VC/Tools/MSVC/')
                if ($match.Success) {
                    $DeveloperShell = Join-Path $match.Groups[1].Value 'Common7/Tools/Launch-VsDevShell.ps1'
                    break
                }
            }
        }
        if (-not $DeveloperShell) {
            $vsWherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            if (Test-Path -LiteralPath $vsWherePath) {
                $installationPath = & $vsWherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
                if ($installationPath) {
                    $DeveloperShell = Join-Path $installationPath 'Common7/Tools/Launch-VsDevShell.ps1'
                }
            }
        }
    }
    if (-not $DeveloperShell -or -not (Test-Path -LiteralPath $DeveloperShell)) {
        throw 'Run in a Visual Studio developer shell or pass -DeveloperShell with Launch-VsDevShell.ps1.'
    }
    & $DeveloperShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
}

$outputPath = Join-Path $projectRoot 'build/tests'
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Push-Location $outputPath
try {
    $runtimeHeader = Join-Path $PSScriptRoot 'Runtime.h'
    $testSource = Join-Path $PSScriptRoot 'FormFilterTests.cpp'
    $managerSource = Join-Path $projectRoot 'src/Manager.cpp'
    $persistenceSource = Join-Path $projectRoot 'src/Persistence.cpp'
    $filterContextSource = Join-Path $projectRoot 'src/FilterContext.cpp'
    $projectInclude = Join-Path $projectRoot 'include'
    & cl.exe /nologo /std:c++latest /EHsc /permissive- /Zc:preprocessor /W4 /D_CRT_SECURE_NO_WARNINGS "/FI$runtimeHeader" "/I$projectInclude" "/I$DependencyInclude" $testSource $managerSource $persistenceSource $filterContextSource /Fe:FormFilterTests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Regression test compilation failed.' }
    & './FormFilterTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Regression tests failed.' }
} finally {
    Pop-Location
}
