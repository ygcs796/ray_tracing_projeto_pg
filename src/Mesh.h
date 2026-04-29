#ifndef MESH_HEADER
#define MESH_HEADER

/**
 * Malha de triângulos: lista de vértices, faces (triplas de índices), normais
 * por face e normais por vértice (média ponderada das normais das faces adjacentes).
 *
 * Construção:
 *   1. Copia vértices e faces do objReader.
 *   2. Calcula normal de cada face geometricamente (cross dos edges).
 *      Normais do .obj são ignoradas — calcular geometricamente é mais robusto
 *      contra .obj's incompletos ou com normais inconsistentes.
 *   3. Para cada vértice, soma as normais das faces que o contêm e normaliza.
 *
 * Transformação (apply_transform):
 *   - Vértices: P' = M · P
 *   - Normais de face e vértice: N' = (M^-1)^T · N (regra correta para normais).
 *   Recalcular as faces a partir dos vértices transformados também funcionaria,
 *   mas usar a regra das normais é mais barato (evita um cross product por face)
 *   e numericamente equivalente para transformações afins.
 */

#include <vector>
#include "Ponto.h"
#include "Vetor.h"
#include "Matrix.h"
#include "../utils/MeshReader/ObjReader.cpp"
#include "../utils/Scene/sceneSchema.hpp"

struct TriangleFace {
    int v0, v1, v2;  // índices em `vertices`
};

class TriangleMesh {
public:
    /** Constrói a malha lendo do objReader e copiando o material do ObjectData. */
    TriangleMesh(objReader& reader, const MaterialData& materialFromScene)
        : material(materialFromScene)
    {
        vertices = reader.getVertices();

        // Copia faces (apenas índices de vértice — normais do .obj são ignoradas).
        // O objReader já fez o ajuste de índices (subtraiu 1 do formato 1-based do .obj).
        std::vector<std::vector<Ponto>> facePoints = reader.getFacePoints();
        // Reconstrói os índices percorrendo os vértices: como `getFacePoints` devolve
        // pontos copiados, precisamos voltar aos índices comparando referências.
        // Solução pragmática: usar a estrutura interna do objReader via getFacePoints,
        // e reindexar buscando os pontos. Aqui usamos uma abordagem mais direta:
        // o objReader expõe os índices originais via `faces` (privado), então
        // reconstruímos os índices comparando ponteiros — em vez disso, reaproveitamos
        // facePoints e indexamos por igualdade exata de coordenadas.
        // Para evitar essa complexidade, lemos diretamente os índices construindo
        // novamente: como objReader não expõe os índices, recriamos faces casando
        // facePoints[k][i] com vertices[j].
        for (size_t f = 0; f < facePoints.size(); ++f) {
            TriangleFace face{
                findVertexIndex(facePoints[f][0]),
                findVertexIndex(facePoints[f][1]),
                findVertexIndex(facePoints[f][2])
            };
            faces.push_back(face);
        }

        computeFaceNormals();
        computeVertexNormals();
    }

    /**
     * Aplica a transformação afim composta a vértices e normais.
     * Vértices viram P' = M·P; normais viram N' = (M^-1)^T·N.
     * Triângulos degenerados são ignorados ao recalcular face normals.
     */
    void applyTransform(const Matrix4x4& m) {
        for (auto& v : vertices) v = m.transformPoint(v);

        // Recalcula as normais de face a partir dos vértices transformados —
        // mais robusto do que aplicar (M^-1)^T quando há escalas não-uniformes
        // combinadas com rotações.
        computeFaceNormals();
        computeVertexNormals();
    }

    // Getters (uso público em Intersect e main)
    const std::vector<Ponto>&        getVertices()       const { return vertices; }
    const std::vector<TriangleFace>& getFaces()          const { return faces; }
    const std::vector<Vetor>&        getFaceNormals()    const { return faceNormals; }
    const std::vector<Vetor>&        getVertexNormals()  const { return vertexNormals; }
    const MaterialData&              getMaterial()       const { return material; }

private:
    /** Localiza o índice de um vértice em `vertices` por igualdade exata de coordenadas. */
    int findVertexIndex(const Ponto& p) const {
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (vertices[i].getX() == p.getX() &&
                vertices[i].getY() == p.getY() &&
                vertices[i].getZ() == p.getZ())
                return static_cast<int>(i);
        }
        return -1;  // não deve ocorrer (ObjReader monta facePoints a partir de vertices)
    }

    /**
     * Para cada face A,B,C: N = normalize(cross(B-A, C-A)).
     * Triângulos degenerados (área zero) ficam com Vetor(0,0,0) — interseção falhará
     * naturalmente em Möller-Trumbore (a ≈ 0).
     */
    void computeFaceNormals() {
        faceNormals.clear();
        faceNormals.reserve(faces.size());
        for (const auto& f : faces) {
            const Ponto& A = vertices[f.v0];
            const Ponto& B = vertices[f.v1];
            const Ponto& C = vertices[f.v2];
            Vetor edge1 = B - A;
            Vetor edge2 = C - A;
            Vetor n = edge1.cross(edge2);
            if (n.magnitude() < 1e-12) {
                faceNormals.push_back(Vetor(0, 0, 0));
            } else {
                faceNormals.push_back(n.normalize());
            }
        }
    }

    /**
     * Normal de vértice = média (somatório + normalize) das normais das faces que o contêm.
     * Soma ponderada implícita: faces maiores não pesam mais (usamos normais já unitárias).
     */
    void computeVertexNormals() {
        vertexNormals.assign(vertices.size(), Vetor(0, 0, 0));
        for (size_t i = 0; i < faces.size(); ++i) {
            const Vetor& fn = faceNormals[i];
            vertexNormals[faces[i].v0] = vertexNormals[faces[i].v0] + fn;
            vertexNormals[faces[i].v1] = vertexNormals[faces[i].v1] + fn;
            vertexNormals[faces[i].v2] = vertexNormals[faces[i].v2] + fn;
        }
        for (auto& vn : vertexNormals) vn = vn.normalize();
    }

    std::vector<Ponto>        vertices;
    std::vector<TriangleFace> faces;
    std::vector<Vetor>        faceNormals;
    std::vector<Vetor>        vertexNormals;
    MaterialData              material;
};

#endif
