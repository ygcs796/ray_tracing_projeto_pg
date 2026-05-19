#ifndef PHONG_HEADER
#define PHONG_HEADER

/**
 * Modelo de iluminação local de Phong (1975), sem termos recursivos.
 *
 * Para cada ponto P de uma superfície atingida por um raio primário, este
 * módulo calcula a cor RGB resultante somando três componentes:
 *
 *   I = ka·Ia + Σ_n [ ILn · Od · kd · max(L·N, 0)
 *                   + ILn · ks      · max(R·V, 0)^η ]
 *
 *   - Ambiente   (ka·Ia)              : luz indireta aproximada; constante.
 *   - Difusa     (Lambert)            : intensidade depende do ângulo entre a
 *                                       direção da luz (L) e a normal (N).
 *   - Especular  (highlight de Phong) : brilho concentrado; depende do ângulo
 *                                       entre o vetor refletido (R) e o
 *                                       observador (V), elevado à rugosidade η.
 *
 * Reflexões e refrações (`kr·Ir` + `kt·It`) NÃO entram nesta entrega — ficam
 * para a Entrega 4 (raio recursivo).
 *
 * Sombras: para cada luz pontual, lançamos um raio de sombra a partir de P (com
 * pequeno offset anti-acne ao longo de N) em direção à luz. Se algum objeto da
 * cena está entre P e a luz, o ponto está em sombra para AQUELA luz — ignoramos
 * a contribuição difusa + especular dessa luz (a ambiente sempre permanece).
 *
 * Convenção de escala: ka/kd/ks/Ia/IL vêm do JSON em [0,1]. A equação opera
 * inteiramente em [0,1]; a conversão para bytes [0,255] do PPM é feita pelo
 * chamador (main.cpp). Clampamos o resultado em [0,1] aqui antes de devolver
 * para evitar bytes inválidos depois.
 */

#include <algorithm>
#include <cmath>

#include "Ray.h"
#include "Ponto.h"
#include "Vetor.h"
#include "Intersect.h"
#include "Mesh.h"
#include "../utils/Scene/sceneSchema.hpp"

/**
 * Cor em ponto flutuante normalizada [0,1] por canal. Existe para diferenciar
 * o resultado contínuo do Phong da tripla de bytes (0..255) escrita no PPM.
 */
struct RGB {
    double r;
    double g;
    double b;
};

// ============================================================================
// RAIO DE SOMBRA
// ============================================================================

/** Offset anti-acne usado na origem do raio de sombra (deslocamento ao longo
 *  da normal). Maior que o EPSILON das interseções normais (1e-6) porque
 *  precisamos sair da superfície com margem suficiente para que erros de
 *  ponto flutuante NÃO façam o raio reintersectar o próprio objeto. */
constexpr double SHADOW_EPSILON = 1e-4;

/**
 * Verifica se o ponto `P` (com normal `N`) está em sombra em relação à fonte
 * `light`. Ou seja: existe algum objeto entre P e a luz?
 *
 * Algoritmo:
 *   1. Desloca a origem do raio de sombra ao longo de N: P_offset = P + ε·N.
 *      Sem isso, o raio frequentemente reintersecta o próprio objeto de onde
 *      saiu (auto-interseção numérica → "acne de sombra").
 *   2. Calcula a direção e a DISTÂNCIA até a luz a partir de P_offset.
 *      A distância é o nosso "tMax" — qualquer interseção com t além da luz
 *      está ATRÁS da luz e não bloqueia.
 *   3. Testa o raio contra todos os objetos. Se algum tem 0 < t < dist_luz,
 *      o ponto está em sombra para esta luz e retornamos true.
 *
 * @param P         Ponto da superfície sendo sombreado.
 * @param N         Normal unitária em P (já com o flip de orientação aplicado
 *                  pelo chamador, se necessário).
 * @param light     Fonte de luz pontual sendo testada.
 * @param objects   Lista de objetos da cena (esferas, planos, malhas).
 * @param meshes    Tabela de malhas pré-carregadas (passar mapa vazio se a
 *                  cena não tem malhas).
 * @return          true se existe um objeto bloqueando a luz; false caso
 *                  contrário.
 */
inline bool isInShadow(const Ponto& P,
                       const Vetor& N,
                       const LightData& light,
                       std::vector<ObjectData>& objects,
                       const MeshLookup& meshes) {
    // Origem do raio de sombra deslocada ao longo da normal — evita acne.
    Ponto shadowOrigin = P + N * SHADOW_EPSILON;

    // Vetor de P_offset até a luz. magnitude() é a distância "tMax" implícita:
    // interseções com t além disso estão ATRÁS da luz e devem ser ignoradas.
    Vetor toLight        = light.pos - shadowOrigin;
    double distanceToLight = toLight.magnitude();
    Vetor shadowDirection  = toLight.normalize();

    Ray shadowRay(shadowOrigin, shadowDirection);

    for (ObjectData& candidate : objects) {
        std::optional<double> tHit = intersect(shadowRay, candidate, meshes);
        bool hitIsBetweenPointAndLight =
            tHit.has_value() && *tHit > 0.0 && *tHit < distanceToLight;
        if (hitIsBetweenPointAndLight) return true;
    }
    return false;
}

// ============================================================================
// EQUAÇÃO DE PHONG (por pixel)
// ============================================================================

