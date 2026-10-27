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
# scoreboard show them. -Res sets the size the game renders at (and its
# screenshots), "960x540" unless given.
# -FixedFps 30 steps the game by a thirtieth of a second a frame however slowly
# it renders, so that a busy PC still sees what a person would at that time (a
# ragdoll falling, not in slow motion).
# -Sound plays it with sound (on the PC's speakers), for what only the audio engine
# does: Quartz's shot sounds (tools/play/gunfire_sound.txt).
param(
  [Parameter(Mandatory=$true)][string]$Script,
  [string]$Tag = "play",
  [string]$StartAt = "-231,-700,100,90",
  [string]$Map = "/Game/Variant_Shooter/Lvl_Shooter",
  [string]$Extra = "",
  [string]$Name = "Host",
  [string]$ClientScript = "",
  [switch]$ClientAlone,
  [string]$ClientStartAt = "-231,-300,100,-90",
  [string]$ClientName = "Joiner",
  [int]$ClientDelay = 25,
  [switch]$Window,
  [switch]$Steam,
  [switch]$Lobby,
  [switch]$Bots,
  [switch]$Mice,
  [switch]$Countdown,
  [switch]$Sound,
  [int]$TimeoutSeconds = 180,
  [string]$Res = "960x540",
  [int]$FixedFps = 0
)
# The match starts at once (?StartNow), so that a script finds a character
# to drive; with -Lobby it waits in the lobby instead, as a hosted match
# does, for a script to start it (cmd CheeseStrikeStart). No bots fill in
# (?Bots=0) unless -Bots, so that the players are the only characters; and no
# mice come (?Mice=0) unless -Mice (with -Extra "?MiceAt=5" to bring them early);
# and there is no count before the match (?Countdown=0) unless -Countdown (with
# -Extra "?Countdown=8" for a longer one).
# Without -Steam the game runs on the Null online service, so that a run on
# this PC neither needs the Steam client nor shows up as a match on it; with
# it, the Steam client must be running, and the scripts can host, find and
# join through it (cmd CheeseStrikeHost, CheeseStrikeFind, CheeseStrikeJoin).
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$project = Join-Path $root "CheeseStrike.uproject"
$version = (Get-Content $project | ConvertFrom-Json).EngineAssociation
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_$version" }
$editor = Join-Path $engine "Engine\Binaries\Win64\UnrealEditor.exe"
# A number of this working copy's own (tools/worktree.py makes others): the pair's port, and the build id that keeps the
# matches it offers on the local network from being found by another copy's games running at the same time.
$hash = 0
foreach ($c in $root.ToLower().ToCharArray()) { $hash = ($hash * 31 + [int]$c) % 1000003 }
$port = 7800 + ($hash % 1000)
$buildId = 100000 + ($hash % 900000)
function Launch($map, $script, $tag, $startAt, $x, $more = @()) {
  $scriptPath = (Resolve-Path $script).Path
  $arguments = @("`"$project`"", $map, "-game", "-resx=$($Res.Split('x')[0])", "-resy=$($Res.Split('x')[1])", "-WinX=$x", "-WinY=40", "-log=$tag.log", "-unattended",
    "-TraceState", "-StartAt=$startAt", "-PlayScript=`"$scriptPath`"", "-BuildIdOverride=$buildId") + $more
  if (-not $Sound) { $arguments += "-nosound" }
  if (-not $Window) { $arguments += "-RenderOffscreen" } else { $arguments += "-windowed" }
  if (-not $Steam) { $arguments += "-NOSTEAM" }
  if ($FixedFps -gt 0) { $arguments += @("-UseFixedTimeStep", "-FPS=$FixedFps") }
  return Start-Process -FilePath $editor -ArgumentList $arguments -PassThru
}
function Summarise($tag) {
  $log = Join-Path $root "Saved\Logs\$tag.log"
  Write-Output "log: $log"
  Get-Content $log | Select-String "PlayScript: |Shot|killed|Kill:|Match over|Free-for-all|PlayScript says|LogNet: Join|Welcomed" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[ *\d+\]Log[A-Za-z]+: (Display|Warning|Error): ', '' } | Select-Object -First 80
}
$pair = $ClientScript -ne ""
$serverExtra = if ($pair -and $Extra -eq "") { "?listen" } else { $Extra }
if (-not $Lobby) { $serverExtra = "${serverExtra}?StartNow" }
if (-not $Bots) { $serverExtra = "${serverExtra}?Bots=0" }
if (-not $Mice) { $serverExtra = "${serverExtra}?Mice=0" }
if (-not $Countdown) { $serverExtra = "${serverExtra}?Countdown=0" }
$game = Launch "${Map}${serverExtra}?Nick=${Name}" $Script $Tag $StartAt 0 @("-port=$port")
$client = $null
if ($pair) {
  Start-Sleep -Seconds $ClientDelay
  # The client joins the host by address, or, with -ClientAlone, starts alone on the level and finds the host through its script.
  $clientMap = if ($ClientAlone) { $Map } else { "127.0.0.1:$port" }
  $client = Launch "${clientMap}?Nick=${ClientName}" $ClientScript "${Tag}_client" $ClientStartAt 970
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
