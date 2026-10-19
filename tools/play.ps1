# Plays the game without a person: starts it windowed on this PC, drives it
# through Windows input (keys and the mouse, so leave both alone while it
# runs, about 90 seconds), takes a screenshot after each step, and stops it.
# The character traces its state to the log ten times a second (-TraceState;
# see ACheeseStrikeCharacter::CheeseStrikeState), so a run can be checked by
# its numbers as well as its pictures.
#
#     powershell -ExecutionPolicy Bypass -File tools/play.ps1 -Tag <name>
#
# Screenshots go to Saved/Shots/<name>_<step>.png and the log to
# Saved/Logs/<name>.log. The character starts at -StartAt, a spot on the
# shooter level with room to run; change it with -StartAt "X,Y,Z,Yaw".
param([string]$Tag = "play", [string]$StartAt = "-231,-700,100,90")
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$project = Join-Path $root "CheeseStrike.uproject"
$version = (Get-Content $project | ConvertFrom-Json).EngineAssociation
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_$version" }
$editor = Join-Path $engine "Engine\Binaries\Win64\UnrealEditor.exe"
$out = Join-Path $root "Saved\Shots"
New-Item -ItemType Directory -Force $out | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Drive {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Explicit)] public struct UNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
  [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public UNION u; }
  [DllImport("user32.dll", SetLastError=true)] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
  [DllImport("user32.dll")] public static extern ushort VkKeyScan(char c);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
  public static void Key(ushort vk, bool down) {
    INPUT[] i = new INPUT[1]; i[0].type = 1; i[0].u.ki.wVk = vk;
    i[0].u.ki.wScan = (ushort)MapVirtualKey(vk, 0);
    i[0].u.ki.dwFlags = (uint)(down ? 0 : 2) | 8u; // KEYEVENTF_SCANCODE
    SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
  }
  public static void Mouse(int dx, int dy) {
    INPUT[] i = new INPUT[1]; i[0].type = 0; i[0].u.mi.dx = dx; i[0].u.mi.dy = dy; i[0].u.mi.dwFlags = 1; // MOUSEEVENTF_MOVE
    SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
  }
  public static void Click() {
    INPUT[] i = new INPUT[2]; i[0].type = 0; i[0].u.mi.dwFlags = 2; i[1].type = 0; i[1].u.mi.dwFlags = 4;
    SendInput(2, i, Marshal.SizeOf(typeof(INPUT)));
  }
}
"@
# Virtual keys.
$VK = @{ W = 0x57; A = 0x41; S = 0x53; D = 0x44; C = 0x43; Space = 0x20; Shift = 0xA0; Ctrl = 0xA2; Tilde = 0xC0; Enter = 0x0D; Esc = 0x1B }
function Hold($name) { [Drive]::Key([uint16]$VK[$name], $true) }
function Release($name) { [Drive]::Key([uint16]$VK[$name], $false) }
function Tap($name) { Hold $name; Start-Sleep -Milliseconds 60; Release $name }
function TypeText($text) {
  foreach ($ch in $text.ToCharArray()) {
    $vk = [Drive]::VkKeyScan($ch)
    $shift = ($vk -band 0x100) -ne 0
    $vk = $vk -band 0xFF
    if ($shift) { Hold "Shift" }
    [Drive]::Key([uint16]$vk, $true); Start-Sleep -Milliseconds 25; [Drive]::Key([uint16]$vk, $false)
    if ($shift) { Release "Shift" }
    Start-Sleep -Milliseconds 25
  }
}
function Console($command) {
  # The console opens with the tilde key and takes a line.
  Tap "Tilde"; Start-Sleep -Milliseconds 250
  TypeText $command; Start-Sleep -Milliseconds 150
  Tap "Enter"; Start-Sleep -Milliseconds 250
}
$game = Start-Process -FilePath $editor -ArgumentList @("`"$project`"", "/Game/Variant_Shooter/Lvl_Shooter", "-game", "-windowed", "-resx=960", "-resy=540", "-WinX=0", "-WinY=40", "-log=$Tag.log", "-nosound", "-TraceState", "-StartAt=$StartAt") -PassThru
Start-Sleep -Seconds 30
$game.Refresh()
$h = $game.MainWindowHandle
[Drive]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 800
[Drive]::Click()
Start-Sleep -Seconds 1
function Shoot($name) {
  $r = New-Object Drive+RECT
  [Drive]::GetWindowRect($h, [ref]$r) | Out-Null
  $w = $r.R - $r.L; $hh = $r.B - $r.T
  $bmp = New-Object System.Drawing.Bitmap $w, $hh
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
  $bmp.Save("$out\${Tag}_$name.png")
  Write-Output ("{0} {1:HH:mm:ss.fff}" -f $name, (Get-Date))
}
Shoot "1_idle"
Hold "W"; Start-Sleep -Milliseconds 1500; Shoot "2_walk"; Release "W"
Start-Sleep -Milliseconds 800
Hold "Shift"; Hold "W"; Start-Sleep -Milliseconds 1500; Shoot "3_sprint"; Release "W"; Release "Shift"
Start-Sleep -Milliseconds 800
Hold "C"; Start-Sleep -Milliseconds 1000; Shoot "4_crouch"
Hold "W"; Start-Sleep -Milliseconds 1200; Shoot "5_crouchwalk"; Release "W"; Release "C"
Start-Sleep -Milliseconds 800
[Drive]::Mouse(0, -300); Start-Sleep -Milliseconds 500; Shoot "6_lookup"
[Drive]::Mouse(0, 600); Start-Sleep -Milliseconds 500; Shoot "7_lookdown"
[Drive]::Mouse(0, -300); Start-Sleep -Milliseconds 300
[Drive]::Mouse(400, 0); Start-Sleep -Milliseconds 500; Shoot "8_lookright"
[Drive]::Mouse(-400, 0); Start-Sleep -Milliseconds 300
Hold "Shift"; Hold "W"; Start-Sleep -Milliseconds 1200; Hold "C"; Start-Sleep -Milliseconds 300; Shoot "9_slide"; Start-Sleep -Milliseconds 400; Shoot "10_slidelate"; Release "C"; Release "W"; Release "Shift"
Start-Sleep -Milliseconds 800
Hold "Space"; Start-Sleep -Milliseconds 250; Shoot "11_jump"; Release "Space"; Start-Sleep -Milliseconds 700
Hold "W"; Start-Sleep -Milliseconds 400; Hold "Space"; Start-Sleep -Milliseconds 250; Release "Space"; Start-Sleep -Milliseconds 300; Shoot "12_runjump"; Release "W"
Start-Sleep -Milliseconds 1000
Stop-Process -Id $game.Id -Force -ErrorAction SilentlyContinue
Write-Output "done"
