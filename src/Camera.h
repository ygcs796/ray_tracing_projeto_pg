#ifndef CAMERA_HEADER
#define CAMERA_HEADER

/**
 * Câmera virtual com base ortonormal e geração de raios primários.
 *
 * A câmera é construída a partir de CameraData (carregado do JSON da cena):
 *   - C   = lookfrom (posição no mundo)
 *   - M   = lookat (ponto para onde aponta)
 *   - Vup = vetor "para cima"
 *   - d   = screen_distance (distância da tela)
 *   - hres, vres = resolução em pixels
 *
 * A base ortonormal (W, U, V) é calculada no construtor e usada por generateRay(i, j)
 * para mapear cada pixel da imagem a um ponto no mundo e construir um raio a partir de C.
 *
 * Convenção da base (ajustada após validação visual do Cornell Box):
 *   W = normalize(C - M)     → aponta "para trás" da câmera
 *   U = normalize(W × Vup)   → aponta para a direita
 *   V = W × U                → aponta para cima na base da câmera
 *
 * Nota histórica:
 *   A convenção literal das anotações (U = Vup × W) gera imagem espelhada horizontalmente
 *   para cenas em que a câmera olha para -Z (como sampleScene.json). Invertemos para
 *   U = W × Vup de modo que a parede vermelha (x=0) apareça à esquerda e a verde (x=555)
 *   à direita, como esperado visualmente.
 */

#include <cmath>
#include "Ponto.h"
#include "Vetor.h"
#include "Ray.h"
#include "../utils/Scene/sceneSchema.hpp"

class Camera {
public:
    /**
     * Constrói a câmera a partir dos dados da cena.
     * Calcula a base ortonormal (W, U, V) uma única vez.
     *
     * @param data configuração da câmera carregada do JSON
     */
    Camera(const CameraData& data)
        : C(data.lookfrom),
          d(data.screen_distance),
          hres(data.image_width),
          vres(data.image_height)
    {
        // W aponta "para trás" da câmera (convenção: oposto à direção de olhar)
        W = (C - data.lookat).normalize();

        // U aponta para a direita: W × Vup (invertido em relação à convenção literal
        // das anotações para evitar espelhamento horizontal em cenas olhando para -Z)
        U = W.cross(data.upVector).normalize();

        // V aponta para cima: U × W (ordem ajustada junto com U para preservar orientação).
        // Com U = W × Vup, temos V = U × W → vetor unitário apontando no mesmo sentido de Vup.
        V = U.cross(W);
    }

    /**
     * Gera o raio primário que parte da câmera e passa pelo centro do pixel (i, j).
     *
     * Tela virtual: largura normalizada = 1, dividida em hres pixels de tamanho 1/hres.
     * Centro da tela: screen_center = C - d * W (à frente da câmera, a distância d).
     * O pixel (i, j) é mapeado para um ponto no mundo por combinação linear de U e V
     * a partir do centro da tela, com offset de +0.5 para amostrar no centro do pixel.
     *
     *     u_offset = pixel_size * (i - hres/2 + 0.5)      (coluna, esquerda→direita)
     *     v_offset = pixel_size * (vres/2 - j - 0.5)      (linha, top-to-bottom)
     *
     * @param i coluna do pixel (0..hres-1)
     * @param j linha do pixel (0..vres-1, 0 = topo)
     * @return  raio com origem em C e direção unitária
     */
    Ray generateRay(int i, int j) const {
        double pixel_size = 1.0 / hres;

        double u_offset = pixel_size * (i - hres / 2.0 + 0.5);
        double v_offset = pixel_size * (vres / 2.0 - j - 0.5);

        // Direção: da câmera até o ponto do pixel na tela.
        // pixel_world = C + U*u_offset + V*v_offset - d*W
        // direction   = pixel_world - C = U*u_offset + V*v_offset - d*W
        Vetor direction = (U * u_offset + V * v_offset - W * d).normalize();

        return Ray(C, direction);
    }

    // ---------- Getters (úteis para debug) ----------
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
