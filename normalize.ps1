#requires -version 7.4
<#
.SYNOPSIS
    Normalize sources: code style + text encoding/EOL.
.DESCRIPTION
    - Formats C/C++ files via clang-format.
    - Normalizes text files to UTF-8 with BOM and platform EOLs
      (CRLF on Windows, LF elsewhere).
    - Use -DryRun to preview changes without modifying files.
#>

[CmdletBinding()]
param(
    [switch]$DebugMode,
    [switch]$DryRun,
    [switch]$Staged,
    [switch]$Help,
    [switch]$Sequential,
    [ValidateRange(1, [int]::MaxValue)]
    [int]$ThrottleLimit = 1,
    [string]$ClangFormatPath,
    # Optional path where the script mirrors console output; invaluable for CI logs.
    [string]$LogPath
)

<#[
.SYNOPSIS
    Formats C/C++ sources and normalizes text files across the repository.
.DESCRIPTION
    Runs clang-format using include/exclude rules from normalize_config.json and
    ensures all configured text files are UTF-8 with BOM and expected EOLs. The
    script is safe to run from the repo root and supports dry-run as well as
    staged-only scenarios for Git hooks.
.EXAMPLE
    pwsh -File .\normalize.ps1
    Runs the formatter and applies any required changes in place.
.EXAMPLE
    pwsh -File .\normalize.ps1 -DryRun -ThrottleLimit 4 -LogPath artifacts/normalize.log
    Performs a read-only check, using four parallel workers, and mirrors all
    output to the specified log file. Exit code 2 signals formatting is needed.
.EXAMPLE
    pwsh -File .\normalize.ps1 -Staged
    Inspects only staged files without modifying them. Useful inside pre-commit
    hooks to warn reviewers about pending normalization work.
.PARAMETER DebugMode
    Emits verbose diagnostics such as full file lists for each normalization
    stage. Helpful when adjusting include/exclude rules.
.PARAMETER DryRun
    Reports the changes that would be made but leaves files untouched. Produces
    exit code 2 when normalization is required so CI can fail the build politely.
.PARAMETER Help
    Prints this help overview and exits without performing any work. Use when
    you need a quick reminder of supported parameters.
.PARAMETER Sequential
    Disables parallel workers so each batch runs sequentially for easier step-by-step debugging.
.PARAMETER Staged
    Limits processing to currently staged files. Implicitly enables -DryRun and
    creates temporary copies to avoid mutating the Git index.
.PARAMETER ThrottleLimit
    Maximum number of concurrent workers spawned for both clang-format and text
    normalization. Defaults to the detected physical core count.
.PARAMETER ClangFormatPath
    Optional override for the clang-format executable. When omitted the script
    searches common install locations including Visual Studio and PATH.
.PARAMETER LogPath
    When provided, the script writes a UTF-8 log file duplicating console output.
    Relative paths are resolved against the repository root and directories are
    created automatically. Ideal for CI artifacts.
]#>

# --- Constants ---
Set-Variable -Name 'ScriptVersion' -Option Constant -Value '2.0'
Set-Variable -Name 'StagedFileInfix' -Option Constant -Value 'git_staged'
Set-Variable -Name 'SuccessExitCode' -Option Constant -Value 0
Set-Variable -Name 'ErrorExitCode' -Option Constant -Value 1
# Exit code dedicated to "formatting needed" so hooks/CI can distinguish from hard failures.
Set-Variable -Name 'PendingChangesExitCode' -Option Constant -Value 2

# --- Runtime state ---

# These are populated on demand when -LogPath is supplied.
$script:LogFileWriter = $null
$script:LogFilePath = $null

function Show-NormalizeHelp {
<#
.SYNOPSIS
    Prints a concise help listing for normalize.ps1.
#>
    [CmdletBinding()]
    param()

    $lines = @(
        'normalize.ps1 - Normalize Source Code',
        '',
        'Usage:',
        '  pwsh -File .\normalize.ps1 [-DryRun] [-Staged] [-DebugMode] [-Help] [-Sequential] [-ThrottleLimit <int>]',
        '                              [-ClangFormatPath <path>] [-LogPath <path>]',
        '',
        'Parameters:',
        '  -DryRun            Reports required changes without modifying files; exit code 2 if updates are needed.',
        '  -Staged            Evaluates only staged files using temporary copies (implies -DryRun).',
        '  -DebugMode         Emits verbose diagnostic logging, including matched file lists.',
        '  -Sequential        Executes all work sequentially to simplify debugging and tracing.',
        '  -ThrottleLimit     Overrides the number of parallel workers (default: physical CPU cores; ignored with -Sequential).',
        '  -ClangFormatPath   Explicit path or command name for clang-format if auto-detection fails.',
        '  -LogPath           Writes a UTF-8 transcript of console output to the specified file or directory.',
        '  -Help              Shows this help overview and exits without performing any normalization.',
        '',
        'Exit codes:',
        '  0  Success (or help shown).',
        '  1  Execution error (e.g., clang-format failure).',
        '  2  Dry run detected files that require normalization.'
    )

    foreach ($line in $lines) {
        Write-Host $line
    }
}

# --- Logging helpers ---

# Central mapping from logical log levels to console colour + short tag.
$script:LogStyles = [ordered]@{
    Heading = @{ Color = 'White';     Tag = 'TASK' }
    Info    = @{ Color = 'Gray';      Tag = 'INFO' }
    Success = @{ Color = 'Green';     Tag = 'SUCCESS' }
    Change  = @{ Color = 'Yellow';    Tag = 'CHANGE' }
    DryRun  = @{ Color = 'Cyan';      Tag = 'DRYRUN' }
    Warning = @{ Color = 'Yellow';    Tag = 'WARN' }
    Error   = @{ Color = 'Red';       Tag = 'ERROR' }
    Debug   = @{ Color = 'DarkGray';  Tag = 'DEBUG' }
}

function Format-LogTimestamp {
    [CmdletBinding()]
    param(
        [TimeSpan]$Elapsed = $script:LogStopwatch.Elapsed
    )
    # Convert the stopwatch reading into text like 00:01:23.45 so every log line shares the same layout.
    # InvariantCulture keeps the decimal point as '.' even on systems that normally use commas.
    return $Elapsed.ToString('hh\:mm\:ss\.ff', [System.Globalization.CultureInfo]::InvariantCulture)
}

function New-LogEntry {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [ValidateSet('Heading', 'Info', 'Success', 'Change', 'DryRun', 'Warning', 'Error', 'Debug')]
        [string]$Level,
        [Parameter(Mandatory)]
        [string]$Message,
        [string[]]$Details = @(),
        [string]$Source
    )

    # Collate non-empty detail strings; Write-LogEntry will expand them line by line.
    $detailBuffer = @()
    foreach ($detail in $Details) {
        if (-not [string]::IsNullOrWhiteSpace($detail)) {
            $detailBuffer += $detail
        }
    }

    $elapsed = $script:LogStopwatch.Elapsed

    return [pscustomobject]@{
        Elapsed = $elapsed
        Level   = $Level
        Message = $Message
        Details = $detailBuffer
        Source  = $Source
    }
}

