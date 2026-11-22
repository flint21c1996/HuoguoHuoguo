[CmdletBinding()]
param(
    [string]$EngineRoot = "E:\UE_5.8",
    [ValidateSet("Development", "DebugGame")]
    [string]$Configuration = "Development"
)

# 에디터 타깃(HuoguoHuoguoEditor)을 명령줄에서 빌드한다.
# 에디터가 켜져 있고 Live Coding이 활성화되어 있으면 UBT가 빌드를 거부하므로 먼저 에디터를 닫는다.

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot "HuoguoHuoguo.uproject"
$buildScript = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"

& $buildScript HuoguoHuoguoEditor Win64 $Configuration "-Project=$projectFile" -WaitMutex
exit $LASTEXITCODE
