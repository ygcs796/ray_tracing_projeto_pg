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

constexpr double INTERSECT_EPSILON = 1e-6;

/**
 * Interseção raio-esfera.
 *
 * Substituir P(t) = O + t·D na equação da esfera |P - C|² = r² gera uma
 * equação quadrática em t:
 *     a·t² + b·t + c = 0
 *   onde:
 *     a = D · D
 *     b = 2(D · OC),   com OC = O - C
 *     c = OC·OC - r²
 *
 * O discriminante Δ = b² - 4ac indica:
 *     Δ < 0 → raio erra a esfera           → std::nullopt
 *     Δ = 0 → raio tangencia               → 1 solução (raiz dupla)
 *     Δ > 0 → raio atravessa               → 2 soluções (entrada e saída)
 *
 * Retornamos t1 (raiz menor) se > ε — o ponto de entrada. Se t1 ≤ ε mas
 * t2 > ε, usamos t2 (caso raro: origem dentro da esfera, raio sai pela
 * parede oposta).
 *
 * O teste `disc < 0` DEVE vir antes do sqrt: √(negativo) retorna NaN e
 * contamina todas as contas subsequentes.
 */
inline std::optional<double> intersectSphere(const Ray& ray,
                                             const Ponto& center,
                                             double radius) {
    Vetor OC = ray.origin - center;                // vetor de C até O

    double a = ray.direction.dot(ray.direction);   // a = D · D
    double b = 2.0 * ray.direction.dot(OC);        // b = 2(D · OC)
    double c = OC.dot(OC) - radius * radius;       // c = OC·OC - r²

    double disc = b * b - 4.0 * a * c;             // Δ = b² - 4ac
    if (disc < 0.0) {
        return std::nullopt;  // raio não toca a esfera
    }

    double sq = std::sqrt(disc);
    double t1 = (-b - sq) / (2.0 * a);  // raiz menor (entrada)
    double t2 = (-b + sq) / (2.0 * a);  // raiz maior (saída)

    if (t1 > INTERSECT_EPSILON) return t1;
    if (t2 > INTERSECT_EPSILON) return t2;  // origem dentro da esfera
    return std::nullopt;                    // esfera inteira atrás da origem
}

/**
 * Interseção raio-plano.
 *
 * Plano definido por um ponto Pp e normal N: um ponto P pertence ao plano
 * se (P - Pp) · N = 0. Substituindo P = O + t·D e isolando t:
 *
 *     t = ((Pp - O) · N) / (D · N)
 *
 * Equação linear (uma raiz; mais simples e rápida que a esfera — sem sqrt).
 *
 * Casos especiais:
 *   D · N ≈ 0 → D perpendicular a N → raio paralelo ao plano → std::nullopt.
 *   t ≤ ε    → plano atrás da origem → std::nullopt.
 *
 * A normal N não precisa ser unitária: aparece em cima e embaixo da divisão,
 * um fator de escala se cancela e o t final é o mesmo.
 */
inline std::optional<double> intersectPlane(const Ray& ray,
                                            const Ponto& pointOnPlane,
                                            const Vetor& normal) {
    double denom = normal.dot(ray.direction);  // D · N

    // std::abs: denom pode ser negativo; queremos "próximo de zero" em ambos sinais.
    if (std::abs(denom) < INTERSECT_EPSILON) {
        return std::nullopt;  // raio paralelo ao plano
    }

    Vetor diff = pointOnPlane - ray.origin;    // Pp - O
    double t = normal.dot(diff) / denom;       // t = (N · (Pp - O)) / (N · D)

    if (t > INTERSECT_EPSILON) return t;
    return std::nullopt;                       // plano atrás da origem
}

/**
 * Dispatcher: chama a função de interseção apropriada pelo tipo do objeto.
 * Tipos suportados: "sphere" e "plane". Outros tipos retornam std::nullopt.
 */
inline std::optional<double> intersect(const Ray& ray, ObjectData& obj) {
    if (obj.objType == "sphere") {
        Ponto center = obj.getPonto("center");
        double radius = obj.getNum("radius");
        return intersectSphere(ray, center, radius);
    }
    if (obj.objType == "plane") {
        // relativePos é preenchido pelo parser a partir de
        // point_on_plane / position / origin (ver isPositionKey em sceneParser.cpp).
        Ponto pointOnPlane = obj.relativePos;
        Vetor normal = obj.getVetor("normal");
        return intersectPlane(ray, pointOnPlane, normal);
    }
    return std::nullopt;
}

#endif
