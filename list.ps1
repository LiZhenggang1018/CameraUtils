# ============================================================
# 列出当前目录下的所有文件与子文件夹（深度 = 1）
# 修复中文乱码：控制台 / txt 文件统一使用系统 OEM 代码页（中文 = GBK）
# ============================================================

# ---------- 编码修复（关键）----------
$ErrorActionPreference = "Continue"

# 取系统 OEM 代码页（中文 Windows = 936 GBK）
$oemCP  = [System.Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage
$oemEnc = [System.Text.Encoding]::GetEncoding($oemCP)

[Console]::OutputEncoding = $oemEnc      # 让 Write-Host 输出 GBK
[Console]::InputEncoding  = $oemEnc
$OutputEncoding           = $oemEnc      # 管道输出也用 GBK

# ---------- 配置 ----------
$TargetFolder = (Get-Location).Path
$MaxDepth     = 2
$OutputFile   = Join-Path (Get-Location).Path ("FolderList_" + (Get-Date -Format "yyyyMMdd_HHmmss") + ".txt")

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("目标文件夹: $TargetFolder")
$lines.Add("生成时间  : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$lines.Add("=" * 70)

function Write-Items {
    param([string]$Path, [int]$Depth, [int]$MaxDepth)
    $indent = ("    " * $Depth)

    try {
        $items = Get-ChildItem -LiteralPath $Path -Force -ErrorAction Stop |
                 Sort-Object @{ Expression = { $_.PSIsContainer }; Descending = $true }, Name
    }
    catch {
        $lines.Add("$indent[无法访问] $Path -> $($_.Exception.Message)")
        return
    }

    if (@($items).Count -eq 0) { $lines.Add("$indent(空)"); return }

    foreach ($it in $items) {
        if ($it.PSIsContainer) {
            $lines.Add("$indent[文件夹] $($it.Name)")
            if ($Depth -lt $MaxDepth) {
                Write-Items -Path $it.FullName -Depth ($Depth + 1) -MaxDepth $MaxDepth
            }
        } else {
            $lines.Add("$indent[文件]   $($it.Name)")
        }
    }
}

Write-Items -Path $TargetFolder -Depth 1 -MaxDepth $MaxDepth

# 写文件：Encoding Default = 系统 ANSI（中文 = GBK），记事本直接正常显示
$lines | Out-File -FilePath $OutputFile -Encoding Default

# 屏幕输出
$lines | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host "结果已保存到: $OutputFile" -ForegroundColor Green
Write-Host "按回车键退出..." -ForegroundColor Yellow
[void][System.Console]::ReadLine()