#ifndef MATRIX_HEADER
#define MATRIX_HEADER

/**
 * Matriz homogênea 4x4 — base de todas as transformações afins (translação,
 * escala, rotação) aplicadas a pontos, vetores e normais em R³.
 *
 * Por que 4x4 e não 3x3?
 *   Translação não é uma transformação linear em R³ — uma matriz 3x3 sempre
 *   mapeia a origem em si mesma. Adicionando uma 4ª coordenada (a "coordenada
 *   homogênea" w), conseguimos codificar a translação na 4ª coluna da matriz.
 *
 * Convenção de coordenadas homogêneas:
 *   - Pontos têm w = 1   → a 4ª coluna da matriz aplica translação normalmente.
 *   - Vetores têm w = 0  → a 4ª coluna é multiplicada por 0 e a translação não
 *                          afeta direções (uma seta apontando pro norte continua
 *                          apontando pro norte se você "andar" 5 km).
 *
 * Convenção de composição:
 *   Multiplicação coluna-vetor: ponto_transformado = matriz · ponto.
 *   Em "M = A · B · C", aplicada a um ponto p, a ordem é:
 *       M · p = A · (B · (C · p))
 *   A matriz mais à direita é aplicada PRIMEIRO. Por isso, ao compor uma lista
 *   de transformações [T1, T2, T3] onde T1 deve ser aplicada antes, montamos
 *   M = T3 · T2 · T1 (a primeira da lista vira a mais interna).
 *
 * Por que normais usam (M⁻¹)ᵀ em vez de M:
 *   Por definição, normal · vetor_tangente = 0 para todo vetor tangente à
 *   superfície. Quando vetores tangentes transformam como v' = M · v, queremos
 *   que normal' · v' continue zero. Resolvendo, sai normal' = (M⁻¹)ᵀ · normal.
 *   Aplicar M direto na normal só funciona pra rotação e escala uniforme — sob
 *   escala não-uniforme a normal "ingênua" aponta numa direção errada.
 */

#include <cmath>
#include <stdexcept>
#include "Ponto.h"
#include "Vetor.h"

class Matrix4x4 {
public:
    /** Constrói a matriz com todas as 16 entradas zeradas. */
    Matrix4x4() {
        for (int rowIndex = 0; rowIndex < 4; ++rowIndex)
            for (int colIndex = 0; colIndex < 4; ++colIndex)
                entries[rowIndex][colIndex] = 0.0;
    }

    /** Acesso direto a uma entrada (linha, coluna). */
    double& at(int rowIndex, int colIndex)             { return entries[rowIndex][colIndex]; }
    double  at(int rowIndex, int colIndex) const       { return entries[rowIndex][colIndex]; }

    // ========================================================================
    // CONSTRUTORES (factory methods)
    //
    // Cada método estático monta uma matriz pronta pra um tipo de transformação.
    // Use-os e componha via operator* para construir transformações complexas.
    // ========================================================================

    /**
     * Matriz identidade — a transformação que NÃO faz nada.
     *
     *   | 1  0  0  0 |
     *   | 0  1  0  0 |
     *   | 0  0  1  0 |
     *   | 0  0  0  1 |
     *
     * Aplicada a qualquer ponto/vetor, devolve ele igual. É o "elemento neutro"
     * da composição matricial — usada como ponto de partida ao acumular várias
     * transformações num loop (análogo a começar uma soma com 0).
     */
    static Matrix4x4 identity() {
        Matrix4x4 result;
        result.entries[0][0] = 1.0;
        result.entries[1][1] = 1.0;
        result.entries[2][2] = 1.0;
        result.entries[3][3] = 1.0;
        return result;
    }

    /**
     * Translação por (deltaX, deltaY, deltaZ).
     *
     *   | 1  0  0  dx |
     *   | 0  1  0  dy |
     *   | 0  0  1  dz |
     *   | 0  0  0  1  |
     *
     * Move pontos pelo vetor (dx, dy, dz). Vetores direção (w=0) não são
     * afetados — a coluna 4 multiplica 0 quando o w do vetor é 0.
     */
    static Matrix4x4 translation(double deltaX, double deltaY, double deltaZ) {
        Matrix4x4 result = identity();
        result.entries[0][3] = deltaX;
        result.entries[1][3] = deltaY;
        result.entries[2][3] = deltaZ;
        return result;
    }

