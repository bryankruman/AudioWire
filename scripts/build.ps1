param([ValidateSet('x64','ARM64')][string]$Architecture='x64',[switch]$CoreOnly)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root "build\$Architecture"
$args=@('-S',$root,'-B',$build,'-G','Visual Studio 17 2022','-A',$Architecture)
if($CoreOnly){$args+='-DAUDIOWIRE_BUILD_APP=OFF'}
& cmake @args
if($LASTEXITCODE){throw 'CMake configure failed'}
& cmake --build $build --config Release --parallel
if($LASTEXITCODE){throw 'Build failed'}
if($Architecture -eq 'ARM64' -and $env:PROCESSOR_ARCHITECTURE -ne 'ARM64'){Write-Output 'ARM64 compiled; runtime tests require an ARM64 Windows host.';exit 0}
& ctest --test-dir $build -C Release --output-on-failure
if($LASTEXITCODE){throw 'Tests failed'}
