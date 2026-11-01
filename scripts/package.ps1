param([string]$Version='0.1.0',[ValidateSet('x64','arm64')][string]$Architecture='x64',[switch]$WithDriver,[switch]$Release)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$app=Join-Path $root "build\$Architecture\AudioWire_artefacts\Release"
$helper=Join-Path $root "build\$Architecture\Release"
$licenses=Join-Path $root "build\$Architecture\licenses"
$driver=Join-Path $root "build\driver\$Architecture\Release"
if($Release -and -not $WithDriver){throw 'Release packages require the validated, signed AudioWire driver.'}
if($Release){
    & signtool verify /pa (Join-Path $app 'AudioWire.exe')
    if($LASTEXITCODE){throw 'App signature verification failed'}
    & signtool verify /kp /c (Join-Path $driver 'audiowire.cat') (Join-Path $driver 'audiowire.sys')
    if($LASTEXITCODE){throw 'Microsoft kernel signature verification failed'}
}
$args=@("/DAppVersion=$Version","/DArch=$Architecture","/DAppDir=$app","/DHelperDir=$helper","/DDriverDir=$driver","/DLicenseDir=$licenses")
if($WithDriver){$args+='/DWithDriver'}
& iscc @args (Join-Path $root 'installer\AudioWire.iss')
if($LASTEXITCODE){throw 'Installer build failed'}
Get-ChildItem (Join-Path $root 'build\installer') -Filter '*.exe' | Get-FileHash -Algorithm SHA256
