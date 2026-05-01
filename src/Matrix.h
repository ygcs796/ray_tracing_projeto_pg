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

    /**
     * Escala em torno de um ponto arbitrário `pivot` (em vez da origem).
     *
     * Composição: T(pivot) · S(sx,sy,sz) · T(-pivot).
     *   1. T(-pivot) leva o pivot para a origem.
     *   2. S escala em torno da origem.
     *   3. T(pivot) leva de volta. O pivot fica fixo após a operação.
     */
    static Matrix4x4 scalingAroundPoint(double sx, double sy, double sz, const Ponto& pivot) {
        Matrix4x4 toPivot   = translation( pivot.getX(),  pivot.getY(),  pivot.getZ());
        Matrix4x4 fromPivot = translation(-pivot.getX(), -pivot.getY(), -pivot.getZ());
        return toPivot * scaling(sx, sy, sz) * fromPivot;
    }

    /**
     * Rotação por ângulo `angleRad` em torno de um eixo arbitrário `axis`
     * (que passa pela origem). Usa a fórmula de Rodrigues em forma matricial.
     *
     * R = I + sinθ · K + (1 - cosθ) · K²
     *
     * onde K é a matriz anti-simétrica do produto vetorial com o eixo unitário u:
     *   |  0  -uz  uy |
     *   | uz   0  -ux |
     *   |-uy  ux   0  |
     *
     * Se o eixo não passa pela origem, compor com translações antes/depois,
     * análogo a scalingAroundPoint.
     */
    static Matrix4x4 rotationAroundAxis(double angleRad, const Vetor& axis) {
        Vetor u = axis.normalize();
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        double t = 1.0 - c;
        double ux = u.getX(), uy = u.getY(), uz = u.getZ();

        Matrix4x4 r = identity();
        r.m[0][0] = c + ux*ux*t;
        r.m[0][1] = ux*uy*t - uz*s;
        r.m[0][2] = ux*uz*t + uy*s;

        r.m[1][0] = uy*ux*t + uz*s;
        r.m[1][1] = c + uy*uy*t;
        r.m[1][2] = uy*uz*t - ux*s;

        r.m[2][0] = uz*ux*t - uy*s;
        r.m[2][1] = uz*uy*t + ux*s;
        r.m[2][2] = c + uz*uz*t;
        return r;
    }

    /**
     * Transformação afim arbitrária a partir de 3 correspondências de pontos.
     *
     * Dadas 3 correspondências A→A', B→B', C→C' (não-colineares), constrói a
     * matriz 4x4 que leva A em A', B em B', C em C'. A 4ª correspondência é
     * implícita: a origem do sistema afim definido pelos 3 pontos.
     *
     * Estratégia: resolvemos 3 sistemas lineares 4x4 (um por linha da matriz
     * de transformação), cada um com 4 pontos de teste — A, B, C e a "origem"
     * D = A + (A→A') na convenção de coordenadas baricêntricas estendidas.
     *
     * Mais robusto: definir D como qualquer ponto fora do plano ABC. Aqui usamos
     * D = A + cross(B-A, C-A) (perpendicular ao plano ABC, garante não-coplanar)
     * com correspondência D' = A' + cross(B'-A', C'-A') (preserva a estrutura
     * geométrica para transformações afins não-degeneradas).
     */
    static Matrix4x4 affineFromCorrespondences(
        const Ponto& A, const Ponto& B, const Ponto& C,
        const Ponto& A2, const Ponto& B2, const Ponto& C2)
    {
        // 4º ponto: D fora do plano ABC, e D' fora do plano A'B'C'.
        Vetor nABC = (B - A).cross(C - A);
        Ponto D  = A + nABC;
        Vetor nA2B2C2 = (B2 - A2).cross(C2 - A2);
        Ponto D2 = A2 + nA2B2C2;

        // Sistema: M · src_i = dst_i, com src_i e dst_i em coordenadas homogêneas.
        // Montamos a matriz Source 4x4 (colunas = A,B,C,D em homogêneas) e
        // resolvemos M = Dest · Source⁻¹.
        Matrix4x4 srcM, dstM;
        auto fillCol = [](Matrix4x4& mat, int col, const Ponto& p) {
            mat.m[0][col] = p.getX();
            mat.m[1][col] = p.getY();
            mat.m[2][col] = p.getZ();
            mat.m[3][col] = 1.0;
        };
        fillCol(srcM, 0, A);  fillCol(srcM, 1, B);  fillCol(srcM, 2, C);  fillCol(srcM, 3, D);
        fillCol(dstM, 0, A2); fillCol(dstM, 1, B2); fillCol(dstM, 2, C2); fillCol(dstM, 3, D2);
        return dstM * srcM.inverse();
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
