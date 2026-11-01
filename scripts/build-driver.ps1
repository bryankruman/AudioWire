param([ValidateSet('x64','ARM64')][string]$Architecture='x64',[ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$msbuildCommand=Get-Command msbuild -ErrorAction SilentlyContinue
if($msbuildCommand){
    $msbuild=$msbuildCommand.Source
} else {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(-not (Test-Path -LiteralPath $vswhere)){throw 'Visual Studio 2022 C++ and the matching WDK are required.'}
    $msbuild=@(& $vswhere -products '*' -version '[17.0,18.0)' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\Current\Bin\MSBuild.exe') | Select-Object -First 1
    if(-not $msbuild){throw 'Visual Studio 2022 C++ MSBuild was not found. Run from a matching WDK developer shell.'}
}
& $msbuild (Join-Path $root 'driver\audiowire.vcxproj') /m "/p:Configuration=$Configuration" "/p:Platform=$Architecture" /nologo
if($LASTEXITCODE){throw 'WDK driver build failed. Check the matching WindowsKernelModeDriver10.0 toolset, SDK/WDK integration and portcls.lib. See driver/README.md; LLVM compilation is not a replacement.'}
