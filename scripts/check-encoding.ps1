# 乱码校验脚本：检测源码文本文件是否被错误编码破坏（如 GBK 读入 UTF-8 再写回）。
# 用法：powershell -ExecutionPolicy Bypass -File scripts/check-encoding.ps1 [路径...]
# 默认扫描仓库根目录下已跟踪的文本文件；退出码非 0 表示发现乱码。
# 本脚本无中文字面量依赖，可用任意编码保存。

param(
    [string[]]$Paths = @()
)

$ErrorActionPreference = 'Stop'

# 仅校验的文本扩展名
$TextExts = @('.h', '.hpp', '.cpp', '.cc', '.cxx', '.md', '.txt', '.json', '.cmake',
              '.clang-format', '.clang-tidy', '.cmake-format', '.yml', '.yaml', '.ps1',
              '.gitignore', '.gitattributes', '.editorconfig')

function Test-GarbledFile {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0) { return @() }

    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    $hits = @()

    # 信号 1：替换字符 U+FFFD（EF BF BD），编码损坏的最强信号
    $replacementCount = 0
    for ($i = 0; $i -lt $bytes.Length - 2; $i++) {
        if ($bytes[$i] -eq 0xEF -and $bytes[$i + 1] -eq 0xBF -and $bytes[$i + 2] -eq 0xBD) {
            $replacementCount++
        }
    }
    if ($replacementCount -gt 0) {
        $hits += "U+FFFD replacement chars: $replacementCount"
    }

    # 信号 2：私用区字符 U+E000-U+F8FF，GBK 错转 UTF-8 的典型特征
    $puaCount = 0
    foreach ($ch in $text.ToCharArray()) {
        $c = [int][char]$ch
        if ($c -ge 0xE000 -and $c -le 0xF8FF) {
            $puaCount++
        }
    }
    if ($puaCount -gt 0) {
        $hits += "private-use-area chars (U+E000-U+F8FF): $puaCount"
    }

    return $hits
}

# 收集待校验文件
$files = @()
if ($Paths.Count -gt 0) {
    foreach ($p in $Paths) {
        if (Test-Path -LiteralPath $p -PathType Container) {
            $files += Get-ChildItem -LiteralPath $p -Recurse -File | Where-Object { $TextExts -contains $_.Extension.ToLower() } | ForEach-Object { $_.FullName }
        } elseif (Test-Path -LiteralPath $p -PathType Leaf) {
            $files += $p
        }
    }
} else {
    $tracked = git ls-files 2>$null
    foreach ($line in $tracked) {
        $ext = [System.IO.Path]::GetExtension($line).ToLower()
        if ($TextExts -contains $ext) {
            $files += $line
        }
    }
}

$totalBad = 0
foreach ($f in $files) {
    $hits = Test-GarbledFile -Path $f
    if ($hits.Count -gt 0) {
        $totalBad++
        Write-Host "[GARBLED] $f"
        foreach ($h in $hits) {
            Write-Host "    - $h"
        }
    }
}

if ($totalBad -eq 0) {
    Write-Host "OK: no garbled text found (checked $($files.Count) files)"
    exit 0
} else {
    Write-Host ""
    Write-Host "Found $totalBad garbled file(s). Fix with read/edit tools (not PowerShell text commands)."
    exit 1
}
