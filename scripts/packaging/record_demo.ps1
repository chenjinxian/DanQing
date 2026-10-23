# record_demo.ps1 - P-4: capture the DisplayTestApp window to PNG frames while
# optionally injecting wheel-zoom at a chosen cursor point. Assemble with
# frames_to_gif.py afterwards. PURE ASCII ONLY (PS 5.1 ANSI read).
# DPI note: SetProcessDPIAware() is MANDATORY before any coordinate call --
# a DPI-unaware host gets virtualized (halved at 200%) GetWindowRect values
# while CopyFromScreen reads physical pixels => top-left-quarter capture bug
# (2026-10-23 record).
# Usage: record_demo.ps1 -FramesDir build\p4-frames\hero -Model joeshouse -Seconds 13 -WheelZoomSteps 6 -AimX 0.62 -AimY 0.45
param(
    [string]$FramesDir = "D:\Github\DanQing\build\p4-frames\run",
    [double]$Seconds = 12,
    [double]$Fps = 9,
    [int]$WheelZoomSteps = 0,       # >0: one wheel notch every interval
    [int]$WheelIntervalMs = 1100,
    [int]$WheelDelta = 120,         # +120 zoom-in / -120 zoom-out
    [double]$AimX = 0.5,     # wheel zoom targets the cursor position --
    [double]$AimY = 0.5,     # aim at CONTENT (e.g. bridge deck), not empty center
    [int]$StartDelayMs = 0,
    [string]$Model = "",            # if set: launch Release exe with DANQING_AUTO_OPEN_MODEL
    [string]$AutoStyle = "",        # DANQING_AUTO_STYLE preset name
    [string]$AutoClip = "",          # ""=off / "1"=default frac / "0.25"=content z fraction
    [switch]$EnableFpsCheckbox,     # UIA-find "FPS" label, click the checkbox left of it
    [switch]$KeepApp
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class RecW32 {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, int d, int e);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    public struct RECT { public int L, T, R, B; }
    // WM_MOUSEWHEEL (0x020A) routes to the FOCUSED window -- a background
    // recorder can't steal foreground (focus follows it), so mouse_event wheel
    // lands nowhere. PostMessage to the app hwnd bypasses focus routing
    // (2026-10-23 record).
    public static void WheelTo(IntPtr hwnd, int screenX, int screenY, int delta) {
        IntPtr w = new IntPtr(delta * 65536);
        IntPtr l = new IntPtr((screenY << 16) | (screenX & 0xFFFF));
        PostMessage(hwnd, 0x020A, w, l);
    }
}
"@
New-Item -ItemType Directory -Force $FramesDir | Out-Null
[RecW32]::SetProcessDPIAware() | Out-Null

