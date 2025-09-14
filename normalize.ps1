#requires -version 7.4
<#
.SYNOPSIS
    Normalize sources: code style + text encoding/EOL.
.DESCRIPTION
    - Formats C/C++ files via clang-format.
    - Normalizes text files to UTF-8 with BOM and platform EOLs
      (CRLF on Windows, LF elsewhere).
#>

[CmdletBinding()]
param(
    [switch]$DebugMode,
    [ValidateRange(1, [int]::MaxValue)]
    [int]$ThrottleLimit = [Math]::Max(1, [Math]::Floor([System.Environment]::ProcessorCount / 2)),
    [string]$ClangFormatPath
)

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

    if ($Candidate) { return $Candidate }

    if ($IsWindows) {
        $defaultVsPath = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-format.exe'
        if (Test-Path -LiteralPath $defaultVsPath) { return $defaultVsPath }
    }

    $cmd = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    return $null
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

    $basePath = [System.IO.Path]::GetFullPath($PSScriptRoot)

    # Normalize exclude patterns to forward slashes for consistent matching
    $excludePatterns = foreach ($p in $Excludes) {
        $np = ($p -replace '\\', '/') -replace '/+', '/'
        [Management.Automation.WildcardPattern]::new($np, [Management.Automation.WildcardOptions]::IgnoreCase)
    }

    # Use a HashSet to avoid duplicates when a file matches multiple include patterns
    $resultFiles = [System.Collections.Generic.HashSet[string]]::new()

    foreach ($includePattern in $Includes) {
        foreach ($filePath in [System.IO.Directory]::EnumerateFiles($basePath, $includePattern, [System.IO.SearchOption]::AllDirectories)) {
            # Build relative path using OS-specific separators
            $relative = [System.IO.Path]::GetRelativePath($basePath, $filePath)
            $relativePath = [System.IO.Path]::Combine('.', $relative)
            # Normalized copy using forward slashes for matching excludes consistently
            $relativePathSlash = ($relativePath -replace '\\','/') -replace '/+','/'

            $isExcluded = $false
            if ($excludePatterns.Count -gt 0) {
                foreach ($pattern in $excludePatterns) {
                    if ($pattern.IsMatch($relativePathSlash)) { 
                        $isExcluded = $true;
                        break 
                    }
                }
            }

            if (-not $isExcluded) { [void]$resultFiles.Add($relativePath) }
        }
    }

    return $resultFiles | Sort-Object
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

    $fileList = @($Files)
    $batches = @()
    if ($fileList.Count -gt 0 -and $BatchSize -gt 0) {
        for ($i = 0; $i -lt $fileList.Count; $i += $BatchSize) {
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
        [string[]]$Excludes,
        [string]$ClangFormatPath,
        [int]$ThrottleLimit,
        [System.Collections.Concurrent.ConcurrentQueue[string]]$ErrorsQueue
    )

    Write-Host -ForegroundColor Yellow 'Formatting with clang-format...'
    $files = Get-MatchingFiles -Includes $Includes -Excludes $Excludes
    if ($DebugMode) {
        $files | Set-Content -Path 'normalize_debug_clangformat.txt' -Encoding utf8
    }
    Write-Host -ForegroundColor Gray "Files: $($files.Count)"
    if ($files.Count -eq 0) { return }

    $batches = Get-FileBatches -Files $files -BatchSize 50
    $batches | ForEach-Object -ThrottleLimit $ThrottleLimit -Parallel {
        # Create or open a named mutex once per runspace to serialize console output
        $consoleMutex = [System.Threading.Mutex]::new($false, $using:ConsoleMutexName)
        $errors = $using:ErrorsQueue
        $fileBatch = $_

        try {
            $pinfo = New-Object System.Diagnostics.ProcessStartInfo
            $pinfo.WorkingDirectory = $using:PSScriptRoot
            $pinfo.FileName = $using:ClangFormatPath
            $pinfo.RedirectStandardError = $true
            $pinfo.RedirectStandardOutput = $true
            $pinfo.UseShellExecute = $false

            $quotedFiles = $fileBatch | ForEach-Object { '"' + $_ + '"' }
            $pinfo.Arguments = "--verbose -i " + ($quotedFiles -join ' ')

            $p = New-Object System.Diagnostics.Process
            $p.StartInfo = $pinfo
            [void]$p.Start()
            $stdoutTask = $p.StandardOutput.ReadToEndAsync()
            $stderrTask = $p.StandardError.ReadToEndAsync()

            # Wait for the process to complete.
            $p.WaitForExit()

            # Get the results of the async reads.
            $stdout = $stdoutTask.Result
            $stderr = $stderrTask.Result

            if ($p.ExitCode -ne 0) {
                [void]$consoleMutex.WaitOne()
                try {
                    if ($stdout) { Write-Host -ForegroundColor Gray $stdout }
                    if ($stderr) { Write-Host -ForegroundColor Red $stderr }
                }
                finally { [void]$consoleMutex.ReleaseMutex() }
                $combined = if ($stdout) { $stderr + "`r`n" + $stdout } else { $stderr }
                $errors.Enqueue((($fileBatch -join ', ') + "`r`n" + $combined))
            }
        }
        catch {
            $errors.Enqueue("clang-format batch failed: $($_.Exception.Message)")
        }
        finally {
            if ($consoleMutex) { $consoleMutex.Dispose() }
        }
    }
}

