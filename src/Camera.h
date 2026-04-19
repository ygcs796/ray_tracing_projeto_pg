#ifndef CAMERA_HEADER
#define CAMERA_HEADER

/**
 * Câmera virtual com base ortonormal e geração de raios primários.
 *
 * Dados (de CameraData, carregado do JSON):
 *   C   = lookfrom         posição da câmera no mundo
 *   M   = lookat           ponto de mira
 *   Vup = up_vector        "para cima" subjetivo (fixa a rotação em torno do eixo de mira)
 *   d   = screen_distance  distância câmera → tela (d pequeno = FOV largo; d grande = estreito)
 *   hres, vres             resolução em pixels
 *
 * Base ortonormal (W, U, V) calculada uma única vez no construtor. Os três
 * vetores são unitários e perpendiculares entre si — sistema de coordenadas
 * próprio da câmera:
 *   W = normalize(C - M)     → "para trás" da câmera
 *   U = normalize(W × Vup)   → "para a direita"
 *   V = U × W                → "para cima"
 *
 * Nota 16/04/2026: a ordem do cross (W × Vup e U × W) foi invertida em relação
 * à convenção literal das anotações da disciplina (Vup × W, W × U), que gera
 * imagem espelhada horizontalmente em cenas olhando para -Z. Com a ordem atual,
 * a parede vermelha do Cornell Box aparece à esquerda e a verde à direita.
 */

#include <cmath>
#include "Ponto.h"
#include "Vetor.h"
#include "Ray.h"
#include "../utils/Scene/sceneSchema.hpp"

class Camera {
public:
    /** Constrói a câmera e calcula a base ortonormal (W, U, V). */
    Camera(const CameraData& data)
        : C(data.lookfrom),
          d(data.screen_distance),
          hres(data.image_width),
          vres(data.image_height)
    {
        // (C - M) tem magnitude arbitrária, precisa normalize.
        W = (C - data.lookat).normalize();

        // Vup pode não ser unitário e pode não ser perpendicular a W,
        // então o cross não sai unitário de fábrica → normalize.
        U = W.cross(data.upVector).normalize();

        // U e W já são unitários e perpendiculares (U = W × Vup é ⊥ a W),
        // então |U × W| = 1 automaticamente → dispensa normalize.
        V = U.cross(W);
    }

    /**
     * Gera o raio primário que parte de C e passa pelo centro do pixel (i, j).
     *
     * Tela virtual: largura 1 (unidade de mundo), perpendicular a W, a distância
     * d à frente de C (centro em C - d·W), dividida em hres × vres pixels.
     *
     * Os offsets (u_offset, v_offset) posicionam o raio no centro do pixel:
     *   - `+0.5` / `-0.5` amostram o centro do pixel, não a borda.
     *   - `vres/2 - j` inverte o eixo vertical (PPM cresce para baixo, V aponta para cima).
     *
     * @param i coluna do pixel (0 .. hres-1)
     * @param j linha do pixel (0 .. vres-1, 0 = topo)
     * @return  raio com origem em C e direção unitária
     */
    Ray generateRay(int i, int j) const {
        double pixel_size = 1.0 / hres;

        double u_offset = pixel_size * (i - hres / 2.0 + 0.5);
        double v_offset = pixel_size * (vres / 2.0 - j - 0.5);

        // Direção: do C até o ponto do pixel na tela.
        // pixel_world = C + U·u_offset + V·v_offset - d·W
        // direction   = pixel_world - C = U·u_offset + V·v_offset - d·W
        Vetor direction = (U * u_offset + V * v_offset - W * d).normalize();

        return Ray(C, direction);
    }

    Ponto getC() const { return C; }
    Vetor getW() const { return W; }
    Vetor getU() const { return U; }
    Vetor getV() const { return V; }
    double getD() const { return d; }
    int getHres() const { return hres; }
    int getVres() const { return vres; }

private:
    Ponto C;              // posição da câmera
    Vetor W, U, V;        // base ortonormal
    double d;             // distância da tela
    int hres, vres;       // resolução em pixels
};

#endif
