# Show-Tree.ps1
# 递归打印文件夹结构。仅对 -RecurseDirs 中列出的子文件夹递归展开，
# 其余根目录下的子文件夹只显示名字、不展开。
#
# 用法：
#   .\Show-Tree.ps1 -Path "C:\Users\11186484\CameraUtils"
#   .\Show-Tree.ps1 -Path "C:\Users\11186484\CameraUtils" -RecurseDirs "inc","src","tests"
#   .\Show-Tree.ps1 -Path "C:\Users\11186484\CameraUtils" -Exclude ".git","out","third_party"
#   .\Show-Tree.ps1 -Path "C:\Users\11186484\CameraUtils" -ShowSize | Out-File -Encoding utf8 tree.txt

param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Path,

    [int]$IndentSize = 2,

    [string[]]$Exclude = @(),

    [switch]$ShowSize,

    # 只对这些根目录下的子文件夹递归展开；其余子文件夹只显示名字。
    # 改成你工程里实际要展开的目录名。
    [string[]]$RecurseDirs = @("inc", "src", "tests")
)

function Format-Size {
    param([long]$Bytes)
    if ($Bytes -ge 1GB) { "{0:N1} GB" -f ($Bytes / 1GB) }
    elseif ($Bytes -ge 1MB) { "{0:N1} MB" -f ($Bytes / 1MB) }
    elseif ($Bytes -ge 1KB) { "{0:N1} KB" -f ($Bytes / 1KB) }
    else { "$Bytes B" }
}

function Write-Tree {
    param(
        [string]$CurrentPath,
        [int]$Level,
        # $true 表示“只有 $script:EffectiveRecurseDirs 里的文件夹才递归”
        [bool]$SelectiveRecurse
    )

    $items = Get-ChildItem -LiteralPath $CurrentPath -Force -ErrorAction SilentlyContinue |
             Where-Object { $Exclude -notcontains $_.Name } |
             Sort-Object @{ Expression = { -not $_.PSIsContainer } }, Name

    $indent = ' ' * ($IndentSize * $Level)

    foreach ($item in $items) {
        if ($item.PSIsContainer) {
            Write-Output "$indent$($item.Name)/"

            # 根目录的直接子级：只有 EffectiveRecurseDirs 里列出的才递归；
            # 已经递归进入的文件夹内部：所有子文件夹都正常递归。
            $shouldRecurse = (-not $SelectiveRecurse) -or ($script:EffectiveRecurseDirs -contains $item.Name)
            if ($shouldRecurse) {
                Write-Tree -CurrentPath $item.FullName `
                           -Level ($Level + 1) `
                           -SelectiveRecurse $false
            }
        } else {
            if ($ShowSize) {
                $size = Format-Size $item.Length
                Write-Output ("{0}{1}  [{2}]" -f $indent, $item.Name, $size)
            } else {
                Write-Output "$indent$($item.Name)"
            }
        }
    }
}

# ---------- 入口检查 ----------
if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
    Write-Error "路径不存在或不是文件夹: $Path"
    exit 1
}

$root = (Resolve-Path -LiteralPath $Path).Path

# 把 RecurseDirs 中存在的目录存到 EffectiveRecurseDirs。
# 注意：不要用 $script:RecurseDirs = @()，否则会清空 param 传入的 $RecurseDirs。
$script:EffectiveRecurseDirs = @()
foreach ($d in $RecurseDirs) {
    $fullPath = Join-Path $root $d
    if (Test-Path -LiteralPath $fullPath -PathType Container) {
        $script:EffectiveRecurseDirs += $d
    } else {
        Write-Warning "配置的递归目录不存在，将被忽略: $fullPath"
    }
}

Write-Output "$root/"
Write-Tree -CurrentPath $root -Level 1 -SelectiveRecurse $true