param(
    [Parameter(Mandatory = $true)][string]$JavaHome,
    [string]$GhidraDirectory = 'C:\Users\PACORCS\Downloads\ghidra_12.1.4_PUBLIC'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$binaryPath = Join-Path $projectRoot 'research\binaries\SPEED2.EXE'
$headless = Join-Path $GhidraDirectory 'support\analyzeHeadless.bat'
$javaPath = Join-Path $JavaHome 'bin\java.exe'
$javacPath = Join-Path $JavaHome 'bin\javac.exe'
foreach ($required in @($binaryPath, $headless, $javaPath, $javacPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing: $required" }
}
if ((Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash -ne
    'F9DD86C054878CE6276BEB07C1FD61874F7A1E4BF1F241B084C65B73E24168A7') {
    throw 'Unexpected binary fingerprint'
}
$oldJavaHome = $env:JAVA_HOME
try {
    $env:JAVA_HOME = (Resolve-Path -LiteralPath $JavaHome).Path
    & $javaPath -version
    if ($LASTEXITCODE -ne 0) { throw 'Java failed' }
    $projectDirectory = Join-Path $projectRoot 'research\ghidra\projects'
    New-Item -ItemType Directory -Force -Path $projectDirectory | Out-Null
    # Unique project preserves previous analyses; no -overwrite.
    $projectName = 'UGWorld-' + [Guid]::NewGuid().ToString('N')
    & $headless $projectDirectory $projectName -import $binaryPath -processor 'x86:LE:32:default' -cspec windows
    if ($LASTEXITCODE -ne 0) { throw "Ghidra failed: $LASTEXITCODE" }
} finally {
    $env:JAVA_HOME = $oldJavaHome
}