function Write-LogEntry {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [pscustomobject]$Entry
    )

    # Fall back to INFO styling when an unexpected level shows up.
    $style = $script:LogStyles[$Entry.Level]
    if (-not $style) {
        $style = $script:LogStyles['Info']
    }

    $tag = $style.Tag
    $color = $style.Color
    $sourceSegment = if ($Entry.Source) { "[{0}] " -f $Entry.Source } else { '' }
    $elapsedProperty = $Entry.PSObject.Properties['Elapsed']
    $elapsed = $script:LogStopwatch.Elapsed
    if ($elapsedProperty -and $null -ne $elapsedProperty.Value) {
        # Background workers stamp their own Elapsed to avoid the race with the shared stopwatch snapshot.
        $elapsed = $elapsedProperty.Value
    }

    $timestampText = Format-LogTimestamp -Elapsed $elapsed
    $message = "{0} [{1}] {2}{3}" -f $timestampText, $tag, $sourceSegment, $Entry.Message
    Write-Host -ForegroundColor $color $message

    # Keep diagnostic spam readable by dropping timestamps on verbose debug chatter
    $suppressDetailTimestamp = ($Entry.Level -eq 'Debug') -or ($Entry.Level -eq 'Info' -and $Entry.Source -eq 'startup')

    # We remember detail lines so they can be echoed both to console and optional log file.
    $detailLineBuffer = @()
    if ($Entry.Details -and $Entry.Details.Count -gt 0) {
        foreach ($detail in $Entry.Details) {
            if ([string]::IsNullOrWhiteSpace($detail)) { continue }
            $detailLines = $detail.Split([string[]]@("`r`n", "`n", "`r"), [System.StringSplitOptions]::RemoveEmptyEntries)
            foreach ($line in $detailLines) {
                $detailLineBuffer += $line
                $detailPrefix = if ($suppressDetailTimestamp) { '            ' } else { "{0}        " -f $timestampText }
                Write-Host -ForegroundColor Gray ("{0}{1}" -f $detailPrefix, $line)
            }
        }
    }

    if ($script:LogFileWriter) {
        # Duplicate the console log to disk; StreamWriter.Flush keeps CI logs up-to-date even on crashes.
        $script:LogFileWriter.WriteLine($message)
        foreach ($line in $detailLineBuffer) {
            $detailPrefix = if ($suppressDetailTimestamp) { '            ' } else { "{0}        " -f $timestampText }
            $script:LogFileWriter.WriteLine(("{0}{1}" -f $detailPrefix, $line))
        }
        $script:LogFileWriter.Flush()
    }
}

function Write-LogEntries {
    [CmdletBinding()]
    param(
        [Parameter(ValueFromPipeline)]
        [pscustomobject[]]$Entries
    )

    process {
        # The process block runs once for every object that flows through the pipeline, even when invoked via foreach.
        foreach ($entry in $Entries) {
            if ($null -ne $entry) {
                Write-LogEntry -Entry $entry
            }
        }
    }
}

function Receive-LogEntries {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [System.Collections.Concurrent.ConcurrentQueue[pscustomobject]]$Queue
    )

    $collected = @()
    $item = $null
    # Drain the concurrent queue (used by parallel workers) into an ordered array.
    while ($Queue.TryDequeue([ref]$item)) {
        if ($null -ne $item) {
            $collected += $item
        }
    }

    return $collected
}

function Test-IsWsl {
<#
.SYNOPSIS
    Detects if the current PowerShell session runs under Windows Subsystem for Linux.
#>
    if (-not $IsLinux) { return $false }

    if ($env:WSL_DISTRO_NAME) { return $true } # Available on newer WSL builds.

    foreach ($path in '/proc/sys/kernel/osrelease', '/proc/version') {
        if (-not (Test-Path -LiteralPath $path)) { continue }
        try {
            $content = Get-Content -LiteralPath $path -Raw
            if ($content -match 'microsoft') { return $true }
        } catch {}
    }

    return $false
}

function Get-HostPlatform {
<#
.SYNOPSIS
    Returns a descriptive name for the current platform.
#>
    if ($IsWindows) { return 'Windows' }
    if ($IsMacOS)   { return 'macOS' }
    if (Test-IsWsl) { return 'WSL' }
    if ($IsLinux)   { return 'Linux' }
    return 'Unknown'
}

function Get-PhysicalProcessorCount {
<#
.SYNOPSIS
    Detects the number of physical CPU cores, falling back to logical count when detection is unavailable.
#>
    # Keep the logic compact: default to physical counts when available, otherwise return logical cores.
    if ($IsWindows) {
        try {
            $coreTotal = (
                Get-CimInstance -ClassName Win32_Processor -ErrorAction Stop |
                    Measure-Object -Property NumberOfCores -Sum
            ).Sum
            if ($coreTotal -ge 1) { return [int]$coreTotal }
        } catch {}
    } elseif ($IsMacOS) {
        try {
            $output = & sysctl -n hw.physicalcpu 2>$null
            if ($output) {
                $coreCount = 0
                if ([int]::TryParse(($output | Select-Object -First 1), [ref]$coreCount) -and $coreCount -ge 1) {
                    return $coreCount
                }
            }
        } catch {}
    } elseif ($IsLinux) {
        try {
            $pairs = & lscpu '--parse=Core,Socket' 2>$null
            if ($pairs) {
                $corePairs = $pairs | Where-Object { $_ -and -not $_.StartsWith('#') }
                $uniquePairCount = ($corePairs | Sort-Object -Unique).Count
                if ($uniquePairCount -ge 1) { return [int]$uniquePairCount }
            }
        } catch {}
    }

    return [System.Environment]::ProcessorCount
}

