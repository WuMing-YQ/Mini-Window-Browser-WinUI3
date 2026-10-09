[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$Executable=[IO.Path]::GetFullPath($Executable)
if(-not $Executable.StartsWith('D:\Projects\Mini-Window-Browser\artifacts\',[StringComparison]::OrdinalIgnoreCase)){throw 'Only the isolated project test EXE is allowed.'}
$directory=Split-Path $Executable
$report=Join-Path $directory 'window-smoke.json'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
Add-Type @'
using System;using System.Runtime.InteropServices;
namespace MiniWindowSmoke {
 public static class Native {
  [DllImport("user32.dll")]public static extern IntPtr GetWindowLongPtr(IntPtr window,int index);
  [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
  [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr window);
  [DllImport("user32.dll")]public static extern bool GetLayeredWindowAttributes(IntPtr window,out uint key,out byte alpha,out uint flags);
  [DllImport("gdi32.dll")]public static extern IntPtr CreateRectRgn(int a,int b,int c,int d);
  [DllImport("gdi32.dll")]public static extern bool DeleteObject(IntPtr region);
  [DllImport("user32.dll")]public static extern int GetWindowRgn(IntPtr window,IntPtr region);
  [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr window);
  [DllImport("dwmapi.dll")]public static extern int DwmGetWindowAttribute(IntPtr window,int attribute,out int value,int size);
 }
}
'@
function Snapshot([IntPtr]$window){
 $ex=[MiniWindowSmoke.Native]::GetWindowLongPtr($window,-20).ToInt64()
 $style=[MiniWindowSmoke.Native]::GetWindowLongPtr($window,-16).ToInt64()
 [byte]$alpha=255;[uint32]$key=0;[uint32]$flags=0
 if(($ex -band 0x80000) -ne 0){if(-not [MiniWindowSmoke.Native]::GetLayeredWindowAttributes($window,[ref]$key,[ref]$alpha,[ref]$flags)){throw 'Layered alpha unavailable.'}}
 $region=[MiniWindowSmoke.Native]::CreateRectRgn(0,0,0,0)
 try{$hasRegion=[MiniWindowSmoke.Native]::GetWindowRgn($window,$region) -ne 0}finally{[void][MiniWindowSmoke.Native]::DeleteObject($region)}
 [int]$dark=0;$darkSupported=[MiniWindowSmoke.Native]::DwmGetWindowAttribute($window,20,[ref]$dark,4) -eq 0
 return @{style=$style;exStyle=$ex;alpha=[int]$alpha;hasRegion=$hasRegion;visible=[MiniWindowSmoke.Native]::IsWindowVisible($window);dark=[bool]$dark;darkSupported=$darkSupported;dpi=[MiniWindowSmoke.Native]::GetDpiForWindow($window)}
}
function AssertNormal($state){if(($state.style -band 0xc00000) -ne 0xc00000 -or ($state.exStyle -band (0x80000 -bor 0x20 -bor 0x8000000)) -ne 0 -or $state.alpha -ne 255 -or $state.hasRegion -or -not $state.visible){throw 'Normal frame/opacity/input restoration failed.'}}
function InvokeButton([IntPtr]$window,[string]$name){
 for($i=0;$i -lt 30;$i++){
  $root=[Windows.Automation.AutomationElement]::FromHandle($window)
  $button=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name))
  if($button){$pattern=$button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern);$pattern.Invoke();Start-Sleep -Milliseconds 400;return}
  Start-Sleep -Milliseconds 200
 }
 throw ('Missing real UI button: '+$name)
}
$result=[ordered]@{cases=@();captionVisual='Manual pending';dpi200='Manual pending';games='Not run'}
$process=$null;$window=[IntPtr]::Zero
$config=Join-Path $directory 'config.winui.ini'
try {
 foreach($theme in @('Light','Dark')) {
  # This is a fresh artifact directory owned by the smoke run, never a user profile.
  [IO.File]::WriteAllText($config,"[Meta]`r`nSchemaVersion=1`r`n[Settings]`r`nTheme=$theme`r`nImmersionStyle=1`r`nAutoHideOpacityPercent=0`r`n",[Text.Encoding]::Unicode)
  $process=Start-Process $Executable -WorkingDirectory $directory -PassThru
  for($i=0;$i -lt 50;$i++){Start-Sleep -Milliseconds 200;$process.Refresh();if($process.MainWindowHandle -ne 0){break}}
  $window=$process.MainWindowHandle;if($window -eq [IntPtr]::Zero){throw 'Main HWND missing.'}
  Start-Sleep -Milliseconds 700
  $normal=Snapshot $window;AssertNormal $normal
  if($normal.darkSupported -and $normal.dark -ne ($theme -eq 'Dark')){throw 'Native caption theme disagrees with settings.'}
  [void][MiniWindowSmoke.Native]::PostMessage($window,0x112,[IntPtr]0xf030,[IntPtr]::Zero);Start-Sleep -Milliseconds 500
  $maximized=Snapshot $window;if(($maximized.style -band 0xc00000) -ne 0){throw 'Seamless maximization did not remove the frame.'}
  InvokeButton $window '切换沉浸模式'
  $immersive=Snapshot $window
  if(($immersive.exStyle -band 0x8000020) -ne 0x8000020 -or $immersive.alpha -ne 0){throw 'Immersion style/basis-zero alpha failed.'}
  # Exercise the real tray callback with this exact HWND; no debug product controls.
  [void][MiniWindowSmoke.Native]::PostMessage($window,0x8032,[IntPtr]::Zero,[IntPtr]0x203);Start-Sleep -Milliseconds 500
  $restored=Snapshot $window;AssertNormal $restored
  if($restored.darkSupported -and $restored.dark -ne ($theme -eq 'Dark')){throw 'Caption theme lost after restoration.'}
  InvokeButton $window '隐藏窗口'
  if([MiniWindowSmoke.Native]::IsWindowVisible($window)){throw 'Explicit hide failed.'}
  [void][MiniWindowSmoke.Native]::PostMessage($window,0x8032,[IntPtr]::Zero,[IntPtr]0x203);Start-Sleep -Milliseconds 500
  AssertNormal (Snapshot $window)
  [void]$process.CloseMainWindow();if(-not $process.WaitForExit(10000)){throw 'Window smoke did not close its exact test process.'}
  $result.cases+=@{theme=$theme;normal=$normal;maximized=$maximized;immersive=$immersive;restored=$restored;hideRecovery=$true}
  $window=[IntPtr]::Zero;$process=$null
 }
 $result.passed=$true
}catch{$result.error=$_.Exception.Message;throw}
finally{
 if($process -and -not $process.HasExited){if($window -ne [IntPtr]::Zero){[void][MiniWindowSmoke.Native]::PostMessage($window,0x8032,[IntPtr]::Zero,[IntPtr]0x203);Start-Sleep -Milliseconds 250};[void]$process.CloseMainWindow();[void]$process.WaitForExit(3000)}
 $result|ConvertTo-Json -Depth 7|Set-Content $report -Encoding UTF8
}
$result|ConvertTo-Json -Depth 7
Write-Output 'WINDOW_SMOKE_OK'
