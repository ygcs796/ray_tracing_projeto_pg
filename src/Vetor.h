#ifndef VETORHEADER
#define VETORHEADER

/**
 * Vetor 3D em R³. Representa direção + magnitude (sem posição fixa).
 *
 * Separado da classe Ponto para impor semântica geométrica via tipos:
 *   Vetor + Vetor   → Vetor
 *   Vetor · Vetor   → número (dot)
 *   Vetor × Vetor   → Vetor (cross)
 *   Vetor * escalar → Vetor
 */

#include <iostream>
#include <cmath>
#include <stdexcept>

class Vetor {
public:
    Vetor(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

    /** Soma componente a componente. */
    Vetor operator+(const Vetor& v) const {
        return Vetor(x + v.x, y + v.y, z + v.z);
    }

    /** Subtração componente a componente. Não comutativa: a-b ≠ b-a. */
    Vetor operator-(const Vetor& v) const {
        return Vetor(x - v.x, y - v.y, z - v.z);
    }

    /** Negação: inverte o sentido, mantém direção e magnitude. */
    Vetor operator-() const {
        return Vetor(-x, -y, -z);
    }

    /** Escala o vetor mantendo a direção. */
    Vetor operator*(double e) const {
        return Vetor(x * e, y * e, z * e);
    }

    /** Divisão por escalar. Lança exceção se o escalar for zero. */
    Vetor operator/(double e) const {
        if (e == 0.0) {
            throw std::runtime_error("Divisão de Vetor por zero");
        }
        return Vetor(x / e, y / e, z / e);
    }

    /**
     * Produto escalar (dot). Retorna NÚMERO, não vetor.
     *
     * Interpretação: u · v = |u| |v| cos(θ).
     * Com vetores unitários, u · v = cos(θ) diretamente.
     *   > 0 → ângulo agudo; = 0 → perpendiculares; < 0 → ângulo obtuso.
     *
     * Usado em: interseção raio-esfera (3 dots para a quadrática),
     * interseção raio-plano (2 dots para o t).
     */
    double dot(const Vetor& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    /**
     * Produto vetorial (cross). Retorna vetor perpendicular a this e v.
     *
     * Anti-comutativo: a × b = -(b × a). Trocar a ordem inverte o sinal.
     * Se a e b são unitários e perpendiculares, o resultado já é unitário.
     *
     * Usado em: construção da base ortonormal da câmera (U = W × Vup; V = U × W).
     */
    Vetor cross(const Vetor& v) const {
        return Vetor(
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        );
    }

    /** Comprimento euclidiano: sqrt(x² + y² + z²). */
    double magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    /**
     * Versão unitária do vetor (mesma direção, magnitude 1).
     *
     * Necessário para que o parâmetro t de um raio represente distância em
     * unidades de mundo. Guarda contra vetor nulo: retorna (0,0,0) se
     * magnitude ≈ 0 (evita divisão por zero / NaN).
     */
    Vetor normalize() const {
        double m = magnitude();
        if (m < 1e-12) {
            return Vetor(0, 0, 0);
        }
        return Vetor(x / m, y / m, z / m);
    }

    friend std::ostream& operator<<(std::ostream& os, const Vetor& v) {
        return os << "(" << v.x << ", " << v.y << ", " << v.z << ")T";
    }

    double getX() const { return x; }
    double getY() const { return y; }
    double getZ() const { return z; }

    double x, y, z;
};

/** Permite sintaxe "escalar * Vetor" (além de "Vetor * escalar"). */
inline Vetor operator*(double e, const Vetor& v) {
    return v * e;
}

#endif
