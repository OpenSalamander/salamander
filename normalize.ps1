#requires -version 7.2

$stopWatch = [Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference = 'Stop'
Set-Location -Path $PSScriptRoot
[Console]::ResetColor()

# set to $false to search for all files regardless of Git status (SLOW)
$gitModifiedOrStagedOnly = $false
$throttleLimit = 12
$clangFormat = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-format.exe'
Write-Host -ForegroundColor White 'Normalize Salamander Source Code'
$clangIncludes = @('*.cpp', '*.h', '*.c')
# from $clangExcludes we moved to .clang-format with DisableFormat: true
$clangExcludes = @()

$utfIncludes = @('*.c', '*.cc', '*.cxx', '*.cpp', '*.c++', '*.cs', '*.hpp', '*.h', '*.h++', '*.hh', '*.tlh',
    '*.tli', '*.inl', '*.pas', '*.dpr', '*.dproj', '*.rh', '*.rc', '*.rc2', '*.htm', '*.sln', '*.csproj',
    '*.vbproj', '*.vcxproj', '*.vcproj', '*.dbproj', '*.fsproj', '*.lsproj', '*.wixproj', '*.modelproj',
    '*.sqlproj', '*.wmaproj', '*.xproj', '*.props', '*.filters', '*.vcxitems', '*.user', '*.config', '*.ps1')
$utfExcludes = @(
    '.\src\common\dep\*',
    '.\src\sfx7zip\7zip\*',
    '.\src\sfx7zip\branch\*',
    '.\src\sfx7zip\lzma\*',
    '.\src\plugins\7zip\7za\c\*',
    '.\src\plugins\7zip\7za\cpp\*',
    '.\src\plugins\automation\generated\*',
    '.\src\plugins\checksum\tomcrypt\*',
    '.\src\plugins\ftp\openssl\*',
    '.\src\plugins\ieviewer\cmark-gfm\*',
    '.\src\plugins\mmviewer\ogg\vorbis\*',
    '.\src\plugins\mmviewer\wma\wmsdk\*',
    '.\src\plugins\pictview\exif\libexif\*',
    '.\src\plugins\pictview\exif\libjpeg\*',
    '.\src\plugins\pictview\twain\*',
    '.\src\plugins\portables\wtl\*',
    '.\src\plugins\tar\lzma\liblzma\*',
    '.\src\plugins\tar\zstd\libzstd\*',
    '.\src\plugins\unchm\chmlib\*',
    '.\src\plugins\winscp\core\*',
    '.\src\plugins\winscp\forms\*',
    '.\src\plugins\winscp\packages\*',
    '.\src\plugins\winscp\putty\*',
    '.\src\plugins\winscp\resource\*',
    '.\src\plugins\winscp\windows\*')

function ArrayContainsItem {
    param ([string[]]$array, [string]$item)
    foreach ($i in $array) { 
        if ($item -ilike $i) { 
            return $true
        }
    }
    return $false
}
function EnumFiles
{
    param ([string[]]$includes, [string[]]$excludes)
    if ( $gitModifiedOrStagedOnly ) {
        # store git output (could be zero or single item) to arrays @()
        $gitModified = @(git diff --name-only --diff-filter=ACMRT)
        $gitStaged = @(git diff --name-only --cached)
        $gitItems = $gitModified + $gitStaged
        $items = $gitItems | %{
            $file = -join('.\\', $_ -replace '/', '\\');
            if (-Not (ArrayContainsItem -array $excludes -item $file)) {
                if (ArrayContainsItem -array $includes -item $file) {
                    $file
                }
            }
        }
        $items
    } else {
        $items = Get-ChildItem -Recurse -Include $includes | Resolve-Path -Relative | %{
            $file = $_
            if (-Not (ArrayContainsItem -array $excludes -item $file)) {
                $file
            }
        }
        $items
    }
}

$errorsQueue = [System.Collections.Concurrent.ConcurrentQueue[string]]::new()

if ($gitModifiedOrStagedOnly) {
    Write-Host -ForegroundColor Gray 'Only Git modified or staged files are processed (FAST PATH)'
} else {
    Write-Host -ForegroundColor Gray 'All files are processed regadless of Git status (SLOW PATH)'
}

Write-Host -ForegroundColor Yellow 'Formating with clang-format...'
$todo = EnumFiles -includes $clangIncludes -excludes $clangExcludes
Write-Host -ForegroundColor Gray 'Files count:' $todo.Count
$todo | ForEach-Object -ThrottleLimit $throttleLimit -Parallel {
    $errors = $using:errorsQueue
    $file = $_

    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.WorkingDirectory = $PWD
    $pinfo.FileName = $using:clangFormat
    $pinfo.RedirectStandardError = $true
    $pinfo.RedirectStandardOutput = $true
    $pinfo.UseShellExecute = $false
    $pinfo.Arguments = "--verbose -i $file"
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $pinfo
    $p.Start() | Out-Null
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()

    if ($p.ExitCode -eq 0) {
        #$mutex = New-Object System.Threading.Mutex($false, 'ConsoleMutex')
        #[void]$mutex.WaitOne()
        #Write-Host -NoNewline -ForegroundColor White "$stderr"
        #[void]$mutex.ReleaseMutex()
    } else {
        $mutex = New-Object System.Threading.Mutex($false, 'ConsoleMutex')
        [void]$mutex.WaitOne()
        Write-Host  -NoNewline -ForegroundColor Red "$stderr"
        [void]$mutex.ReleaseMutex()
        [void]$errors.TryAdd($file+'`r`n'+$stderr)
    }
}

Write-Host -ForegroundColor Yellow 'Checking utf-8-bom files...'
$todo = EnumFiles -includes $utfIncludes -excludes $utfExcludes
Write-Host -ForegroundColor Gray 'Files count:' $todo.Count
$todo | ForEach-Object -ThrottleLimit $throttleLimit -Parallel {
    $errors = $using:errorsQueue
    $file = $_
    $fullname = $(Join-Path -Path $using:PSScriptRoot -ChildPath $file)

    $contents = new-object byte[] 3
    $stream = [System.IO.File]::OpenRead($fullname)
    $stream.Read($contents, 0, 3) | Out-Null
    $streamLength = $stream.Length
    $stream.Close()

    if ($streamLength -lt 3) {
        Write-Host -ForegroundColor Red "Small file: $file"
        [void]$errors.TryAdd($file)
    }
    
    if ($streamLength -ge 3 -and ($contents[0] -ne 0xEF -or $contents[1] -ne 0xBB -or $contents[2] -ne 0xBF)) {
        (Get-Content -path "$fullname" -Encoding utf8) | Set-Content -Encoding utf8BOM -Path "$fullname"
        $mutex = New-Object System.Threading.Mutex($false, 'ConsoleMutex')
        [void]$mutex.WaitOne()
        Write-Host -ForegroundColor White "Converted to utf-8-bom: $file"
        [void]$mutex.ReleaseMutex()
        #[void]$errors.TryAdd($file)
    }
}

if ($errorsQueue.Count -gt 0) {
    Write-Host -ForegroundColor Red 'ERRORS FOUND ^^^^^^^^^^^^^^^^^^^^'
} else {
    Write-Host -ForegroundColor Green '✓ SUCCESS'
}

$stopWatch.Stop()
Write-Host -ForegroundColor White "Execution time: $($stopWatch.Elapsed)"