function New-IncludePatternSet {
    [CmdletBinding()]
    param(
        [string[]]$Patterns
    )

    # We build two lookup tables so hot paths can be decided quickly:
    #   1. Extension buckets for simple '*.ext' style globs that only need a
    #      suffix check. The dictionary maps extensions to WildcardPattern
    #      lists so matching files skip evaluating unrelated patterns.
    #   2. A generic list for the remaining expressions that touch directory
    #      names or contain complex tokens ([], **, etc.). These are slower but
    #      unavoidable for richer include rules.
    # Split patterns into extension-only matches (cheap) and more complex wildcards (expensive).
    $extensionBuckets =
        [System.Collections.Generic.Dictionary[string, System.Collections.Generic.List[Management.Automation.WildcardPattern]]]::new(
            [System.StringComparer]::OrdinalIgnoreCase
        )
    $genericPatterns = [System.Collections.Generic.List[Management.Automation.WildcardPattern]]::new()

    foreach ($pattern in $Patterns) {
        if ([string]::IsNullOrWhiteSpace($pattern)) { continue }

        # Canonicalise path separators so the resulting wildcard behaves the
        # same on Windows and POSIX paths. Using '/' keeps the logic compatible
        # with git-style globbing used in normalize_config.json.
        $normalized = ($pattern -replace '\\', '/') -replace '/+', '/'
        $wildcard = [Management.Automation.WildcardPattern]::new($normalized, [Management.Automation.WildcardOptions]::IgnoreCase)

        $patternForExtension = $normalized
        # Many glob examples start with './'; trimming it keeps the later extension check nice and simple for relative paths.
        if ($patternForExtension.StartsWith('./')) { $patternForExtension = $patternForExtension.Substring(2) }

        $hasDirectory = $patternForExtension.IndexOf('/') -ge 0
        # Treat [] like complex because they can encode multiple literal characters, confusing the extension map bucket.
        $hasComplexWildcard = ($patternForExtension -match '[\[\]]')

        if (-not $hasDirectory -and -not $hasComplexWildcard) {
            $ext = [System.IO.Path]::GetExtension($patternForExtension)
            if ($ext) {
                # Extension map: fetch or create the bucket for the current suffix
                # and keep the compiled WildcardPattern for later use.
                $bucket = $null
                if (-not $extensionBuckets.TryGetValue($ext, [ref]$bucket)) {
                    $bucket = [System.Collections.Generic.List[Management.Automation.WildcardPattern]]::new()
                    $extensionBuckets[$ext] = $bucket
                }
                $bucket.Add($wildcard)
                continue
            }
        }

        $genericPatterns.Add($wildcard)
    }

    $extensionMap =
        [System.Collections.Generic.Dictionary[string, Management.Automation.WildcardPattern[]]]::new(
            [System.StringComparer]::OrdinalIgnoreCase
        )
    foreach ($kvp in $extensionBuckets.GetEnumerator()) {
        # Freeze the buckets into arrays so later consumers avoid repeated list
        # enumerations and we signal immutability (matching code never mutates
        # the collection).
        $extensionMap[$kvp.Key] = $kvp.Value.ToArray()
    }

    return [pscustomobject]@{
        ExtensionMap    = $extensionMap
        GenericPatterns = $genericPatterns.ToArray()
    }
}

# --- Functions ---

function Read-NormalizeConfig {
<#
.SYNOPSIS
    Loads normalize_config.json and validates required sections.
.OUTPUTS
    PSCustomObject with 'clangformat' and 'textfiles' sections.
#>
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Configuration file not found: $Path"
    }

    # Be permissive with trailing commas to avoid friction.
    $raw = Get-Content -Raw -LiteralPath $Path
    # ConvertFrom-Json in Windows PowerShell still rejects trailing commas; prune them manually to reduce churn in PR reviews.
    $sanitized = $raw -replace "(?m)\s*,\s*(?=\]|\})", ''
    try {
        $cfg = $sanitized | ConvertFrom-Json
    } catch {
        throw "Failed to parse configuration file '$Path': $($_.Exception.Message)"
    }

    if (-not $cfg.clangformat) { throw "Configuration is missing required 'clangformat' section" }
    if (-not $cfg.textfiles)  { throw "Configuration is missing required 'textfiles' section" }
    if (-not $cfg.clangformat.includes) { throw "Configuration 'clangformat' is missing 'includes'" }
    if (-not $cfg.clangformat.excludes) { throw "Configuration 'clangformat' is missing 'excludes'" }
    if (-not $cfg.textfiles.includes)   { throw "Configuration 'textfiles' is missing 'includes'" }
    if (-not $cfg.textfiles.excludes)   { throw "Configuration 'textfiles' is missing 'excludes'" }

    return $cfg
}

function Resolve-ClangFormatPath {
<#
.SYNOPSIS
    Resolves clang-format path from explicit value or PATH/VS install.
.OUTPUTS
    String path or $null if not found.
#>
    param(
        [string]$Candidate
    )

    if ($Candidate) {
        if (Test-Path -LiteralPath $Candidate) {
            return (Resolve-Path -LiteralPath $Candidate).ProviderPath
        }

        $candidateCmd = Get-Command $Candidate -ErrorAction SilentlyContinue
        if ($candidateCmd) { return $candidateCmd.Source }

        # Bubble the unresolved candidate up so the caller can surface a precise error message for humans.
        return $Candidate
    }

    $platform = Get-HostPlatform
    # Collect possible executable locations so we can return the first existing one.
    $candidatePaths = [System.Collections.Generic.List[string]]::new()

    if (-Not $IsWindows) {
        foreach ($name in 'clang-format', 'clang-format-19') {
            $cmd = Get-Command $name -ErrorAction SilentlyContinue
            if ($cmd) { [void]$candidatePaths.Add($cmd.Source) }
        }
    }

    if ($IsWindows) {
        # Mirror the documented install paths to minimise surprises for contributors.
        $candidatePaths.Add('C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-format.exe')
        $candidatePaths.Add('C:\Program Files\LLVM\bin\clang-format.exe')
    } elseif ($platform -eq 'macOS') {
        $candidatePaths.Add('/opt/homebrew/opt/llvm@19/bin/clang-format')
    } elseif ($platform -eq 'WSL') {
        $candidatePaths.Add('/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang-format.exe')
        $candidatePaths.Add('/mnt/c/Program Files/LLVM/bin/clang-format.exe')
    }

    # Weed out duplicates produced by PATH scans vs. hard-coded fallbacks.
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $candidatePaths) {
        if ([string]::IsNullOrWhiteSpace($path)) { continue }
        if (-not $seen.Add($path)) { continue }
        try {
            if (Test-Path -LiteralPath $path) {
                $resolved = Resolve-Path -LiteralPath $path -ErrorAction Stop
                if ($resolved) { return $resolved.ProviderPath }
            }
        } catch {}
    }

    return $null
}

function Get-CommandVersion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Command
    )

    try {
        $output = & $Command --version 2>$null
        if ($null -eq $output) { return 'unknown (no output)' }
        if ($output -is [System.Array] -and $output.Length -gt 0) {
            $line = $output[0]
        } else {
            $line = $output
        }

        $versionText = [string]$line
        if ([string]::IsNullOrWhiteSpace($versionText)) {
            return 'unknown (empty output)'
        }

        return $versionText.Trim()
    } catch {
        return "unknown ($($_.Exception.Message))"
    }
}


