@echo off
REM render.bat — CLI de renderização do ray-tracer para Windows (CMD)
REM
REM Uso: render.bat <caminho-do-cenario.json> [pasta-de-saida]
REM Ex.:  render.bat utils\input\sampleScene.json
REM Ex.:  render.bat utils\input\entrega-1-cenarios\1-tres-esferas.json renders

setlocal

REM ---------------------------------------------------------------------------
REM Defaults
REM ---------------------------------------------------------------------------
set "SCENE=%~1"
if "%SCENE%"=="" set "SCENE=utils\input\sampleScene.json"

set "OUT_DIR=%~2"
if "%OUT_DIR%"=="" set "OUT_DIR=renders"

REM Extrai o nome base (sem extensão)
for %%F in ("%SCENE%") do set "BASE=%%~nF"

set "PPM=%OUT_DIR%\%BASE%.ppm"
set "PNG=%OUT_DIR%\%BASE%.png"

REM ---------------------------------------------------------------------------
REM Validação
REM ---------------------------------------------------------------------------
if not exist "%SCENE%" (
    echo Erro: cena nao encontrada: %SCENE% 1>&2
    echo.
    echo Uso: render.bat ^<caminho-do-cenario.json^> [pasta-de-saida] 1>&2
    echo Ex.: render.bat utils\input\sampleScene.json 1>&2
    exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM ---------------------------------------------------------------------------
REM Compilação (se binário não existir)
REM ---------------------------------------------------------------------------
if not exist raytracer.exe (
    echo [render] Compilando raytracer...
    g++ -std=c++17 -O2 main.cpp -o raytracer.exe
    if errorlevel 1 (
        echo [render] Erro na compilacao 1>&2
        exit /b 1
    )
)

REM ---------------------------------------------------------------------------
REM Renderização
REM ---------------------------------------------------------------------------
echo [render] Cena:  %SCENE%
echo [render] Saida: %PPM% + %PNG%
echo.

raytracer.exe "%SCENE%" > "%PPM%"
if errorlevel 1 (
    echo [render] Erro na renderizacao 1>&2
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM Conversão PPM → PNG
REM ---------------------------------------------------------------------------
where magick >nul 2>&1
if %errorlevel%==0 (
    magick "%PPM%" "%PNG%"
    echo [render] PNG gerado ^(via ImageMagick^)
    goto done
)

where python >nul 2>&1
if %errorlevel%==0 (
    python -c "import PIL" >nul 2>&1
    if not errorlevel 1 (
        python utils\convert_ppm.py "%PPM%" "%PNG%"
        echo [render] PNG gerado ^(via Pillow^)
        goto done
    )
)

echo [render] Aviso: nenhum conversor PPM-^>PNG disponivel. 1>&2
echo [render]        Instale ImageMagick ou execute: pip install Pillow 1>&2
echo [render]        PPM permanece em: %PPM% 1>&2
exit /b 0

:done
echo.
echo [render] Concluido:
echo   %PPM%
echo   %PNG%

endlocal
