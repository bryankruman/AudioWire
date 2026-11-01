# Compile/link check only. This does not install, sign, or certify the driver.
param(
    [Parameter(Mandatory=$true)][string]$WdkRoot,
    [string]$Arm64WdkRoot=$WdkRoot,
    [string]$WindowsSdkRoot='C:\Program Files (x86)\Windows Kits\10',
    [string]$LlvmRoot='C:\Program Files\LLVM',
    [ValidateSet('x64','ARM64')][string]$Architecture='x64',
    [string]$KitVersion='10.0.26100.0'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=Join-Path $root "build\driver\clang\$Architecture"
New-Item -ItemType Directory -Force $output | Out-Null
# Match WDF's fixed enum forward declaration for Clang in a private copy.
$wdfHeaders=Join-Path $output 'include\wdf'
New-Item -ItemType Directory -Force $wdfHeaders | Out-Null
Copy-Item -Path (Join-Path $WdkRoot 'Include\wdf\kmdf\1.15\*') -Destination $wdfHeaders -Force
$request=Join-Path $wdfHeaders 'wdfrequest.h'
$definition=@('typedef enum _WDF_REQUEST_TYPE','#ifdef __cplusplus',': int','#endif','{') -join [Environment]::NewLine
$source=[IO.File]::ReadAllText($request).Replace('typedef enum _WDF_REQUEST_TYPE {',$definition)
[IO.File]::WriteAllText($request,$source)
$compile=@('/nologo','/c','/std:c++17','/kernel','/GR-','/EHs-c-','/GS','/O2',
    '/clang:-Wno-ignored-pragma-intrinsic','/clang:-Wno-pragma-pack','/clang:-Wno-invalid-token-paste',
    '/D_WIN64','/D_WIN32','/DUNICODE','/D_UNICODE','/DPC_IMPLEMENTATION','/D_USE_WAVERT_',
    '/D_USE_IPortClsRuntimePower','/D_NEW_DELETE_OPERATORS_','/DDEBUG_LEVEL=0',
    "/I$WdkRoot\Include\$KitVersion\km","/I$WdkRoot\Include\$KitVersion\km\crt",
    "/I$wdfHeaders","/I$WindowsSdkRoot\Include\$KitVersion\shared",
    "/I$root\driver","/I$root\driver\vendor\Inc","/I$root\driver\vendor\Main",
    "/I$root\driver\vendor\Filters","/I$root\driver\vendor\Utilities")
$machine=$Architecture.ToLower()
$libs=$WdkRoot
if($Architecture -eq 'ARM64'){
    $compile+=@('--target=aarch64-pc-windows-msvc','/D_ARM64_','/DARM64')
    $libs=$Arm64WdkRoot
}else{$compile+=@('/D_AMD64_','/DAMD64')}
$sources=@(Get-Item "$root\driver\AwFeed.cpp")+@(Get-ChildItem "$root\driver\vendor\Main\*.cpp")+
    @(Get-ChildItem "$root\driver\vendor\Filters\*.cpp")+@(Get-Item "$root\driver\vendor\Utilities\hw.cpp")+
    @(Get-Item "$root\driver\vendor\Utilities\kshelper.cpp")
$objects=@()
foreach($sourceFile in $sources){
    $object=Join-Path $output "$($sourceFile.BaseName).obj"
    & "$LlvmRoot\bin\clang-cl.exe" @compile "/Fo$object" $sourceFile.FullName *> (Join-Path $output "$($sourceFile.BaseName).log")
    if($LASTEXITCODE){Get-Content (Join-Path $output "$($sourceFile.BaseName).log");throw "Compile failed: $($sourceFile.Name)"}
    $objects+=$object
}
$link=@('/nologo','/driver','/subsystem:native,10.00',"/machine:$machine",'/entry:FxDriverEntry','/nodefaultlib',
    '/release','/dynamicbase','/nxcompat','/integritycheck','/opt:ref','/opt:icf',"/out:$output\audiowire.sys",
    "/libpath:$libs\Lib\$KitVersion\km\$machine","/libpath:$libs\Lib\wdf\kmdf\$machine\1.15")+
    $objects+@('ntoskrnl.lib','hal.lib','portcls.lib','stdunk.lib','libcntpr.lib','wdmsec.lib','drmk.lib',
    'BufferOverflowFastFailK.lib','WdfLdr.lib','WdfDriverEntry.lib')
& "$LlvmRoot\bin\lld-link.exe" @link *> (Join-Path $output 'link.log')
if($LASTEXITCODE){Get-Content (Join-Path $output 'link.log');throw 'Driver link failed'}
$inf=[IO.File]::ReadAllText("$root\driver\audiowire.inx").Replace('$ARCH$',$(if($Architecture -eq 'x64'){'amd64'}else{'arm64'})).Replace('$KMDFVERSION$','1.15')
[IO.File]::WriteAllText((Join-Path $output 'audiowire.inf'),$inf)
Write-Output "Unsigned $Architecture compile/link candidate: $output"
Write-Output 'This is not a supported WDK/MSVC build, VM validation, or a deployable signed release.'
