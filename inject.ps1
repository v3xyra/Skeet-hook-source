<#
.SYNOPSIS
Load the built source DLL into a running CS:GO instance.
.DESCRIPTION
Build the client with build.ps1 first and start CS:GO through Steam.
Install the Python dependencies once:
  python -m pip install -r tools/requirements-loader.txt
The default DLL is build/native-core-x86/bin/skeet_client.dll.
With multiple CS:GO instances, select one with -ProcessId or -GamePath.
An already loaded DLL is inspected, never loaded a second time.
.EXAMPLE
.\inject.ps1
.EXAMPLE
.\inject.ps1 -Check
.EXAMPLE
.\inject.ps1 -ProcessId 1234 -DllPath C:\Build\skeet_client.dll -Inspect
#>
[CmdletBinding()]
param(
    [ValidateRange(0, 2147483647)]
    [int]$ProcessId = 0,
    [string]$GamePath,
    [string]$DllPath,
    [string]$Python = 'python.exe',
    [switch]$Inspect,
    [switch]$Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

try {
    if ([string]::IsNullOrWhiteSpace($DllPath)) {
        $DllPath = Join-Path $PSScriptRoot 'build\native-core-x86\bin\skeet_client.dll'
    }
    if ($Check -and $Inspect) {
        throw 'Use either -Check (file validation) or -Inspect (read-only process inspection).'
    }
    $sourceDll = (Resolve-Path -LiteralPath $DllPath -ErrorAction Stop).Path
    $loader = Join-Path $PSScriptRoot 'tools\load_source_dll.py'
    $pythonExe = (Get-Command $Python -CommandType Application -ErrorAction Stop |
        Select-Object -First 1).Source
    $loaderArguments = @($loader, '--dll', $sourceDll)
    if ($Check) {
        $loaderArguments += '--check'
    } else {
        if ($ProcessId -gt 0) {
            $targets = @(Get-Process -Id $ProcessId -ErrorAction Stop |
                Where-Object { $_.ProcessName -eq 'csgo' })
        } else {
            $targets = @(Get-Process -Name csgo -ErrorAction SilentlyContinue)
        }
        if ($GamePath) {
            $expectedGame = (Resolve-Path -LiteralPath $GamePath -ErrorAction Stop).Path
            $targets = @($targets | Where-Object { $_.Path -eq $expectedGame })
        }
        if ($targets.Count -ne 1) {
            if (!$targets.Count) { throw 'No matching CS:GO process.' }
            throw 'Select one CS:GO instance with -ProcessId.'
        }
        $loaderArguments += @('--pid', [string]$targets[0].Id, '--reuse')
        if ($Inspect) {
            $loaderArguments += '--inspect'
        }
    }
    & $pythonExe @loaderArguments
    exit $LASTEXITCODE
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
