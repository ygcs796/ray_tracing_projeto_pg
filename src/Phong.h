#ifndef PHONG_HEADER
#define PHONG_HEADER

/**
 * Modelo de iluminação de Phong (1975) COM os termos recursivos da Entrega 4.
 *
 * Para cada ponto P de uma superfície atingida por um raio, este módulo calcula
 * a cor RGB resultante somando a iluminação local de Phong com a contribuição
 * de raios secundários refletidos e refratados:
 *
 *   I = ka·Ia + Σ_n [ ILn · Od · kd · max(L·N, 0)
 *                   + ILn · ks      · max(R·V, 0)^η ]   (Phong local, Entrega 3)
 *       + kr · Ir                                       (reflexão,  Entrega 4)
 *       + kt · It                                       (refração,  Entrega 4)
 *
 *   - Ambiente   (ka·Ia)              : luz indireta aproximada; constante.
 *   - Difusa     (Lambert)            : intensidade depende do ângulo entre a
 *                                       direção da luz (L) e a normal (N).
 *   - Especular  (highlight de Phong) : brilho concentrado; depende do ângulo
 *                                       entre o vetor refletido (R) e o
 *                                       observador (V), elevado à rugosidade η.
 *   - Reflexão   (kr·Ir)              : cor vinda do raio refletido (recursivo).
 *   - Refração   (kt·It)              : cor vinda do raio refratado (recursivo).
 *
 * Recursão: Ir e It são obtidos chamando o próprio computeColor para os raios
 * secundários, decrementando `depth` a cada nível. Ao atingir `depth <= 0`
 * devolvemos preto — isso impede a recursão de rodar para sempre (ex: dois
 * espelhos frente a frente refletindo um ao outro infinitamente).
 *
 * Sombras: para cada luz pontual, lançamos um raio de sombra a partir de P (com
 * pequeno offset anti-acne ao longo de N) em direção à luz. Se algum objeto da
 * cena está entre P e a luz, o ponto está em sombra para AQUELA luz — ignoramos
 * a contribuição difusa + especular dessa luz (a ambiente sempre permanece).
 *
 * Convenção de escala: ka/kd/ks/kr/kt/Ia/IL vêm do JSON em [0,1]. A equação
 * opera inteiramente em [0,1]; a conversão para bytes [0,255] do PPM é feita
 * pelo chamador (main.cpp). Clampamos o resultado em [0,1] aqui (DEPOIS de somar
 * reflexão e refração, que podem estourar) antes de devolver.
 */

#include <algorithm>
#include <cmath>
#include <optional>

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
// RAIOS SECUNDÁRIOS (Entrega 4): direções de reflexão e refração
// ============================================================================

/**
 * Direção de um raio refletido em relação à normal.
 *
 *   Rref = D - 2·(D·N)·N
 *
 * Dedução geométrica: decompomos D em uma componente paralela a N, (D·N)·N, e
 * uma componente tangente à superfície. A reflexão inverte apenas a componente
 * paralela; subtrair 2·(D·N)·N faz exatamente isso. Equivale à fórmula do
 * monitor R = 2·(N·V)·N - V quando D = -V (V aponta do ponto para o observador,
 * D aponta do observador para o ponto — sentidos opostos).
 *
 * @param D Direção incidente (do observador para a superfície). Unitária.
 * @param N Normal unitária no ponto, do lado de onde D chega.
 * @return  Direção refletida (mesma magnitude de D).
 */
inline Vetor reflect(const Vetor& D, const Vetor& N) {
    return D - N * (2.0 * D.dot(N));
}

/**
 * Direção de um raio refratado pela lei de Snell, ou std::nullopt em caso de
 * reflexão total interna.
 *
 *   ratio = n1 / n2          (n1 = meio de onde D vem, n2 = meio para onde vai)
 *   cosI  = -(D·N)           (N deve apontar contra D, então cosI > 0)
 *   sin²θt = ratio² · (1 - cosI²)
 *   cosT  = sqrt(1 - sin²θt)
 *   T     = ratio·D + (ratio·cosI - cosT)·N
 *
 * Reflexão total interna: quando o raio tenta passar de um meio mais denso para
 * um menos denso (n1 > n2) em ângulo raso, sin²θt > 1 e não existe raio
 * refratado — devolvemos std::nullopt e o chamador simplesmente não soma o
 * termo de refração naquele ponto.
 *
 * Pré-condição: D é unitário e N aponta para o lado de onde D vem (o chamador
 * é responsável por orientar N e escolher n1/n2 conforme o raio esteja entrando
 * ou saindo do objeto).
 *
 * @param D  Direção incidente (unitária).
 * @param N  Normal unitária orientada contra D.
 * @param n1 Índice de refração do meio de origem (ar = 1).
 * @param n2 Índice de refração do meio de destino.
 * @return   Direção refratada unitária, ou std::nullopt se houver reflexão total interna.
 */
inline std::optional<Vetor> refract(const Vetor& D, const Vetor& N, double n1, double n2) {
    double ratio = n1 / n2;
    double cosI  = -(D.dot(N));
    double sin2T = ratio * ratio * (1.0 - cosI * cosI);

    if (sin2T > 1.0) return std::nullopt;   // reflexão total interna: sem refração

    double cosT = std::sqrt(1.0 - sin2T);
    return (D * ratio + N * (ratio * cosI - cosT)).normalize();
}

