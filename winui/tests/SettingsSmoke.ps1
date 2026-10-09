[CmdletBinding()]
param([IntPtr]$Window,[int]$ApplicationPid,[ScriptBlock]$Evaluate)
$ErrorActionPreference='Stop'
$application=Get-Process -Id $ApplicationPid
if(-not $application.Path.StartsWith('D:\Projects\Mini-Window-Browser\artifacts\',[StringComparison]::OrdinalIgnoreCase)){throw 'Only this project test process is allowed.'}
$directory=Split-Path $application.Path
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Windows.Forms
function SettingsFind([string]$name){
 for($attempt=0;$attempt -lt 20;$attempt++){
  $condition=[Windows.Automation.AndCondition]::new([Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name),[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty,$ApplicationPid))
  $items=[Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Descendants,$condition)
  foreach($item in $items){if(-not $item.Current.IsOffscreen){return $item}}
  Start-Sleep -Milliseconds 100
 };throw ('Settings UI not found: '+$name)
}
function SettingsInvoke([string]$name){(SettingsFind $name).GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 180}
function SettingsChoose([string]$name,[string]$choice){
 $combo=SettingsFind $name;$combo.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand();Start-Sleep -Milliseconds 120
 $item=SettingsFind $choice;$item.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select();Start-Sleep -Milliseconds 180
}
function SettingsToggle([string]$name,[bool]$enabled){$item=SettingsFind $name;$pattern=$item.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern);if(($pattern.Current.ToggleState -eq [Windows.Automation.ToggleState]::On) -ne $enabled){$pattern.Toggle();Start-Sleep -Milliseconds 180}}
function SettingsText([string]$name,[string]$value){$item=SettingsFind $name;$item.SetFocus();$item.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue($value);(SettingsFind '设置类别').SetFocus();Start-Sleep -Milliseconds 250}
function SettingsValue([string]$name){return (SettingsFind $name).GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).Current.Value}
function SettingsCapture([IntPtr]$owner,[string]$name){
 Add-Type -AssemblyName System.Drawing
 $old=[MiniWindowChromeSmoke.Native]::SetThreadDpiAwarenessContext([IntPtr](-4))
 [void](& $Evaluate '(()=>{const d=document.createElement("div");d.id="settings-visibility-probe";d.style="position:fixed;inset:0;background:rgb(17,201,79);z-index:2147483647";document.body.appendChild(d);return true})()')
 Start-Sleep -Milliseconds 200
 $pointElement=if($name -like 'settings-light*'){SettingsFind '主题'}else{SettingsFind '诊断结果'}
 $point=$pointElement.Current.BoundingRectangle
 $pixel=[Drawing.Bitmap]::new(1,1);$pixelGraphics=[Drawing.Graphics]::FromImage($pixel)
 try{$pixelGraphics.CopyFromScreen([int]($point.X+$point.Width/2),[int]($point.Y+$point.Height/2),0,0,$pixel.Size);$color=$pixel.GetPixel(0,0);if($color.R -eq 17 -and $color.G -eq 201 -and $color.B -eq 79){throw 'Main browser covers the settings window after applying settings.'}}finally{$pixelGraphics.Dispose();$pixel.Dispose();[void](& $Evaluate 'document.querySelector("#settings-visibility-probe").remove();true')}
 $bounds=[Windows.Automation.AutomationElement]::FromHandle($owner).Current.BoundingRectangle
 $screen=[Windows.Forms.Screen]::FromHandle($owner).Bounds
 $x=[Math]::Max([int]$bounds.X,$screen.X);$y=[Math]::Max([int]$bounds.Y,$screen.Y)
 $bitmap=[Drawing.Bitmap]::new([Math]::Min([int]$bounds.Right,$screen.Right)-$x,[Math]::Min([int]$bounds.Bottom,$screen.Bottom)-$y);$g=[Drawing.Graphics]::FromImage($bitmap)
 try{$g.CopyFromScreen($x,$y,0,0,$bitmap.Size);$bitmap.Save((Join-Path $directory $name),[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$bitmap.Dispose();[void][MiniWindowChromeSmoke.Native]::SetThreadDpiAwarenessContext($old)}
}
$result=[ordered]@{categories=$false;fixedZoom=$false;invalidDraft=$false;dependencies=$false;renderDeferred=$false;capture=$false;diagnostics=$false;theme=$false;games='Not run'}
try{
 SettingsInvoke '更多';SettingsInvoke '设置'
 $management=SettingsFind '小窗浏览器 · 管理';$managementWindow=[IntPtr]$management.Current.NativeWindowHandle
 $origin=& $Evaluate 'performance.timeOrigin'
 SettingsChoose '设置类别' '网页与媒体'
 SettingsToggle '固定网页缩放' $true
 SettingsText '网页缩放 (%)' '125'
 $expectedZoom=[MiniWindowChromeSmoke.Native]::GetDpiForWindow($Window)/96.0*1.25
 $actual=& $Evaluate 'devicePixelRatio'
 if([Math]::Abs($actual-$expectedZoom) -gt .01){throw ('Actual WebView2 fixed zoom did not apply at current DPI: '+$actual)}
 $result.fixedZoom=$true
 SettingsText '网页缩放 (%)' '24'
 if((SettingsValue '网页缩放 (%)') -ne '24'){throw 'Invalid numeric draft was overwritten.'}
 if([Math]::Abs((& $Evaluate 'devicePixelRatio')-$expectedZoom) -gt .01){throw 'Invalid zoom changed the document.'}
 SettingsText '网页缩放 (%)' '125';$result.invalidDraft=$true
 SettingsToggle '固定网页缩放' $false
 if((SettingsFind '网页缩放 (%)').Current.IsEnabled){throw 'Disabled zoom dependency remains editable.'}
 SettingsToggle '视频全屏自动按比例调窗' $false
 if((SettingsFind '锁定视频全屏比例').Current.IsEnabled){throw 'Disabled aspect dependency remains editable.'}
 $result.dependencies=$true
 SettingsChoose '设置类别' '快捷键'
 SettingsInvoke '播放/暂停 修改快捷键'
 [Windows.Forms.SendKeys]::SendWait('{ESC}');Start-Sleep -Milliseconds 200
 SettingsInvoke '播放/暂停 修改快捷键'
 [Windows.Forms.SendKeys]::SendWait('{F10}');Start-Sleep -Milliseconds 300
 if(-not [IO.File]::ReadAllText((Join-Path $directory 'config.winui.ini')).Contains('PlayPause=0,121,1')){throw 'Captured F10 was not persisted.'}
 SettingsToggle '播放/暂停 启用' $false;SettingsToggle '播放/暂停 启用' $true
 $result.capture=$true
 SettingsChoose '设置类别' '外观';SettingsChoose '主题' '浅色';SettingsCapture $managementWindow 'settings-light.png';SettingsChoose '主题' '深色';$result.theme=$true
 SettingsChoose '设置类别' '诊断与关于';SettingsChoose '网页渲染模式' '软件兼容'
 if((& $Evaluate 'performance.timeOrigin') -ne $origin){throw 'Settings reloaded the playing webpage.'}
 $result.renderDeferred=$true
 SettingsInvoke '获取诊断'
 for($attempt=0;$attempt -lt 30;$attempt++){$diagnostic=SettingsValue '诊断结果';if($diagnostic.Contains('Windows:') -and $diagnostic.Contains('Media:')){break};Start-Sleep -Milliseconds 100}
 if($diagnostic -notmatch 'WebView2 Runtime: [0-9]+\.' -or -not $diagnostic.Contains('Pending restart: yes')){throw 'Diagnostics did not contain actual Runtime and pending restart state.'}
 $result.diagnostics=$true;SettingsCapture $managementWindow 'settings-diagnostics-dark.png'
 SettingsChoose '网页渲染模式' '自动 GPU'
 foreach($category in @('窗口与行为','沉浸','网页与媒体','快捷键','外观','诊断与关于')){SettingsChoose '设置类别' $category}
 $result.categories=$true
 [void][MiniWindowChromeSmoke.Native]::PostMessage($managementWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 $result.passed=$true
} catch{$result.error=$_.Exception.Message;$result.stack=$_.ScriptStackTrace;throw}
finally{$result|ConvertTo-Json -Depth 5|Set-Content (Join-Path $directory 'settings-smoke.json') -Encoding UTF8}
Write-Output 'SETTINGS_SMOKE_OK'
