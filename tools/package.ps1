# Packages the game for playing on other PCs, checks the packaged build, and
# puts it on Google Drive as a zip:
#
#     powershell -ExecutionPolicy Bypass -File tools/package.ps1
#
# 1. Packages a Development build for Windows (Unreal's BuildCookRun) into
#    -Out\Windows. Only the maps in Config/DefaultGame.ini's MapsToCook are
#    cooked, and what they and the code reach. Development, not Shipping:
#    Shipping would have Steam relaunch it as the test app id's game, and
#    the console stays for the matchmaking commands.
# 2. Plays the packaged build twice, offscreen and without Steam
#    (tools/play/packaged_menu.txt and packaged_match.txt): the front end,
#    the credits and the lobby, then a match on The Creamery with bots and
#    the mice. It fails if a run doesn't finish its script or its log shows
#    something missing. Its screenshots are kept in -Out\checks to look at.
# 3. Zips the build as CheeseStrike_<date>_<commit>.zip in -Out, with a
#    README for whoever plays it (the debug symbols left out, and kept in
#    -Out\symbols\<name> for reading a crash), and copies the zip into -Drive, a folder
#    that Google Drive for desktop syncs, which uploads it. The newest -Keep
#    zips are kept there and in -Out, and older ones deleted.
#
# -NoCheck skips step 2; -NoUpload stops before the copy to Drive; -SkipPackage
# zips and uploads the build already in -Out\Windows. The editor and the game
# must be closed: a running one holds the files the build needs.
param(
  [string]$Out = "",
  [string]$Drive = "",
  [int]$Keep = 3,
  [switch]$NoCheck,
  [switch]$NoUpload,
  [switch]$SkipPackage
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$project = Join-Path $root "CheeseStrike.uproject"
$version = (Get-Content $project | ConvertFrom-Json).EngineAssociation
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_$version" }
if ($Out -eq "") { $Out = if ($env:CHEESESTRIKE_BUILDS) { $env:CHEESESTRIKE_BUILDS } else { Join-Path (Split-Path -Parent $root) "CheeseStrikeBuilds" } }
if ($Drive -eq "") { $Drive = if ($env:CHEESESTRIKE_DRIVE) { $env:CHEESESTRIKE_DRIVE } else { "G:\My Drive\CheeseStrike Builds" } }
$build = Join-Path $Out "Windows"
$exe = Join-Path $build "CheeseStrike.exe"

function Fail($message) {
  Write-Output "FAILED: $message"
  exit 1
}

# What is being packaged: the commit, marked if the working tree (code or assets) differs from it.
Push-Location $root
$commit = (git rev-parse --short HEAD).Trim()
$dirty = (git -c core.usebuiltinfsmonitor=false status --porcelain) -ne $null
Pop-Location
$name = "CheeseStrike_$(Get-Date -Format 'yyyy-MM-dd_HHmm')_$commit$(if ($dirty) { '-modified' })"
Write-Output "== $name"

if (-not $SkipPackage) {
  $running = Get-Process UnrealEditor, CheeseStrike -ErrorAction SilentlyContinue
  if ($running) { Fail "close the editor and the game first (process $($running.Id -join ', '))" }
  if (Test-Path $build) { Remove-Item $build -Recurse -Force }
  Write-Output "== packaging into $build (the first time compiles every shader: a long while)"
  $uat = Join-Path $engine "Engine\Build\BatchFiles\RunUAT.bat"
  $log = Join-Path $Out "package.log"
  New-Item -ItemType Directory -Force $Out | Out-Null
  & $uat BuildCookRun "-project=$project" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -prereqs -archive "-archivedirectory=$Out" -unattended -utf8output *> $log
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
    Get-Content $log | Select-String "error|BUILD FAILED" | Select-Object -Last 15 | ForEach-Object { $_.Line }
    Fail "packaging failed; the whole log is $log"
  }
}
if (-not (Test-Path $exe)) { Fail "no build in $build" }

