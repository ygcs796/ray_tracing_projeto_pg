#ifndef RAY_HEADER
#define RAY_HEADER

/**
 * Raio paramétrico P(t) = origin + t * direction.
 *
 * Convenção do projeto:
 *  - direction deve estar normalizada (unitário) para que t represente distância em unidades de mundo
 *  - t > 0 aponta para frente do raio; t < 0 é atrás da origem
 */

#include "Ponto.h"
#include "Vetor.h"

struct Ray {
    Ponto origin;
    Vetor direction;  // deve ser unitário

    Ray(const Ponto& o, const Vetor& d) : origin(o), direction(d) {}

    /**
     * Retorna o ponto P(t) sobre o raio a uma distância t da origem.
     * @param t parâmetro >= 0 (em unidades de mundo se direction é unitário)
     */
    Ponto at(double t) const {
        return origin + direction * t;
    }
};

#endif
