#ifndef INTERSECT_HEADER
#define INTERSECT_HEADER

/**
 * Funções de interseção raio-objeto para a Entrega 1.
 *
 * Escopo: esferas e planos apenas. Malhas (mesh) são tratadas na Entrega 2.
 *
 * Convenção: retornam std::optional<double> contendo o parâmetro t > ε
 * do ponto de interseção mais próximo, ou std::nullopt se não há interseção válida.
 *
 * EPSILON = 1e-6 é usado para:
 *  - evitar retornar t ≈ 0 (interseção com o próprio ponto de origem)
 *  - detectar raios paralelos a planos (denominador ≈ 0)
 */

#include <optional>
#include <cmath>
#include "Ray.h"
#include "Ponto.h"
#include "Vetor.h"
#include "../utils/Scene/sceneSchema.hpp"

constexpr double INTERSECT_EPSILON = 1e-6;

/**
 * Interseção raio-esfera.
 *
 * Substitui P(t) = O + t*D na equação da esfera |P - C|² = r²,
 * obtendo uma equação quadrática em t:
 *
 *     a = D · D
 *     b = 2 D · (O - C)
 *     c = (O - C) · (O - C) - r²
 *     Δ = b² - 4ac
 *
 * Retorna o menor t > ε (a raiz negativa da fórmula de Bhaskara, se positiva).
 * Se ambas as raízes forem ≤ ε, retorna std::nullopt (esfera atrás da origem).
 *
 * @param ray    raio (origem + direção, direção idealmente unitária)
 * @param center centro da esfera
 * @param radius raio da esfera (> 0)
 * @return       menor t > ε se há interseção; std::nullopt caso contrário
 */
inline std::optional<double> intersectSphere(const Ray& ray,
                                             const Ponto& center,
                                             double radius) {
    // OC = vetor do centro da esfera até a origem do raio
    Vetor OC = ray.origin - center;

    double a = ray.direction.dot(ray.direction);
    double b = 2.0 * ray.direction.dot(OC);
    double c = OC.dot(OC) - radius * radius;

    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) {
        return std::nullopt;  // raio não toca a esfera
    }

    double sq = std::sqrt(disc);
    double t1 = (-b - sq) / (2.0 * a);  // raiz menor (interseção mais próxima, se positiva)
    double t2 = (-b + sq) / (2.0 * a);  // raiz maior (saída da esfera)

    if (t1 > INTERSECT_EPSILON) return t1;
    if (t2 > INTERSECT_EPSILON) return t2;  // origem dentro da esfera: retorna a saída
    return std::nullopt;
}

/**
 * Interseção raio-plano.
 *
 * Plano definido por um ponto Pp e um vetor normal N. Um ponto P está no plano
 * se (P - Pp) · N = 0. Substituindo P = O + t*D:
 *
 *     (O + tD - Pp) · N = 0
 *     t = ((Pp - O) · N) / (D · N)
 *
 * Se D · N ≈ 0 o raio é paralelo ao plano (sem interseção).
 * Se t ≤ ε o plano está atrás da origem (ou muito próximo).
 *
 * @param ray           raio
 * @param pointOnPlane  qualquer ponto do plano
 * @param normal        vetor normal do plano (não precisa ser unitário para o t)
 * @return              t > ε se há interseção; std::nullopt caso contrário
 */
inline std::optional<double> intersectPlane(const Ray& ray,
                                            const Ponto& pointOnPlane,
                                            const Vetor& normal) {
    double denom = normal.dot(ray.direction);
    if (std::abs(denom) < INTERSECT_EPSILON) {
        return std::nullopt;  // raio paralelo ao plano
    }

    Vetor diff = pointOnPlane - ray.origin;  // Ponto - Ponto → Vetor
    double t = normal.dot(diff) / denom;

    if (t > INTERSECT_EPSILON) return t;
    return std::nullopt;
}

/**
 * Dispatcher: despacha para a interseção apropriada conforme o tipo do objeto.
 *
 * Suporta apenas "sphere" e "plane" nesta entrega. Objetos de outros tipos
 * (por ex. "mesh") retornam std::nullopt — serão implementados na Entrega 2.
 *
 * @param ray raio a testar
 * @param obj objeto da cena (ObjectData)
 * @return    menor t > ε se há interseção; std::nullopt caso contrário
 */
inline std::optional<double> intersect(const Ray& ray, ObjectData& obj) {
    if (obj.objType == "sphere") {
        Ponto center = obj.getPonto("center");
        double radius = obj.getNum("radius");
        return intersectSphere(ray, center, radius);
    }
    if (obj.objType == "plane") {
        // relativePos é preenchido automaticamente pelo parser a partir de
        // point_on_plane / position / origin (ver isPositionKey em sceneParser.cpp).
        Ponto pointOnPlane = obj.relativePos;
        Vetor normal = obj.getVetor("normal");
        return intersectPlane(ray, pointOnPlane, normal);
    }
    // "mesh" e outros tipos ficam para a Entrega 2
    return std::nullopt;
}

#endif
