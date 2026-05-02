#ifndef INTERSECT_HEADER
#define INTERSECT_HEADER

/**
 * Funções de interseção raio-objeto. Suporta esferas e planos.
 *
 * Convenção: retornam std::optional<double> com o parâmetro t > ε do ponto
 * de interseção mais próximo, ou std::nullopt se não há interseção válida.
 *
 * std::optional representa explicitamente "pode haver ou não haver valor".
 *
 * EPSILON = 1e-6 é usado para:
 *   - Rejeitar t ≈ 0 (evita auto-interseção por erro numérico na origem do raio).
 *   - Detectar raios paralelos a planos (denominador D·N ≈ 0, divisão instável).
 */

#include <optional>
#include <cmath>
#include <map>
#include <limits>
#include "Ray.h"
#include "Ponto.h"
#include "Vetor.h"
#include "Mesh.h"
#include "../utils/Scene/sceneSchema.hpp"

// Um número extremamente pequeno
// O equivalente a falarmos "> 0", mas tem cenários que é necessário por conta da precisão do double
constexpr double INTERSECT_EPSILON = 1e-6; 

// ============================================================================
// RAIO-ESFERA
// ============================================================================

/**
 * Coeficientes da equação quadrática a·t² + b·t + c = 0 obtida ao substituir
 * P(t) = O + t·D na equação da esfera |P - C|² = r².
 */
struct SphereQuadratic {
    double aCoefficient;   // a = D · D
    double bCoefficient;   // b = 2 (D · OC), com OC = O - C
    double cCoefficient;   // c = OC · OC - r²
};

/**
 * Monta os coeficientes a, b, c da quadrática para uma interseção raio-esfera.
 *
 * A quadrática sai de substituir o raio P(t) = O + t·D na equação da esfera
 * |P - C|² = r² e expandir o dot product. O vetor `originRelativeToCenter`
 * (OC = O - C) aparece em todos os três coeficientes e por isso é pré-calculado.
 *
 * @param ray           raio sendo testado
 * @param sphereCenter  centro da esfera (C)
 * @param sphereRadius  raio da esfera (r)
 * @return              coeficientes (a, b, c) prontos para Bhaskara
 */
inline SphereQuadratic buildSphereQuadratic(const Ray& ray,
                                            const Ponto& sphereCenter,
                                            double sphereRadius) {
    Vetor originRelativeToCenter = ray.origin - sphereCenter;  // OC = O - C

    return SphereQuadratic {
        ray.direction.dot(ray.direction),                                           // a = D · D
        2.0 * ray.direction.dot(originRelativeToCenter),                            // b = 2(D · OC)
        originRelativeToCenter.dot(originRelativeToCenter) - sphereRadius * sphereRadius  // c = OC·OC - r²
    };
}

/** Discriminante Δ = b² - 4ac da quadrática. */
inline double sphereDiscriminant(const SphereQuadratic& quadratic) {
    return quadratic.bCoefficient * quadratic.bCoefficient
         - 4.0 * quadratic.aCoefficient * quadratic.cCoefficient;
}

/**
 * Escolhe a menor raiz positiva (> ε) da quadrática.
 * - Primeiro tenta a raiz de entrada (nearRoot). Se > ε, é o ponto de entrada.
 * - Se nearRoot ≤ ε mas farRoot > ε, a origem está dentro da esfera e retornamos
 *   a raiz de saída (farRoot).
 * - Caso contrário, ambas as raízes estão atrás da origem → std::nullopt.
 *
 * Assume discriminante ≥ 0 (deve ser verificado antes).
 */
inline std::optional<double> pickNearestPositiveRoot(const SphereQuadratic& quadratic,
                                                     double discriminant) {
    double sqrtDiscriminant = std::sqrt(discriminant);
    double twiceA = 2.0 * quadratic.aCoefficient;

    double nearRoot = (-quadratic.bCoefficient - sqrtDiscriminant) / twiceA;
    double farRoot  = (-quadratic.bCoefficient + sqrtDiscriminant) / twiceA;

    // tMin ( tMax está na função findClosestHit - o closest_t funciona como tMax dinâmico )
    // Otimização pro futuro, receber o tMax aqui e fazer early exit se a raiz for maior que tMax (não é o mais próximo possível).
    if (nearRoot > INTERSECT_EPSILON) return nearRoot;  // ponto de entrada
    if (farRoot  > INTERSECT_EPSILON) return farRoot;   // origem dentro da esfera
    return std::nullopt;                                // esfera atrás da origem
}

