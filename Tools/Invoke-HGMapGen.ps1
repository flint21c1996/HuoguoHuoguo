[CmdletBinding()]
param(
    [string]$EngineRoot = "E:\UE_5.8",
    # 이미 있는 맵을 지우고 새로 만든다. 에디터에서 고친 배치가 사라지므로 주의.
    [switch]$Force
)

# 에디터를 띄우지 않고 커맨드렛으로 Tools/GenerateGreyboxMaps.py를 실행한다.
# 같은 프로젝트를 연 에디터가 켜져 있으면 맵 파일이 잠기거나 에디터 쪽 내용과 어긋나므로 먼저 닫는다.

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot "HuoguoHuoguo.uproject"
$editorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$script = Join-Path $PSScriptRoot "GenerateGreyboxMaps.py"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like "HuoguoHuoguo*" }) {
    throw "HuoguoHuoguo 에디터가 열려 있습니다. 닫고 다시 실행하세요."
}

$env:HG_MAPGEN_FORCE = if ($Force) { "1" } else { "0" }
& $editorCmd $projectFile -run=pythonscript "-script=$script" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
$exitCode = $LASTEXITCODE
Remove-Item Env:HG_MAPGEN_FORCE -ErrorAction SilentlyContinue
exit $exitCode
