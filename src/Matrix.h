#ifndef MATRIX_HEADER
#define MATRIX_HEADER

/**
 * Matriz homogênea 4x4 para transformações afins (translação, escala, rotação)
 * em coordenadas de pontos, vetores e normais em R³.
 *
 * Convenções:
 *   - Coordenadas homogêneas: ponto = (x, y, z, 1), vetor direção = (x, y, z, 0).
 *   - Multiplicação coluna-vetor:  P' = M · P.
 *   - Composição da esquerda para a direita "do mais externo para o mais interno":
 *       (A · B) · v = A · (B · v)  → B é aplicada primeiro, depois A.
 *
 * Por que normais usam (M^-1)^T:
 *   Uma normal é definida por  N · v = 0  para todo vetor v tangente à superfície.
 *   Se v transforma como v' = M v, queremos N' tal que N' · v' = 0.
 *   Isso vale com  N' = (M^-1)^T · N. Usar M direta deformaria a normal sob
 *   escalas não-uniformes (ex: scaling(2,1,1) inclinaria as normais).
 */

#include <cmath>
#include <stdexcept>
#include "Ponto.h"
#include "Vetor.h"

class Matrix4x4 {
public:
    /** Constrói matriz a partir de array 4x4 já preenchido. */
    Matrix4x4() {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                m[i][j] = 0.0;
    }

    /** Acesso direto às entradas (linha, coluna). */
    double& at(int row, int col) { return m[row][col]; }
    double  at(int row, int col) const { return m[row][col]; }

    // ========================================================================
    // CONSTRUTORES (factory methods)
    // ========================================================================

    /** Identidade: I · v = v. */
    static Matrix4x4 identity() {
        Matrix4x4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0;
        return r;
    }

    /**
     * Translação por (tx, ty, tz). Move pontos; vetores direção (w=0) ficam intocados.
     *   | 1  0  0  tx |
     *   | 0  1  0  ty |
     *   | 0  0  1  tz |
     *   | 0  0  0  1  |
     */
    static Matrix4x4 translation(double tx, double ty, double tz) {
        Matrix4x4 r = identity();
        r.m[0][3] = tx;
        r.m[1][3] = ty;
        r.m[2][3] = tz;
        return r;
    }

    /**
     * Escala não-uniforme em torno da origem.
     *   | sx  0   0   0 |
     *   | 0   sy  0   0 |
     *   | 0   0   sz  0 |
     *   | 0   0   0   1 |
     * Para escalar em torno de um ponto P arbitrário: T(P) · S · T(-P).
     */
    static Matrix4x4 scaling(double sx, double sy, double sz) {
        Matrix4x4 r;
        r.m[0][0] = sx;
        r.m[1][1] = sy;
        r.m[2][2] = sz;
        r.m[3][3] = 1.0;
        return r;
    }

    /**
     * Rotação em torno do eixo X (sentido anti-horário olhando do +X para a origem).
     *   | 1    0       0    0 |
     *   | 0  cosθ   -sinθ   0 |
     *   | 0  sinθ    cosθ   0 |
     *   | 0    0       0    1 |
     */
    static Matrix4x4 rotationX(double angleRad) {
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        Matrix4x4 r = identity();
        r.m[1][1] =  c;  r.m[1][2] = -s;
        r.m[2][1] =  s;  r.m[2][2] =  c;
        return r;
    }

    /** Rotação em torno do eixo Y. */
    static Matrix4x4 rotationY(double angleRad) {
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        Matrix4x4 r = identity();
        r.m[0][0] =  c;  r.m[0][2] =  s;
        r.m[2][0] = -s;  r.m[2][2] =  c;
        return r;
    }

    /** Rotação em torno do eixo Z. */
    static Matrix4x4 rotationZ(double angleRad) {
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        Matrix4x4 r = identity();
        r.m[0][0] =  c;  r.m[0][1] = -s;
        r.m[1][0] =  s;  r.m[1][1] =  c;
        return r;
    }

    // ========================================================================
    // OPERAÇÕES
    // ========================================================================