if ($Model -ne "") {
    $envLines = "set DANQING_AUTO_OPEN_MODEL=$Model"
    if ($AutoStyle -ne "") { $envLines = "$envLines&set DANQING_AUTO_STYLE=$AutoStyle" }
    if ($AutoClip -ne "") { $envLines = "$envLines&set DANQING_AUTO_CLIP=$AutoClip" }
    Start-Process cmd -ArgumentList "/c $envLines&`"D:\Github\DanQing\build\samples\DisplayTestApp\Release\DisplayTestApp.exe`""
    Start-Sleep -Seconds 6
}

$proc = Get-Process -Name "DisplayTestApp" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { throw "DisplayTestApp not running" }
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { throw "no main window handle" }
[RecW32]::ShowWindow($hwnd, 3) | Out-Null        # SW_MAXIMIZE (full canvas for README)
# Foreground GUARANTEE: mouse wheel on this machine routes to the focused
# window only ("scroll inactive windows" off) -- a background recorder's
# SetForegroundWindow can be denied, so verify and retry; abort loudly if the
# app never takes foreground (wheeltest evidence 2026-10-23).
$fgOk = $false
for ($try = 0; $try -lt 8 -and -not $fgOk; ++$try) {
    [RecW32]::mouse_event(1, 0, 0, 0, 0)  # MOUSEEVENTF_MOVE (0,0): null input --
    # grants this background process foreground-set rights (MSDN workaround;
    # dbg3 SetForegroundWindow denial 2026-10-23).
    [RecW32]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 400
    $fgOk = ([RecW32]::GetForegroundWindow() -eq $hwnd)
}
if (-not $fgOk) { throw "app never became foreground -- wheel would not reach it" }
# (no TOPMOST -- it broke mouse_event wheel routing on this machine: wheel
#  only zoomed with the app foreground AND non-topmost; diag-vs-record_demo
#  bisect 2026-10-23. Foreground + maximized is enough for clean frames.)
if ($StartDelayMs -gt 0) { Start-Sleep -Milliseconds $StartDelayMs }

$r = New-Object RecW32+RECT
[RecW32]::GetWindowRect($hwnd, [ref]$r) | Out-Null
$w = $r.R - $r.L; $h = $r.B - $r.T
$scr = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
Write-Output ("rect " + $w + "x" + $h + " @(" + $r.L + "," + $r.T + ") screen=" + $scr.Width + "x" + $scr.Height)
if ($w -le 0 -or $h -le 0) { throw "bad window rect" }
if ($w * 2 -le $scr.Width + 100) { Write-Output "WARN: rect looks logical-halved (DPI virtualization?)" }
$cx = $r.L + [int]($w * $AimX); $cy = $r.T + [int]($h * $AimY)
[RecW32]::SetCursorPos($cx, $cy) | Out-Null

if ($EnableFpsCheckbox) {
    # UIA: find the "FPS" Text element in the status bar, click the checkbox
    # immediately left of it (physical coords -- process is DPI-aware).
    $rootEl = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
    $fpsEl = $rootEl.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
        (New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::NameProperty, "FPS")))
    if ($fpsEl) {
        $b = $fpsEl.Current.BoundingRectangle
        $bx = [int]($b.Left - 14); $by = [int]($b.Y + $b.Height / 2)
        [RecW32]::SetCursorPos($bx, $by) | Out-Null
        Start-Sleep -Milliseconds 250
        [RecW32]::mouse_event(2, 0, 0, 0, 0); Start-Sleep -Milliseconds 60
        [RecW32]::mouse_event(4, 0, 0, 0, 0)
        Write-Output ("FPS checkbox clicked @(" + $bx + "," + $by + ")")
        Start-Sleep -Milliseconds 400
        [RecW32]::SetCursorPos($cx, $cy) | Out-Null
    } else {
        Write-Output "WARN: FPS label not found via UIA -- continuing without it"
    }
}

$intervalMs = [int](1000.0 / $Fps)
$frameCount = [int]($Seconds * $Fps)
$nextWheel = [DateTime]::Now.AddMilliseconds($WheelIntervalMs)
Write-Output ("REC " + $frameCount + " frames @ " + $intervalMs + "ms")
for ($i = 0; $i -lt $frameCount; ++$i) {
    $t0 = [DateTime]::Now
    if ($WheelZoomSteps -gt 0 -and [DateTime]::Now -ge $nextWheel) {
        [RecW32]::mouse_event(0x0800, 0, 0, $WheelDelta, 0)   # MOUSEEVENTF_WHEEL
        # (hardware injection: works with the foreground guarantee above;
        #  PostMessage WM_MOUSEWHEEL is NOT honored by this app -- 2026-10-23 record)
        $WheelZoomSteps--
        $nextWheel = [DateTime]::Now.AddMilliseconds($WheelIntervalMs)
    }
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $g.Dispose()
    $bmp.Save((Join-Path $FramesDir ("f{0:D4}.png" -f $i)), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $elapsed = ([DateTime]::Now - $t0).TotalMilliseconds
    $sleep = $intervalMs - [int]$elapsed
    if ($sleep -gt 0) { Start-Sleep -Milliseconds $sleep }
}
if (-not $KeepApp) {
    $proc.CloseMainWindow() | Out-Null
    Start-Sleep -Seconds 3
    $proc.Refresh()
    if (-not $proc.HasExited) { $proc.Kill() }
}
Write-Output ("REC DONE -> " + $FramesDir)
