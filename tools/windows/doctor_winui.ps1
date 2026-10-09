[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not(Test-Path $vswhere)){throw 'Missing Visual Studio Installer (vswhere).'}
$vs=(& $vswhere -latest -products '*' -version '[18.0,19.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.WindowsAppSdkSupport.Cpp -property installationPath).Trim()
if(-not $vs){throw 'Install VS 2026: Desktop development with C++ and C++ WinUI app development tools.'}
$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
if(-not(Test-Path $msbuild)){throw 'MSBuild missing.'}
$sdk=(Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10
foreach($relative in @('Include\10.0.26100.0\um\Windows.h','Lib\10.0.26100.0\um\x64\user32.lib','bin\10.0.26100.0\x64\rc.exe')) {
 if(-not(Test-Path (Join-Path $sdk $relative))){throw "Missing Windows SDK 26100 component: $relative"}
}
$runtime=$null
foreach($key in @('HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\*','HKCU:\SOFTWARE\Microsoft\EdgeUpdate\Clients\*')) {
 $runtime=Get-ItemProperty $key -ErrorAction SilentlyContinue | Where-Object {$_.name -match 'WebView2' -and $_.pv -ne '0.0.0.0'} | Select-Object -First 1
 if($runtime){break}
}
if(-not $runtime){throw 'WebView2 Runtime missing. Install it explicitly before running the application.'}
[pscustomobject]@{VisualStudio=$vs;MSBuild=$msbuild;WindowsSdk=$sdk;WebView2=$runtime.pv}
