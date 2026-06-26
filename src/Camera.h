#ifndef CAMERA_HEADER
#define CAMERA_HEADER

/**
 * Câmera virtual com base ortonormal e geração de raios primários.
 *
 * Dados (de CameraData, carregado do JSON):
 *   cameraPosition     posição da câmera no mundo          (C / lookfrom)
 *   targetPoint        ponto de mira                        (M / lookat)
 *   upVector           "para cima" subjetivo                (Vup)
 *                      (fixa a rotação em torno do eixo de mira)
 *   screenDistance     distância câmera → tela              (d)
 *                      (d pequeno = FOV largo; d grande = estreito)
 *   imageWidth, imageHeight   resolução em pixels           (hres, vres)
 *
 * Base ortonormal calculada uma única vez no construtor. Os três vetores são
 * unitários e perpendiculares entre si — sistema de coordenadas próprio da câmera:
 *   backward = normalize(cameraPosition - targetPoint)   → "para trás" da câmera
 *   right    = normalize(upVector × backward)            → "para a direita"
 *   up       = backward × right                          → "para cima"
 *
 */

#include <cmath>
#include "Ponto.h"
#include "Vetor.h"
#include "Ray.h"
#include "../utils/Scene/sceneSchema.hpp"

class Camera {
public:
    /** Constrói a câmera e calcula a base ortonormal (backward, right, up). */
    Camera(const CameraData& data)
        : cameraPosition(data.lookfrom),
          screenDistance(data.screen_distance),
          imageWidth(data.image_width),
          imageHeight(data.image_height)
    {
        computeOrthonormalBasis(data.lookat, data.upVector);
    }

    /**
     * Gera o raio primário que parte da câmera e passa pelo centro do pixel.
     * @param pixelColumn coluna do pixel (0 .. imageWidth-1)
     * @param pixelRow    linha do pixel (0 .. imageHeight-1, 0 = topo)
     */
    Ray generateRay(int pixelColumn, int pixelRow) const {
        double offsetRight = horizontalOffsetForColumn(pixelColumn);
        double offsetUp    = verticalOffsetForRow(pixelRow);
        Vetor rayDirection = directionTowardsPixel(offsetRight, offsetUp);
        return Ray(cameraPosition, rayDirection);
    }

    Ponto  getCameraPosition() const { return cameraPosition; }
    Vetor  getBackward()       const { return backward; }
    Vetor  getRight()          const { return right; }
    Vetor  getUp()             const { return up; }
    double getScreenDistance() const { return screenDistance; }
    int    getImageWidth()     const { return imageWidth; }
    int    getImageHeight()    const { return imageHeight; }

private:
    /**
     * Calcula os três eixos da câmera a partir do ponto de mira e do vetor up.
     *   backward aponta para trás da câmera.
     *   right    aponta para a direita (perpendicular a backward e upVector).
     *   up       aponta para cima (perpendicular a right e backward).
     */
    void computeOrthonormalBasis(const Ponto& targetPoint, const Vetor& upVector) {
        // Convenção literal das anotações da disciplina:
        //   W = normalize(C - M)   — aponta "para trás" da câmera (nosso `backward`)
        //   U = normalize(Vup × W) — aponta "para a direita" da câmera (nosso `right`)
        //   V = W × U              — aponta "para cima" da câmera (nosso `up`)
        //
        // Importante: a ordem do cross em `right` (Vup × W e não W × Vup) determina o
        // sentido do eixo horizontal da câmera. Invertê-la espelha a imagem
        // horizontalmente — o que faz objetos em +x aparecerem à esquerda da tela
        // em vez de à direita.
        backward = (cameraPosition - targetPoint).normalize();
        right    = upVector.cross(backward).normalize();
        up       = backward.cross(right);  // já unitário (right ⊥ backward, ambos unitários)
    }

    /** Tamanho de um pixel no mundo. A tela virtual tem largura 1. */
    double pixelSizeInWorldUnits() const {
        return 1.0 / imageWidth;
    }

    /** Deslocamento horizontal (em `right`) do centro do pixel, a partir do centro da tela. */
    double horizontalOffsetForColumn(int pixelColumn) const {
        return pixelSizeInWorldUnits() * (pixelColumn - imageWidth / 2.0 + 0.5);
    }

    /**
     * Deslocamento vertical (em `up`) do centro do pixel, a partir do centro da tela.
     * Sinal invertido porque pixelRow cresce para baixo (PPM) e `up` aponta para cima.
     */
    double verticalOffsetForRow(int pixelRow) const {
        return pixelSizeInWorldUnits() * (imageHeight / 2.0 - pixelRow - 0.5);
    }

    /**
     * Direção unitária do raio: combina os deslocamentos na base da câmera
     * com o avanço de `screenDistance` para a frente (-backward).
     */
    Vetor directionTowardsPixel(double offsetRight, double offsetUp) const {
        Vetor directionNotNormalized =
            right    * offsetRight
          + up       * offsetUp
          - backward * screenDistance;
        return directionNotNormalized.normalize();
    }

    Ponto cameraPosition;            // posição da câmera
    Vetor backward, right, up;       // base ortonormal
    double screenDistance;           // distância da tela
    int imageWidth, imageHeight;     // resolução em pixels
};

#endif