    /**
     * Escala não-uniforme em torno da origem.
     *
     *   | sx  0   0   0 |
     *   | 0   sy  0   0 |
     *   | 0   0   sz  0 |
     *   | 0   0   0   1 |
     *
     * Multiplica cada coordenada pelo seu fator. Pontos longe da origem
     * "afastam-se" mais que pontos perto. Para escalar em torno de um ponto P
     * arbitrário (não a origem), usar `scalingAroundPoint`.
     */
    static Matrix4x4 scaling(double scaleX, double scaleY, double scaleZ) {
        Matrix4x4 result;
        result.entries[0][0] = scaleX;
        result.entries[1][1] = scaleY;
        result.entries[2][2] = scaleZ;
        result.entries[3][3] = 1.0;
        return result;
    }

    /**
     * Rotação em torno do eixo X (anti-horária, vista do +X em direção à origem).
     *
     *   | 1    0       0    0 |
     *   | 0  cosθ   -sinθ   0 |
     *   | 0  sinθ    cosθ   0 |
     *   | 0    0       0    1 |
     *
     * Mantém a coordenada X intacta. Y e Z giram entre si.
     */
    static Matrix4x4 rotationX(double angleInRadians) {
        double cosTheta = std::cos(angleInRadians);
        double sinTheta = std::sin(angleInRadians);
        Matrix4x4 result = identity();
        result.entries[1][1] =  cosTheta;  result.entries[1][2] = -sinTheta;
        result.entries[2][1] =  sinTheta;  result.entries[2][2] =  cosTheta;
        return result;
    }

    /** Rotação em torno do eixo Y. Mantém Y intacto; X e Z giram entre si. */
    static Matrix4x4 rotationY(double angleInRadians) {
        double cosTheta = std::cos(angleInRadians);
        double sinTheta = std::sin(angleInRadians);
        Matrix4x4 result = identity();
        result.entries[0][0] =  cosTheta;  result.entries[0][2] =  sinTheta;
        result.entries[2][0] = -sinTheta;  result.entries[2][2] =  cosTheta;
        return result;
    }

    /** Rotação em torno do eixo Z. Mantém Z intacto; X e Y giram entre si. */
    static Matrix4x4 rotationZ(double angleInRadians) {
        double cosTheta = std::cos(angleInRadians);
        double sinTheta = std::sin(angleInRadians);
        Matrix4x4 result = identity();
        result.entries[0][0] =  cosTheta;  result.entries[0][1] = -sinTheta;
        result.entries[1][0] =  sinTheta;  result.entries[1][1] =  cosTheta;
        return result;
    }

    /**
     * Escala em torno de um ponto arbitrário (em vez da origem).
     *
     * Estratégia: T(pivot) · S(sx, sy, sz) · T(-pivot).
     *   1. T(-pivot) leva o pivot para a origem.
     *   2. S aplica a escala (que sempre fixa a origem).
     *   3. T(pivot) leva tudo de volta.
     * Resultado: o pivot fica fixo; os outros pontos esticam relativos a ele.
     */
    static Matrix4x4 scalingAroundPoint(double scaleX, double scaleY, double scaleZ,
                                        const Ponto& pivot) {
        Matrix4x4 moveToPivot     = translation( pivot.getX(),  pivot.getY(),  pivot.getZ());
        Matrix4x4 moveBackToWorld = translation(-pivot.getX(), -pivot.getY(), -pivot.getZ());
        return moveToPivot * scaling(scaleX, scaleY, scaleZ) * moveBackToWorld;
    }

