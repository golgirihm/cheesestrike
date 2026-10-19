# Plays the game without a person: starts it on this PC with a play script
# (see Source/CheeseStrike/Dev/CheeseStrikePlayScript.h for the commands), which
# drives the character through injected input, so that the person at the PC
# keeps their mouse and keyboard, and waits for it to end. Rendered
# offscreen unless -Window is given, so no window appears either; screenshots
# still come out.
#
#     powershell -ExecutionPolicy Bypass -File tools/play.ps1 -Script tools/play/firing.txt -Tag firing
#
# Screenshots land in Saved/Screenshots/WindowsEditor/, the log in
# Saved/Logs/<tag>.log, and the character traces its state ten times a
# second (-TraceState; see ACheeseStrikeCharacter::CheeseStrikeState). The
# character starts at -StartAt, a spot on the shooter level with room to
# run. -Extra passes more arguments to the game, such as "?listen".
# With -ClientScript, a pair: this game listens for the other, which joins it
# after -ClientDelay seconds, starts at -ClientStartAt and plays that script.
# Both logs are summarised; the client's is Saved/Logs/<tag>_client.log.
# The players are named -Name and -ClientName, as the kill feed and the
# scoreboard show them.
param(
  [Parameter(Mandatory=$true)][string]$Script,
  [string]$Tag = "play",
  [string]$StartAt = "-231,-700,100,90",
  [string]$Map = "/Game/Variant_Shooter/Lvl_Shooter",
  [string]$Extra = "",
  [string]$Name = "Host",
  [string]$ClientScript = "",
  [string]$ClientStartAt = "-231,-1200,100,90",
  [string]$ClientName = "Joiner",
  [int]$ClientDelay = 25,
  [switch]$Window,
  [int]$TimeoutSeconds = 180
)
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$project = Join-Path $root "CheeseStrike.uproject"
$version = (Get-Content $project | ConvertFrom-Json).EngineAssociation
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_$version" }
$editor = Join-Path $engine "Engine\Binaries\Win64\UnrealEditor.exe"
function Launch($map, $script, $tag, $startAt, $x) {
  $scriptPath = (Resolve-Path $script).Path
  $arguments = @("`"$project`"", $map, "-game", "-resx=960", "-resy=540", "-WinX=$x", "-WinY=40", "-log=$tag.log", "-nosound", "-unattended",
    "-TraceState", "-StartAt=$startAt", "-PlayScript=`"$scriptPath`"")
  if (-not $Window) { $arguments += "-RenderOffscreen" } else { $arguments += "-windowed" }
  return Start-Process -FilePath $editor -ArgumentList $arguments -PassThru
}
function Summarise($tag) {
  $log = Join-Path $root "Saved\Logs\$tag.log"
  Write-Output "log: $log"
  Get-Content $log | Select-String "PlayScript: |Shot|killed|Kill:|Match over|Free-for-all|PlayScript says|LogNet: Join|Welcomed" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[ *\d+\]Log[A-Za-z]+: (Display|Warning|Error): ', '' } | Select-Object -First 80
}
$pair = $ClientScript -ne ""
$serverExtra = if ($pair -and $Extra -eq "") { "?listen" } else { $Extra }
$game = Launch "${Map}${serverExtra}?Nick=${Name}" $Script $Tag $StartAt 0
$client = $null
if ($pair) {
  Start-Sleep -Seconds $ClientDelay
  $client = Launch "127.0.0.1?Nick=${ClientName}" $ClientScript "${Tag}_client" $ClientStartAt 970
}
if (-not $game.WaitForExit($TimeoutSeconds * 1000)) {
  Stop-Process -Id $game.Id -Force -ErrorAction SilentlyContinue
  Write-Output "timed out after $TimeoutSeconds s and was stopped"
}
if ($client -ne $null) {
  if (-not $client.WaitForExit(30 * 1000)) { Stop-Process -Id $client.Id -Force -ErrorAction SilentlyContinue }
}
Summarise $Tag
if ($pair) { Write-Output "--- client"; Summarise "${Tag}_client" }