/**
 * Calcula a cor RGB do ponto `hit.point` aplicando o modelo de Phong.
 *
 * Vetores envolvidos (todos unitários):
 *   N — normal da superfície no ponto (vem de `hit.normal`).
 *   V — do ponto até o observador. Para raios primários, observador = origem
 *       do raio (câmera). V = normalize(ray.origin - P).
 *   L — do ponto até a luz n. L = normalize(luz_n.pos - P). Calculado por luz.
 *   R — reflexão de L em relação a N. R = 2·(L·N)·N - L. Calculado por luz.
 *
 * Componentes:
 *   Ambiente   : Iamb = ka * Ia                                       (constante)
 *   Difusa     : Idif = ILn * Od * kd * max(L·N, 0)                   (por luz)
 *   Especular  : Iesp = ILn * ks      * max(R·V, 0)^η                 (por luz)
 *
 * Cada produto entre cores é COMPONENTE A COMPONENTE (r·r', g·g', b·b'). O
 * canal R não influencia G ou B.
 *
 * Flip de normal: se a normal estiver virada para o lado oposto do observador
 * (N·V < 0), invertemos N. Isso é necessário porque:
 *   - Planos podem ser atingidos pelo "lado de trás" (a normal está apontando
 *     para o lado errado em relação à câmera). Sem o flip, V·N < 0 e a luz
 *     vinda do lado "certo" parece chegar de trás → todo o sombreamento some.
 *   - Esferas vistas por dentro (refração futura) também precisam disso.
 *
 * Em sombra (raio de sombra bloqueado), descartamos as componentes difusa +
 * especular daquela luz; a ambiente sempre permanece, garantindo que regiões
 * sombreadas não fiquem totalmente pretas.
 *
 * Clamping: cada componente do resultado é clampada a [0,1]. Se η pequeno +
 * ks alto, R·V^η pode passar de 1 e estourar o canal. O clamp aqui evita
 * overflow no cast para byte que o main.cpp fará depois.
 *
 * @param hit      Resultado da interseção primária (P, N, material).
 * @param ray      Raio primário usado para encontrar `hit` (origem = câmera).
 * @param scene    Cena com globalLight (Ia) e lightList (luzes pontuais).
 * @param objects  Objetos da cena, passados para o teste de sombra.
 * @param meshes   Malhas pré-carregadas, passadas para o teste de sombra.
 * @return         Cor RGB em [0,1]^3.
 */
inline RGB computeColor(const HitRecord& hit,
                        const Ray& ray,
                        const SceneData& scene,
                        std::vector<ObjectData>& objects,
                        const MeshLookup& meshes) {
    const MaterialData& mat = hit.material;
    Ponto P = hit.point;
    Vetor N = hit.normal;

    // V = ponto -> observador. Para raios primários, o observador é a câmera
    // (ray.origin). Para raios secundários (Entrega 4), seria a origem do raio
    // refletido/refratado — o conceito é o mesmo.
    Vetor V = (ray.origin - P).normalize();

    // Flip da normal se ela aponta para o lado oposto de V. Necessário para
    // que planos atingidos pelo "lado de trás" sejam sombreados corretamente
    // e para que esferas vistas por dentro (futuro) funcionem.
    if (N.dot(V) < 0.0) N = -N;

    // ---------- Componente ambiente: ka * Ia (constante, não depende de luz) -
    const ColorData& Ia = scene.globalLight.color;
    double accumR = mat.ka.r * Ia.r;
    double accumG = mat.ka.g * Ia.g;
    double accumB = mat.ka.b * Ia.b;

    // ---------- Somatório sobre cada luz pontual --------------------------
    for (const LightData& light : scene.lightList) {
        const ColorData& IL = light.color;

        // L = ponto -> luz. Unitário.
        Vetor L = (light.pos - P).normalize();

        // Em sombra para esta luz: pula difusa + especular. Ambiente já foi
        // somada acima e permanece.
        if (isInShadow(P, N, light, objects, meshes)) continue;

        // Difusa (Lambert): max(L·N, 0). Se negativo, a luz vem por trás da
        // superfície — clampamos a 0 para não subtrair iluminação.
        double NdotL = std::max(N.dot(L), 0.0);
        accumR += IL.r * mat.color.r * NdotL;
        accumG += IL.g * mat.color.g * NdotL;
        accumB += IL.b * mat.color.b * NdotL;

        // Reflexão R = 2(L·N)·N - L. Quando L está atrás da superfície (NdotL
        // foi clampado a 0), o brilho especular também deveria sumir; o
        // max(R·V, 0) abaixo garante isso porque R cai do lado errado.
        Vetor R = (N * (2.0 * NdotL) - L).normalize();
        double RdotV = std::max(R.dot(V), 0.0);

        // Especular de Phong: max(R·V, 0)^η. η alto = brilho concentrado.
        // std::pow(0, η) = 0, ok para η > 0 (o caso η = 0 é degenerado e o
        // monitor disse para tratar η > 0).
        double specularFactor = std::pow(RdotV, mat.ns);
        accumR += IL.r * mat.ks.r * specularFactor;
        accumG += IL.g * mat.ks.g * specularFactor;
        accumB += IL.b * mat.ks.b * specularFactor;
    }

    // Clamp final em [0,1]. Sem isto, um valor 1.5 viraria 255*1.5 = 382 no
    // byte (overflow) e a cor "daria a volta", virando preto.
    accumR = std::clamp(accumR, 0.0, 1.0);
    accumG = std::clamp(accumG, 0.0, 1.0);
    accumB = std::clamp(accumB, 0.0, 1.0);

    return RGB{ accumR, accumG, accumB };
}

#endif
