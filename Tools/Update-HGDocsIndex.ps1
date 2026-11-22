[CmdletBinding()]
param()

# Docs/ 아래 문서들의 제목(#, ##, ###)만 모아 Docs/INDEX.md를 다시 만든다.
#
# 왜 자동 생성인가:
#   색인을 손으로 고치면 문서와 어긋나기 쉽다. 문서 본문만 고치고 이 스크립트를 돌리면 색인은 언제나 실제 제목과 같다.
#   생성 시각을 넣지 않는 것도 같은 이유다. 제목이 바뀌지 않았으면 INDEX.md도 바뀌지 않아 Git 변경이 생기지 않는다.
#
# 순서: Project -> Source -> Content -> Tools -> 그 밖의 폴더(이름순). 폴더 안에서는 Overview.md를 먼저, 나머지는 이름순.

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$docsRoot = Join-Path $projectRoot "Docs"
$indexPath = Join-Path $docsRoot "INDEX.md"

$folderOrder = @("Project", "Source", "Content", "Tools")

$files = Get-ChildItem -Path $docsRoot -Recurse -Filter *.md -File | Where-Object { $_.FullName -ne $indexPath }

function Get-RelativeDocPath([System.IO.FileInfo]$File) {
    return $File.FullName.Substring($docsRoot.Length + 1).Replace('\', '/')
}

function Get-FolderRank([string]$RelPath) {
    $top = ($RelPath -split '/')[0]
    $rank = [array]::IndexOf($folderOrder, $top)
    if ($rank -lt 0) { $rank = $folderOrder.Count }
    return $rank
}

$sorted = $files | Sort-Object `
    @{ Expression = { Get-FolderRank (Get-RelativeDocPath $_) } }, `
    @{ Expression = { (Split-Path -Parent (Get-RelativeDocPath $_)) } }, `
    @{ Expression = { if ($_.Name -eq "Overview.md") { 0 } else { 1 } } }, `
    @{ Expression = { $_.Name } }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# 문서 색인")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("> 자동 생성 파일이다. 손으로 고치지 말고 문서를 고친 뒤 ``Tools/Update-HGDocsIndex.ps1``을 실행한다.")
[void]$sb.AppendLine("> 각 문서의 제목과 소제목만 모았다. 작업할 폴더의 문서를 골라 본문을 읽은 뒤, 실제 코드로 확인한다.")

$currentFolder = $null
foreach ($file in $sorted) {
    $rel = Get-RelativeDocPath $file
    $folder = Split-Path -Parent $rel
    if ([string]::IsNullOrEmpty($folder)) { $folder = "(Docs 루트)" }
    if ($folder -ne $currentFolder) {
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("## " + $folder.Replace('\', '/'))
        $currentFolder = $folder
    }

    $lines = Get-Content -Path $file.FullName -Encoding UTF8
    $inCode = $false
    $title = $null
    $items = New-Object System.Collections.Generic.List[string]
    foreach ($line in $lines) {
        # 코드 블록 안의 "#"은 제목이 아니다 (PowerShell 주석 등).
        if ($line -match '^\s*```') { $inCode = -not $inCode; continue }
        if ($inCode) { continue }
        if ($line -match '^(#{1,3})\s+(.+?)\s*$') {
            $level = $Matches[1].Length
            $text = $Matches[2]
            if ($level -eq 1) {
                if (-not $title) { $title = $text }
            }
            elseif ($level -eq 2) { $items.Add("- " + $text) }
            else { $items.Add("  - " + $text) }
        }
    }
    if (-not $title) { $title = $file.BaseName }

    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("### [" + $title + "](" + $rel + ")")
    [void]$sb.AppendLine("")
    foreach ($item in $items) { [void]$sb.AppendLine($item) }
}

# BOM 없는 UTF-8로 쓴다. 줄바꿈은 LF로 통일해서 다시 만들어도 같은 내용이면 같은 바이트가 되게 한다.
$text = $sb.ToString().Replace("`r`n", "`n")
[System.IO.File]::WriteAllText($indexPath, $text, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("Docs/INDEX.md 갱신: 문서 {0}개" -f @($sorted).Count)
