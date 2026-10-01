#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [Parameter(Mandatory)][string]$Destination,
    [string]$BackupRoot,
    [switch]$AllowDirtySource
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-ExistingDirectory([string]$Path, [string]$Label) {
    if (!(Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Label does not exist: $Path"
    }
    [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path).Path)
}

function Get-ContainedPath([string]$Root, [string]$RelativePath) {
    if ([IO.Path]::IsPathRooted($RelativePath)) {
        throw "Manifest path must be relative: $RelativePath"
    }
    $normalizedRoot = $Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $full = [IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    if (!$full.StartsWith($normalizedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Manifest path escapes the runtime: $RelativePath"
    }
    $full
}

$sourceRoot = Resolve-ExistingDirectory $RuntimeDirectory 'Validated runtime'
$manifestPath = Join-Path $sourceRoot 'build-manifest.json'
if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Validated runtime manifest is missing: $manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1 -or $manifest.configuration -ne 'Release' -or
    $manifest.platform -ne 'x64' -or ($manifest.sourceDirty -and !$AllowDirtySource)) {
    throw 'Refusing to install a dirty, non-Release, or unsupported runtime manifest. Pass -AllowDirtySource only for a separately validated candidate.'
}

$destinationRoot = [IO.Path]::GetFullPath($Destination)
if (!(Test-Path -LiteralPath $destinationRoot -PathType Container)) {
    New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
}
$destinationRoot = Resolve-ExistingDirectory $destinationRoot 'Destination runtime'
if ($sourceRoot -eq $destinationRoot) {
    throw 'Source and destination runtime directories must differ.'
}

$destinationExe = [IO.Path]::GetFullPath((Join-Path $destinationRoot 'Magpie.exe'))
$running = Get-Process Magpie -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and [IO.Path]::GetFullPath($_.Path) -eq $destinationExe }
if ($running) {
    throw 'Exit the installed Magpie instance before updating its runtime.'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (!$BackupRoot) {
    $BackupRoot = Join-Path (Split-Path $sourceRoot -Parent) "installed-backups/$stamp"
}
$backupRootFull = [IO.Path]::GetFullPath($BackupRoot)
if ($backupRootFull -eq $destinationRoot -or
    $backupRootFull.StartsWith($destinationRoot.TrimEnd('\', '/') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The backup directory must be outside the installed runtime.'
}
New-Item -ItemType Directory -Path $backupRootFull -Force | Out-Null

$records = @($manifest.files)
if (!$records.Count) { throw 'The runtime manifest contains no files.' }
$plan = foreach ($record in $records) {
    $relative = [string]$record.path
    $source = Get-ContainedPath $sourceRoot $relative
    $target = Get-ContainedPath $destinationRoot $relative
    $backup = Get-ContainedPath $backupRootFull $relative
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Manifest source file is missing: $relative"
    }
    $actual = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ($actual -ne [string]$record.sha256) {
        throw "Manifest source hash mismatch: $relative"
    }
    [pscustomobject]@{
        Relative = $relative
        Source = $source
        Target = $target
        Backup = $backup
        ExpectedHash = [string]$record.sha256
        Existed = Test-Path -LiteralPath $target -PathType Leaf
    }
}

$manifestTarget = Get-ContainedPath $destinationRoot 'build-manifest.json'
$manifestBackup = Get-ContainedPath $backupRootFull 'build-manifest.json'
$manifestExisted = Test-Path -LiteralPath $manifestTarget -PathType Leaf
$installed = [Collections.Generic.List[object]]::new()

try {
    foreach ($item in $plan) {
        if ($item.Existed) {
            New-Item -ItemType Directory -Path (Split-Path $item.Backup -Parent) -Force | Out-Null
            Copy-Item -LiteralPath $item.Target -Destination $item.Backup
            if ((Get-FileHash -LiteralPath $item.Target -Algorithm SHA256).Hash -ne
                (Get-FileHash -LiteralPath $item.Backup -Algorithm SHA256).Hash) {
                throw "Backup verification failed: $($item.Relative)"
            }
        }
        New-Item -ItemType Directory -Path (Split-Path $item.Target -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $item.Source -Destination $item.Target -Force
        $installed.Add($item)
    }

    foreach ($item in $plan) {
        if ((Get-FileHash -LiteralPath $item.Target -Algorithm SHA256).Hash -ne $item.ExpectedHash) {
            throw "Installed file hash mismatch: $($item.Relative)"
        }
    }

    if ($manifestExisted) {
        Copy-Item -LiteralPath $manifestTarget -Destination $manifestBackup
    }
    Copy-Item -LiteralPath $manifestPath -Destination $manifestTarget -Force
} catch {
    for ($i = $installed.Count - 1; $i -ge 0; --$i) {
        $item = $installed[$i]
        if ($item.Existed) {
            Copy-Item -LiteralPath $item.Backup -Destination $item.Target -Force
        } elseif (Test-Path -LiteralPath $item.Target -PathType Leaf) {
            Remove-Item -LiteralPath $item.Target -Force
        }
    }
    if ($manifestExisted -and (Test-Path -LiteralPath $manifestBackup -PathType Leaf)) {
        Copy-Item -LiteralPath $manifestBackup -Destination $manifestTarget -Force
    } elseif (!$manifestExisted -and (Test-Path -LiteralPath $manifestTarget -PathType Leaf)) {
        Remove-Item -LiteralPath $manifestTarget -Force
    }
    throw
}

$result = [ordered]@{
    installed = $true
    version = [string]$manifest.version
    commit = [string]$manifest.commit
    source = $sourceRoot
    destination = $destinationRoot
    backup = $backupRootFull
    files = $records.Count
    installedAtUtc = [DateTime]::UtcNow.ToString('o')
}
$result | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $backupRootFull 'install-result.json') -Encoding utf8
$result | ConvertTo-Json -Depth 4
