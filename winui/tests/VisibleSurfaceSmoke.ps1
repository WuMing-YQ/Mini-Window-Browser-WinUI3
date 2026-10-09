param([IntPtr]$Window,[ScriptBlock]$Evaluate,[string]$OutputDirectory)
Add-Type -AssemblyName System.Drawing,UIAutomationClient,UIAutomationTypes,System.Windows.Forms
Add-Type @'
using System;using System.Text;using System.Runtime.InteropServices;using System.Collections.Generic;
namespace SurfaceProbe {
 public static class Native {
 [StructLayout(LayoutKind.Sequential)]public struct RECT{public int left,top,right,bottom;}
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr window);
 [DllImport("user32.dll")]public static extern IntPtr GetWindow(IntPtr window,uint command);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr window,out RECT rect);
 [DllImport("user32.dll")]public static extern int GetClassName(IntPtr window,StringBuilder name,int size);
 [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")]public static extern IntPtr GetWindowLongPtr(IntPtr window,int index);
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int w,int h,uint flags);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static string Class(IntPtr window){var text=new StringBuilder(256);GetClassName(window,text,256);return text.ToString();}
 }
}
'@
$oldDpi=[SurfaceProbe.Native]::SetThreadDpiAwarenessContext([IntPtr](-4))
function Children([IntPtr]$parent,[int]$depth=0){
 $child=[SurfaceProbe.Native]::GetWindow($parent,5)
 while($child -ne [IntPtr]::Zero){
  $rect=[SurfaceProbe.Native+RECT]::new();[void][SurfaceProbe.Native]::GetWindowRect($child,[ref]$rect)
  @{hwnd=$child.ToInt64();parent=$parent.ToInt64();depth=$depth;class=[SurfaceProbe.Native]::Class($child);visible=[SurfaceProbe.Native]::IsWindowVisible($child);style=[SurfaceProbe.Native]::GetWindowLongPtr($child,-16).ToInt64();exStyle=[SurfaceProbe.Native]::GetWindowLongPtr($child,-20).ToInt64();rect=$rect}
  if($depth -lt 4){Children $child ($depth+1)}
  $child=[SurfaceProbe.Native]::GetWindow($child,2)
 }
}
function Pixel([int]$x,[int]$y){$bitmap=[Drawing.Bitmap]::new(1,1);$g=[Drawing.Graphics]::FromImage($bitmap);try{$g.CopyFromScreen($x,$y,0,0,$bitmap.Size);$color=$bitmap.GetPixel(0,0);return @{r=[int]$color.R;g=[int]$color.G;b=[int]$color.B}}finally{$g.Dispose();$bitmap.Dispose()}}
$dpi=[SurfaceProbe.Native]::GetDpiForWindow($Window)
[void][SurfaceProbe.Native]::SetWindowPos($Window,[IntPtr]::Zero,80,80,[int](820*$dpi/96),[int](560*$dpi/96),0x14)
Start-Sleep -Milliseconds 300
[void](& $Evaluate '(()=>{const div=document.createElement("div");div.id="surface-probe";div.style="position:fixed;top:0;left:0;width:240px;height:100px;background:rgb(17,201,79);z-index:2147483647";document.body.appendChild(div);return true})()')
Start-Sleep -Milliseconds 400
$children=@(Children $Window)
$browser=$children|Where-Object {$_.class -eq 'Chrome_WidgetWin_0'}|Select-Object -First 1
$result=@{children=$children;page=(& $Evaluate 'JSON.stringify({url:location.href,ready:document.readyState,viewport:[innerWidth,innerHeight],color:getComputedStyle(document.querySelector("#surface-probe")).backgroundColor})')}
try {
 if(-not $browser){throw 'Native webpage surface was not created.'}
 $result.pixel=Pixel ($browser.rect.left+20) ($browser.rect.top+20)
 if($result.pixel.r -ne 17 -or $result.pixel.g -ne 201 -or $result.pixel.b -ne 79){throw 'Webpage is covered by the native UI: expected the rendered fixture marker.'}
 # Verify actual popup pixels too: UI Automation can invoke a menu even
 # when a native airspace window physically covers it.
 [void](& $Evaluate 'document.querySelector("#surface-probe").style="position:fixed;inset:0;background:rgb(17,201,79);z-index:2147483647";true')
 $root=[Windows.Automation.AutomationElement]::FromHandle($Window)
 $button=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'书签'))
 if(-not $button){throw 'Wide layout bookmark button missing.'}
 $button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 300
 $pidCondition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty,$root.Current.ProcessId)
 $footerCondition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'管理书签…')
 $footer=[Windows.Automation.AutomationElement]::RootElement.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.AndCondition]::new($pidCondition,$footerCondition))
 if(-not $footer){throw 'Bookmark popup missing from the desktop.'}
 $box=$footer.Current.BoundingRectangle
 $result.popupPixel=Pixel ([int]$box.Left+4) ([int]$box.Top+4)
 [Windows.Forms.SendKeys]::SendWait('{ESC}')
 if($result.popupPixel.r -eq 17 -and $result.popupPixel.g -eq 201 -and $result.popupPixel.b -eq 79){throw 'Native browser airspace covers bookmark popup.'}
 $result.passed=$true
}finally{[void](& $Evaluate 'document.querySelector("#surface-probe").remove();true');[void][SurfaceProbe.Native]::SetThreadDpiAwarenessContext($oldDpi)}
$result|ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputDirectory 'visible-surface-smoke.json') -Encoding UTF8

Write-Output 'VISIBLE_SURFACE_SMOKE_OK'
