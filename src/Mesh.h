#ifndef MESH_HEADER
#define MESH_HEADER

/**
 * Malha de triângulos: representa qualquer superfície (carro, rosto, monkey)
 * como uma coleção de triângulos planos.
 *
 * Estrutura:
 *   - vertices              — todos os vértices únicos da malha (sem duplicação)
 *   - faces                 — cada face é uma tripla de ÍNDICES em `vertices`
 *   - faceNormals[i]        — normal da face i (perpendicular ao seu plano)
 *   - vertexNormals[i]      — normal do vértice i (média das faces adjacentes)
 *   - material              — cor difusa, especular, etc., comum a toda a malha
 *
 * Por que indexar vértices em vez de duplicar?
 *   Em um cubo, cada vértice é compartilhado por 3 faces. Indexar economiza
 *   memória e — mais importante — garante que ao transformar um vértice, todas
 *   as faces que o usam ficam corretas automaticamente.
 *
 * Por que ignoramos as normais que vêm do .obj?
 *   .obj's nem sempre exportam normais para todas as faces, e quando exportam
 *   podem ter inconsistências de orientação. Recalcular geometricamente via
 *   cross product das arestas é sempre robusto.
 */

#include <vector>
#include "Ponto.h"
#include "Vetor.h"
#include "Matrix.h"
#include "../utils/MeshReader/ObjReader.cpp"
#include "../utils/Scene/sceneSchema.hpp"

/** Uma face triangular: três índices apontando para vértices em `vertices`. */
struct TriangleFace {
    int firstVertexIndex;
    int secondVertexIndex;
    int thirdVertexIndex;
};

class TriangleMesh {
public:
    /**
     * Carrega a malha a partir de um leitor de .obj e copia o material da cena.
     * Após isto, a malha está pronta para interseção (vértices, faces e normais
     * todos calculados).
     */
    TriangleMesh(objReader& meshReader, const MaterialData& materialFromScene)
        : material(materialFromScene)
    {
        vertices = meshReader.getVertices();

        // O `objReader` já fez o ajuste de índices (.obj é 1-based; convertemos
        // para 0-based). `getFacePoints` devolve cada face como uma cópia dos
        // 3 pontos — para reconstruir os índices, casamos por igualdade de
        // coordenadas com o array `vertices`.
        std::vector<std::vector<Ponto>> facesAsPointTriples = meshReader.getFacePoints();
        for (const auto& threePointsOfFace : facesAsPointTriples) {
            TriangleFace face{
                findVertexIndexByPosition(threePointsOfFace[0]),
                findVertexIndexByPosition(threePointsOfFace[1]),
                findVertexIndexByPosition(threePointsOfFace[2])
            };
            faces.push_back(face);
        }

        recomputeFaceNormalsFromCurrentVertices();
        recomputeVertexNormalsFromFaceNormals();
    }

    /**
     * Aplica uma transformação 4x4 (composição de translação, escala, rotação)
     * a todos os vértices da malha.
     *
     * Estratégia para as normais: em vez de aplicar (M⁻¹)ᵀ a cada normal já
     * existente, RECALCULAMOS as normais geometricamente a partir dos novos
     * vértices. Tem duas vantagens:
     *   - Mais barato (1 cross product por face vs. inversão da matriz).
     *   - Mais robusto contra escalas não-uniformes combinadas com rotações.
     */
    void applyTransform(const Matrix4x4& transformationMatrix) {
        for (Ponto& vertex : vertices) {
            vertex = transformationMatrix.transformPoint(vertex);
        }
        recomputeFaceNormalsFromCurrentVertices();
        recomputeVertexNormalsFromFaceNormals();
    }

    // ---------- getters usados pelo Intersect.h e pelo main.cpp ----------
    const std::vector<Ponto>&        getVertices()      const { return vertices; }
    const std::vector<TriangleFace>& getFaces()         const { return faces; }
    const std::vector<Vetor>&        getFaceNormals()   const { return faceNormals; }
    const std::vector<Vetor>&        getVertexNormals() const { return vertexNormals; }
    const MaterialData&              getMaterial()      const { return material; }

private:
    /**
     * Procura o índice de um vértice em `vertices` casando por coordenadas
     * exatas. Usado apenas durante a construção da malha.
     */
    int findVertexIndexByPosition(const Ponto& targetPoint) const {
        for (size_t candidateIndex = 0; candidateIndex < vertices.size(); ++candidateIndex) {
            const Ponto& candidate = vertices[candidateIndex];
            bool sameX = candidate.getX() == targetPoint.getX();
            bool sameY = candidate.getY() == targetPoint.getY();
            bool sameZ = candidate.getZ() == targetPoint.getZ();
            if (sameX && sameY && sameZ) return static_cast<int>(candidateIndex);
        }
        return -1;
    }

    /**
     * Calcula a normal de cada face pelo cross product das arestas.
     *
     * Para uma face ABC:
     *   primeiraAresta = B - A
     *   segundaAresta  = C - A
     *   normal = normalize(primeiraAresta × segundaAresta)
     *
     * A ordem dos vértices define o sentido da normal (regra da mão direita).
     * Convenção do .obj: anti-horário visto do lado de fora → normal aponta para fora.
     *
     * Triângulos degenerados (3 pontos colineares) têm cross zero. Salvamos
     * (0,0,0) — Möller-Trumbore vai descartar a face naturalmente no teste
     * "raio paralelo".
     */
    void recomputeFaceNormalsFromCurrentVertices() {
        faceNormals.clear();
        faceNormals.reserve(faces.size());
        for (const TriangleFace& face : faces) {
            const Ponto& vertexA = vertices[face.firstVertexIndex];
            const Ponto& vertexB = vertices[face.secondVertexIndex];
            const Ponto& vertexC = vertices[face.thirdVertexIndex];

            Vetor edgeFromAToB = vertexB - vertexA;
            Vetor edgeFromAToC = vertexC - vertexA;
            Vetor unnormalizedNormal = edgeFromAToB.cross(edgeFromAToC);

            bool faceIsDegenerate = unnormalizedNormal.magnitude() < 1e-12;
            if (faceIsDegenerate) {
                faceNormals.push_back(Vetor(0, 0, 0));
            } else {
                faceNormals.push_back(unnormalizedNormal.normalize());
            }
        }
    }

    /**
     * Calcula a normal de cada vértice como soma das normais das faces que
     * o contêm, depois normaliza. Útil para interpolar normais entre vértices
     * via coordenadas baricêntricas (suaviza superfícies aproximadas por triângulos).
     */
    void recomputeVertexNormalsFromFaceNormals() {
        vertexNormals.assign(vertices.size(), Vetor(0, 0, 0));
        for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
            const Vetor& currentFaceNormal = faceNormals[faceIndex];
            const TriangleFace& face = faces[faceIndex];

            vertexNormals[face.firstVertexIndex]  = vertexNormals[face.firstVertexIndex]  + currentFaceNormal;
            vertexNormals[face.secondVertexIndex] = vertexNormals[face.secondVertexIndex] + currentFaceNormal;
            vertexNormals[face.thirdVertexIndex]  = vertexNormals[face.thirdVertexIndex]  + currentFaceNormal;
        }
        for (Vetor& vertexNormal : vertexNormals) {
            vertexNormal = vertexNormal.normalize();
        }
    }

    std::vector<Ponto>        vertices;
    std::vector<TriangleFace> faces;
    std::vector<Vetor>        faceNormals;
    std::vector<Vetor>        vertexNormals;
    MaterialData              material;
};

#endif