function Get-MatchingFiles {
<#
.SYNOPSIS
    Enumerates files matching include/exclude globs.
.OUTPUTS
    Relative file paths ('./path/to/file').
#>
    param (
        [string[]]$Includes,
        [string[]]$Excludes
    )

    if (-not $Includes -or $Includes.Count -eq 0) { return @() }

    $basePath = [System.IO.Path]::GetFullPath($PSScriptRoot)

    # Compile include globs once so each file match only performs quick
    # WildcardPattern checks instead of re-parsing the patterns.
    $includeSet = New-IncludePatternSet -Patterns $Includes
    # Convert excludes into WildcardPattern upfront; they are typically few and
    # reused for every candidate file.
    $excludePatterns = foreach ($p in $Excludes) {
        $np = ($p -replace '\\', '/') -replace '/+', '/'
        [Management.Automation.WildcardPattern]::new($np, [Management.Automation.WildcardOptions]::IgnoreCase)
    }

    # HashSet keeps duplicates out when overlapping includes resolve to same file.
    $resultFiles = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    # Use the modern enumeration API to avoid exceptions on unreadable directories.
    $enumOptions = [System.IO.EnumerationOptions]::new()
    $enumOptions.RecurseSubdirectories = $true
    $enumOptions.IgnoreInaccessible = $true
    $enumOptions.ReturnSpecialDirectories = $false

    # EnumerateFiles is dramatically faster than Get-ChildItem for large trees and gracefully skips ACL issues when configured.
    foreach ($filePath in [System.IO.Directory]::EnumerateFiles($basePath, '*', $enumOptions)) {
        $relative = [System.IO.Path]::GetRelativePath($basePath, $filePath)
        $relativePath = [System.IO.Path]::Combine('.', $relative)
        # Normalise separators once so the wildcard engine behaves deterministically.
        $relativePathSlash = ($relativePath -replace '\\', '/') -replace '/+', '/'

        $isIncluded = $false
        $extension = [System.IO.Path]::GetExtension($relativePath)

        $extPatterns = $null
        if ($extension -and $includeSet.ExtensionMap.TryGetValue($extension, [ref]$extPatterns)) {
            # First try the cheap extension bucket – ideal for the common case of
            # rules like '**/*.cpp'. We stop as soon as a pattern matches.
            foreach ($pattern in $extPatterns) {
                if ($pattern.IsMatch($relativePathSlash)) { $isIncluded = $true; break }
            }
        }

        if (-not $isIncluded -and $includeSet.GenericPatterns.Length -gt 0) {
            # Fallback to generic patterns for paths with directories or complex
            # tokens. These are more expensive, so they are only evaluated when
            # extension buckets did not yield a match.
            foreach ($pattern in $includeSet.GenericPatterns) {
                if ($pattern.IsMatch($relativePathSlash)) { $isIncluded = $true; break }
            }
        }

        if (-not $isIncluded) { continue }

        $isExcluded = $false
        if ($excludePatterns.Count -gt 0) {
            # Exclusions always run, even after an include match, so blacklist
            # entries can override broad includes (mirrors gitignore semantics).
            foreach ($pattern in $excludePatterns) {
                if ($pattern.IsMatch($relativePathSlash)) { $isExcluded = $true; break }
            }
        }

        if (-not $isExcluded) { [void]$resultFiles.Add($relativePath) }
    }

    return $resultFiles | Sort-Object -Culture "en-US"
}

function Get-FileBatches {
<#
.SYNOPSIS
    Splits a collection of items into batches of a specified size.
#>
    param(
        [string[]]$Files,
        [int]$BatchSize
    )

    # Wrapping $Files in @() guarantees we always work with a proper array, even when only one file is supplied.
    $fileList = @($Files)
    $batches = @()
    if ($fileList.Count -gt 0 -and $BatchSize -gt 0) {
        for ($i = 0; $i -lt $fileList.Count; $i += $BatchSize) {
            # Using .. slices means the last batch can be smaller without extra bookkeeping.
            $end = [Math]::Min($i + $BatchSize - 1, $fileList.Count - 1)
            $batches += , @($fileList[$i..$end])
        }
    }
    return $batches
}

