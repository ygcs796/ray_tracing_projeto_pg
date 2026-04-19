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
#include "Ray.h"
#include "Ponto.h"
#include "Vetor.h"
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

    if (intersectionParameter > INTERSECT_EPSILON) return intersectionParameter;
    return std::nullopt;  // plano atrás da origem
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
 * Dispatcher: chama a função de interseção apropriada pelo tipo do objeto.
 * Tipos suportados: "sphere" e "plane". Outros tipos retornam std::nullopt.
 */
inline std::optional<double> intersect(const Ray& ray, ObjectData& obj) {
    if (obj.objType == "sphere") return intersectSphereObject(ray, obj);
    if (obj.objType == "plane")  return intersectPlaneObject(ray, obj);
    return std::nullopt;
}

#endif
