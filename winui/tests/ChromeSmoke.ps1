[CmdletBinding()]
param([Parameter(Mandatory=$true)][IntPtr]$Window,[Parameter(Mandatory=$true)][int]$ApplicationPid,[Parameter(Mandatory=$true)][string]$FixtureUrl,[scriptblock]$CheckProtected)
$ErrorActionPreference='Stop'
$application=Get-Process -Id $ApplicationPid
if(-not $application.Path.StartsWith('D:\Projects\Mini-Window-Browser\artifacts\',[StringComparison]::OrdinalIgnoreCase)){throw 'Only this project test process is allowed.'}
$directory=Split-Path $application.Path
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing,System.Windows.Forms
Add-Type @'
using System;using System.Runtime.InteropServices;
namespace MiniWindowChromeSmoke {
 public static class Native {
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr window);
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int w,int h,uint flags);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 }
}
'@
$oldDpi=[MiniWindowChromeSmoke.Native]::SetThreadDpiAwarenessContext([IntPtr](-4))
function ChromeFind([string]$name,[IntPtr]$owner=[IntPtr]::Zero){
 for($i=0;$i -lt 20;$i++){
  $root=if($owner -ne [IntPtr]::Zero){[Windows.Automation.AutomationElement]::FromHandle($owner)}else{[Windows.Automation.AutomationElement]::RootElement}
  $nameCondition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name)
  $pidCondition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty,$ApplicationPid)
  $condition=[Windows.Automation.AndCondition]::new($nameCondition,$pidCondition)
  $elements=$root.FindAll([Windows.Automation.TreeScope]::Descendants,$condition)
  foreach($element in $elements){if(-not $element.Current.IsOffscreen){return $element}};Start-Sleep -Milliseconds 100
 };throw ('UI not found: '+$name)
}
function ChromeInvoke([string]$name,[IntPtr]$owner=[IntPtr]::Zero){(ChromeFind $name $owner).GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 250}
function ChromeValue([string]$name,[string]$value,[IntPtr]$owner){(ChromeFind $name $owner).GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue($value)}
function ChromeSelect([string]$name,[IntPtr]$owner){(ChromeFind $name $owner).GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select();Start-Sleep -Milliseconds 250}
function ChromeCapture([IntPtr]$owner,[string]$name){
 $bounds=[Windows.Automation.AutomationElement]::FromHandle($owner).Current.BoundingRectangle
 $screen=[System.Windows.Forms.Screen]::FromHandle($owner).Bounds
 $x=[Math]::Max([int]$bounds.X,$screen.X);$y=[Math]::Max([int]$bounds.Y,$screen.Y)
 $width=[Math]::Min([int]$bounds.Right,$screen.Right)-$x;$height=[Math]::Min([int]$bounds.Bottom,$screen.Bottom)-$y
 $bitmap=[Drawing.Bitmap]::new($width,$height);$graphics=[Drawing.Graphics]::FromImage($bitmap)
 try{$graphics.CopyFromScreen($x,$y,0,0,$bitmap.Size);$bitmap.Save((Join-Path $directory $name),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
}
$result=[ordered]@{dpi=[MiniWindowChromeSmoke.Native]::GetDpiForWindow($Window);layouts=@();bookmarkFlyout=$false;managementCrud=$false;draftProtection=$false;managementTyping=$false;dpi100='Manual pending'}
try {
 [void][MiniWindowChromeSmoke.Native]::SetWindowPos($Window,[IntPtr]::Zero,80,80,[int](820*$result.dpi/96),[int](560*$result.dpi/96),0x14);Start-Sleep -Milliseconds 500
 $configFile=Join-Path $directory 'config.winui.ini'
 $existing=if(Test-Path $configFile){[regex]::Match([IO.File]::ReadAllText($configFile),'(?s)\[Bookmarks\](.*?)(?:\[|$)').Value}else{''}
 if(-not $existing.Contains($FixtureUrl)){ChromeInvoke '收藏当前网页' $Window}
 ChromeCapture $Window ('chrome-820-dpi'+$result.dpi+'.png')
 ChromeInvoke '书签' $Window
 $item=ChromeFind '管理书签…'
 $result.bookmarkFlyout=$item.Current.ControlType -eq [Windows.Automation.ControlType]::MenuItem
 if(-not $result.bookmarkFlyout){throw 'Bookmarks did not open a native list.'}
 ChromeCapture $Window ('bookmarks-flyout-dpi'+$result.dpi+'.png')
 ChromeInvoke '管理书签…'
 $management=ChromeFind '小窗浏览器 · 管理'
 $managementWindow=[IntPtr]$management.Current.NativeWindowHandle
 if($managementWindow -eq [IntPtr]::Zero -or $managementWindow -eq $Window){throw 'Management must be an independent native window.'}
 ChromeInvoke '新增书签' $managementWindow
 ChromeValue '书签标题' '视频🚀／你好' $managementWindow
 ChromeValue '书签网址' ($FixtureUrl+'?bookmark=one') $managementWindow
 ChromeInvoke '保存书签' $managementWindow
 if(-not [IO.File]::ReadAllText((Join-Path $directory 'config.winui.ini')).Contains('视频🚀／你好')){throw 'Bookmark was not atomically saved.'}
 ChromeCapture $managementWindow ('management-bookmarks-dpi'+$result.dpi+'.png')
 (ChromeFind '书签标题' $managementWindow).SetFocus();Start-Sleep -Milliseconds 350
 [void][MiniWindowChromeSmoke.Native]::PostMessage($Window,0x312,[IntPtr]102,[IntPtr]::Zero);Start-Sleep -Milliseconds 300
 if($CheckProtected){$result.managementTyping=[bool](& $CheckProtected);if(-not $result.managementTyping){throw 'Management text focus did not protect hotkeys.'}}
 ChromeValue '书签标题' '未保存草稿' $managementWindow
 [void][MiniWindowChromeSmoke.Native]::PostMessage($managementWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 ChromeInvoke '继续编辑' $managementWindow
 if(-not [MiniWindowChromeSmoke.Native]::IsWindowVisible($managementWindow)){throw 'Draft cancel closed the editor.'}
 [void][MiniWindowChromeSmoke.Native]::PostMessage($managementWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 ChromeInvoke '放弃修改' $managementWindow
 if([MiniWindowChromeSmoke.Native]::IsWindowVisible($managementWindow)){throw 'Discard did not close management.'}
 $result.draftProtection=$true
 ChromeInvoke '场景预设' $Window
 ChromeSelect '场景预设' $managementWindow
 ChromeInvoke '保存当前场景…' $managementWindow
 ChromeValue '场景名称' ('办公 🚀 '+$ApplicationPid) $managementWindow
 ChromeInvoke '保存当前场景' $managementWindow
 ChromeValue '场景名称' ('办公重命名 '+$ApplicationPid) $managementWindow
 ChromeInvoke '重命名场景' $managementWindow
 ChromeInvoke '应用场景' $managementWindow
 ChromeInvoke '应用' $managementWindow
 Start-Sleep -Milliseconds 1800
 if(-not [IO.File]::ReadAllText((Join-Path $directory 'config.winui.ini')).Contains('办公重命名')){throw 'Preset save/rename was not retained.'}
 [void][MiniWindowChromeSmoke.Native]::PostMessage($managementWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero);Start-Sleep -Milliseconds 250
 $result.managementCrud=$true
 foreach($width in @(820,560,360,240,160)){
  [void][MiniWindowChromeSmoke.Native]::SetWindowPos($Window,[IntPtr]::Zero,80,80,[int]($width*$result.dpi/96),[int](560*$result.dpi/96),0x14);Start-Sleep -Milliseconds 400
  $root=[Windows.Automation.AutomationElement]::FromHandle($Window);$bounds=$root.Current.BoundingRectangle
  foreach($name in @('网址或搜索','前往','切换沉浸模式','更多','隐藏窗口','最大化或还原')){
   $control=ChromeFind $name $Window;$box=$control.Current.BoundingRectangle
   if($control.Current.IsOffscreen -or $box.Width -lt 4 -or $box.X -lt $bounds.X -or $box.Right -gt $bounds.Right+2){throw ('Lost or clipped control at '+$width+' DIP: '+$name)}
  }
  if($width -le 360){ChromeInvoke '标签菜单' $Window;[void](ChromeFind '新建标签');ChromeInvoke '关闭其他标签'}
  ChromeInvoke '更多' $Window;[void](ChromeFind '书签');ChromeInvoke '设置';Start-Sleep -Milliseconds 250
  if(-not [MiniWindowChromeSmoke.Native]::IsWindowVisible($managementWindow)){throw 'Narrow settings entry failed.'}
  [void][MiniWindowChromeSmoke.Native]::PostMessage($managementWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero);Start-Sleep -Milliseconds 200
  $result.layouts+=@{widthDip=$width;entries=$true}
 }
 $result.passed=$true
} catch {$result.error=$_.Exception.Message;$result.stack=$_.ScriptStackTrace;throw}
finally {
 [void][MiniWindowChromeSmoke.Native]::SetWindowPos($Window,[IntPtr]::Zero,80,80,[int](820*$result.dpi/96),[int](560*$result.dpi/96),0x14)
 [void][MiniWindowChromeSmoke.Native]::SetThreadDpiAwarenessContext($oldDpi)
 $result|ConvertTo-Json -Depth 5|Set-Content (Join-Path $directory 'chrome-smoke.json') -Encoding UTF8
}
$result|ConvertTo-Json -Depth 5
Write-Output 'CHROME_SMOKE_OK'
