"""
Exemplo de uso do SceneJsonLoader.
Execute a partir da raiz do projeto:
    python utils/Scene/useExample.py
"""
import sys
from pathlib import Path

# Garante que a raiz do projeto está no path quando executado diretamente
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from utils.Scene.sceneParser import SceneJsonLoader


def main():
    scene_file = Path(__file__).parent.parent / "input" / "sampleScene.json"
    try:
        scene = SceneJsonLoader.load_file(str(scene_file))
        print(f"Objetos carregados: {len(scene.objects)}", file=sys.stderr)
        print(f"Luzes carregadas:   {len(scene.light_list)}", file=sys.stderr)
        print(f"Resolução da câmera: {scene.camera.image_width} x {scene.camera.image_height}", file=sys.stderr)
    except Exception as e:
        print(f"Erro ao carregar cena: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