function Invoke-ClangFormat {
<#
.SYNOPSIS
    Formats C/C++ files using clang-format in batches.
#>
    param (
        [string[]]$Includes,
        [string[]]$Excludes
    )

    Write-LogEntry (New-LogEntry -Level 'Info' -Message 'Formatting with clang-format...' -Source 'clang-format')
    $files = Get-MatchingFiles -Includes $Includes -Excludes $Excludes
    if ($DebugMode) {
        $debugDetails = if ($files.Count -gt 0) { $files } else { @() }
        Write-LogEntry (New-LogEntry -Level 'Debug' `
            -Message ("Matched {0} file(s) for clang-format." -f $files.Count) `
            -Details $debugDetails `
            -Source 'clang-format')
    }
    Write-LogEntry (New-LogEntry -Level 'Info' -Message ("Files queued: {0}" -f $files.Count) -Source 'clang-format')
    if ($files.Count -eq 0) { return @() }

    $rootPath = $PSScriptRoot

    # Snapshot file hashes upfront so normal runs can later determine which files clang-format truly rewrote.
    $preHashWarnings = @()
    $initialFileHashes = $null
    if (-not $DryRun) {
        $initialFileHashes = [System.Collections.Generic.Dictionary[string, string]]::new([System.StringComparer]::OrdinalIgnoreCase)
        foreach ($file in $files) {
            try {
                $fullPath = [System.IO.Path]::GetFullPath((Join-Path -Path $rootPath -ChildPath $file))
                if (-not (Test-Path -LiteralPath $fullPath)) { continue }
                $hash = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
                $initialFileHashes[$file] = $hash
            }
            catch {
                # Keep the run going but surface IO/permission issues that prevented baseline hashing.
                $warning = New-LogEntry -Level 'Warning' `
                    -Message ("Failed to capture pre-format hash: {0}" -f $file) `
                    -Source 'clang-format' `
                    -Details @($_.Exception.Message)
                $preHashWarnings += $warning
            }
        }
    }

    $batches = Get-FileBatches -Files $files -BatchSize 50
    $logQueue = [System.Collections.Concurrent.ConcurrentQueue[pscustomobject]]::new()

    # This script block describes the work a single worker performs; PowerShell clones it for parallel runspaces.
    $processClangBatch = {
        param(
            [string[]]$FileBatch,
            [System.Collections.Concurrent.ConcurrentQueue[pscustomobject]]$Queue,
            [string]$ClangFormatPath,
            [bool]$DryRun,
            [string]$Root
        )

        $pathLookup = $null
        $rootNormalized = $null
        if ($DryRun) {
            # clang-format prints absolute paths with forward slashes; cache normalized
            # variants so warnings can be correlated back to the relative paths we queued.
            $rootNormalized = ($Root -replace '\\', '/').TrimEnd('/')
            $pathLookup = @()
            foreach ($file in $FileBatch) {
                if ([string]::IsNullOrWhiteSpace($file)) { continue }
                # Remove leading ./ or directory separators so we can compare against
                # clang-format output regardless of how the batch path was written.
                $trimmed = [System.Text.RegularExpressions.Regex]::Replace($file, '^[\.\\/]+', '')
                if ([string]::IsNullOrWhiteSpace($trimmed)) { continue }
                # Normalize separators to forward slashes because clang-format warnings
                # use that style even on Windows.
                $normalized = ($trimmed -replace '\\', '/')
                $pathLookup += [pscustomobject]@{
                    Relative  = $file
                    Normalized = $normalized
                }
            }
        }

        try {
            $startInfo = New-Object System.Diagnostics.ProcessStartInfo
            $startInfo.WorkingDirectory = $Root
            $startInfo.FileName = $ClangFormatPath
            $startInfo.RedirectStandardError = $true
            $startInfo.RedirectStandardOutput = $true
            $startInfo.UseShellExecute = $false

            $quotedFiles = $FileBatch | ForEach-Object { '"' + $_ + '"' }
            if ($DryRun) {
                # Dry-run relies on warning output so we keep exit code 0 and analyse the results manually.
                $startInfo.Arguments = "--dry-run"
            } else {
                $startInfo.Arguments = "-i"
            }
            # --verbose keeps clang-format chatty, helping diagnose odd formatting rules in CI logs.
            $startInfo.Arguments = "--verbose " + $startInfo.Arguments + " " + ($quotedFiles -join ' ')

            $process = New-Object System.Diagnostics.Process
            $process.StartInfo = $startInfo
            [void]$process.Start()
            # Explicit async reads avoid the classic deadlock where synchronous ReadToEnd waits for buffers to drain.
            $stdoutTask = $process.StandardOutput.ReadToEndAsync()
            $stderrTask = $process.StandardError.ReadToEndAsync()
            $process.WaitForExit()

            $stdout = $stdoutTask.Result
            $stderr = $stderrTask.Result

            # Track the relative paths that clang-format says would change so the
            # summary can flag pending work without relying on exit codes.
            $violationList = $null
            if ($DryRun) {
                $violationList = [System.Collections.Generic.List[string]]::new()
                $violationSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
                $outputBlocks = @()
                if (-not [string]::IsNullOrWhiteSpace($stdout)) { $outputBlocks += $stdout }
                if (-not [string]::IsNullOrWhiteSpace($stderr)) { $outputBlocks += $stderr }

                if ($outputBlocks.Count -gt 0) {
                    $stringComparison = [System.StringComparison]::OrdinalIgnoreCase

                    foreach ($block in $outputBlocks) {
                        foreach ($line in ($block -split '\r?\n')) {
                            if ([string]::IsNullOrWhiteSpace($line)) { continue }
                            # The dry-run warning format is: "<path>:line:col: warning: code should be clang-formatted"
                            if ($line -match '^(?<path>.+?):\d+:\d+:\s+warning: code should be clang-formatted') {
                                $rawPath = $Matches['path'].Trim()
                                if ([string]::IsNullOrWhiteSpace($rawPath)) { continue }

                                $normalizedPath = ($rawPath -replace '\\', '/').Trim()
                                $relativePath = $null

                                if ($pathLookup -and $pathLookup.Count -gt 0) {
                                    # Fast-path: clang-format often echoes the absolute path rooted at the
                                    # working directory. If that prefix matches the current batch root we can
                                    # strip it and rebuild the repo-style relative path.
                                    if (-not [string]::IsNullOrWhiteSpace($rootNormalized) -and
                                        $normalizedPath.StartsWith($rootNormalized, $stringComparison)) {
                                        $relativeCandidate = $normalizedPath.Substring($rootNormalized.Length)
                                        if ($relativeCandidate.StartsWith('/')) {
                                            $relativeCandidate = $relativeCandidate.Substring(1)
                                        }
                                        if (-not [string]::IsNullOrWhiteSpace($relativeCandidate)) {
                                            $relativeSystem = $relativeCandidate.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
                                            $relativePath = [System.IO.Path]::Combine('.', $relativeSystem)
                                        }
                                    }

                                    if (-not $relativePath) {
                                        # Fallback: compare against the cached batch entries to catch cases
                                        # where clang-format prints relative paths or omits the root prefix.
                                        foreach ($entry in $pathLookup) {
                                            $candidate = $entry.Normalized
                                            if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
                                            if ($normalizedPath.Equals($candidate, $stringComparison) -or
                                                $normalizedPath.EndsWith('/' + $candidate, $stringComparison)) {
                                                $relativePath = $entry.Relative
                                                break
                                            }
                                        }
                                    }
                                }

                                if (-not $relativePath) {
                                    # As a last resort keep the original string so at least the warning is
                                    # surfaced; this should be rare but avoids hiding data.
                                    $relativePath = $rawPath
                                }

                                # Guard against duplicate warnings so the summary remains concise.
                                if ($violationSet.Add($relativePath)) {
                                    $null = $violationList.Add($relativePath)
                                }
                            }
                        }
                    }
                }
            }

            if ($process.ExitCode -ne 0) {
                $message = if ($DryRun) {
                    "clang-format would reformat files: $($FileBatch -join ', ')"
                } else {
                    "clang-format failed for files: $($FileBatch -join ', ')"
                }

                $details = @()
                if (-not [string]::IsNullOrWhiteSpace($stderr)) { $details += $stderr }
                if (-not [string]::IsNullOrWhiteSpace($stdout)) { $details += $stdout }

                $Queue.Enqueue([pscustomobject]@{
                    Level              = 'Error'
                    Message            = $message
                    Details            = $details
                    Source             = 'clang-format'
                    # Flag dry-run hits as "pending" so the caller can tell formatting is required.
                    ChangePending      = $DryRun
                    IsDryRunChange     = $DryRun
                    # Provide the exact file batch; summary logic relies on this later.
                    Files              = $FileBatch
                    ChangeDescription  = if ($DryRun) { 'clang-format' } else { 'clang-format failure' }
                })
            }
            elseif ($DryRun -and $violationList -and $violationList.Count -gt 0) {
                foreach ($fileNeedsFormat in $violationList) {
                    $Queue.Enqueue([pscustomobject]@{
                        Level              = 'Change'
                        Message            = "Would format: $fileNeedsFormat"
                        Details            = @()
                        Source             = 'clang-format'
                        ChangePending      = $true
                        IsDryRunChange     = $true
                        Files              = @($fileNeedsFormat)
                        ChangeDescription  = 'clang-format'
                    })
                }
            }
        }
        catch {
            $Queue.Enqueue([pscustomobject]@{
                Level             = 'Error'
                Message           = "clang-format batch failed for files: $($FileBatch -join ', ')"
                Details           = @($_.Exception.Message)
                Source            = 'clang-format'
                # No ChangePending flag here; these are true errors, not "needs formatting".
                Files             = $FileBatch
                ChangeDescription = 'clang-format failure'
            })
        }
    }

    if ($Sequential) {
        foreach ($batch in $batches) {
            & $processClangBatch -FileBatch $batch -Queue $logQueue -ClangFormatPath $ClangFormatPath -DryRun:$DryRun -Root $rootPath
        }
    } else {
        # Serialize the script block so background runspaces can recreate it without sharing state objects.
        $processClangBatchSerialized =
            [System.Management.Automation.PSSerializer]::Serialize($processClangBatch)
        $batchIndices = 0..($batches.Count - 1)
        $batchIndices | ForEach-Object -ThrottleLimit $ThrottleLimit -Parallel {
            $index = $_
            $batchProcessor = [scriptblock]::Create(
                [System.Management.Automation.PSSerializer]::Deserialize($using:processClangBatchSerialized)
            )
            $allBatches = $using:batches
            $batch = $allBatches[$index]
            & $batchProcessor -FileBatch $batch `
                -Queue $using:logQueue `
                -ClangFormatPath $using:ClangFormatPath `
                -DryRun:$using:DryRun `
                -Root $using:rootPath
        }
    }

    $queuedEntries = @(Receive-LogEntries -Queue $logQueue)
    $orderedEntries = @($queuedEntries)
    $hashComparisonWarnings = @()

    if (-not $DryRun -and $initialFileHashes) {
        # Compare the post-run hashes against the baseline so we can flag exactly which files changed.
        $changedEntries = @()
        foreach ($file in ($initialFileHashes.Keys | Sort-Object)) {
            try {
                $fullPath = [System.IO.Path]::GetFullPath((Join-Path -Path $rootPath -ChildPath $file))
                if (-not (Test-Path -LiteralPath $fullPath)) {
                    # Surface unexpected file removal; clang-format should never delete inputs.
                    $missingEntry = New-LogEntry -Level 'Warning' `
                        -Message ("File missing after clang-format: {0}" -f $file) `
                        -Source 'clang-format'
                    $hashComparisonWarnings += $missingEntry
                    continue
                }

                $currentHash = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
                if ($initialFileHashes[$file] -ne $currentHash) {
                    # Emit a synthetic info log so the summary knows this file was reformatted.
                    $entry = New-LogEntry -Level 'Change' -Message ("Formatted: {0}" -f $file) -Source 'clang-format'
                    $entry | Add-Member -NotePropertyName 'Files' -NotePropertyValue @($file)
                    $entry | Add-Member -NotePropertyName 'ChangeDescription' -NotePropertyValue 'clang-format'
                    $changedEntries += $entry
                }
            }
            catch {
                # Capture hash verification failures (e.g., permissions flipped between runs).
                $warning = New-LogEntry -Level 'Warning' `
                    -Message ("Failed to verify post-format hash: {0}" -f $file) `
                    -Source 'clang-format' `
                    -Details @($_.Exception.Message)
                $hashComparisonWarnings += $warning
            }
        }

        if ($changedEntries) {
            $orderedEntries += $changedEntries
        }
    }

    if ($preHashWarnings) {
        $orderedEntries += $preHashWarnings
    }
    if ($hashComparisonWarnings) {
        $orderedEntries += $hashComparisonWarnings
    }

    return $orderedEntries
}

function Invoke-TextNormalization {
<#
.SYNOPSIS
    Normalizes file encoding to UTF-8 with BOM and consistent EOLs.
#>
    param (
        [string[]]$Includes,
        [string[]]$Excludes
    )

    Write-LogEntry (New-LogEntry -Level 'Info' `
        -Message 'Checking text normalization (UTF-8 BOM + EOL)...' `
        -Source 'text-normalization')
    $files = Get-MatchingFiles -Includes $Includes -Excludes $Excludes
    if ($DebugMode) {
        $debugDetails = if ($files.Count -gt 0) { $files } else { @() }
        Write-LogEntry (New-LogEntry -Level 'Debug' `
            -Message ("Matched {0} file(s) for text normalization." -f $files.Count) `
            -Details $debugDetails `
            -Source 'text-normalization')
    }
    Write-LogEntry (New-LogEntry -Level 'Info' -Message ("Files queued: {0}" -f $files.Count) -Source 'text-normalization')
    if ($files.Count -eq 0) { return @() }

    $platform = Get-HostPlatform
    # Windows + WSL repos agree on CRLF because git.exe expects it; native Linux/macOS prefer LF.
    $useWindowsEol = ($IsWindows -or $platform -eq 'WSL')
    $desiredNewline = if ($useWindowsEol) { "`r`n" } else { "`n" }
    $newlineLabel = if ($useWindowsEol) { 'CRLF' } else { 'LF' }

    # Process up to 200 files per worker; keeps memory use contained for huge repos.
    $batches = Get-FileBatches -Files $files -BatchSize 200
    $logQueue = [System.Collections.Concurrent.ConcurrentQueue[pscustomobject]]::new()
    $rootPath = $PSScriptRoot

    # Define the per-batch work for text files; each worker will run this block with its slice of paths.
    $processTextBatch = {
        param(
            [string[]]$FileBatch,
            [System.Collections.Concurrent.ConcurrentQueue[pscustomobject]]$Queue,
            [string]$Root,
            [string]$DesiredNewline,
            [string]$NewlineLabel,
            [bool]$DryRun
        )

        foreach ($file in $FileBatch) {
            try {
                $fullPath = [System.IO.Path]::GetFullPath((Join-Path -Path $Root -ChildPath $file))

                # Read raw bytes so we can detect BOM + normalise line endings without relying on PowerShell encodings.
                $bytes = [System.IO.File]::ReadAllBytes($fullPath)
                $streamLength = $bytes.Length

                $hasBom = $false
                if ($streamLength -ge 3) {
                    $hasBom = ($bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
                }

                # Use strict UTF-8 decoding to surface invalid byte sequences as exceptions.
                $utf8Strict = [System.Text.UTF8Encoding]::new($false, $true)
                if ($hasBom) {
                    $text = $utf8Strict.GetString($bytes, 3, $bytes.Length - 3)
                } else {
                    $text = $utf8Strict.GetString($bytes)
                }

                # First collapse every variant to LF so the subsequent replace can fan back out to the desired newline.
                $eolNormalized = $text -replace "`r`n", "`n" -replace "`r", "`n"
                if ($DesiredNewline -eq "`r`n") {
                    $eolNormalized = $eolNormalized -replace "`n", "`r`n"
                }

                $shouldAddBom = (-not $hasBom)
                $eolChanged = ($text -ne $eolNormalized)

                if ($shouldAddBom -or $eolChanged) {
                    $changes = @()
                    if ($shouldAddBom) { $changes += 'BOM' }
                    if ($eolChanged) { $changes += "EOL to $NewlineLabel" }
                    $changeString = $changes -join ', '

                    if ($DryRun) {
                        $Queue.Enqueue([pscustomobject]@{
                            Level             = 'Change'
                            Message           = "Would normalize ($changeString): $file"
                            Details           = @()
                            Source            = 'text-normalization'
                            # Mark as pending so CI exit code can report "needs normalization" without touching files.
                            ChangePending     = $true
                            IsDryRunChange    = $true
                            Files             = @($file)
                            ChangeDescription = $changeString
                        })
                    } else {
                        # Emit BOM + CRLF/LF using .NET to avoid PowerShell's default UTF-16 output.
                        $encoding = New-Object System.Text.UTF8Encoding($true, $true)
                        [System.IO.File]::WriteAllText($fullPath, $eolNormalized, $encoding)
                        $Queue.Enqueue([pscustomobject]@{
                            Level             = 'Change'
                            Message           = "Normalized ($changeString): $file"
                            Details           = @()
                            Source            = 'text-normalization'
                            Files             = @($file)
                            ChangeDescription = $changeString
                        })
                    }
                }
            }
            catch {
                $Queue.Enqueue([pscustomobject]@{
                    Level             = 'Error'
                    Message           = "utf-8-bom/eol failed for $file"
                    Details           = @($_.Exception.Message)
                    Source            = 'text-normalization'
                    # Include the file for completeness even on errors; aids log triage.
                    Files             = @($file)
                    ChangeDescription = 'utf-8-bom/eol failure'
                })
            }
        }
    }

    if ($Sequential) {
        foreach ($batch in $batches) {
            & $processTextBatch -FileBatch $batch `
                -Queue $logQueue `
                -Root $rootPath `
                -DesiredNewline $desiredNewline `
                -NewlineLabel $newlineLabel `
                -DryRun:$DryRun
        }
    } else {
        # Serialize the block so the Parallel switch can send it to background threads safely.
        $processTextBatchSerialized =
            [System.Management.Automation.PSSerializer]::Serialize($processTextBatch)
        $batchIndices = 0..($batches.Count - 1)
        $batchIndices | ForEach-Object -ThrottleLimit $ThrottleLimit -Parallel {
            $index = $_
            $batchProcessor = [scriptblock]::Create(
                [System.Management.Automation.PSSerializer]::Deserialize($using:processTextBatchSerialized)
            )
            $allBatches = $using:batches
            $batch = $allBatches[$index]
            & $batchProcessor -FileBatch $batch `
                -Queue $using:logQueue `
                -Root $using:rootPath `
                -DesiredNewline $using:desiredNewline `
                -NewlineLabel $using:newlineLabel `
                -DryRun:$using:DryRun
        }
    }

    return Receive-LogEntries -Queue $logQueue
}

function Enter-StagedMode {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [psobject]$Config,
        [Parameter(Mandatory)]
        [string]$StagedFileInfix,
        [Parameter(Mandatory)]
        [string]$ScriptRoot
    )

    Write-LogEntry (New-LogEntry -Level 'Info' -Message 'Running in -Staged mode (implies -DryRun)' -Source 'staged-mode')

    # Check for pre-existing temp files which would indicate a problem
    $existingTempFiles = Get-ChildItem -Path $ScriptRoot -Recurse -File -Filter "*$StagedFileInfix*"
    if ($existingTempFiles) {
        $fileList = ($existingTempFiles.FullName | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
        throw (
            "Found existing files with staged infix ('$StagedFileInfix'), indicating a prior script run failed to clean up. " +
            "Please remove them before running with -Staged:`n$fileList"
        )
    }

    $stagedGitFiles = & git diff --name-only --cached --diff-filter=ACMR
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to enumerate staged files (git diff exit code $LASTEXITCODE)."
    }

    $tempFiles = @()
    $tempDirs = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($fileRelPath in @($stagedGitFiles)) {
        $extension = [System.IO.Path]::GetExtension($fileRelPath)
        # Build a parallel filename (e.g. Foo.git_staged.cpp) so we modify a throwaway copy rather than the staged blob.
        $fileNameWithoutExt = $fileRelPath.Substring(0, $fileRelPath.Length - $extension.Length)
        $tempFileRelPath = "$fileNameWithoutExt.$StagedFileInfix$extension"
        $tempFileAbsPath = Join-Path $ScriptRoot $tempFileRelPath
        $tempDir = Split-Path $tempFileAbsPath -Parent
        if (-not (Test-Path $tempDir)) {
            # PowerShell's -Force handles nested directory creation for staged paths.
            New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
            [void]$tempDirs.Add($tempDir)
        }
        # --filters honours .gitattributes (LFS, eol conversions, etc.), and the '>' redirection writes the staged bytes
        # into our temp copy so we never touch the index.
        git cat-file --filters ":0:$fileRelPath" > $tempFileAbsPath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to materialize staged file '$fileRelPath' (git cat-file exit code $LASTEXITCODE)."
        }
        $tempFiles += $tempFileAbsPath
    }

    # Modify config to use .staged files
    $newClangIncludes = $Config.clangformat.includes | ForEach-Object {
        $ext = [System.IO.Path]::GetExtension($_)
        # Clone every include entry so staged copies opt-out of touching the original tracked files.
        if ($ext) { [System.IO.Path]::ChangeExtension($_, ".$StagedFileInfix$ext") } else { "$_.$StagedFileInfix" }
    }
    $Config.clangformat.includes = $newClangIncludes

    $newTextIncludes = $Config.textfiles.includes | ForEach-Object {
        $ext = [System.IO.Path]::GetExtension($_)
        # Text normalization honours the same staged suffix so clang-format/text passes stay in lockstep.
        if ($ext) { [System.IO.Path]::ChangeExtension($_, ".$StagedFileInfix$ext") } else { "$_.$StagedFileInfix" }
    }
    $Config.textfiles.includes = $newTextIncludes

    return [pscustomobject]@{
        Config    = $Config
        TempFiles = $tempFiles
        TempDirs  = $tempDirs
    }
}

