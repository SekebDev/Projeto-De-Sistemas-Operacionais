# Roda o experimento completo no Windows nativo.
#
#   cd experimentos
#   powershell -ExecutionPolicy Bypass -File .\run_windows.ps1
#
# Antes: compilar com code\build_windows.bat (bench.exe precisa existir).
# Mesma logica do run_linux.sh: 12 configs x $Reps, ordem embaralhada,
# 1 aquecimento descartado por configuracao, depois latencia.
param(
    [int]$Itens   = 400000,
    [int]$Reps    = 30,
    [int]$LatReps = 5
)
$ErrorActionPreference = 'Stop'

$Raiz  = Split-Path -Parent $PSScriptRoot
$Bench = Join-Path $Raiz 'code\bench.exe'
$Dados = Join-Path $Raiz 'dados\windows'
$Vazao = Join-Path $Dados 'vazao.csv'
$Lat   = Join-Path $Dados 'latencia.csv'

if (-not (Test-Path $Bench)) { throw "bench.exe nao encontrado. Rode code\build_windows.bat antes." }
New-Item -ItemType Directory -Force $Dados | Out-Null

# ---- ambiente ----
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$os  = Get-CimInstance Win32_OperatingSystem
$cl  = 'n/d'
try { $cl = (cmd /c "cl 2>&1" | Select-Object -First 1) } catch { }
@(
    "data: $(Get-Date -Format o)"
    "so: $($os.Caption) $($os.Version) build $($os.BuildNumber)"
    "cpu: $($cpu.Name)"
    "nucleos fisicos: $($cpu.NumberOfCores)  logicos: $($cpu.NumberOfLogicalProcessors)"
    "ram: $([math]::Round($os.TotalVisibleMemorySize / 1MB, 1)) GB"
    "compilador: $cl"
    "flags: /O2 /W4"
    "plano de energia: $((powercfg /getactivescheme) -join ' ')"
    "itens: $Itens  reps: $Reps  lat_reps: $LatReps"
) | Set-Content -Encoding utf8 (Join-Path $Dados 'ambiente.txt')

# ---- vazao ----
if (-not (Test-Path $Vazao)) {
    'so,versao,produtores,consumidores,buffer,itens,rep,tempo_s,vazao_itens_s' |
        Set-Content -Encoding ascii $Vazao
}

$configs = foreach ($v in 'A','B') {
    foreach ($pc in 1,4) {
        foreach ($n in 1,16,256) { [pscustomobject]@{ V=$v; P=$pc; C=$pc; N=$n } }
    }
}

function Get-BenchArgs($e) {
    @('--version', $e.V, '--producers', $e.P, '--consumers', $e.C,
      '--buffer', $e.N, '--items', $Itens, '--os', 'windows')
}

# aquecimento: 1 execucao descartada por configuracao
foreach ($e in $configs) {
    $a = Get-BenchArgs $e
    & $Bench @a --rep 0 | Out-Null
}

$lista = foreach ($e in $configs) {
    foreach ($r in 1..$Reps) { [pscustomobject]@{ V=$e.V; P=$e.P; C=$e.C; N=$e.N; R=$r } }
}
$lista = $lista | Get-Random -Count $lista.Count   # embaralha

$i = 0
foreach ($e in $lista) {
    $i++
    $a = Get-BenchArgs $e
    $linha = & $Bench @a --rep $e.R
    if ($LASTEXITCODE -ne 0) { throw "bench falhou em $($e.V) $($e.P)x$($e.C) buf $($e.N) rep $($e.R)" }
    Add-Content -Encoding ascii $Vazao $linha
    Write-Progress -Activity 'vazao' -Status "$i de $($lista.Count)" -PercentComplete (100 * $i / $lista.Count)
}

# ---- latencia ----
if (-not (Test-Path $Lat)) { 'so,versao,rep,seq,latencia_ns' | Set-Content -Encoding ascii $Lat }
$lb = @('--producers', 1, '--consumers', 1, '--buffer', 1, '--items', $Itens, '--os', 'windows')
foreach ($v in 'A','B') { & $Bench --version $v @lb --rep 0 | Out-Null }
foreach ($r in 1..$LatReps) {
    foreach ($v in ('A','B' | Get-Random -Count 2)) {
        & $Bench --version $v @lb --rep $r --latency $Lat | Out-Null
    }
}

# compacta para nao estourar o limite de arquivo do GitHub
$gz  = "$Lat.gz"
$in  = [IO.File]::OpenRead($Lat)
$out = [IO.File]::Create($gz)
$z   = New-Object IO.Compression.GZipStream($out, [IO.Compression.CompressionMode]::Compress)
$in.CopyTo($z); $z.Close(); $in.Close()
Remove-Item $Lat

Write-Host "pronto: $Vazao e $gz"
