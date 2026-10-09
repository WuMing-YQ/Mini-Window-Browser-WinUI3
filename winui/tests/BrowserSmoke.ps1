[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$Fixture,[int]$Port=15395)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
$projectRoot='D:\Projects\Mini-Window-Browser\'
$Executable=[IO.Path]::GetFullPath($Executable)
$Fixture=[IO.Path]::GetFullPath($Fixture)
if(-not $Executable.StartsWith($projectRoot,[StringComparison]::OrdinalIgnoreCase) -or -not $Fixture.StartsWith($projectRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Only this project test files are allowed.'}
if(-not(Test-Path $Executable) -or -not(Test-Path $Fixture)){throw 'Missing test EXE/fixture.'}
$directory=Split-Path $Executable
$report=Join-Path $directory 'browser-smoke.json'
$previousArguments=$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$socket=$null
$process=$null
$keyHeld=$false
$result=[ordered]@{initialization=$false;navigation=$false;media=$false;typingSignal=$false;typingGuard=$false;ownedTypingGuard=$false;holdCancel=$false;close=$false;fixedZoom='Deferred until settings UI is connected';games='Not run'}
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
Add-Type @'
using System;using System.Runtime.InteropServices;
namespace MiniWindowInputSmoke {
 public static class Native {
  [StructLayout(LayoutKind.Sequential)]public struct INPUT {public uint type;public UNION data;}
  [StructLayout(LayoutKind.Explicit)]public struct UNION {[FieldOffset(0)]public KEY key;[FieldOffset(0)]public MOUSE mouse;}
  [StructLayout(LayoutKind.Sequential)]public struct KEY {public ushort vk,scan;public uint flags,time;public UIntPtr extra;}
  [StructLayout(LayoutKind.Sequential)]public struct MOUSE {public int x,y;public uint data,flags,time;public UIntPtr extra;}
  [DllImport("user32.dll")]public static extern uint SendInput(uint count,INPUT[] input,int size);
  [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
  [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr window);
  [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
  public static bool Key(bool up){var input=new INPUT();input.type=1;input.data.key.vk=0x36;input.data.key.flags=up?2u:0u;return SendInput(1,new[]{input},Marshal.SizeOf(typeof(INPUT)))==1;}
 }
}
'@
function Element([IntPtr]$window,[string]$name){
 $root=[Windows.Automation.AutomationElement]::FromHandle($window)
 $found=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name))
 if(-not $found){throw ('Missing UI: '+$name)};return $found
}
function PlayHotkey { [void][MiniWindowInputSmoke.Native]::PostMessage($window,0x312,[IntPtr]102,[IntPtr]::Zero);Start-Sleep -Milliseconds 300 }
try {
 # Run this script in the signed-in desktop session; an SSH service cannot enumerate its HWNDs.
 Get-Process | Where-Object {try {$_.Path -eq $Executable}catch{$false}} | ForEach-Object {[void]$_.CloseMainWindow();[void]$_.WaitForExit(3000)}
 $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS="--remote-debugging-port=$Port"
 $started=[DateTime]::Now
 $process=Start-Process -FilePath $Executable -WorkingDirectory $directory -PassThru
 $pages=$null
 for($i=0;$i -lt 30;$i++){
  Start-Sleep -Milliseconds 500
  try {$pages=Invoke-RestMethod "http://127.0.0.1:$Port/json/list" -TimeoutSec 1}catch{}
  if($pages){break}
 }
 if(-not $pages){throw 'No WebView2 debugging endpoint; desktop launch may have been blocked.'}
 $page=$pages | Where-Object {$_.type -eq 'page'} | Select-Object -First 1
 if(-not $page){throw 'No browser page.'}
 $socket=[Net.WebSockets.ClientWebSocket]::new()
 $cancel=[Threading.CancellationTokenSource]::new(10000)
 [void]$socket.ConnectAsync([uri]$page.webSocketDebuggerUrl,$cancel.Token).GetAwaiter().GetResult()
 $script:requestId=0
 function Request([string]$method,[hashtable]$parameters){
  $script:requestId++
  $id=$script:requestId
  $json=@{id=$id;method=$method;params=$parameters}|ConvertTo-Json -Compress -Depth 12
  $bytes=[Text.Encoding]::UTF8.GetBytes($json)
  $cancel=[Threading.CancellationTokenSource]::new(10000)
  try {
   [void]$socket.SendAsync([ArraySegment[byte]]::new($bytes),[Net.WebSockets.WebSocketMessageType]::Text,$true,$cancel.Token).GetAwaiter().GetResult()
   do {
    $text=[Text.StringBuilder]::new()
    do {
     $buffer=New-Object byte[] 65536
     $received=$socket.ReceiveAsync([ArraySegment[byte]]::new($buffer),$cancel.Token).GetAwaiter().GetResult()
     [void]$text.Append([Text.Encoding]::UTF8.GetString($buffer,0,$received.Count))
    }while(-not $received.EndOfMessage)
    $reply=$text.ToString()|ConvertFrom-Json
   }while($reply.id -ne $id)
   if($reply.error){throw ($method+': '+($parameters|ConvertTo-Json -Compress)+' '+($reply.error|ConvertTo-Json -Compress))}
   return $reply.result
  }finally{$cancel.Dispose()}
 }
 function Evaluate([string]$expression){(Request 'Runtime.evaluate' @{expression=$expression;returnByValue=$true;awaitPromise=$true}).result.value}
 $result.initialization=$true
 for($i=0;$i -lt 40;$i++){if((Evaluate 'location.href!=="about:blank"')){break};Start-Sleep -Milliseconds 250}
 $fixtureUrl=([uri]$Fixture).AbsoluteUri
 $navigation=Request 'Page.navigate' @{url=$fixtureUrl}
 $result.requestedUrl=$fixtureUrl
 $result.navigationResponse=$navigation
 if($navigation.errorText){throw $navigation.errorText}
 for($i=0;$i -lt 20;$i++){Start-Sleep -Milliseconds 250;if((Evaluate 'document.readyState==="complete" && location.href.includes("browser-fixture.html") && typeof frames==="number" && fixtureReady')){break}}
 $result.navigation=(Evaluate 'document.title') -eq '小窗浏览器桌面测试'
 if(-not $result.navigation){throw ('Wrong document: '+(Evaluate 'location.href+" | "+document.title'))}
 $first=Evaluate 'frames'
 Start-Sleep -Seconds 1
 $result.media=((Evaluate 'frames') -gt $first) -and (Evaluate 'document.querySelector("video").paused===false')
 $process.Refresh();$window=$process.MainWindowHandle
 if($window -eq [IntPtr]::Zero){throw 'Main HWND missing.'}
 [void][MiniWindowInputSmoke.Native]::SetForegroundWindow($window)
 $address=Element $window '网址或搜索'
 $address.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue($fixtureUrl)
 (Element $window '前往').GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
 Start-Sleep -Milliseconds 2200
 if(-not(Evaluate 'document.hasFocus()')){throw 'Browser did not receive real focus.'}
 PlayHotkey
 if(-not(Evaluate 'document.querySelector("video").paused')){throw 'Unprotected media action did not fire.'}
 PlayHotkey
 if(Evaluate 'document.querySelector("video").paused'){throw 'Media action did not resume.'}
 [void](Evaluate 'document.querySelector("input").focus();true');Start-Sleep -Milliseconds 400
 $result.typingSignal=Evaluate 'document.activeElement.tagName==="INPUT" && document.hasFocus()'
 PlayHotkey
 $result.typingGuard=-not(Evaluate 'document.querySelector("video").paused')
 if(-not $result.typingGuard){throw 'Web text focus did not protect media hotkeys.'}
 [void](Evaluate 'document.activeElement.blur();window.chrome.webview.postMessage("MWB_TYPING:0");true')
 $address.SetFocus();Start-Sleep -Milliseconds 400
 PlayHotkey
 $result.ownedTypingGuard=-not(Evaluate 'document.querySelector("video").paused')
 if(-not $result.ownedTypingGuard){throw 'XAML address focus did not protect media hotkeys.'}
 (Element $window '前往').GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 2200
 if([MiniWindowInputSmoke.Native]::GetForegroundWindow() -ne $window){throw 'Test HWND lost foreground; no input sent.'}
 PlayHotkey
 if(-not(Evaluate 'document.querySelector("video").paused')){throw 'Guard remained active after returning to browser.'}
 PlayHotkey
 $keyHeld=[MiniWindowInputSmoke.Native]::Key($false)
 if(-not $keyHeld){throw 'SendInput key down failed.'}
 Start-Sleep -Milliseconds 650
 $result.holdRate=Evaluate 'document.querySelector("video").playbackRate'
 if($result.holdRate -le 1){throw 'Real 400ms media hold did not start.'}
 $address.SetFocus();Start-Sleep -Milliseconds 400
 $result.holdCancel=(Evaluate 'document.querySelector("video").playbackRate') -eq 1
 [void][MiniWindowInputSmoke.Native]::Key($true);$keyHeld=$false
 if(-not $result.holdCancel){throw 'Changing to XAML text focus left hold speed active.'}
 $socket.Dispose();$socket=$null
 $process.Refresh()
 if($process.MainWindowHandle -eq 0){throw 'Test application has no HWND.'}
 [void]$process.CloseMainWindow()
 $result.close=$process.WaitForExit(10000)
 $config=Join-Path $directory 'config.winui.ini'
 $result.configSaved=(Test-Path $config) -and ([IO.File]::ReadAllText($config).Contains('SchemaVersion=1')) -and ([IO.File]::ReadAllText($config).Contains($fixtureUrl))
 if(-not $result.configSaved){throw 'Window closed without saving its versioned session.'}
 if(-not($result.navigation -and $result.media -and $result.close)){throw 'Browser smoke assertions failed.'}
 $result|ConvertTo-Json -Depth 5|Set-Content $report -Encoding UTF8
 $result|ConvertTo-Json -Depth 5
 Write-Output 'BROWSER_SMOKE_OK'
}catch{
 $result.error=$_.Exception.Message
 throw
}finally{
 if($keyHeld){[void][MiniWindowInputSmoke.Native]::Key($true)}
 $result|ConvertTo-Json -Depth 5|Set-Content $report -Encoding UTF8
 if($socket){$socket.Dispose()}
 if($process -and -not $process.HasExited){[void]$process.CloseMainWindow();[void]$process.WaitForExit(3000)}
 $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$previousArguments
}