function Exit-StagedMode {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$TempFiles,
        [System.Collections.Generic.HashSet[string]]$TempDirs
    )

    if ($TempFiles.Count -eq 0) { return }

    Write-LogEntry (New-LogEntry -Level 'Info' -Message 'Cleaning up temporary staged files...' -Source 'staged-mode')
    foreach ($tempFile in $TempFiles) {
        if (Test-Path $tempFile) {
            Remove-Item $tempFile -Force
        }
    }

    if ($null -ne $TempDirs) {
        # Clean up directories we created, starting from the deepest, if they are empty
        $dirsToRemove = $TempDirs.GetEnumerator() | Sort-Object -Descending
        foreach ($dir in $dirsToRemove) {
            if (Test-Path $dir) {
                if ((Get-ChildItem -Path $dir -Force).Count -eq 0) {
                    Remove-Item -LiteralPath $dir -Force -Recurse
                }
            }
        }
    }
}

# Respond to explicit help requests before performing any expensive work.
if (-not $PSBoundParameters.ContainsKey('ThrottleLimit')) {
    try {
        $physicalCoreCount = [int](Get-PhysicalProcessorCount)
        $ThrottleLimit = [Math]::Max(1, $physicalCoreCount)
    }
    catch {
        $ThrottleLimit = [Math]::Max(1, [System.Environment]::ProcessorCount)
    }
}