/**
 * Interseção raio-esfera.
 *
 * Substituir P(t) = O + t·D na equação |P - C|² = r² gera uma quadrática em t.
 * O discriminante Δ decide o caso geométrico:
 *     Δ < 0 → raio erra a esfera           → std::nullopt
 *     Δ = 0 → raio tangencia               → 1 solução
 *     Δ > 0 → raio atravessa               → 2 soluções (entrada e saída)
 *
 * √(negativo) retorna NaN — o teste de Δ < 0 DEVE vir antes do sqrt.
 */
inline std::optional<double> intersectSphere(const Ray& ray,
                                             const Ponto& sphereCenter,
                                             double sphereRadius) {
    SphereQuadratic quadratic = buildSphereQuadratic(ray, sphereCenter, sphereRadius);
    double discriminant = sphereDiscriminant(quadratic);

    if (discriminant < 0.0) {
        return std::nullopt;  // raio não toca a esfera
    }
    
    return pickNearestPositiveRoot(quadratic, discriminant);
}

// ============================================================================
// RAIO-PLANO
// ============================================================================

/**
 * Resolve t na equação linear obtida ao substituir P(t) = O + t·D em
 * (P - Pp) · N = 0:
 *     t = ((Pp - O) · N) / (D · N)
 *
 * Pré-requisito: o chamador já verificou que o raio NÃO é paralelo ao plano
 * (denominador D·N ≠ 0). Passar um denominador ≈ 0 causa divisão instável.
 */
inline double solvePlaneParameter(const Ray& ray,
                                  const Ponto& pointOnPlane,
                                  const Vetor& planeNormal,
                                  double denominator) {
    Vetor originToPlanePoint = pointOnPlane - ray.origin;
    return planeNormal.dot(originToPlanePoint) / denominator;
}

/**
 * Interseção raio-plano.
 * (P - Pp) · N = 0
 *
 * Casos especiais:
 *   D · N ≈ 0 → raio paralelo ao plano → std::nullopt.
 *   t ≤ ε    → plano atrás da origem  → std::nullopt.
 *
 * A normal não precisa ser unitária: um fator de escala se cancela na divisão.
 */
inline std::optional<double> intersectPlane(const Ray& ray,
                                            const Ponto& pointOnPlane,
                                            const Vetor& planeNormal) {
    double denominator = planeNormal.dot(ray.direction);  // D · N

    // std::abs: denominator pode ser negativo; queremos "próximo de zero" em ambos sinais.
    if (std::abs(denominator) < INTERSECT_EPSILON) {
        return std::nullopt;  // raio paralelo ao plano
    }

    double intersectionParameter = solvePlaneParameter(ray, pointOnPlane, planeNormal, denominator);

    // tMin
    if (intersectionParameter > INTERSECT_EPSILON) return intersectionParameter;
    return std::nullopt;  // plano atrás da origem
}

// ============================================================================
// RAIO-TRIÂNGULO  (algoritmo de Möller–Trumbore)
// ============================================================================

/**
 * Resultado de uma interseção raio-triângulo bem-sucedida:
 *   - distanceAlongRay: parâmetro t > 0 do raio (distância em unidades de mundo,
 *                       já que assumimos a direção do raio normalizada).
 *   - alpha, beta, gamma: coordenadas baricêntricas do ponto de interseção em
 *                         relação aos 3 vértices do triângulo.
 *                         alpha + beta + gamma = 1 e cada uma >= 0 dentro do triângulo.
 *                         Úteis para interpolar atributos (cor, normal) entre os
 *                         vértices da face.
 */
struct TriangleHit {
    double distanceAlongRay;
    double alpha;
    double beta;
    double gamma;
};