// ============================================================================
// EQUAÇÃO DE PHONG (por pixel) + termos recursivos (reflexão e refração)
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
 * Reflexão e refração (Entrega 4): depois do Phong local, se o material tem
 * kr > 0 lançamos um raio refletido e somamos kr·Ir; se tem kt > 0 lançamos um
 * raio refratado (lei de Snell) e somamos kt·It. Ambos chamam computeColor
 * recursivamente com `depth - 1`. O parâmetro `depth` limita a recursão: ao
 * chegar a 0, devolvemos preto imediatamente (sem isto, dois espelhos frente a
 * frente se refletiriam para sempre).
 *
 * @param hit      Resultado da interseção (P, N, material).
 * @param ray      Raio que encontrou `hit` (primário = câmera; secundário =
 *                 raio refletido/refratado de um nível acima).
 * @param scene    Cena com globalLight (Ia) e lightList (luzes pontuais).
 * @param objects  Objetos da cena, para teste de sombra e raios secundários.
 * @param meshes   Malhas pré-carregadas, idem.
 * @param depth    Profundidade de recursão restante. 0 → devolve preto. O
 *                 chamador inicial usa MAX_DEPTH (3); cada nível passa depth-1.
 * @return         Cor RGB em [0,1]^3.
 */
inline RGB computeColor(const HitRecord& hit,
                        const Ray& ray,
                        const SceneData& scene,
                        std::vector<ObjectData>& objects,
                        const MeshLookup& meshes,
                        int depth = 3) {
    // Limite da recursão: ao esgotar a profundidade, o raio secundário não
    // contribui com cor (preto). É a condição de parada que garante término.
    if (depth <= 0) return RGB{ 0.0, 0.0, 0.0 };

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

    // ======================================================================
    // TERMOS RECURSIVOS (Entrega 4): reflexão (kr·Ir) e refração (kt·It)
    // ======================================================================
    // Estes raios secundários partem de P e usam o MESMO computeColor para
    // descobrir a cor que chega por reflexão/refração — daí a recursão.

    // ---------- Reflexão: kr · Ir -----------------------------------------
    // Só vale a pena se o material reflete em algum canal (kr > 0).
    bool materialReflete = mat.kr.r > 0.0 || mat.kr.g > 0.0 || mat.kr.b > 0.0;
    if (materialReflete) {
        // Reflete a direção do raio incidente sobre N (já flipada para o lado
        // do observador). reflect() preserva a magnitude; normalizamos por
        // segurança contra acúmulo de erro numérico.
        Vetor Rref = reflect(ray.direction, N).normalize();

        // Offset anti-acne PARA FORA da superfície (+ε·N): impede o raio
        // refletido de reintersectar o próprio objeto de onde saiu.
        Ray reflectRay(P + N * SHADOW_EPSILON, Rref);

        // Lança o raio refletido e calcula a cor recursivamente (depth-1).
        std::optional<HitRecord> reflectHit = findClosestHit(reflectRay, objects, meshes);
        RGB Ir{ 0.0, 0.0, 0.0 };
        if (reflectHit) {
            Ir = computeColor(*reflectHit, reflectRay, scene, objects, meshes, depth - 1);
        }

        // Soma componente a componente, ponderado pelo coeficiente de reflexão.
        accumR += mat.kr.r * Ir.r;
        accumG += mat.kr.g * Ir.g;
        accumB += mat.kr.b * Ir.b;
    }

    // ---------- Refração: kt · It -----------------------------------------
    bool materialTransmite = mat.kt.r > 0.0 || mat.kt.g > 0.0 || mat.kt.b > 0.0;
    if (materialTransmite) {
        // Orientação da normal: para a lei de Snell precisamos saber se o raio
        // está ENTRANDO ou SAINDO do objeto. Usamos a normal ORIGINAL do hit
        // (geométrica, para fora), NÃO a `N` local que foi flipada para o
        // observador — a flipagem mascararia o lado real do raio.
        double n1, n2;
        Vetor  Nrefr;
        if (ray.direction.dot(hit.normal) < 0.0) {
            // Raio vem de fora (D contra a normal): ar → objeto.
            n1 = 1.0;      n2 = mat.ni;  Nrefr = hit.normal;
        } else {
            // Raio vem de dentro (D a favor da normal): objeto → ar. Invertendo
            // a normal e trocando n1/n2 reaproveitamos a mesma fórmula.
            n1 = mat.ni;   n2 = 1.0;     Nrefr = -hit.normal;
        }

        // refract() devolve nullopt em reflexão total interna (sin²θt > 1):
        // nesse caso não há raio refratado e simplesmente não somamos kt·It.
        std::optional<Vetor> T = refract(ray.direction, Nrefr, n1, n2);
        if (T.has_value()) {
            // Offset anti-acne PARA DENTRO (−ε·Nrefr): o raio refratado atravessa
            // a superfície, então deslocamos a origem para o outro lado.
            Ray refractRay(P - Nrefr * SHADOW_EPSILON, *T);

            std::optional<HitRecord> refractHit = findClosestHit(refractRay, objects, meshes);
            RGB It{ 0.0, 0.0, 0.0 };
            if (refractHit) {
                It = computeColor(*refractHit, refractRay, scene, objects, meshes, depth - 1);
            }

            accumR += mat.kt.r * It.r;
            accumG += mat.kt.g * It.g;
            accumB += mat.kt.b * It.b;
        }
    }

    // Clamp final em [0,1] — DEPOIS de somar reflexão/refração, que podem
    // estourar o intervalo. Sem isto, um valor 1.5 viraria 255*1.5 = 382 no
    // byte (overflow) e a cor "daria a volta", virando preto.
    accumR = std::clamp(accumR, 0.0, 1.0);
    accumG = std::clamp(accumG, 0.0, 1.0);
    accumB = std::clamp(accumB, 0.0, 1.0);

    return RGB{ accumR, accumG, accumB };
}

#endif