if ($Help) {
    Show-NormalizeHelp
    exit $SuccessExitCode
}

# --- Initialization ---

$script:LogStopwatch = [Diagnostics.Stopwatch]::StartNew() # Shared timer produces consistent timestamps across threads.
$ErrorActionPreference = 'Stop'
# Normalization should operate relative to the repo root even when invoked through symlinks or different cwd.
Set-Location -Path $PSScriptRoot
[Console]::ResetColor()

$exitCode = $SuccessExitCode
$allLogEntries = @()
$enteredStagedMode = $false
$stagedFiles = @()
$stagedDirs = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

try {
    if ($LogPath) {
        # Resolve user supplied path relative to repo root when needed; GitHub runners typically pass relative paths.
        $logFullPath = if ([System.IO.Path]::IsPathRooted($LogPath)) {
            [System.IO.Path]::GetFullPath($LogPath)
        } else {
            [System.IO.Path]::GetFullPath((Join-Path -Path $PSScriptRoot -ChildPath $LogPath))
        }

        $logDirectory = [System.IO.Path]::GetDirectoryName($logFullPath)
        if ($logDirectory -and -not (Test-Path -LiteralPath $logDirectory)) {
            # Ensure the folder exists up-front; without this StreamWriter would throw in CI.
            New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
        }

        # Use UTF-8 without BOM for log files; human-friendly yet still ASCII-safe.
        $logEncoding = New-Object System.Text.UTF8Encoding($false)
        $script:LogFileWriter = New-Object System.IO.StreamWriter($logFullPath, $false, $logEncoding)
        $script:LogFilePath = $logFullPath
    }

    $configPath = Join-Path -Path $PSScriptRoot -ChildPath 'normalize_config.json'
    # Shared include/exclude rules live in JSON so teams can tweak without touching the script.
    $config = Read-NormalizeConfig -Path $configPath

    $ClangFormatPath = Resolve-ClangFormatPath -Candidate $ClangFormatPath

    # Staged mode works on disposable copies so the user's index stays unchanged.
    if ($Staged) {
        $DryRun = $true # Never mutate staged content directly; report-only so users can accept changes.
        $stagedSession = Enter-StagedMode -Config $config -StagedFileInfix $StagedFileInfix -ScriptRoot $PSScriptRoot
        $config = $stagedSession.Config
        $stagedFiles = $stagedSession.TempFiles
        $stagedDirs = $stagedSession.TempDirs
        $enteredStagedMode = $true
    }

    if ($DryRun) {
        Write-LogEntry (New-LogEntry -Level 'Heading' -Message 'Normalize Source Code [DRY RUN MODE]' -Source 'startup')
    } else {
        Write-LogEntry (New-LogEntry -Level 'Heading' -Message 'Normalize Source Code' -Source 'startup')
    }

    if ($script:LogFileWriter) {
        # Make it obvious to humans skimming the console where the archived log lives.
        Write-LogEntry (New-LogEntry -Level 'Info' -Message ("Writing detailed log to: $script:LogFilePath") -Source 'startup')
    }

    $platformName = Get-HostPlatform
    $osVersion = [System.Environment]::OSVersion.Version
    $powerShellVersion = $PSVersionTable.PSVersion
    $clangPathValid = $false
    $clangVersionText = 'n/a (not found)'
    $gitVersionText = Get-CommandVersion -Command 'git'

    if ($ClangFormatPath -and (Test-Path -LiteralPath $ClangFormatPath)) {
        $clangPathValid = $true
        $clangVersionText = Get-CommandVersion -Command $ClangFormatPath
    }

    $parallelWorkers = if ($Sequential) { 'disabled (-Sequential)' } else { $ThrottleLimit }

    Write-LogEntry (New-LogEntry -Level 'Info' -Message 'Runtime information' -Source 'startup' -Details @(
        "Script version: $ScriptVersion",
        "Git: $gitVersionText",
        "clang-format: $clangVersionText",
        "Platform: $platformName ($osVersion)",
        "PowerShell: $powerShellVersion",
        "Parallel workers: $parallelWorkers"
    ))

    if (-not $clangPathValid) {
        throw 'clang-format not found. Please install it or specify the path via -ClangFormatPath.'
    }

    # Collect every log entry so final summary can reason about results in aggregate.
    $clangEntries = @(Invoke-ClangFormat -Includes $config.clangformat.includes -Excludes $config.clangformat.excludes)
    if ($clangEntries.Count -gt 0) {
        $clangEntries | Write-LogEntries
        $allLogEntries += $clangEntries
    }

    $textEntries = @(Invoke-TextNormalization -Includes $config.textfiles.includes -Excludes $config.textfiles.excludes)
    if ($textEntries.Count -gt 0) {
        $textEntries | Write-LogEntries
        $allLogEntries += $textEntries
    }

    if ($script:LogStopwatch.IsRunning) {
        $script:LogStopwatch.Stop()
    }

    $summaryElapsedText = Format-LogTimestamp -Elapsed $script:LogStopwatch.Elapsed

    $errors = @($allLogEntries | Where-Object {
        $_.Level -eq 'Error' -and (-not ($_.PSObject.Properties['IsDryRunChange'] -and $_.IsDryRunChange))
    })

    $pendingChanges = @($allLogEntries | Where-Object {
        $prop = $_.PSObject.Properties['ChangePending']
        $prop -and $prop.Value
    })

    if ($errors.Count -gt 0) {
        $summaryMessage = if ($DryRun) {
            "Dry run finished with $($errors.Count) execution error(s)."
        } else {
            "Normalization finished with $($errors.Count) execution error(s)."
        }

        Write-LogEntry (New-LogEntry -Level 'Error' -Message $summaryMessage -Source 'summary')
        $exitCode = $ErrorExitCode
    }
    elseif ($DryRun -and $pendingChanges.Count -gt 0) {
        # Keep the summary terse; prior steps already emitted detailed warnings.
        $summaryMessage = 'Dry run detected pending normalization.'
        Write-LogEntry (New-LogEntry -Level 'Error' -Message $summaryMessage -Source 'summary')

        $instructionMessage = if ($Staged) {
            'Run normalize.ps1 without -Staged to apply the fixes, then restage the updated files.'
        } else {
            'Run normalize.ps1 (without -DryRun) to apply the required formatting changes.'
        }
        Write-LogEntry (New-LogEntry -Level 'Info' -Message $instructionMessage -Source 'summary')
        # Exit code 2 tells CI "work is needed" without being a hard error: allows targeted messaging.
        $exitCode = $PendingChangesExitCode
    }
    else {
        Write-LogEntry (New-LogEntry -Level 'Success' -Message 'Normalization finished successfully.')
    }

    Write-LogEntry (New-LogEntry -Level 'Info' -Message ("Total execution time: {0}" -f $summaryElapsedText) -Source 'summary')
}
catch {
    if ($script:LogStopwatch.IsRunning) {
        $script:LogStopwatch.Stop()
    }

    $exitCode = $ErrorExitCode
    $errorMessage = $_.Exception.Message
    $detailLines = @()
    if (-not [string]::IsNullOrWhiteSpace($_.ScriptStackTrace)) {
        $detailLines += $_.ScriptStackTrace
    }

    # Prefix fatal errors explicitly so the calling automation can surface the stack trace.
    Write-LogEntry (New-LogEntry -Level 'Error' -Message "Fatal error: $errorMessage" -Details $detailLines -Source 'fatal')
    $summaryElapsedText = Format-LogTimestamp -Elapsed $script:LogStopwatch.Elapsed
    Write-LogEntry (New-LogEntry -Level 'Info' -Message ("Total execution time: {0}" -f $summaryElapsedText) -Source 'summary')
}
finally {
    if ($enteredStagedMode) {
        try {
            # Always attempt to delete temp staged files; otherwise repeated runs keep failing.
            Exit-StagedMode -TempFiles $stagedFiles -TempDirs $stagedDirs
        }
        catch {
            $cleanupDetails = @($_.Exception.Message)
            Write-LogEntry (New-LogEntry -Level 'Warning' `
                -Message 'Failed to clean up temporary staged files.' `
                -Details $cleanupDetails `
                -Source 'staged-mode')
            if ($exitCode -eq $SuccessExitCode) {
                $exitCode = $ErrorExitCode
            }
        }
    }

    if ($script:LogFileWriter) {
        # Ensure the log file is flushed/closed; omitting Dispose() can truncate logs on Windows.
        $script:LogFileWriter.Flush()
        $script:LogFileWriter.Dispose()
        $script:LogFileWriter = $null
    }
}

exit $exitCode