    /**
     * Rotação por um ângulo em torno de um eixo arbitrário (que passa pela origem).
     *
     * Implementa a fórmula de Rodrigues em forma matricial:
     *
     *   R = I + sinθ · K + (1 - cosθ) · K²
     *
     * onde K é a "matriz cross-product" do eixo unitário u = (ux, uy, uz):
     *
     *   |  0  -uz  uy |
     *   | uz   0  -ux |
     *   |-uy  ux   0  |
     *
     * (K · v = u × v para qualquer vetor v.) Expandindo a fórmula nas 9 entradas
     * 3x3 da rotação, chega-se nas expressões abaixo.
     *
     * Se o eixo NÃO passa pela origem, compor com translações antes/depois,
     * análogo a `scalingAroundPoint`.
     */
    static Matrix4x4 rotationAroundAxis(double angleInRadians, const Vetor& rotationAxis) {
        Vetor unitAxis = rotationAxis.normalize();
        double cosTheta            = std::cos(angleInRadians);
        double sinTheta            = std::sin(angleInRadians);
        double oneMinusCosTheta    = 1.0 - cosTheta;
        double axisX = unitAxis.getX();
        double axisY = unitAxis.getY();
        double axisZ = unitAxis.getZ();

        Matrix4x4 result = identity();
        result.entries[0][0] = cosTheta + axisX * axisX * oneMinusCosTheta;
        result.entries[0][1] = axisX * axisY * oneMinusCosTheta - axisZ * sinTheta;
        result.entries[0][2] = axisX * axisZ * oneMinusCosTheta + axisY * sinTheta;

        result.entries[1][0] = axisY * axisX * oneMinusCosTheta + axisZ * sinTheta;
        result.entries[1][1] = cosTheta + axisY * axisY * oneMinusCosTheta;
        result.entries[1][2] = axisY * axisZ * oneMinusCosTheta - axisX * sinTheta;

        result.entries[2][0] = axisZ * axisX * oneMinusCosTheta - axisY * sinTheta;
        result.entries[2][1] = axisZ * axisY * oneMinusCosTheta + axisX * sinTheta;
        result.entries[2][2] = cosTheta + axisZ * axisZ * oneMinusCosTheta;
        return result;
    }

    /**
     * Transformação afim arbitrária a partir de 3 correspondências de pontos.
     *
     * Dadas 3 correspondências (sourceA → targetA, sourceB → targetB,
     * sourceC → targetC) com sourceA, sourceB, sourceC NÃO-colineares, constrói
     * a matriz 4x4 que realiza essa transformação.
     *
     * O problema tem 12 incógnitas (a parte afim de uma matriz 4x4) mas só 9
     * equações vindas dos 3 pontos. Para fechar o sistema, precisamos de um 4º
     * ponto não-coplanar com os 3 originais. Escolha robusta:
     *
     *   fourthSource = sourceA + (sourceB - sourceA) × (sourceC - sourceA)
     *   fourthTarget = targetA + (targetB - targetA) × (targetC - targetA)
     *
     * O cross product gera um vetor perpendicular ao plano dos 3 pontos, então
     * `fourthSource` é garantidamente fora do plano original.
     *
     * Tendo 4 correspondências, resolvemos M · sourceMatrix = targetMatrix
     * (com cada coluna sendo um ponto em coordenadas homogêneas), o que dá
     * M = targetMatrix · sourceMatrix⁻¹.
     */
    static Matrix4x4 affineFromCorrespondences(
        const Ponto& sourceA, const Ponto& sourceB, const Ponto& sourceC,
        const Ponto& targetA, const Ponto& targetB, const Ponto& targetC)
    {
        // Geramos um 4º ponto não-coplanar pelo cross product das arestas do
        // triângulo de origem; mesmo procedimento no destino para preservar a
        // estrutura geométrica.
        Vetor sourceNormal = (sourceB - sourceA).cross(sourceC - sourceA);
        Vetor targetNormal = (targetB - targetA).cross(targetC - targetA);
        Ponto fourthSource = sourceA + sourceNormal;
        Ponto fourthTarget = targetA + targetNormal;

        // Cada coluna da matriz 4x4 é um ponto em coordenadas homogêneas (w=1).
        Matrix4x4 sourceMatrix;
        Matrix4x4 targetMatrix;
        auto fillColumnWithPoint = [](Matrix4x4& matrix, int columnIndex, const Ponto& point) {
            matrix.entries[0][columnIndex] = point.getX();
            matrix.entries[1][columnIndex] = point.getY();
            matrix.entries[2][columnIndex] = point.getZ();
            matrix.entries[3][columnIndex] = 1.0;
        };
        fillColumnWithPoint(sourceMatrix, 0, sourceA);
        fillColumnWithPoint(sourceMatrix, 1, sourceB);
        fillColumnWithPoint(sourceMatrix, 2, sourceC);
        fillColumnWithPoint(sourceMatrix, 3, fourthSource);
        fillColumnWithPoint(targetMatrix, 0, targetA);
        fillColumnWithPoint(targetMatrix, 1, targetB);
        fillColumnWithPoint(targetMatrix, 2, targetC);
        fillColumnWithPoint(targetMatrix, 3, fourthTarget);
        return targetMatrix * sourceMatrix.inverse();
    }

