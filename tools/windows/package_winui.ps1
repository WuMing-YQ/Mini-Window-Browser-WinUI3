[CmdletBinding()]
param([string]$SourceDirectory=(Join-Path $PSScriptRoot '..\..'),[string]$OutputRoot,[switch]$UserAcceptanceOnly,[string]$ExistingBuildDirectory)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$SourceDirectory=[IO.Path]::GetFullPath($SourceDirectory)
if(-not $OutputRoot){$OutputRoot=Join-Path $SourceDirectory 'artifacts'}
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
$identifier='WinUI-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6)
$build=Join-Path $OutputRoot ($identifier+'-build')
$package=Join-Path $OutputRoot $identifier
$zip=$package+'.zip'
if(Test-Path $package){throw 'Package destination already exists.'}
if($ExistingBuildDirectory -and -not $UserAcceptanceOnly){throw 'Existing build reuse is only allowed for untested user acceptance packages.'}
if($ExistingBuildDirectory){$build=[IO.Path]::GetFullPath($ExistingBuildDirectory)}else{
 & (Join-Path $SourceDirectory 'tools/windows/build_winui.ps1') -OutputRoot $build -Configuration Release -BuildOnly:$UserAcceptanceOnly
 if($LASTEXITCODE){throw 'Build failed; package not created.'}
}
$environment=& (Join-Path $SourceDirectory 'tools/windows/doctor_winui.ps1')
$app=Join-Path $build 'app'
foreach($required in @('MiniWindow.exe','Microsoft.UI.Xaml.dll','Microsoft.WindowsAppRuntime.dll','MiniWindow.pri')){if(-not(Test-Path (Join-Path $app $required))){throw ('Missing application dependency: '+$required)}}
New-Item -ItemType Directory -Path $package | Out-Null
try{
 Copy-Item (Join-Path $app '*') $package -Recurse -Force
 # WebView2 NuGet may statically link the loader; include the x64 loader explicitly for a complete reusable folder.
 $loader=Get-ChildItem (Join-Path $SourceDirectory 'winui/packages/Microsoft.Web.WebView2.1.0.3179.45') -File -Recurse -Filter 'WebView2Loader.dll' | Where-Object {$_.DirectoryName -match '[\\/]x64([\\/]|$)'} | Select-Object -First 1
 if(-not $loader){throw 'x64 WebView2 loader payload missing.'}
 Copy-Item $loader.FullName (Join-Path $package 'WebView2Loader.dll')
 Get-ChildItem $package -Recurse -File | Where-Object {$_.Extension -in @('.pdb','.lib','.exp','.ilk')} | Remove-Item
 # Redistributable native CRT beside the EXE; do not require a new system install.
 $redist=Get-ChildItem (Join-Path $environment.VisualStudio 'VC/Redist/MSVC') -Directory | Where-Object {$_.Name -match '^\d+\.\d+\.\d+$'} | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1
 if(-not $redist){throw 'Versioned native VC redistributable directory missing.'}
 $crt=Get-ChildItem (Join-Path $redist.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
 if(-not $crt){throw 'Native x64 VC redistributable CRT folder missing.'}
 Copy-Item (Join-Path $crt.FullName '*.dll') $package -Force
 foreach($required in @('vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll')){if(-not(Test-Path (Join-Path $package $required))){throw ('Missing native CRT: '+$required)}}
 $licenseDirectory=Join-Path $package 'licenses'
 New-Item -ItemType Directory -Path $licenseDirectory | Out-Null
 Copy-Item (Join-Path $SourceDirectory 'LICENSE.txt') (Join-Path $licenseDirectory 'MiniWindow-GPL-3.0.txt')
 [xml]$packages=Get-Content (Join-Path $SourceDirectory 'winui/packages.config')
 $dependencies=@()
 foreach($dependency in $packages.packages.package){
  $dependencies+=@{name=$dependency.id;version=$dependency.version}
  $directory=Join-Path $SourceDirectory ('winui/packages/'+$dependency.id+'.'+$dependency.version)
  $notices=@(Get-ChildItem $directory -File -Recurse | Where-Object {$_.Name -match '^(LICENSE|NOTICE|ThirdPartyNotices|EULA)([._-]|$)'})
  if($notices.Count){$target=Join-Path $licenseDirectory ($dependency.id+'.'+$dependency.version);New-Item -ItemType Directory -Path $target | Out-Null;foreach($notice in $notices){$relative=$notice.FullName.Substring($directory.Length).TrimStart('\');$dest=Join-Path $target $relative;New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null;Copy-Item $notice.FullName $dest}}
 }
 @'
MiniWindow WinUI edition is derived from azurplain/Mini-Window-Browser (GPL-3.0).
Source and build instructions accompany this private test delivery.
Microsoft Windows App SDK / WinUI and WebView2 packages: see included package licenses.
Microsoft Visual C++ Runtime DLLs are redistributed from the licensed Visual Studio redist directory.
Microsoft Visual Studio license and REDIST list govern those native runtime files:
https://visualstudio.microsoft.com/license-terms/
WebView2 Evergreen Runtime is a separately installed prerequisite; it is not bundled or downloaded.
No Flutter, Dart or .NET application runtime is used.
'@ | Set-Content (Join-Path $licenseDirectory 'THIRD-PARTY-NOTICES.txt') -Encoding UTF8
 Copy-Item (Join-Path $SourceDirectory 'docs/validation/winui-redesign.md') (Join-Path $package '使用说明.md')
 Copy-Item (Join-Path $SourceDirectory 'README.md') (Join-Path $package 'README.md')
 $readmeAssets=Join-Path $package 'assets'
 New-Item -ItemType Directory -Force $readmeAssets | Out-Null
 Copy-Item (Join-Path $SourceDirectory 'assets/MiniWindow.png') (Join-Path $readmeAssets 'MiniWindow.png')
 Copy-Item (Join-Path $SourceDirectory 'LICENSE.txt') (Join-Path $package 'LICENSE.txt')
 $guideDirectory=Join-Path $package 'docs/validation'
 New-Item -ItemType Directory -Force $guideDirectory | Out-Null
 Copy-Item (Join-Path $SourceDirectory 'docs/validation/winui-redesign.md') (Join-Path $guideDirectory 'winui-redesign.md')
 $sourceManifest=Join-Path $SourceDirectory 'source-manifest.json'
 if(-not(Test-Path $sourceManifest)){throw 'Missing staging source-manifest.json. Stage with tools/sync_windows.py.'}
 $source=Get-Content $sourceManifest -Raw -Encoding UTF8 | ConvertFrom-Json
 if($source.sha256 -notmatch '^[a-f0-9]{64}$'){throw 'Invalid source digest.'}
 # Extract the embedded manifest to prove this is the formal elevated binary.
 $mt=Join-Path $environment.WindowsSdk 'bin/10.0.26100.0/x64/mt.exe'
 $embedded=Join-Path $build 'embedded.manifest'
 & $mt "-inputresource:$(Join-Path $package 'MiniWindow.exe');#1" "-out:$embedded" *> (Join-Path $build 'manifest-extract.log')
 if($LASTEXITCODE -ne 0 -or (Get-Content $embedded -Raw) -notmatch 'requireAdministrator'){throw 'Formal EXE does not request administrator privileges.'}
 $files=@(Get-ChildItem $package -File -Recurse | Sort-Object FullName | ForEach-Object {@{path=$_.FullName.Substring($package.Length+1).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant();bytes=$_.Length}})
 $manifest=[ordered]@{version='2.0.0-winui-preview';configuration='Release';platform='x64';elevation='requireAdministrator';selfContained=$true;sourceSha256=$source.sha256;createdUtc=[DateTime]::UtcNow.ToString('o');dependencies=$dependencies;webView2Runtime=$environment.WebView2;nativeCrt=$redist.Name;automatedTests=if($UserAcceptanceOnly){'not run at user request'}else{'passed'};manualAcceptance='pending user';gameCompatibility='not run';files=$files}
 $manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $package 'manifest.json') -Encoding UTF8
 foreach($entry in $files){if((Get-FileHash (Join-Path $package $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256){throw ('Package file changed: '+$entry.path)}}
 Compress-Archive -Path $package -DestinationPath $zip -CompressionLevel Optimal
 Write-Output ('WINUI_PACKAGE_EXE='+ (Join-Path $package 'MiniWindow.exe'))
 Write-Output ('WINUI_PACKAGE_ZIP='+$zip)
 Write-Output ('WINUI_PACKAGE_SHA256='+(Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant())
}catch{Remove-Item $package -Recurse -Force -ErrorAction SilentlyContinue;Remove-Item $zip -Force -ErrorAction SilentlyContinue;throw}
