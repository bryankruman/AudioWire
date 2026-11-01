param([ValidateSet('x64','ARM64')][string]$Architecture='x64',[ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
& msbuild (Join-Path $root 'driver\audiowire.vcxproj') /m "/p:Configuration=$Configuration" "/p:Platform=$Architecture"
if($LASTEXITCODE){throw 'WDK driver build failed. Check Visual Studio C++, matching Windows SDK/WDK and portcls.lib.'}
