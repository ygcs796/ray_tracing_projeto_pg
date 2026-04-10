"""
Leitor de arquivos .mtl (Material Template Library).

Propriedades de material:
    kd = Difuso (cor do objeto)
    ks = Especular (reflexivo)
    ke = Emissivo
    ka = Ambiente
    kr = Reflexivo  (considere kr = ke)
    kt = Transmissivo
    ns = Brilho / rugosidade
    ni = Índice de refração
    d  = Opacidade
"""
from __future__ import annotations
import sys
from dataclasses import dataclass, field
from pathlib import Path

from src.Vetor import Vetor


@dataclass
class MaterialProperties:
    kd: Vetor = field(default_factory=lambda: Vetor(0, 0, 0))  # Difuso
    ks: Vetor = field(default_factory=lambda: Vetor(0, 0, 0))  # Especular
    ka: Vetor = field(default_factory=lambda: Vetor(0, 0, 0))  # Ambiente
    kr: Vetor = field(default_factory=lambda: Vetor(0, 0, 0))  # Reflexivo  (alias ke)
    ke: Vetor = field(default_factory=lambda: Vetor(0, 0, 0))  # Emissivo   (alias kr)
    kt: Vetor = field(default_factory=lambda: Vetor(0, 0, 0))  # Transmissivo
    ns: float = 0.0   # Brilho / rugosidade
    ni: float = 0.0   # Índice de refração
    d:  float = 0.0   # Opacidade


class Colormap:
    """Lê um arquivo .mtl e expõe as propriedades de cada material por nome."""

    def __init__(self, filepath: str = ""):
        self.mp: dict[str, MaterialProperties] = {}
        if not filepath:
            return

        path = Path(filepath)
        if not path.exists():
            print(f"erro abrindo arquivo {filepath}", file=sys.stderr)
            return

        current_material = ""
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                parts = line.strip().split()
                if not parts:
                    continue
                keyword, *rest = parts

                if keyword == "newmtl":
                    current_material = rest[0] if rest else ""
                    if current_material:
                        self.mp[current_material] = MaterialProperties()

                elif current_material:
                    mat = self.mp[current_material]
                    if keyword == "Kd" and len(rest) >= 3:
                        mat.kd = Vetor(float(rest[0]), float(rest[1]), float(rest[2]))
                    elif keyword == "Ks" and len(rest) >= 3:
                        mat.ks = Vetor(float(rest[0]), float(rest[1]), float(rest[2]))
                    elif keyword == "Ke" and len(rest) >= 3:
                        mat.ke = Vetor(float(rest[0]), float(rest[1]), float(rest[2]))
                    elif keyword == "Kr" and len(rest) >= 3:
                        mat.kr = Vetor(float(rest[0]), float(rest[1]), float(rest[2]))
                    elif keyword == "Ka" and len(rest) >= 3:
                        mat.ka = Vetor(float(rest[0]), float(rest[1]), float(rest[2]))
                    elif keyword == "Ns" and rest:
                        mat.ns = float(rest[0])
                    elif keyword == "Ni" and rest:
                        mat.ni = float(rest[0])
                    elif keyword == "d" and rest:
                        mat.d = float(rest[0])

    def get_color(self, name: str) -> Vetor:
        if name in self.mp:
            return self.mp[name].kd
        print(f"Error: cor {name!r} indefinida no arquivo .mtl", file=sys.stderr)
        return Vetor(0, 0, 0)

    def get_material_properties(self, name: str) -> MaterialProperties:
        if name in self.mp:
            return self.mp[name]
        print(f"Error: material {name!r} indefinido no arquivo .mtl", file=sys.stderr)
        return MaterialProperties()
