#ifndef VETORHEADER
#define VETORHEADER

/**
 * Vetor 3D em R³ com operações de álgebra linear.
 *
 * Usa double para todas as coordenadas para manter coerência numérica no projeto.
 * Esta classe NÃO depende de Ponto (Ponto.h é que inclui Vetor.h).
 */

#include <iostream>
#include <cmath>
#include <stdexcept>

class Vetor {
public:
    Vetor(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

    // ---------- Operadores aritméticos ----------

    /** Vetor + Vetor (soma componente a componente). */
    Vetor operator+(const Vetor& v) const {
        return Vetor(x + v.x, y + v.y, z + v.z);
    }

    /** Vetor - Vetor (subtração componente a componente). */
    Vetor operator-(const Vetor& v) const {
        return Vetor(x - v.x, y - v.y, z - v.z);
    }

    /** -Vetor (negação unária). */
    Vetor operator-() const {
        return Vetor(-x, -y, -z);
    }

    /** Vetor * escalar (multiplicação por escalar). */
    Vetor operator*(double e) const {
        return Vetor(x * e, y * e, z * e);
    }

    /** Vetor / escalar. Lança exceção se o escalar for zero. */
    Vetor operator/(double e) const {
        if (e == 0.0) {
            throw std::runtime_error("Divisão de Vetor por zero");
        }
        return Vetor(x / e, y / e, z / e);
    }

    // ---------- Produtos ----------

    /**
     * Produto escalar (dot product).
     * Representa o cosseno do ângulo entre os vetores multiplicado pelas magnitudes:
     *     u · v = |u| |v| cos θ
     *
     * @param v vetor à direita
     * @return  resultado escalar
     */
    double dot(const Vetor& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    /**
     * Produto vetorial (cross product).
     * Retorna um vetor perpendicular ao plano formado por this e v, seguindo a regra da mão direita.
     *
     *     u × v = ( u.y*v.z - u.z*v.y,
     *              u.z*v.x - u.x*v.z,
     *              u.x*v.y - u.y*v.x )
     *
     * @param v vetor à direita
     * @return  vetor perpendicular
     */
    Vetor cross(const Vetor& v) const {
        return Vetor(
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        );
    }

    // ---------- Magnitude e normalização ----------

    /** Comprimento euclidiano: sqrt(x² + y² + z²). */
    double magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    /**
     * Retorna uma versão unitária do vetor (mesma direção, magnitude 1).
     * Guarda contra vetor nulo: retorna Vetor(0,0,0) se magnitude ≈ 0.
     */
    Vetor normalize() const {
        double m = magnitude();
        if (m < 1e-12) {
            return Vetor(0, 0, 0);
        }
        return Vetor(x / m, y / m, z / m);
    }

    // ---------- Impressão ----------

    friend std::ostream& operator<<(std::ostream& os, const Vetor& v) {
        return os << "(" << v.x << ", " << v.y << ", " << v.z << ")T";
    }

    // ---------- Getters ----------

    double getX() const { return x; }
    double getY() const { return y; }
    double getZ() const { return z; }

    // Campos públicos para acesso direto por classes colaboradoras (Ponto, Camera, etc.)
    double x, y, z;
};

/**
 * Permite a sintaxe "escalar * Vetor" (além de "Vetor * escalar").
 * Delega para o operador* da classe.
 */
inline Vetor operator*(double e, const Vetor& v) {
    return v * e;
}

#endif
