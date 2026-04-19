#ifndef RAY_HEADER
#define RAY_HEADER

/**
 * Raio paramétrico P(t) = origin + t * direction.
 *
 * Representa uma semi-reta que parte de `origin` na direção `direction`.
 * O parâmetro t > 0 "anda" ao longo do raio; t < 0 seria atrás da origem
 * (não usamos — raios não andam para trás).
 *
 * Convenção do projeto: `direction` deve estar normalizada. Com |D| = 1,
 * o valor de t representa distância em unidades de mundo, o que torna
 * comparações entre raios consistentes. O construtor NÃO normaliza
 * automaticamente (quem chama é responsável — ver Camera::generateRay).
 */

#include "Ponto.h"
#include "Vetor.h"

struct Ray {
    Ponto origin;
    Vetor direction;  // deve ser unitário

    Ray(const Ponto& o, const Vetor& d) : origin(o), direction(d) {}

    /** Retorna o ponto sobre o raio a distância t da origem. */
    Ponto at(double t) const {
        return origin + direction * t;
    }
};

#endif