    /** Multiplicação de matrizes 4x4 (composição de transformações). */
    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 r;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k) sum += m[i][k] * other.m[k][j];
                r.m[i][j] = sum;
            }
        }
        return r;
    }

    /** Transposição: (M^T)[i][j] = M[j][i]. */
    Matrix4x4 transpose() const {
        Matrix4x4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.m[i][j] = m[j][i];
        return r;
    }

    /**
     * Transforma um ponto: P' = M · (x, y, z, 1).
     * Faz a divisão perspectiva por w' caso w' != 1 (robustez; em afim w' sempre = 1).
     */
    Ponto transformPoint(const Ponto& p) const {
        double x = m[0][0]*p.getX() + m[0][1]*p.getY() + m[0][2]*p.getZ() + m[0][3];
        double y = m[1][0]*p.getX() + m[1][1]*p.getY() + m[1][2]*p.getZ() + m[1][3];
        double z = m[2][0]*p.getX() + m[2][1]*p.getY() + m[2][2]*p.getZ() + m[2][3];
        double w = m[3][0]*p.getX() + m[3][1]*p.getY() + m[3][2]*p.getZ() + m[3][3];
        if (std::abs(w) > 1e-12 && std::abs(w - 1.0) > 1e-12) {
            x /= w; y /= w; z /= w;
        }
        return Ponto(x, y, z);
    }

    /**
     * Transforma um vetor direção: V' = M · (x, y, z, 0).
     * Translação não afeta vetores (a coluna 4 é multiplicada por 0).
     */
    Vetor transformVector(const Vetor& v) const {
        double x = m[0][0]*v.getX() + m[0][1]*v.getY() + m[0][2]*v.getZ();
        double y = m[1][0]*v.getX() + m[1][1]*v.getY() + m[1][2]*v.getZ();
        double z = m[2][0]*v.getX() + m[2][1]*v.getY() + m[2][2]*v.getZ();
        return Vetor(x, y, z);
    }

    /**
     * Transforma uma normal: N' = (M^-1)^T · N.
     * Usar M direto deforma normais sob escala não-uniforme. A regra correta é
     * derivada da invariância de N · v = 0 sob a transformação de v.
     */
    Vetor transformNormal(const Vetor& n) const {
        Matrix4x4 invT = inverse().transpose();
        return invT.transformVector(n).normalize();
    }

    /**
     * Inversa da matriz 4x4 via método de cofatores (adjugada / determinante).
     * Funciona para qualquer matriz não-singular. Lança exceção se det ≈ 0.
     */
    Matrix4x4 inverse() const {
        // Algoritmo: M^-1 = adj(M) / det(M), com adj(M)[i][j] = cofator(j, i).
        Matrix4x4 result;
        Matrix4x4 cofactors;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                double sub[3][3];
                copyMinor(i, j, sub);
                double minor = determinant3x3(sub);
                double sign = ((i + j) % 2 == 0) ? 1.0 : -1.0;
                cofactors.m[i][j] = sign * minor;
            }
        }

        // det(M) = soma dos produtos da primeira linha pelos seus cofatores.
        double det = 0.0;
        for (int j = 0; j < 4; ++j) det += m[0][j] * cofactors.m[0][j];
        if (std::abs(det) < 1e-12) {
            throw std::runtime_error("Matrix4x4::inverse: matriz singular");
        }

        // Adjugada = transposta da matriz de cofatores; M^-1 = adj / det.
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = cofactors.m[j][i] / det;
        return result;
    }

private:
    /** Determinante 3x3 (regra de Sarrus). */
    static double determinant3x3(const double s[3][3]) {
        return s[0][0] * (s[1][1] * s[2][2] - s[1][2] * s[2][1])
             - s[0][1] * (s[1][0] * s[2][2] - s[1][2] * s[2][0])
             + s[0][2] * (s[1][0] * s[2][1] - s[1][1] * s[2][0]);
    }

    /** Preenche `sub` com a submatriz 3x3 obtida removendo linha `excludeRow` e coluna `excludeCol`. */
    void copyMinor(int excludeRow, int excludeCol, double sub[3][3]) const {
        int rr = 0;
        for (int i = 0; i < 4; ++i) {
            if (i == excludeRow) continue;
            int cc = 0;
            for (int j = 0; j < 4; ++j) {
                if (j == excludeCol) continue;
                sub[rr][cc] = m[i][j];
                ++cc;
            }
            ++rr;
        }
    }

    double m[4][4];
};

#endif