/**
 * Interseção raio-triângulo pelo algoritmo de Möller-Trumbore.
 *
 * Por que Möller-Trumbore em vez de eliminação gaussiana?
 *   O problema "raio P(t) = O + tD intercepta o triângulo (v0,v1,v2)?" pode ser
 *   escrito como sistema linear 3x3 nas incógnitas (t, β, γ). Eliminação gaussiana
 *   resolveria, mas tem custo fixo (~17 operações). Möller-Trumbore explora a
 *   estrutura específica do sistema (regra de Cramer + identidades de cross product)
 *   para resolver com menos contas e — mais importante — permitir EARLY EXITS:
 *   se o raio é paralelo, descartamos antes de calcular β e γ; se β já saiu fora
 *   de [0,1], descartamos antes de calcular γ e t.
 *
 * Sequência geométrica:
 *   firstEdge       = v1 - v0
 *   secondEdge      = v2 - v0
 *   crossDirEdge2   = D × secondEdge
 *   determinant     = firstEdge · crossDirEdge2     (det do sistema; ≈0 ⇒ paralelo)
 *   originMinusV0   = O - v0
 *   beta            = (originMinusV0 · crossDirEdge2) / determinant
 *   crossSrcEdge1   = originMinusV0 × firstEdge
 *   gamma           = (D · crossSrcEdge1) / determinant
 *   t               = (secondEdge · crossSrcEdge1) / determinant
 *   alpha           = 1 - beta - gamma
 *
 * Cada early exit corresponde a um caso geométrico:
 *   |determinant| < ε  → raio paralelo ao plano do triângulo
 *   beta < 0 ou > 1    → ponto de interseção fora da aresta v0-v1
 *   gamma < 0 ou       → ponto fora da aresta v0-v2 ou da aresta v1-v2
 *     beta+gamma > 1
 *   t <= ε             → triângulo atrás da origem (ou auto-interseção numérica)
 */
inline std::optional<TriangleHit> intersectTriangle(const Ray& ray,
                                                    const Ponto& vertexV0,
                                                    const Ponto& vertexV1,
                                                    const Ponto& vertexV2) {
    Vetor firstEdgeFromV0  = vertexV1 - vertexV0;
    Vetor secondEdgeFromV0 = vertexV2 - vertexV0;

    // Determinante do sistema 3x3 implícito. |determinante| ≈ 0 indica que o raio
    // é paralelo ao plano do triângulo (o sistema é singular).
    Vetor crossOfDirAndSecondEdge = ray.direction.cross(secondEdgeFromV0);
    double determinant            = firstEdgeFromV0.dot(crossOfDirAndSecondEdge);
    bool rayIsParallelToTriangle  = std::abs(determinant) < INTERSECT_EPSILON;
    if (rayIsParallelToTriangle) return std::nullopt;

    double inverseDeterminant = 1.0 / determinant;

    // Coordenada baricêntrica beta. Geometricamente, é o "peso" do vértice v1 na
    // combinação linear que reconstrói o ponto de interseção.
    Vetor originMinusV0 = ray.origin - vertexV0;
    double beta         = inverseDeterminant * originMinusV0.dot(crossOfDirAndSecondEdge);
    bool betaIsOutsideTriangle = (beta < 0.0) || (beta > 1.0);
    if (betaIsOutsideTriangle) return std::nullopt;

    // Coordenada baricêntrica gamma. Peso do vértice v2 na combinação linear.
    Vetor crossOfOriginAndFirstEdge = originMinusV0.cross(firstEdgeFromV0);
    double gamma                    = inverseDeterminant * ray.direction.dot(crossOfOriginAndFirstEdge);
    bool gammaIsOutsideTriangle     = (gamma < 0.0) || (beta + gamma > 1.0);
    if (gammaIsOutsideTriangle) return std::nullopt;

    // Distância t ao longo do raio até o ponto de interseção.
    double distanceAlongRay = inverseDeterminant * secondEdgeFromV0.dot(crossOfOriginAndFirstEdge);
    bool triangleIsBehindRay = distanceAlongRay <= INTERSECT_EPSILON;
    if (triangleIsBehindRay) return std::nullopt;

    double alpha = 1.0 - beta - gamma;
    return TriangleHit{ distanceAlongRay, alpha, beta, gamma };
}

// ============================================================================
// RAIO-MALHA  (loop sobre todos os triângulos da malha)
// ============================================================================

/**
 * Resultado de interseção contra uma malha:
 *   - distanceAlongRay: parâmetro t do triângulo MAIS PRÓXIMO atingido.
 *   - hitFaceIndex:     índice do triângulo na malha (permite recuperar a
 *                       normal da face ou interpolar normais via baricêntricas).
 */
struct MeshHit {
    double distanceAlongRay;
    int    hitFaceIndex;
};

/**
 * Testa o raio contra TODAS as faces da malha (algoritmo "brute force" O(F))
 * e devolve a interseção mais próxima.
 */
