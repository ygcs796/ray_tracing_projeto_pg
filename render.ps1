# render.ps1 — CLI de renderização do ray-tracer para Windows (PowerShell)
#
# Uso: .\render.ps1 <caminho-do-cenario.json> [pasta-de-saida]
# Ex.:  .\render.ps1 utils\input\sampleScene.json
# Ex.:  .\render.ps1 utils\input\entrega-1-cenarios\1-tres-esferas.json renders

param(
    [string]$Scene = "utils\input\sampleScene.json",
    [string]$OutDir = "renders"
)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

# ---------------------------------------------------------------------------
# Validação
# ---------------------------------------------------------------------------
if (-not (Test-Path $Scene)) {
    Write-Error "Cena não encontrada: $Scene"
    Write-Host ""
    Write-Host "Uso: .\render.ps1 <caminho-do-cenario.json> [pasta-de-saida]"
    Write-Host "Ex.: .\render.ps1 utils\input\sampleScene.json"
    exit 1
}

$Base = [System.IO.Path]::GetFileNameWithoutExtension($Scene)
$PPM  = Join-Path $OutDir "$Base.ppm"
$PNG  = Join-Path $OutDir "$Base.png"

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# ---------------------------------------------------------------------------
# Compilação (se binário não existir)
# ---------------------------------------------------------------------------
if (-not (Test-Path "raytracer.exe")) {
    Write-Host "[render] Compilando raytracer..."
    g++ -std=c++17 -O2 main.cpp -o raytracer.exe
    if ($LASTEXITCODE -ne 0) { Write-Error "Falha na compilação"; exit 1 }
}

# ---------------------------------------------------------------------------
# Renderização
# ---------------------------------------------------------------------------
Write-Host "[render] Cena:  $Scene"
Write-Host "[render] Saída: $PPM + $PNG"
Write-Host ""

& .\raytracer.exe $Scene > $PPM
if ($LASTEXITCODE -ne 0) { Write-Error "Falha na renderização"; exit 1 }

# ---------------------------------------------------------------------------
# Conversão PPM → PNG
# ---------------------------------------------------------------------------
$converted = $false

# ImageMagick 7+
if (Get-Command magick -ErrorAction SilentlyContinue) {
    magick $PPM $PNG
    Write-Host "[render] PNG gerado (via ImageMagick)"
    $converted = $true
}
# Python + Pillow
elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $hasPil = & python -c "import PIL" 2>$null
    if ($LASTEXITCODE -eq 0) {
        python utils\convert_ppm.py $PPM $PNG
        Write-Host "[render] PNG gerado (via Pillow)"
        $converted = $true
    }
}

if (-not $converted) {
    Write-Warning "Nenhum conversor PPM→PNG disponível."
    Write-Warning "Instale ImageMagick ou execute: pip install Pillow"
    Write-Warning "PPM permanece em: $PPM"
    exit 0
}

Write-Host ""
Write-Host "[render] Concluído:"
Write-Host "  $PPM"
Write-Host "  $PNG"
