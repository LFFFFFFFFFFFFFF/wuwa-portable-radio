param([switch]$WithTests)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo 'build'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio Build Tools with Desktop development with C++.' }
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x64 build tools were not found.' }
$compilerRoot=Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$kitRoot=(Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10
$kitVersion=(Get-ChildItem -LiteralPath (Join-Path $kitRoot 'Include') -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'cppwinrt\winrt\Windows.Media.Control.h') } | Sort-Object Name -Descending | Select-Object -First 1).Name
if (-not $kitVersion) { throw 'Install a Windows 10/11 SDK with C++/WinRT.' }
$includes=@((Join-Path $compilerRoot 'include'))
foreach ($part in @('ucrt','um','shared','cppwinrt','winrt')) { $includes+=Join-Path $kitRoot ('Include\'+$kitVersion+'\'+$part) }
$env:INCLUDE=$includes -join ';'
$env:LIB=@((Join-Path $compilerRoot 'lib\x64'),(Join-Path $kitRoot ('Lib\'+$kitVersion+'\ucrt\x64')),(Join-Path $kitRoot ('Lib\'+$kitVersion+'\um\x64'))) -join ';'
$cc=Join-Path $compilerRoot 'bin\Hostx64\x64\cl.exe'
$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $csc)) { throw '.NET Framework 4.x compiler was not found.' }
function Run([string]$Exe,[string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw ('Build command failed: '+(Split-Path -Leaf $Exe)+' (exit '+$LASTEXITCODE+')') }
}
New-Item -ItemType Directory -Path $out -Force | Out-Null
Push-Location $out
try {
    $src=Join-Path $repo 'src';$tests=Join-Path $repo 'tests';$mh=Join-Path $repo 'third_party\minhook'
    $common=@('/nologo','/W4','/WX','/O2','/MT','/utf-8',('/I'+$src),('/I'+(Join-Path $mh 'include')))
    $objects=@('buffer.obj','hook.obj','trampoline.obj','hde64.obj')
    Run $cc @('/nologo','/W3','/O2','/MT','/c',(Join-Path $mh 'src\buffer.c'),(Join-Path $mh 'src\hook.c'),(Join-Path $mh 'src\trampoline.c'),(Join-Path $mh 'src\hde\hde64.c'))
    Run $cc ($common+@('/utf-8','/EHsc','/std:c++20','/c',(Join-Path $src 'controls.cpp'),(Join-Path $src 'cloud.cpp'),(Join-Path $src 'status.cpp')))
    $libraries=@('/link','/DYNAMICBASE','/NXCOMPAT','user32.lib','mfplay.lib','mfplat.lib','mfuuid.lib','ole32.lib','windowsapp.lib','shell32.lib','normaliz.lib')
    $bridge=@((Join-Path $src 'bridge.c'),(Join-Path $src 'native_info.c'),(Join-Path $src 'native_ui.c'),(Join-Path $src 'native_bgm.c'),'controls.obj','cloud.obj','status.obj')+$objects
    Run $cc ($common+@('/LD',(Join-Path $src 'plugin.c'))+$bridge+@('/Fe:PortableRadioNative-loadlate.asi')+$libraries)
    $uiRefs=@('/r:System.Windows.Forms.dll','/r:System.Drawing.dll','/r:System.Net.Http.dll','/r:System.Web.Extensions.dll')
    Run $csc (@('/nologo','/target:winexe','/out:NetEaseImporter.exe')+$uiRefs+@((Join-Path $src 'Importer.cs'),(Join-Path $src 'RadioTheme.cs')))
    Run $csc (@('/nologo','/target:winexe','/main:HudProgram','/out:RadioHud.exe')+$uiRefs+@((Join-Path $src 'RadioHud.cs'),(Join-Path $src 'Importer.cs'),(Join-Path $src 'RadioTheme.cs')))
    foreach ($program in @('NetEaseImporter.exe','RadioHud.exe')) {
    $selfTest=[Diagnostics.Process]::new()
    $selfTest.StartInfo=[Diagnostics.ProcessStartInfo]::new((Join-Path $out $program),'--self-test')
    $selfTest.StartInfo.UseShellExecute=$false
    $selfTest.StartInfo.CreateNoWindow=$true
    $selfTest.StartInfo.RedirectStandardOutput=$true
    $selfTest.StartInfo.RedirectStandardError=$true
    $selfTest.Start() | Out-Null
    $testOutput=$selfTest.StandardOutput.ReadToEndAsync()
    $testError=$selfTest.StandardError.ReadToEndAsync()
    if (-not $selfTest.WaitForExit(30000)) { $selfTest.Kill();throw ($program+' self-test timed out.') }
    $message=$testOutput.GetAwaiter().GetResult()
    $errorText=$testError.GetAwaiter().GetResult()
    if ($selfTest.ExitCode -ne 0) { throw ($program+' self-tests failed: '+$errorText) }
    Write-Output $message
    $selfTest.Dispose()
    }
    if ($WithTests) {
        Run $cc ($common+@('/utf-8','/EHsc','/std:c++20',(Join-Path $tests 'track_info_lab.cpp'),'/Fe:track_info_lab.exe','/link','normaliz.lib'))
        Run (Join-Path $out 'track_info_lab.exe') @()
        Run $cc ($common+@('/utf-8','/EHsc','/std:c++20',(Join-Path $tests 'cloud_status_lab.cpp'),'cloud.obj','/Fe:cloud_status_lab.exe')+$libraries)
        Run $csc @('/nologo','/target:library','/optimize+','/out:Aki.Game.dll',(Join-Path $tests 'fixture.cs'))
        Run $csc @('/nologo','/target:exe','/out:FixtureIdentity.exe',(Join-Path $tests 'FixtureIdentity.cs'))
        Run (Join-Path $out 'FixtureIdentity.exe') @((Join-Path $out 'Aki.Game.dll'),(Join-Path $out 'fixture_identity.h'))
        $testFlags=$common+@('/I'+$out)
        Run $cc ($testFlags+@((Join-Path $tests 'lab.c'))+$bridge+@('/Fe:radio_lab.exe')+$libraries)
        Run $cc ($common+@((Join-Path $tests 'loader_lab.c'),'/Fe:radio_loader_lab.exe')+$libraries)
        Run $cc ($testFlags+@('/DRADIO_LAB','/LD',(Join-Path $src 'plugin.c'))+$bridge+@('/Fe:PortableRadioTest.asi')+$libraries)
    }
    Copy-Item -LiteralPath (Join-Path $mh 'LICENSE.txt') -Destination (Join-Path $out 'MinHook-LICENSE.txt') -Force
    Write-Output ('Build and importer/HUD self-tests passed: '+$out)
} finally { Pop-Location }
