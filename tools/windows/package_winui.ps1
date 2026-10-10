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
$launcherOutput=Join-Path $build 'launcher'
& $environment.MSBuild (Join-Path $SourceDirectory 'winui/launcher/MiniWindow.Launcher.vcxproj') /t:Build /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 "/p:OutDir=$launcherOutput/" "/p:IntDir=$build/obj-launcher/" *> (Join-Path $build 'launcher-build.log')
if($LASTEXITCODE -ne 0){Get-Content (Join-Path $build 'launcher-build.log') -Tail 18;throw 'Portable launcher build failed.'}
$launcherExe=Join-Path $launcherOutput 'MiniWindow.exe'
if(-not(Test-Path $launcherExe)){throw 'Portable launcher EXE missing.'}
$payload=Join-Path $package 'app'
$documentation=Join-Path $package 'docs'
foreach($required in @('MiniWindow.exe','Microsoft.UI.Xaml.dll','Microsoft.WindowsAppRuntime.dll','MiniWindow.pri')){if(-not(Test-Path (Join-Path $app $required))){throw ('Missing application dependency: '+$required)}}
New-Item -ItemType Directory -Path $package | Out-Null
try{
 New-Item -ItemType Directory -Path $payload,$documentation | Out-Null
 Copy-Item (Join-Path $app '*') $payload -Recurse -Force
 Copy-Item $launcherExe (Join-Path $package 'MiniWindow.exe')
 # WebView2 NuGet may statically link the loader; include the x64 loader explicitly for a complete reusable folder.
 $loader=Get-ChildItem (Join-Path $SourceDirectory 'winui/packages/Microsoft.Web.WebView2.1.0.3179.45') -File -Recurse -Filter 'WebView2Loader.dll' | Where-Object {$_.DirectoryName -match '[\\/]x64([\\/]|$)'} | Select-Object -First 1
 if(-not $loader){throw 'x64 WebView2 loader payload missing.'}
 Copy-Item $loader.FullName (Join-Path $payload 'WebView2Loader.dll')
 Get-ChildItem $package -Recurse -File | Where-Object {$_.Extension -in @('.pdb','.lib','.exp','.ilk','.ps1','.cmd','.bat')} | Remove-Item
 # Redistributable native CRT beside the EXE; do not require a new system install.
 $redist=Get-ChildItem (Join-Path $environment.VisualStudio 'VC/Redist/MSVC') -Directory | Where-Object {$_.Name -match '^\d+\.\d+\.\d+$'} | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1
 if(-not $redist){throw 'Versioned native VC redistributable directory missing.'}
 $crt=Get-ChildItem (Join-Path $redist.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
 if(-not $crt){throw 'Native x64 VC redistributable CRT folder missing.'}
 Copy-Item (Join-Path $crt.FullName '*.dll') $payload -Force
 foreach($required in @('vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll')){if(-not(Test-Path (Join-Path $payload $required))){throw ('Missing native CRT: '+$required)}}
 [xml]$packages=Get-Content (Join-Path $SourceDirectory 'winui/packages.config')
 $dependencies=@()
 $noticeGroups=[ordered]@{}
 foreach($dependency in $packages.packages.package){
  $dependencies+=@{name=$dependency.id;version=$dependency.version}
  $directory=Join-Path $SourceDirectory ('winui/packages/'+$dependency.id+'.'+$dependency.version)
  $notices=@(Get-ChildItem $directory -File -Recurse | Where-Object {$_.Name -match '^(LICENSE|NOTICE|ThirdPartyNotices|EULA)([._-]|$)'})
  foreach($notice in ($notices | Sort-Object FullName)){
   # Keep every distinct notice verbatim, but store identical package notices once.
   $hash=(Get-FileHash $notice.FullName -Algorithm SHA256).Hash
   if(-not $noticeGroups.Contains($hash)){$noticeGroups[$hash]=@{sources=[Collections.Generic.List[string]]::new();text=[IO.File]::ReadAllText($notice.FullName)}}
   $relative=$notice.FullName.Substring($directory.Length).TrimStart('\').Replace('\','/')
   $noticeGroups[$hash].sources.Add(($dependency.id+'.'+$dependency.version+'/'+$relative))
  }
 }
 $thirdPartyHeader=@'
MiniWindow WinUI edition is derived from azurplain/Mini-Window-Browser (GPL-3.0).
Source and build instructions accompany this private test delivery.
Microsoft Windows App SDK / WinUI, WebView2 and build dependencies: notices below.
Identical texts are included once, with all applicable package/source names listed.
Microsoft Visual C++ Runtime DLLs are redistributed from the licensed Visual Studio redist directory.
Microsoft Visual Studio license and REDIST list govern those native runtime files:
https://visualstudio.microsoft.com/license-terms/
WebView2 Evergreen Runtime is a separately installed prerequisite; it is not bundled or downloaded.
No Flutter, Dart or .NET application runtime is used.
'@
 $combined=[Text.StringBuilder]::new()
 [void]$combined.AppendLine($thirdPartyHeader)
 foreach($group in $noticeGroups.Values){
  [void]$combined.AppendLine("`n================================================================")
  [void]$combined.AppendLine('Applies to these package files:')
  foreach($sourceName in $group.sources){[void]$combined.AppendLine(' - '+$sourceName)}
  [void]$combined.AppendLine('----------------------------------------------------------------')
  [void]$combined.AppendLine($group.text)
 }
 [IO.File]::WriteAllText((Join-Path $documentation 'THIRD-PARTY-NOTICES.txt'),$combined.ToString(),[Text.UTF8Encoding]::new($false))
 Copy-Item (Join-Path $SourceDirectory 'docs/validation/winui-redesign.md') (Join-Path $documentation '使用说明.md')
 $readme=Get-Content (Join-Path $SourceDirectory 'README.md') -Raw -Encoding UTF8
 $readme=$readme.Replace('src="assets/MiniWindow.png"','src="docs/assets/MiniWindow.png"').Replace('](LICENSE.txt)','](docs/LICENSE.txt)').Replace('docs/validation/winui-redesign.md','docs/使用说明.md')
 [IO.File]::WriteAllText((Join-Path $package 'README.md'),$readme,[Text.UTF8Encoding]::new($false))
 $readmeAssets=Join-Path $documentation 'assets'
 New-Item -ItemType Directory -Force $readmeAssets | Out-Null
 Copy-Item (Join-Path $SourceDirectory 'assets/MiniWindow.png') (Join-Path $readmeAssets 'MiniWindow.png')
 Copy-Item (Join-Path $SourceDirectory 'LICENSE.txt') (Join-Path $documentation 'LICENSE.txt')
 $sourceManifest=Join-Path $SourceDirectory 'source-manifest.json'
 if(-not(Test-Path $sourceManifest)){throw 'Missing staging source-manifest.json. Stage with tools/sync_windows.py.'}
 $source=Get-Content $sourceManifest -Raw -Encoding UTF8 | ConvertFrom-Json
 if($source.sha256 -notmatch '^[a-f0-9]{64}$'){throw 'Invalid source digest.'}
 # Extract the embedded manifest to prove this is the formal elevated binary.
 $mt=Join-Path $environment.WindowsSdk 'bin/10.0.26100.0/x64/mt.exe'
 $embedded=Join-Path $build 'embedded.manifest'
 & $mt "-inputresource:$(Join-Path $payload 'MiniWindow.exe');#1" "-out:$embedded" *> (Join-Path $build 'manifest-extract.log')
 if($LASTEXITCODE -ne 0 -or (Get-Content $embedded -Raw) -notmatch 'requireAdministrator'){throw 'Formal EXE does not request administrator privileges.'}
 # Check the launcher has the same elevation policy as the real application.
 $launcherManifest=Join-Path $build 'launcher-embedded.manifest'
 & $mt "-inputresource:$(Join-Path $package 'MiniWindow.exe');#1" "-out:$launcherManifest" *> (Join-Path $build 'launcher-manifest-extract.log')
 if($LASTEXITCODE -ne 0 -or (Get-Content $launcherManifest -Raw) -notmatch 'requireAdministrator'){throw 'Launcher elevation does not match the application.'}
 $rootEntries=@(Get-ChildItem $package | ForEach-Object {$_.Name} | Sort-Object)
 if(($rootEntries -join '|') -ne 'app|docs|MiniWindow.exe|README.md'){throw 'Unexpected files at portable package root.'}
 $files=@(Get-ChildItem $package -File -Recurse | Sort-Object FullName | ForEach-Object {@{path=$_.FullName.Substring($package.Length+1).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant();bytes=$_.Length}})
 $manifest=[ordered]@{version='2.0.0-winui-preview';configuration='Release';platform='x64';elevation='requireAdministrator';selfContained=$true;entryPoint='MiniWindow.exe';application='app/MiniWindow.exe';sourceSha256=$source.sha256;createdUtc=[DateTime]::UtcNow.ToString('o');dependencies=$dependencies;webView2Runtime=$environment.WebView2;nativeCrt=$redist.Name;automatedTests=if($UserAcceptanceOnly){'not run at user request'}else{'passed'};manualAcceptance='pending user';gameCompatibility='not run';files=$files}
 $manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $documentation 'manifest.json') -Encoding UTF8
 foreach($entry in $files){if((Get-FileHash (Join-Path $package $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256){throw ('Package file changed: '+$entry.path)}}
 Compress-Archive -Path $package -DestinationPath $zip -CompressionLevel Optimal
 Write-Output ('WINUI_PACKAGE_EXE='+ (Join-Path $package 'MiniWindow.exe'))
 Write-Output ('WINUI_PACKAGE_ZIP='+$zip)
 Write-Output ('WINUI_PACKAGE_SHA256='+(Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant())
}catch{Remove-Item $package -Recurse -Force -ErrorAction SilentlyContinue;Remove-Item $zip -Force -ErrorAction SilentlyContinue;throw}
