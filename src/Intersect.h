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

// O equivalente a "> 0", mas tem cenários que é necessário por conta da precisão do double
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
// RAIO-TRIÂNGULO (Möller–Trumbore)
// ============================================================================

/**
 * Interseção raio-triângulo via algoritmo de Möller-Trumbore.
 *
 * Resolve simultaneamente t e as coordenadas baricêntricas (β, γ) sem montar
 * sistema linear 3x3 explícito. Os vetores h, q reusam parcelas comuns para
 * substituir a eliminação gaussiana por dot/cross products diretos.
 *
 * Equação base: O + t·D = (1-β-γ)·v0 + β·v1 + γ·v2
 *               D·t - (v1-v0)·β - (v2-v0)·γ = O - v0
 *
 * Variáveis intermediárias:
 *   edge1 = v1 - v0
 *   edge2 = v2 - v0
 *   h     = D × edge2
 *   a     = edge1 · h          (det da matriz 3x3 do sistema; |a| ≈ 0 ⇒ paralelo)
 *   s     = O - v0
 *   β     = (s · h) / a
 *   q     = s × edge1
 *   γ     = (D · q) / a
 *   t     = (edge2 · q) / a
 *
 * Retorna (t, α, β, γ) com α = 1-β-γ (todas em [0,1] num hit). Para
 * retornar apenas t (sem baricêntricas), use intersectTriangleT().
 */
struct TriangleHit {
    double t;
    double alpha, beta, gamma;
};

inline std::optional<TriangleHit> intersectTriangle(const Ray& ray,
                                                    const Ponto& v0,
                                                    const Ponto& v1,
                                                    const Ponto& v2) {
    Vetor edge1 = v1 - v0;
    Vetor edge2 = v2 - v0;
    Vetor h     = ray.direction.cross(edge2);
    double a    = edge1.dot(h);

    // Raio paralelo ao plano do triângulo: a ≈ 0 → divisão instável.
    if (std::abs(a) < INTERSECT_EPSILON) return std::nullopt;

    double f    = 1.0 / a;
    Vetor s     = ray.origin - v0;
    double beta = f * s.dot(h);
    if (beta < 0.0 || beta > 1.0) return std::nullopt;

    Vetor q     = s.cross(edge1);
    double gamma = f * ray.direction.dot(q);
    if (gamma < 0.0 || beta + gamma > 1.0) return std::nullopt;

    double t = f * edge2.dot(q);
    if (t <= INTERSECT_EPSILON) return std::nullopt;  // atrás da origem ou auto-interseção

    return TriangleHit{ t, 1.0 - beta - gamma, beta, gamma };
}

// ============================================================================
// RAIO-MALHA
// ============================================================================

/**
 * Resultado de interseção contra uma malha: t do triângulo mais próximo + índice
 * da face. A face permite, mais à frente, recuperar a normal correta na fase de
 * sombreamento (Phong, Entrega 3+).
 */
struct MeshHit {
    double t;
    int    faceIndex;
};

/**
 * Testa o raio contra todas as faces da malha e retorna a interseção mais próxima.
 * Implementação O(N) por raio — sem estruturas aceleradoras (BVH/Octree). Para
 * malhas pequenas (~12 faces, cubo) é instantâneo; para o monkey (~1000 faces)
 * é o gargalo, ver renders/ para tempos.
 */
inline std::optional<MeshHit> intersectMesh(const Ray& ray, const TriangleMesh& mesh) {
    std::optional<MeshHit> closest;
    double closestT = std::numeric_limits<double>::infinity();

    const auto& vs = mesh.getVertices();
    const auto& fs = mesh.getFaces();

    for (size_t i = 0; i < fs.size(); ++i) {
        const TriangleFace& f = fs[i];
        auto hit = intersectTriangle(ray, vs[f.v0], vs[f.v1], vs[f.v2]);
        if (hit.has_value() && hit->t < closestT) {
            closestT = hit->t;
            closest = MeshHit{ hit->t, static_cast<int>(i) };
        }
    }
    return closest;
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
 * Mapa de malhas pré-carregadas, indexado por endereço do ObjectData.
 *
 * Por que ponteiro como chave: ObjectData não tem um identificador único; usar o
 * endereço do objeto na lista da cena dá uma chave estável durante a vida do
 * processo (a lista de objetos não é redimensionada após carregar a cena).
 */
using MeshLookup = std::map<const ObjectData*, const TriangleMesh*>;

/** Recupera a malha pré-carregada associada a `obj` e delega para intersectMesh. */
inline std::optional<double> intersectMeshObject(const Ray& ray,
                                                 const ObjectData& obj,
                                                 const MeshLookup& meshes) {
    auto it = meshes.find(&obj);
    if (it == meshes.end()) return std::nullopt;
    auto hit = intersectMesh(ray, *it->second);
    if (!hit.has_value()) return std::nullopt;
    return hit->t;
}

/**
 * Dispatcher: chama a função de interseção apropriada pelo tipo do objeto.
 * Tipos suportados: "sphere", "plane", "mesh". Outros tipos retornam std::nullopt.
 *
 * `meshes` é o lookup de malhas pré-carregadas. Para esferas/planos é ignorado
 * (passar mapa vazio funciona). Para "mesh", uma entrada faltando devolve nullopt.
 */
inline std::optional<double> intersect(const Ray& ray, ObjectData& obj,
                                       const MeshLookup& meshes) {
    if (obj.objType == "sphere") return intersectSphereObject(ray, obj);
    if (obj.objType == "plane")  return intersectPlaneObject(ray, obj);
    if (obj.objType == "mesh")   return intersectMeshObject(ray, obj, meshes);
    return std::nullopt;
}

#endif
