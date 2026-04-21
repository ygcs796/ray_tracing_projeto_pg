#ifndef PONTOHEADER
#define PONTOHEADER

/**
 * Ponto 3D em R³. Representa uma posição no espaço.
 *
 * Separado de Vetor para impor semântica geométrica via tipos:
 *   Ponto + Vetor → Ponto  (translação)
 *   Ponto - Vetor → Ponto  (translação oposta)
 *   Ponto - Ponto → Vetor  (deslocamento do segundo até o primeiro)
 *   Ponto + Ponto → não existe (sem sentido geométrico; o compilador rejeita)
 */

#include <iostream>
#include "Vetor.h"

class Ponto {
public:
    Ponto(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

    /** Translação: parte de P, anda na direção v, chega numa nova posição. */
    Ponto operator+(const Vetor& v) const {
        return Ponto(x + v.getX(), y + v.getY(), z + v.getZ());
    }

    /** Translação no sentido oposto. Usado, por exemplo, em screen_center = C - d·W. */
    Ponto operator-(const Vetor& v) const {
        return Ponto(x - v.getX(), y - v.getY(), z - v.getZ());
    }

    /**
     * Deslocamento entre pontos: vetor que sai de `a` e chega em `this`.
     * Usos: W = (C - lookat).normalize(); OC = ray.origin - sphere.center.
     */
    Vetor operator-(const Ponto& a) const {
        return Vetor(x - a.x, y - a.y, z - a.z);
    }

    friend std::ostream& operator<<(std::ostream& os, const Ponto& p) {
        return os << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    }

    double getX() const { return x; }
    double getY() const { return y; }
    double getZ() const { return z; }

    double x, y, z;
};

#endif
