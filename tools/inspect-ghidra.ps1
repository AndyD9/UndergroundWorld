param(
    [Parameter(Mandatory = $true)][string]$OutputName,
    [Parameter(Mandatory = $true)][string[]]$Addresses
)
$ErrorActionPreference = 'Stop'
if ($OutputName -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Use a simple output name' }
foreach ($address in $Addresses) {
    if ($address -notmatch '^[0-9a-fA-F]{8}$') { throw 'Pass each address as a quoted 8-digit hexadecimal string' }
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$binaryPath = Join-Path $projectRoot 'research\binaries\SPEED2.EXE'
if ((Get-FileHash -LiteralPath $binaryPath).Hash -ne 'F9DD86C054878CE6276BEB07C1FD61874F7A1E4BF1F241B084C65B73E24168A7') { throw 'Wrong fingerprint' }
$oldJavaHome = $env:JAVA_HOME
try {
    $env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-25.0.4.101-hotspot'
    $projectDirectory = Join-Path $projectRoot 'research\ghidra\projects'
    $scriptDirectory = Join-Path $projectRoot 'research\ghidra'
    $outputPath = Join-Path $scriptDirectory "exports\$OutputName.json"
    if (Test-Path -LiteralPath $outputPath) { throw 'Choose a new output name to preserve evidence' }
    & 'C:\Users\PACORCS\Downloads\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat' $projectDirectory 'UGWorld-6ed470fc646e492b8255f886e9bc3f8b' -process SPEED2.EXE -readOnly -noanalysis -scriptPath $scriptDirectory -postScript ExportCandidates.java $outputPath @Addresses
    if ($LASTEXITCODE -ne 0) { throw "Ghidra failed: $LASTEXITCODE" }
    if (-not (Test-Path -LiteralPath $outputPath)) { throw 'Export missing; inspect Ghidra log' }
} finally { $env:JAVA_HOME = $oldJavaHome }
