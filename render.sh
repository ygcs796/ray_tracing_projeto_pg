#!/usr/bin/env bash
# render.sh — CLI de renderização do ray-tracer para macOS / Linux
#
# Recebe o path do JSON de cena, roda o raytracer, gera o PPM e converte para PNG.
# Se o binário não existir, compila antes. Se o JSON não for passado, usa sampleScene.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---------------------------------------------------------------------------
# Defaults e argumentos
# ---------------------------------------------------------------------------
SCENE="${1:-utils/input/sampleScene.json}"
OUT_DIR="${2:-renders}"

# Nome base = nome do arquivo JSON sem extensão
BASE="$(basename "$SCENE" .json)"
PPM="$OUT_DIR/$BASE.ppm"
PNG="$OUT_DIR/$BASE.png"

# ---------------------------------------------------------------------------
# Validação
# ---------------------------------------------------------------------------
if [ ! -f "$SCENE" ]; then
  echo "Erro: cena não encontrada: $SCENE" >&2
  echo "" >&2
  echo "Uso:   ./render.sh <caminho-do-cenario.json> [pasta-de-saida]" >&2
  echo "Ex.:   ./render.sh utils/input/entrega-1-cenarios/1-tres-esferas.json" >&2
  echo "Ex.:   ./render.sh utils/input/sampleScene.json renders" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

# ---------------------------------------------------------------------------
# Compilação (se binário não existir ou estiver desatualizado)
# ---------------------------------------------------------------------------
NEED_BUILD=0
if [ ! -x ./raytracer ]; then
  NEED_BUILD=1
else
  # Recompila se algum .cpp/.h for mais recente que o binário
  if [ -n "$(find main.cpp src utils -newer ./raytracer -type f 2>/dev/null | head -1)" ]; then
    NEED_BUILD=1
  fi
fi

if [ "$NEED_BUILD" -eq 1 ]; then
  echo "[render] Compilando raytracer..."
  g++ -std=c++17 -O2 main.cpp -o raytracer
fi

# ---------------------------------------------------------------------------
# Renderização
# ---------------------------------------------------------------------------
echo "[render] Cena:  $SCENE"
echo "[render] Saída: $PPM + $PNG"
echo ""

./raytracer "$SCENE" > "$PPM"

# ---------------------------------------------------------------------------
# Conversão PPM → PNG
# ---------------------------------------------------------------------------
if command -v sips >/dev/null 2>&1; then
  # macOS (nativo)
  sips -s format png "$PPM" --out "$PNG" >/dev/null 2>&1
  echo "[render] PNG gerado (via sips)"
elif command -v magick >/dev/null 2>&1; then
  # ImageMagick 7+
  magick "$PPM" "$PNG"
  echo "[render] PNG gerado (via ImageMagick)"
elif command -v convert >/dev/null 2>&1; then
  # ImageMagick 6
  convert "$PPM" "$PNG"
  echo "[render] PNG gerado (via ImageMagick)"
elif command -v python3 >/dev/null 2>&1 && python3 -c "import PIL" 2>/dev/null; then
  python3 utils/convert_ppm.py "$PPM" "$PNG"
  echo "[render] PNG gerado (via Pillow)"
else
  echo "[render] Aviso: nenhum conversor PPM→PNG disponível." >&2
  echo "[render]        Instale 'sips' (macOS), 'imagemagick' ou 'pip install Pillow'." >&2
  echo "[render]        PPM permanece em: $PPM" >&2
  exit 0
fi

echo ""
echo "[render] Concluído:"
echo "  $PPM"
echo "  $PNG"