inline std::optional<MeshHit> intersectMesh(const Ray& ray, const TriangleMesh& mesh) {
    std::optional<MeshHit> closestHit;
    double closestDistanceFound = std::numeric_limits<double>::infinity();

    const std::vector<Ponto>&        meshVertices = mesh.getVertices();
    const std::vector<TriangleFace>& meshFaces    = mesh.getFaces();

    for (size_t faceIndex = 0; faceIndex < meshFaces.size(); ++faceIndex) {
        const TriangleFace& currentFace = meshFaces[faceIndex];
        const Ponto& vertexV0 = meshVertices[currentFace.firstVertexIndex];
        const Ponto& vertexV1 = meshVertices[currentFace.secondVertexIndex];
        const Ponto& vertexV2 = meshVertices[currentFace.thirdVertexIndex];

        std::optional<TriangleHit> currentHit = intersectTriangle(ray, vertexV0, vertexV1, vertexV2);

        bool foundCloserHit = currentHit.has_value() &&
                              currentHit->distanceAlongRay < closestDistanceFound;
        if (foundCloserHit) {
            closestDistanceFound = currentHit->distanceAlongRay;
            closestHit = MeshHit{ currentHit->distanceAlongRay, static_cast<int>(faceIndex) };
        }
    }
    return closestHit;
}

// ============================================================================
// DISPATCHER
// ============================================================================

/** Extrai os parâmetros da esfera e delega para intersectSphere. */
inline std::optional<double> intersectSphereObject(const Ray& ray, ObjectData& obj) {
    Ponto  sphereCenter = obj.getPonto("center");
    double sphereRadius = obj.getNum("radius");
    return intersectSphere(ray, sphereCenter, sphereRadius);
}

/** Extrai os parâmetros do plano e delega para intersectPlane. */
inline std::optional<double> intersectPlaneObject(const Ray& ray, ObjectData& obj) {
    // relativePos é preenchido pelo parser a partir de
    // point_on_plane / position / origin (ver isPositionKey em sceneParser.cpp).
    Ponto pointOnPlane = obj.relativePos;
    Vetor planeNormal  = obj.getVetor("normal");
    return intersectPlane(ray, pointOnPlane, planeNormal);
}

/**
 * Mapa de malhas pré-carregadas, indexado pelo ENDEREÇO do ObjectData.
 *
 * Por que indexar por ponteiro? Um `ObjectData` não tem campo de ID único;
 * usar o endereço do objeto na lista da cena dá uma chave estável durante a
 * vida do programa (a lista de objetos não é redimensionada após o load).
 *
 * As malhas em si vivem em outro container (com std::unique_ptr) — este mapa
 * armazena apenas ponteiros não-donos para consulta rápida.
 */
using MeshLookup = std::map<const ObjectData*, const TriangleMesh*>;

/**
 * Localiza a malha pré-carregada que pertence ao `objectData` e delega a
 * interseção para `intersectMesh`. Se o objeto não tem malha registrada
 * (ex: o .obj não foi encontrado em disco), devolve nullopt.
 */
inline std::optional<double> intersectMeshObject(const Ray& ray,
                                                 const ObjectData& objectData,
                                                 const MeshLookup& preloadedMeshes) {
    auto meshLookupIterator = preloadedMeshes.find(&objectData);
    bool meshNotPreloadedForThisObject = meshLookupIterator == preloadedMeshes.end();
    if (meshNotPreloadedForThisObject) return std::nullopt;

    const TriangleMesh& meshForThisObject = *meshLookupIterator->second;
    std::optional<MeshHit> meshHit = intersectMesh(ray, meshForThisObject);
    if (!meshHit.has_value()) return std::nullopt;
    return meshHit->distanceAlongRay;
}

/**
 * Dispatcher central: olha o `objType` do objeto e chama a função de interseção
 * apropriada. Tipos suportados: "sphere", "plane", "mesh". Outros tipos
 * (ex: "cone" no futuro) caem no fallback `nullopt`.
 *
 * O parâmetro `preloadedMeshes` é ignorado para esferas/planos (passar mapa
 * vazio funciona). Para malhas, o mapa precisa conter uma entrada para o objeto
 * — caso contrário retornamos nullopt sem erro fatal.
 */
inline std::optional<double> intersect(const Ray& ray,
                                       ObjectData& objectFromScene,
                                       const MeshLookup& preloadedMeshes) {
    if (objectFromScene.objType == "sphere") return intersectSphereObject(ray, objectFromScene);
    if (objectFromScene.objType == "plane")  return intersectPlaneObject(ray, objectFromScene);
    if (objectFromScene.objType == "mesh")   return intersectMeshObject(ray, objectFromScene, preloadedMeshes);
    return std::nullopt;
}

#endif
