#ifndef PONTOHEADER
#define PONTOHEADER

/**
 * Ponto 3D em R³.
 *
 * Distinção semântica: Ponto representa uma posição no espaço;
 * Vetor representa uma direção/deslocamento. Operações que
 * misturam os dois preservam essa semântica:
 *     Ponto + Vetor  → Ponto  (translada o ponto)
 *     Ponto - Vetor  → Ponto  (translada no sentido oposto)
 *     Ponto - Ponto  → Vetor  (deslocamento entre pontos)
 */

#include <iostream>
#include "Vetor.h"

class Ponto {
public:
    Ponto(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

    /** Ponto + Vetor → Ponto (translação). */
    Ponto operator+(const Vetor& v) const {
        return Ponto(x + v.getX(), y + v.getY(), z + v.getZ());
    }

    /** Ponto - Vetor → Ponto (translação no sentido oposto). */
    Ponto operator-(const Vetor& v) const {
        return Ponto(x - v.getX(), y - v.getY(), z - v.getZ());
    }

    /** Ponto - Ponto → Vetor (deslocamento do segundo até o primeiro). */
    Vetor operator-(const Ponto& a) const {
        return Vetor(x - a.x, y - a.y, z - a.z);
    }

    friend std::ostream& operator<<(std::ostream& os, const Ponto& p) {
        return os << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    }

    double getX() const { return x; }
    double getY() const { return y; }
    double getZ() const { return z; }

    // Campos públicos para acesso direto por classes colaboradoras
    double x, y, z;
};

#endif