# Plays the packaged build with a play script, offscreen, without Steam or sound; returns the problems found.
function Check($url, $script, $tag) {
  $logs = Join-Path $build "CheeseStrike\Saved\Logs"
  $arguments = @($url, "-PlayScript=`"$(Join-Path $root $script)`"", "-RenderOffscreen", "-NOSTEAM", "-nosound", "-unattended", "-resx=1280", "-resy=720", "-log=$tag.log")
  $game = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru
  $problems = @()
  if (-not $game.WaitForExit(180 * 1000)) {
    Stop-Process -Id $game.Id -Force -ErrorAction SilentlyContinue
    $problems += "$tag did not finish in three minutes"
  }
  $log = Join-Path $logs "$tag.log"
  if (-not (Test-Path $log)) { return $problems + "$tag left no log" }
  $text = Get-Content $log
  if (-not ($text | Select-String "PlayScript: quit")) { $problems += "$tag did not reach the end of its script" }
  $text | Select-String "Fatal error|Assertion failed|Failed to find object|Failed to load '/|Couldn't find file|Could not find file|LogStreaming: Error|Unable to load" | Select-Object -First 10 | ForEach-Object { $problems += "${tag}: $($_.Line)" }
  return $problems
}

if (-not $NoCheck) {
  Write-Output "== checking the packaged build"
  $saved = Join-Path $build "CheeseStrike\Saved"
  if (Test-Path $saved) { Remove-Item $saved -Recurse -Force }
  $problems = @()
  $problems += Check "/Game/Variant_Shooter/Lvl_Shooter?Bots=0?Mice=0" "tools\play\packaged_menu.txt" "packaged_menu"
  $problems += Check "/Game/CheeseStrike/Maps/Lvl_Creamery?StartNow?Bots=5?MiceAt=2" "tools\play\packaged_match.txt" "packaged_match"
  $match = Get-Content (Join-Path $saved "Logs\packaged_match.log") -ErrorAction SilentlyContinue
  $mice = ($match | Select-String "Display: Mouse: ").Count
  if ($mice -eq 0) { $problems += "no mice came into the match" }
  $match | Select-String "Display: Mouse: .*missing" | Select-Object -First 3 | ForEach-Object { $problems += "a mouse without its body or sounds: $($_.Line)" }
  $checks = Join-Path $Out "checks"
  if (Test-Path $checks) { Remove-Item $checks -Recurse -Force }
  New-Item -ItemType Directory -Force $checks | Out-Null
  Get-ChildItem (Join-Path $saved "Screenshots") -Recurse -Filter *.png -ErrorAction SilentlyContinue | Copy-Item -Destination $checks
  Copy-Item (Join-Path $saved "Logs\packaged_*.log") $checks -ErrorAction SilentlyContinue
  Remove-Item $saved -Recurse -Force
  if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output "  $_" }
    Fail "the packaged build didn't pass its checks; screenshots and logs are in $checks"
  }
  Write-Output "   passed: $mice mice logged; screenshots and logs in $checks"
}

# The README for whoever plays it, then the zip.
$prereqs = Get-ChildItem $build -Recurse -Filter "UEPrereqSetup_x64.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$prereqsPath = if ($prereqs) { $prereqs.FullName.Substring($build.Length + 1) } else { "Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe" }
@"
CheeseStrike: Gouda Overload - test build $name

To play
  1. Unzip the whole folder anywhere, and run CheeseStrike.exe.
  2. If Windows says "Windows protected your PC", press More info, then Run anyway (the game isn't signed).
  3. If it says a DLL such as VCRUNTIME140.dll is missing, run $prereqsPath once.

Playing together
  Have Steam running and signed in on every PC. One player presses Host a Match;
  the others press Find a Match and pick it from the list.
  On the same network, the console (the ` key) also has CheeseStrikeHostLan,
  CheeseStrikeFindLan and CheeseStrikeJoin 1.

This is a development build for testing: the console is open to everyone, and it
logs to CheeseStrike\Saved\Logs\CheeseStrike.log, which is worth sending with a bug.
"@ | Set-Content -Encoding utf8 (Join-Path $build "README.txt")

$zip = Join-Path $Out "$name.zip"
Write-Output "== zipping into $zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
# Windows' own tar writes zips, quicker than Compress-Archive and without its 2 GB limit. The debug
# symbols (a third of the build) stay here, under the build's name, for reading a crash log sent back.
& (Join-Path $env:SystemRoot "System32\tar.exe") -a -c -f "$zip" --exclude "*.pdb" -C "$build" .
if ($LASTEXITCODE -ne 0) { Fail "zipping failed" }
$symbols = Join-Path $Out "symbols\$name"
New-Item -ItemType Directory -Force $symbols | Out-Null
Get-ChildItem $build -Recurse -Filter *.pdb | Copy-Item -Destination $symbols
Get-ChildItem (Join-Path $Out "symbols") -Directory | Sort-Object LastWriteTime -Descending | Select-Object -Skip $Keep | Remove-Item -Recurse -Force
Write-Output ("   {0:N0} MB" -f ((Get-Item $zip).Length / 1MB))

function Prune($folder) {
  Get-ChildItem $folder -Filter "CheeseStrike_*.zip" | Sort-Object LastWriteTime -Descending | Select-Object -Skip $Keep | ForEach-Object {
    Write-Output "   deleting the older $($_.FullName)"
    Remove-Item $_.FullName -Force
  }
}
Prune $Out

if (-not $NoUpload) {
  if (-not (Test-Path (Split-Path -Parent $Drive))) { Fail "no Google Drive at $(Split-Path -Parent $Drive): start Google Drive for desktop, or give -Drive" }
  New-Item -ItemType Directory -Force $Drive | Out-Null
  Write-Output "== copying into $Drive"
  Copy-Item $zip $Drive
  Prune $Drive
  Write-Output "   Google Drive for desktop uploads it in the background: its icon in the taskbar shows the progress."
}
Write-Output "== done: $name"
