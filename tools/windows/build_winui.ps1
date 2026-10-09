[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release',[ValidateSet('x64')][string]$Platform='x64',[switch]$TestsOnly,[string]$OutputRoot,[switch]$SmokeTestAsInvoker,[switch]$BuildOnly)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$environment=& (Join-Path $PSScriptRoot 'doctor_winui.ps1')
if(-not $OutputRoot){$OutputRoot=Join-Path $root 'artifacts\WinUI-build'}
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force $OutputRoot | Out-Null
function Build([string]$label,[string]$project,[string]$target,[string]$out,[string[]]$extra=@()) {
 $log=Join-Path $OutputRoot ($label+'.log')
 $arguments=@($project,"/t:$target",'/nologo','/v:minimal',"/p:Configuration=$Configuration","/p:Platform=$Platform","/p:OutDir=$OutputRoot\$out\","/p:IntDir=$OutputRoot\obj-$out\")+$extra
 & $environment.MSBuild @arguments *> $log
 $code=$LASTEXITCODE
 Get-Content $log -Encoding UTF8 -Tail 18
 if($code -ne 0){throw "$label failed ($code). Log: $log"}
}
if(-not $TestsOnly){
 Build 'restore' (Join-Path $root 'winui\MiniWindow.slnx') 'Restore' 'app' @('/p:RestorePackagesConfig=true',"/p:RestoreRepositoryPath=$root\winui\packages")
 Build 'winui' (Join-Path $root 'winui\MiniWindow.vcxproj') 'Rebuild' 'app' @("/p:SmokeTestAsInvoker=$($SmokeTestAsInvoker.IsPresent.ToString().ToLowerInvariant())")
 if(-not(Test-Path (Join-Path $OutputRoot 'app\MiniWindow.exe'))){throw 'WinUI EXE missing after build.'}
 Write-Output 'WINUI_BUILD_OK'
}
if($BuildOnly){Write-Output 'WINUI_TESTS_NOT_RUN_USER_ACCEPTANCE';return}
$tests=Join-Path $root 'winui\tests\WinUICore.Tests.vcxproj'
if(Test-Path $tests){
 Build 'winui-tests' $tests 'Rebuild' 'tests'
 & (Join-Path $OutputRoot 'tests\WinUICore.Tests.exe')
 if($LASTEXITCODE -ne 0){throw 'WinUI core tests failed.'}
 Write-Output 'WINUI_CORE_TESTS_OK'
} elseif($TestsOnly){throw 'WinUI core test project not implemented yet.'}
Build 'legacy-tests' (Join-Path $root 'tests\XiaoChuang.Tests.vcxproj') 'Rebuild' 'legacy-tests'
& (Join-Path $OutputRoot 'legacy-tests\XiaoChuang.Tests.exe')
if($LASTEXITCODE -ne 0){throw 'Legacy core tests failed.'}
Write-Output 'LEGACY_CORE_TESTS_OK'