    // ========================================================================
    // OPERAÇÕES
    // ========================================================================

    /**
     * Multiplicação de matrizes 4x4 (composição de transformações).
     * Se A tem efeito X e B tem efeito Y, então (A · B) tem o efeito de
     * "primeiro Y, depois X" quando aplicada a um ponto.
     */
    Matrix4x4 operator*(const Matrix4x4& rightOperand) const {
        Matrix4x4 result;
        for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
            for (int colIndex = 0; colIndex < 4; ++colIndex) {
                double accumulatedSum = 0.0;
                for (int innerIndex = 0; innerIndex < 4; ++innerIndex) {
                    accumulatedSum += entries[rowIndex][innerIndex] *
                                      rightOperand.entries[innerIndex][colIndex];
                }
                result.entries[rowIndex][colIndex] = accumulatedSum;
            }
        }
        return result;
    }

    /** Transposta: troca linhas por colunas. (Mᵀ)[i][j] = M[j][i]. */
    Matrix4x4 transpose() const {
        Matrix4x4 result;
        for (int rowIndex = 0; rowIndex < 4; ++rowIndex)
            for (int colIndex = 0; colIndex < 4; ++colIndex)
                result.entries[rowIndex][colIndex] = entries[colIndex][rowIndex];
        return result;
    }

    /**
     * Aplica a transformação a um ponto (w = 1).
     *
     *   resultado = M · (x, y, z, 1)
     *
     * A divisão pelo w' final torna a função robusta também para projeções
     * em perspectiva (onde w' ≠ 1). Em transformações afins puras (translação,
     * escala, rotação), w' sempre vale 1 e a divisão é um no-op.
     */
    Ponto transformPoint(const Ponto& sourcePoint) const {
        double resultX = entries[0][0] * sourcePoint.getX()
                       + entries[0][1] * sourcePoint.getY()
                       + entries[0][2] * sourcePoint.getZ()
                       + entries[0][3];
        double resultY = entries[1][0] * sourcePoint.getX()
                       + entries[1][1] * sourcePoint.getY()
                       + entries[1][2] * sourcePoint.getZ()
                       + entries[1][3];
        double resultZ = entries[2][0] * sourcePoint.getX()
                       + entries[2][1] * sourcePoint.getY()
                       + entries[2][2] * sourcePoint.getZ()
                       + entries[2][3];
        double resultW = entries[3][0] * sourcePoint.getX()
                       + entries[3][1] * sourcePoint.getY()
                       + entries[3][2] * sourcePoint.getZ()
                       + entries[3][3];

        bool needsPerspectiveDivision =
            std::abs(resultW)         > 1e-12 &&
            std::abs(resultW - 1.0)   > 1e-12;
        if (needsPerspectiveDivision) {
            resultX /= resultW;
            resultY /= resultW;
            resultZ /= resultW;
        }
        return Ponto(resultX, resultY, resultZ);
    }

    /**
     * Aplica a transformação a um vetor direção (w = 0).
     *
     *   resultado = M · (x, y, z, 0)
     *
     * O w=0 zera a coluna 4, então translação não tem efeito — direções não têm
     * "posição" pra ser deslocada. Apenas a parte 3x3 superior esquerda da
     * matriz (rotação + escala) age sobre o vetor.
     */
    Vetor transformVector(const Vetor& sourceVector) const {
        double resultX = entries[0][0] * sourceVector.getX()
                       + entries[0][1] * sourceVector.getY()
                       + entries[0][2] * sourceVector.getZ();
        double resultY = entries[1][0] * sourceVector.getX()
                       + entries[1][1] * sourceVector.getY()
                       + entries[1][2] * sourceVector.getZ();
        double resultZ = entries[2][0] * sourceVector.getX()
                       + entries[2][1] * sourceVector.getY()
                       + entries[2][2] * sourceVector.getZ();
        return Vetor(resultX, resultY, resultZ);
    }

    /**
     * Aplica a transformação a uma normal: N' = (M⁻¹)ᵀ · N, normalizada.
     *
     * A regra correta sai da invariância "normal · vetor_tangente = 0".
     * Sob escala não-uniforme, aplicar M direto na normal a deformaria pro
     * lado errado — por exemplo, S(2,1,1) aplicada a (1,1,0)/√2 daria (2,1,0)/√5
     * (incorreto), enquanto (M⁻¹)ᵀ dá (0.5,1,0)/√1.25 (correto).
     */
    Vetor transformNormal(const Vetor& sourceNormal) const {
        Matrix4x4 inverseTransposed = inverse().transpose();
        return inverseTransposed.transformVector(sourceNormal).normalize();
    }

    /**
     * Inversa da matriz 4x4 via método de cofatores: M⁻¹ = adj(M) / det(M).
     *
     * Para cada posição (i, j):
     *   1. Calcula a "submatriz menor" (3x3 obtida removendo linha i e coluna j).
     *   2. Calcula seu determinante (esse é o "menor" de [i][j]).
     *   3. Multiplica pelo sinal alternado (-1)^(i+j) — sai o "cofator".
     *
     * Depois:
     *   - Determinante de M = soma dos produtos da primeira linha pelos seus cofatores.
     *   - Adjugada(M) = transposta da matriz de cofatores.
     *   - M⁻¹ = adjugada(M) / det(M).
     *
     * Lança exceção se a matriz for singular (det ≈ 0). Singularidade significa
     * que M "achatou" o espaço em uma dimensão menor — ex: scaling(0, 1, 1).
     */
    Matrix4x4 inverse() const {
        Matrix4x4 cofactorMatrix;

        // Etapa 1: monta a matriz de cofatores.
        for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
            for (int colIndex = 0; colIndex < 4; ++colIndex) {
                double minorSubmatrix[3][3];
                copySubmatrixWithoutRowAndColumn(rowIndex, colIndex, minorSubmatrix);
                double minorDeterminant = determinantOf3x3(minorSubmatrix);
                double sign = ((rowIndex + colIndex) % 2 == 0) ? 1.0 : -1.0;
                cofactorMatrix.entries[rowIndex][colIndex] = sign * minorDeterminant;
            }
        }

        // Etapa 2: determinante de M = primeira linha · cofatores correspondentes.
        double determinantOfThisMatrix = 0.0;
        for (int colIndex = 0; colIndex < 4; ++colIndex) {
            determinantOfThisMatrix += entries[0][colIndex] * cofactorMatrix.entries[0][colIndex];
        }
        if (std::abs(determinantOfThisMatrix) < 1e-12) {
            throw std::runtime_error("Matrix4x4::inverse: matriz singular (det ≈ 0)");
        }

        // Etapa 3: M⁻¹ = adjugada / det. Adjugada = transposta dos cofatores,
        // então acessamos cofactorMatrix[colIndex][rowIndex] ao preencher result[rowIndex][colIndex].
        Matrix4x4 result;
        for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
            for (int colIndex = 0; colIndex < 4; ++colIndex) {
                result.entries[rowIndex][colIndex] =
                    cofactorMatrix.entries[colIndex][rowIndex] / determinantOfThisMatrix;
            }
        }
        return result;
    }

private:
    /** Determinante 3x3 pela regra de Sarrus (expansão na primeira linha). */
    static double determinantOf3x3(const double matrix[3][3]) {
        return matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1])
             - matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0])
             + matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
    }

    /**
     * Preenche `submatrixOut` com a submatriz 3x3 obtida removendo `rowToExclude`
     * e `columnToExclude` da matriz atual. Usado para calcular cofatores.
     */
    void copySubmatrixWithoutRowAndColumn(int rowToExclude,
                                          int columnToExclude,
                                          double submatrixOut[3][3]) const {
        int submatrixRow = 0;
        for (int sourceRow = 0; sourceRow < 4; ++sourceRow) {
            if (sourceRow == rowToExclude) continue;
            int submatrixCol = 0;
            for (int sourceCol = 0; sourceCol < 4; ++sourceCol) {
                if (sourceCol == columnToExclude) continue;
                submatrixOut[submatrixRow][submatrixCol] = entries[sourceRow][sourceCol];
                ++submatrixCol;
            }
            ++submatrixRow;
        }
    }

    /** Os 16 valores da matriz, indexados por entries[linha][coluna]. */
    double entries[4][4];
};

#endif
