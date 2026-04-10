"""
Leitor de arquivos .obj (Wavefront) para malhas trianguladas.

Lê vértices (v), normais (vn) e faces triangulares (f).
Coordenadas de textura (vt) são ignoradas, conforme especificação do projeto.

Formatos de face suportados:
    v/vt/vn   →  ex: "2/1/1"
    v//vn     →  ex: "2//1"
"""
from __future__ import annotations
import sys
from dataclasses import dataclass, field
from pathlib import Path

from src.Ponto import Ponto
from src.Vetor import Vetor
from utils.MeshReader.Colormap import Colormap, MaterialProperties


@dataclass
class FaceData:
    vertice_indice: list[int]          = field(default_factory=lambda: [0, 0, 0])
    normal_indice:  list[int]          = field(default_factory=lambda: [0, 0, 0])
    material:       MaterialProperties = field(default_factory=MaterialProperties)


def _parse_face_token(token: str) -> tuple[int, int]:
    """
    Converte um token de face ("v/vt/vn" ou "v//vn") em
    (vertex_index, normal_index) com índices baseados em 0.
    """
    parts = token.split("/")
    vertex_idx = int(parts[0]) - 1
    normal_idx = int(parts[2]) - 1 if len(parts) >= 3 and parts[2] else 0
    return vertex_idx, normal_idx


class ObjReader:
    """
    Lê um arquivo .obj triangulado.
    O material de cada face é definido pela diretiva 'usemtl' ativa no momento.
    """

    def __init__(self, filename: str):
        self._filename    = filename
        self._vertices:    list[Ponto]       = []
        self._normals:     list[Vetor]       = []
        self._faces:       list[FaceData]    = []
        self._face_points: list[list[Ponto]] = []

        self._cur_material = MaterialProperties()
        self._cmap = Colormap()

        path = Path(filename)
        if not path.exists():
            print(f"Erro ao abrir o arquivo: {filename}", file=sys.stderr)
            return

        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                self._process_line(line.strip(), path)

        # Monta lista de pontos por face após todos os vértices serem lidos
        for face in self._faces:
            self._face_points.append([
                self._vertices[face.vertice_indice[0]],
                self._vertices[face.vertice_indice[1]],
                self._vertices[face.vertice_indice[2]],
            ])

    def _process_line(self, line: str, obj_path: Path) -> None:
        parts = line.split()
        if not parts:
            return
        prefix, *rest = parts

        if prefix == "mtllib" and rest:
            mtl_path = obj_path.with_suffix(".mtl")
            self._cmap = Colormap(str(mtl_path))

        elif prefix == "usemtl" and rest:
            self._cur_material = self._cmap.get_material_properties(rest[0])

        elif prefix == "v" and len(rest) >= 3:
            self._vertices.append(Ponto(float(rest[0]), float(rest[1]), float(rest[2])))

        elif prefix == "vn" and len(rest) >= 3:
            self._normals.append(Vetor(float(rest[0]), float(rest[1]), float(rest[2])))

        elif prefix == "f" and len(rest) >= 3:
            face = FaceData(material=self._cur_material)
            for i in range(3):
                vi, ni = _parse_face_token(rest[i])
                face.vertice_indice[i] = vi
                face.normal_indice[i]  = ni
            self._faces.append(face)

    # ------------------------------------------------------------------
    # Getters (espelham a API C++)
    # ------------------------------------------------------------------

    def get_face_points(self) -> list[list[Ponto]]:
        """Retorna lista de faces; cada face contém 3 Ponto com as coordenadas."""
        return self._face_points

    def get_faces(self) -> list[FaceData]:
        """Retorna lista de FaceData com índices e material."""
        return self._faces

    def get_vertices(self) -> list[Ponto]:
        return self._vertices

    def get_normals(self) -> list[Vetor]:
        return self._normals

    def get_kd(self) -> Vetor:
        return self._cur_material.kd

    def get_ka(self) -> Vetor:
        return self._cur_material.ka

    def get_ks(self) -> Vetor:
        return self._cur_material.ks

    def get_ke(self) -> Vetor:
        return self._cur_material.ke

    def get_ns(self) -> float:
        return self._cur_material.ns

    def get_ni(self) -> float:
        return self._cur_material.ni

    def get_d(self) -> float:
        return self._cur_material.d

    def get_filename(self) -> str:
        return self._filename

    def print_faces(self) -> None:
        for i, face_pts in enumerate(self._face_points, start=1):
            pts_str = "".join(f"({p.x}, {p.y}, {p.z})" for p in face_pts)
            print(f"Face {i}: {pts_str}", file=sys.stderr)
