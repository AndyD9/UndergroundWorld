param(
    [Parameter(Mandatory = $true)][string]$OutputName,
    [Parameter(Mandatory = $true)][string[]]$Queries,
    # Dossier contenant research/binaries et research/ghidra/projects (copie principale si worktree).
    [string]$DataRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
if ($OutputName -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Use a simple output name' }
# powershell -File transmet une liste séparée par des virgules comme une seule chaîne.
$Queries = @($Queries | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
foreach ($query in $Queries) {
    if ($query -notmatch '^(fn|refs|ptrscan):[0-9a-fA-F]{8}$|^(bytes|dwords):[0-9a-fA-F]{8}:[0-9]{1,5}$|^disp:[0-9a-fA-F]{1,8}:[0-9a-fA-F]{8}:[0-9a-fA-F]{8}$|^ins:[0-9a-fA-F]{8}:[0-9a-fA-F]{8}$|^pat:([0-9a-fA-F]{2}|xx){2,64}$') {
        throw "Invalid query: $query (quote each one, e.g. 'fn:00602650')"
    }
}
$binaryPath = Join-Path $DataRoot 'research\binaries\SPEED2.EXE'
if ((Get-FileHash -LiteralPath $binaryPath).Hash -ne 'F9DD86C054878CE6276BEB07C1FD61874F7A1E4BF1F241B084C65B73E24168A7') { throw 'Wrong fingerprint' }
$oldJavaHome = $env:JAVA_HOME
try {
    $env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-25.0.4.101-hotspot'
    $projectDirectory = Join-Path $DataRoot 'research\ghidra\projects'
    $scriptDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'research\ghidra'
    $outputPath = Join-Path $DataRoot "research\ghidra\exports\$OutputName.json"
    if (Test-Path -LiteralPath $outputPath) { throw 'Choose a new output name to preserve evidence' }
    & 'C:\Users\PACORCS\Downloads\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat' $projectDirectory 'UGWorld-6ed470fc646e492b8255f886e9bc3f8b' -process SPEED2.EXE -readOnly -noanalysis -scriptPath $scriptDirectory -postScript ExportQueries.java $outputPath @Queries
    if ($LASTEXITCODE -ne 0) { throw "Ghidra failed: $LASTEXITCODE" }
    if (-not (Test-Path -LiteralPath $outputPath)) { throw 'Export missing; inspect Ghidra log' }
} finally { $env:JAVA_HOME = $oldJavaHome }
