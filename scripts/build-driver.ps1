param(
    [ValidateSet('x64','ARM64')][string]$Architecture='x64',
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [string]$KitVersion='',
    [string]$MSBuildPath=''
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $MSBuildPath){
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(-not (Test-Path -LiteralPath $vswhere)){throw 'Visual Studio C++/WDK was not found. An EWDK shell can supply -MSBuildPath and -KitVersion.'}
    $instances=(& $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | Out-String) | ConvertFrom-Json
    foreach($instance in ($instances | Sort-Object { [version]$_.installationVersion } -Descending)){
        $major=([version]$instance.installationVersion).Major
        $toolset=Join-Path $instance.installationPath "MSBuild\Microsoft\VC\v${major}0\Platforms\$Architecture\PlatformToolsets\WindowsKernelModeDriver10.0\Toolset.props"
        # Current WDK package verification supplies x64/ARM64 native DLLs, not x86.
        $candidate=Join-Path $instance.installationPath 'MSBuild\Current\Bin\amd64\MSBuild.exe'
        if((Test-Path -LiteralPath $toolset) -and (Test-Path -LiteralPath $candidate)){
            $MSBuildPath=$candidate
            break
        }
    }
    if(-not $MSBuildPath){throw "No Visual Studio instance has the WindowsKernelModeDriver10.0 toolset for $Architecture. Install matching WDK integration and target compiler tools."}
}
if(-not (Test-Path -LiteralPath $MSBuildPath -PathType Leaf)){throw 'The selected MSBuild executable does not exist.'}
$kits=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
if(-not $KitVersion){
    $versions=Get-ChildItem -LiteralPath (Join-Path $kits 'Include') -Directory | Where-Object {
        $_.Name -match '^\d+\.\d+\.\d+\.\d+$' -and
        (Test-Path -LiteralPath (Join-Path $_.FullName 'km\wdm.h')) -and
        (Test-Path -LiteralPath (Join-Path $kits "Lib\$($_.Name)\km\$Architecture\portcls.lib"))
    } | Sort-Object { [version]$_.Name } -Descending
    $KitVersion=($versions | Select-Object -First 1).Name
}
if(-not $KitVersion -or $KitVersion -notmatch '^\d+\.\d+\.\d+\.\d+$'){throw 'No matching installed SDK/WDK kernel headers and PortCls library were found. Supply the installed -KitVersion if necessary.'}
Write-Output "Building $Architecture with SDK/WDK $KitVersion"
Write-Output "MSBuild: $MSBuildPath"
& $MSBuildPath (Join-Path $root 'driver\audiowire.vcxproj') /m "/p:Configuration=$Configuration" "/p:Platform=$Architecture" "/p:WindowsTargetPlatformVersion=$KitVersion" /nologo
if($LASTEXITCODE){throw 'WDK driver build failed. Inspect the native build output; LLVM compilation does not replace the supported toolchain check.'}