function Invoke-Utf8BomEolNormalization {
<#
.SYNOPSIS
    Normalizes file encoding to UTF-8 with BOM and consistent EOLs.
#>
    param (
        [string[]]$Includes,
        [string[]]$Excludes,
        [int]$ThrottleLimit,
        [System.Collections.Concurrent.ConcurrentQueue[string]]$ErrorsQueue
    )

    Write-Host -ForegroundColor Yellow 'Checking utf-8-bom + EOL style...'
    $files = Get-MatchingFiles -Includes $Includes -Excludes $Excludes
    if ($DebugMode) {
        $files | Set-Content -Path 'normalize_debug_textfiles.txt' -Encoding utf8
    }
    Write-Host -ForegroundColor Gray "Files: $($files.Count)"
    if ($files.Count -eq 0) { return }

    $desiredNewline = if ($IsWindows) { "`r`n" } else { "`n" }
    $newlineLabel = if ($IsWindows) { 'CRLF' } else { 'LF' }

    $batches = Get-FileBatches -Files $files -BatchSize 200
    $batches | ForEach-Object -ThrottleLimit $ThrottleLimit -Parallel {
        # Create or open a named mutex once per runspace to serialize console output
        $consoleMutex = [System.Threading.Mutex]::new($false, $using:ConsoleMutexName)
        $errors = $using:ErrorsQueue
        $fileBatch = $_
        $PSScriptRoot = $using:PSScriptRoot
        $desiredNewline = $using:desiredNewline
        $newlineLabel = $using:newlineLabel

        foreach ($file in $fileBatch) {
            try {
                $fullname = [System.IO.Path]::GetFullPath((Join-Path -Path $PSScriptRoot -ChildPath $file))

                # Read file once and check for BOM
                $bytes = [System.IO.File]::ReadAllBytes($fullname)
                $streamLength = $bytes.Length

                $hasBom = $false
                if ($streamLength -ge 3) {
                    $hasBom = ($bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
                }

                # Convert bytes to text while stripping an initial BOM and throwing on invalid sequences
                $utf8Strict = [System.Text.UTF8Encoding]::new($false, $true)
                if ($hasBom) {
                    $text = $utf8Strict.GetString($bytes, 3, $bytes.Length - 3)
                } else {
                    $text = $utf8Strict.GetString($bytes)
                }

                # EOL normalization
                $eolNormalized = $text -replace "`r`n", "`n" -replace "`r", "`n"
                if ($desiredNewline -eq "`r`n") {
                    $eolNormalized = $eolNormalized -replace "`n", "`r`n"
                }

                # Enforce exactly one newline at EOF
                #$normalized = ($eolNormalized -replace "(`r`n|`r|`n)+$", "") + $desiredNewline
                $normalized = $eolNormalized

                $shouldAddBom = (-not $hasBom)
                $eolChanged = ($text -ne $eolNormalized)
                $eofChanged = ($eolNormalized -ne $normalized)

                if ($shouldAddBom -or $eolChanged -or $eofChanged) {
                    $encoding = New-Object System.Text.UTF8Encoding($true, $true)
                    [System.IO.File]::WriteAllText($fullname, $normalized, $encoding)

                    [void]$consoleMutex.WaitOne()
                    try {
                        $changes = @()
                        if ($shouldAddBom) { $changes += 'BOM' }
                        if ($eolChanged) { $changes += "EOL to $newlineLabel" }
                        if ($eofChanged) { $changes += 'EOF' }

                        if ($changes.Count -gt 0) {
                            $changeString = $changes -join ', '
                            Write-Host -ForegroundColor White "Normalized ($changeString): $file"
                        }
                    }
                    finally { [void]$consoleMutex.ReleaseMutex() }
                }
            }
            catch {
                $errors.Enqueue("utf-8-bom/eol failed for ${file}: $($_.Exception.Message)")
            }
        }
        if ($consoleMutex) { $consoleMutex.Dispose() }
    }
}

# --- Initialization ---

$stopWatch = [Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference = 'Stop'
Set-Location -Path $PSScriptRoot
[Console]::ResetColor()

# Named console mutex to serialize output across parallel runspaces
$ConsoleMutexName = 'NormalizeConsoleMutex'

# --- Configuration ---

$configPath = Join-Path -Path $PSScriptRoot -ChildPath 'normalize_config.json'
$config = Read-NormalizeConfig -Path $configPath

# Resolve clang-format path
$ClangFormatPath = Resolve-ClangFormatPath -Candidate $ClangFormatPath

# --- Main ---

Write-Host -ForegroundColor White 'Normalize Source Code'

$errorsQueue = [System.Collections.Concurrent.ConcurrentQueue[string]]::new()

if (-not ($ClangFormatPath -and (Test-Path -LiteralPath $ClangFormatPath))) {
    throw 'clang-format not found. Please install it or specify the path via -ClangFormatPath.'
}

Invoke-ClangFormat -Includes $config.clangformat.includes -Excludes $config.clangformat.excludes -ClangFormatPath $ClangFormatPath -ThrottleLimit $ThrottleLimit -ErrorsQueue $errorsQueue

Invoke-Utf8BomEolNormalization -Includes $config.textfiles.includes -Excludes $config.textfiles.excludes -ThrottleLimit $ThrottleLimit -ErrorsQueue $errorsQueue

# --- Finalization ---

$stopWatch.Stop()
Write-Host

if ($errorsQueue.Count -gt 0) {
    Write-Host -ForegroundColor Red "Normalization finished with $($errorsQueue.Count) ERRORS."
    Write-Host -ForegroundColor Red ('-' * 60)
    $item = $null
    while ($errorsQueue.TryDequeue([ref]$item)) {
        if ($null -ne $item) { Write-Host -ForegroundColor Red $item }
    }
    Write-Host -ForegroundColor Red ('-' * 60)
    Write-Host -ForegroundColor White "Total execution time: $($stopWatch.Elapsed)"
    exit 1
} else {
    Write-Host -ForegroundColor Green '✓ Normalization finished successfully.'
    Write-Host -ForegroundColor White "Total execution time: $($stopWatch.Elapsed)"
}