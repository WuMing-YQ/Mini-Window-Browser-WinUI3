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
$result=[ordered]@{initialization=$false;navigation=$false;media=$false;typingSignal=$false;close=$false;fixedZoom='Deferred until settings UI is connected';games='Not run'}
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
 for($i=0;$i -lt 20;$i++){Start-Sleep -Milliseconds 250;if((Evaluate 'document.readyState==="complete" && location.href.includes("browser-fixture.html") && typeof frames==="number"')){break}}
 $result.navigation=(Evaluate 'document.title') -eq '小窗浏览器桌面测试'
 if(-not $result.navigation){throw ('Wrong document: '+(Evaluate 'location.href+" | "+document.title'))}
 $first=Evaluate 'frames'
 Start-Sleep -Seconds 1
 $result.media=((Evaluate 'frames') -gt $first) -and (Evaluate 'document.querySelector("video").paused===false')
 [void](Evaluate 'window.chrome.webview.postMessage("MWB_TYPING:1");true')
 $result.typingSignal=$true # Delivered; effective input suppression is checked after Task 6.
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
 $result|ConvertTo-Json -Depth 5|Set-Content $report -Encoding UTF8
 if($socket){$socket.Dispose()}
 if($process -and -not $process.HasExited){[void]$process.CloseMainWindow();[void]$process.WaitForExit(3000)}
 $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$previousArguments
}
