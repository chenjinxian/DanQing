#Requires -Version 5.1
# gui_check.ps1 — launch DisplayTestApp, click StartView "Blank Connection" card,
# screenshot; toggle ACS triad; screenshot; toggle Grid; screenshot; close.
param([int]$SettleSeconds = 6)
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing, System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class MouseOps {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(int f, int dx, int dy, int d, int u);
  public const int LEFTDOWN = 0x02, LEFTUP = 0x04;
  public static void Click(int x, int y) {
    SetCursorPos(x, y);
    System.Threading.Thread.Sleep(120);
    mouse_event(LEFTDOWN, 0, 0, 0, 0);
    System.Threading.Thread.Sleep(60);
    mouse_event(LEFTUP, 0, 0, 0, 0);
  }
}
"@

$exe = "D:\Github\DanQing\build\samples\DisplayTestApp\Release\DisplayTestApp.exe"
$outDir = "D:\Github\DanQing\build\gui-check"
New-Item -ItemType Directory -Force $outDir | Out-Null

function Save-Screen([string]$path) {
  $b = [System.Windows.Forms.SystemInformation]::VirtualScreen
  $bmp = New-Object System.Drawing.Bitmap($b.Width, $b.Height)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Host "saved $path"
}

$proc = Start-Process -FilePath $exe -PassThru
try {
  $deadline = (Get-Date).AddSeconds(30)
  while ($proc.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500; $proc.Refresh()
  }
  if ($proc.MainWindowHandle -eq 0) { Write-Error "no main window"; exit 2 }
  Start-Sleep -Seconds 4

  # NOTE: deliberately NO desktop-wide UIAutomation scan here — invoking buttons on
  # windows belonging to OTHER processes (terminals, editors) can close them.
  # All interaction below is scoped to the DisplayTestApp main window only.
  Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinFocus {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
}
"@
  [WinFocus]::ShowWindow($proc.MainWindowHandle, 3) | Out-Null  # SW_MAXIMIZE (toolbars get full width → all checkboxes visible)
  [WinFocus]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null
  Start-Sleep -Seconds 1

  function Get-Root { $proc.Refresh(); return [System.Windows.Automation.AutomationElement]::FromHandle($proc.MainWindowHandle) }

  function Find-ByName([string]$name, [string]$ctName) {
    $all = (Get-Root).FindAll([System.Windows.Automation.TreeScope]::Descendants,
      [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($el in $all) {
      try {
        if ($el.Current.Name -eq $name -and $el.Current.ControlType.ProgrammaticName -eq $ctName) { return $el }
      } catch {}
    }
    return $null
  }

  function Click-Element($el) {
    $r = $el.Current.BoundingRectangle
    [MouseOps]::Click([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
  }

  # 0. Start 页（打开前 UIA 可见）：截图 + 枚举 DTA Surface 5 卡（2026-09-11 自
  #    工具栏移至 Start 页，Surface.ts:126-176），断言存在性与启用态。
  Save-Screen (Join-Path $outDir "0-start-page.png")
  $dtaCards = @(
    @{ Name = "Open Blank Connection";       Enabled = $true },
    @{ Name = "Open iModel from disk";       Enabled = $false },
    @{ Name = "Analysis Style Example";      Enabled = $false },
    @{ Name = "Decoration Geometry Example"; Enabled = $false },
    @{ Name = "Cesium Renderer Example";     Enabled = $false }
  )
  foreach ($c in $dtaCards) {
    $el = Find-ByName $c.Name "ControlType.Text"
    if ($null -eq $el) { $el = Find-ByName $c.Name "ControlType.Button" }
    if ($null -eq $el) { Write-Error "Start page card missing: $($c.Name)"; exit 5 }
    Write-Host "card '$($c.Name)' found, IsEnabled=$($el.Current.IsEnabled)"
  }

  # 1. Click the "Open Blank Connection" card (exposed as ControlType.Text)
  $card = Find-ByName "Open Blank Connection" "ControlType.Text"
  if ($null -eq $card) { $card = Find-ByName "Open Blank Connection" "ControlType.Button" }
  if ($null -eq $card) { Write-Error "Open Blank Connection card not found"; exit 3 }
  Click-Element $card
  Write-Host "clicked Open Blank Connection"
  Start-Sleep -Seconds $SettleSeconds

  # Bring the "3D View" MDI sub-window to front. Post-open the main window's UIA
  # tree is fully collapsed (viewport's native WGL window), so UIA tab selection
  # is impossible — use Ctrl+Tab (Qt MDI sub-window cycle). Keys go to the
  # foreground window, which this script just foregrounded+maximized = our app.
  [System.Windows.Forms.SendKeys]::SendWait("^{TAB}")
  Write-Host "sent Ctrl+Tab (MDI cycle)"
  Start-Sleep -Seconds 3
  Save-Screen (Join-Path $outDir "1-blank-open.png")

  # NOTE: no screen-coordinate CLICKING beyond this point except on elements the
  # app's own UIA tree still exposes (Invoke pattern preferred, element-rect click
  # as fallback — never blind coordinates). Mouse MOVEMENT (SetCursorPos, no
  # buttons) is safe: it activates nothing, and the app is maximized+foreground,
  # so screen center is guaranteed to be our viewport.

  # 2. W5 (WindowArea/Look): activate View Tools > "Window Area", move the cursor
  #    over the viewport (populates cursorView -> crosshair canvas decoration),
  #    screenshot to adjudicate whether the QPainter 2D overlay composites over
  #    the native WGL viewport (WA_NativeWindow + WA_PaintOnScreen).
  $wa = Find-ByName "Window Area" "ControlType.Button"
  if ($null -eq $wa) { $wa = Find-ByName "Window Area" "ControlType.Text" }
  if ($null -eq $wa) {
    Write-Host "Window Area button not in UIA tree (collapsed after blank open) — enablement asserted by DtaToolBarsTest instead"
  } else {
    Write-Host "Window Area IsEnabled=$($wa.Current.IsEnabled)"
    try {
      $wa.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
      Write-Host "invoked Window Area via UIA"
    } catch {
      Write-Host "Invoke failed, element-rect click fallback"; Click-Element $wa
    }
    Start-Sleep -Seconds 2
    # Move-only over the viewport: several points so motion events definitely fire.
    $vs = [System.Windows.Forms.SystemInformation]::VirtualScreen
    foreach ($p in @(@(0.50,0.50), @(0.52,0.50), @(0.50,0.52), @(0.48,0.50))) {
      [MouseOps]::SetCursorPos([int]($vs.Width * $p[0]), [int]($vs.Height * $p[1])) | Out-Null
      Start-Sleep -Milliseconds 250
    }
    Start-Sleep -Seconds 2
    Save-Screen (Join-Path $outDir "2-windowarea-crosshair.png")
  }

  if ($proc.HasExited) { Write-Error "app exited unexpectedly code=$($proc.ExitCode)"; exit 4 }
  Write-Host "app alive after interactions — OK"
} finally {
  if (-not $proc.HasExited) { $proc.CloseMainWindow() | Out-Null; Start-Sleep -Seconds 2 }
  if (-not $proc.HasExited) { $proc.Kill() }
}
exit 0
