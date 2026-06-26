/**
 * Testes da feature extra: Soft Shadows (fonte de luz extensa amostrada).
 *
 * Compilar: g++ -std=c++17 tests/test_soft_shadows.cpp -o test_soft_shadows
 * Rodar:    ./test_soft_shadows
 *
 * Cobertura (com amostragem em GRADE — determinística):
 *   - lightRadius: lê "radius" do extraData; ausente/inválido → 0.
 *   - occluded: bloqueado / desobstruído / alvo atrás do obstáculo.
 *   - lightVisibility (luz pontual, radius=0): 1.0 sem bloqueio, 0.0 bloqueada
 *     (equivale à sombra dura — regressão).
 *   - lightVisibility (fonte extensa, radius>0):
 *       · sem bloqueio              → 1.0 (totalmente iluminado);
 *       · bloqueador cobrindo tudo  → 0.0 (umbra total);
 *       · bloqueador parcial        → 0 < vis < 1 (PENUMBRA — o ponto-chave).
 *   - computeColor: penumbra (vis parcial) escurece a difusa proporcionalmente
 *     em relação ao ponto totalmente iluminado.
 */

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "../src/Ponto.h"
#include "../src/Vetor.h"
#include "../src/Ray.h"
#include "../src/Intersect.h"
#include "../src/Phong.h"

static int passed = 0, failed = 0;

static bool approx(double a, double b, double tol = 1e-9) {
    return std::abs(a - b) < tol;
}
static void check(const std::string& name, bool ok) {
    if (ok) { std::cout << "[OK]   " << name << "\n"; ++passed; }
    else    { std::cout << "[FAIL] " << name << "\n"; ++failed; }
}

static ObjectData makeSphereObject(double cx, double cy, double cz, double radius,
                                   const MaterialData& mat) {
    ObjectData obj;
    obj.objType = "sphere";
    obj.vetorPointData["center"] = Vetor(cx, cy, cz);
    obj.numericData["radius"]    = radius;
    obj.material = mat;
    return obj;
}

static MaterialData whiteDiffuse() {
    MaterialData mat;
    mat.name  = "white";
    mat.color = ColorData(1, 1, 1);   // kd branco
    mat.ks    = ColorData(0, 0, 0);
    mat.ka    = ColorData(0, 0, 0);
    mat.kr    = ColorData(0, 0, 0);
    mat.kt    = ColorData(0, 0, 0);
    mat.ns    = 1; mat.ni = 1.0; mat.d = 1.0;
    return mat;
}

/** Luz no ponto (x,y,z) com cor branca e, opcionalmente, um raio de área. */
static LightData makeLight(double x, double y, double z, double radius = 0.0) {
    LightData light;
    light.pos = Ponto(x, y, z);
    light.color = ColorData(1, 1, 1);
    if (radius > 0.0) light.extraData["radius"] = std::to_string(radius);
    return light;
}

int main() {
    MeshLookup noMeshes;

    // -----------------------------------------------------------------------
    // lightRadius: lê o campo extra; ausente/ruim → 0.
    // -----------------------------------------------------------------------
    {
        check("radius_absent_is_zero", approx(lightRadius(makeLight(0, 10, 0)), 0.0));
        check("radius_present_is_read", approx(lightRadius(makeLight(0, 10, 0, 3.0)), 3.0));
        LightData bad = makeLight(0, 10, 0);
        bad.extraData["radius"] = "nao-e-numero";
        check("radius_invalid_is_zero", approx(lightRadius(bad), 0.0));
    }

    // -----------------------------------------------------------------------
    // occluded: teste de oclusão até um alvo qualquer.
    // -----------------------------------------------------------------------
    {
        // Bloqueador esférico em (0,5,0) entre P=(0,0,0) e o alvo (0,10,0).
        std::vector<ObjectData> objects { makeSphereObject(0, 5, 0, 1.0, whiteDiffuse()) };
        Ponto P(0, 0, 0); Vetor N(0, 1, 0);
        check("occluded_blocked",  occluded(P, N, Ponto(0, 10, 0), objects, noMeshes));
        check("occluded_clear",   !occluded(P, N, Ponto(10, 0, 0), objects, noMeshes)); // alvo lateral, livre

        // Alvo ANTES do obstáculo (entre P e a esfera) → não bloqueado.
        check("occluded_target_before_blocker",
              !occluded(P, N, Ponto(0, 3, 0), objects, noMeshes));
    }

    // -----------------------------------------------------------------------
    // lightVisibility — luz PONTUAL (radius=0): 1.0 livre, 0.0 bloqueada.
    // -----------------------------------------------------------------------
    {
        Ponto P(0, 0, 0); Vetor N(0, 1, 0);
        std::vector<ObjectData> none;
        check("point_light_full_visibility",
              approx(lightVisibility(P, N, makeLight(0, 10, 0), none, noMeshes), 1.0));

        std::vector<ObjectData> blocker { makeSphereObject(0, 5, 0, 1.0, whiteDiffuse()) };
        check("point_light_blocked_zero",
              approx(lightVisibility(P, N, makeLight(0, 10, 0), blocker, noMeshes), 0.0));
    }

    // -----------------------------------------------------------------------
    // lightVisibility — fonte EXTENSA (radius>0).
    // -----------------------------------------------------------------------
    {
        Ponto P(0, 0, 0); Vetor N(0, 1, 0);

        // (a) Sem bloqueio → totalmente visível.
        std::vector<ObjectData> none;
        check("area_light_full_visibility",
              approx(lightVisibility(P, N, makeLight(0, 10, 0, 3.0), none, noMeshes), 1.0));

        // (b) Bloqueador grande no meio do caminho cobre TODAS as amostras → umbra.
        //     A meio caminho (y=5) os raios às amostras afastam-se no máximo
        //     ~1.5 do eixo; uma esfera r=3 em (0,5,0) cobre tudo (e não contém P,
        //     que está a 5 do centro — evita origem-dentro-da-esfera).
        std::vector<ObjectData> bigBlocker { makeSphereObject(0, 5, 0, 3.0, whiteDiffuse()) };
        check("area_light_fully_blocked_zero",
              approx(lightVisibility(P, N, makeLight(0, 10, 0, 3.0), bigBlocker, noMeshes), 0.0));

        // (c) Bloqueador PARCIAL: esfera r=1 em (0,5,0). Cobre as amostras
        //     centrais mas não as das quinas → 0 < vis < 1 (PENUMBRA).
        std::vector<ObjectData> partial { makeSphereObject(0, 5, 0, 1.0, whiteDiffuse()) };
        double vis = lightVisibility(P, N, makeLight(0, 10, 0, 3.0), partial, noMeshes);
        check("area_light_partial_is_penumbra", vis > 0.0 && vis < 1.0);
    }

    // -----------------------------------------------------------------------
    // computeColor: penumbra escurece a difusa proporcionalmente.
    // -----------------------------------------------------------------------
    {
        // Ponto totalmente iluminado vs. ponto em penumbra, mesma geometria de
        // luz/normal, diferindo só pelo bloqueador parcial.
        MaterialData mat = whiteDiffuse();           // kd=1, ka=0, ks=0
        HitRecord hit{ 1.0, Ponto(0, 0, 0), Vetor(0, 1, 0), mat };
        // Observador ACIMA (mesmo lado da normal) para V·N > 0 — sem flip de
        // normal, a difusa fica com NdotL = 1.
        Ray ray(Ponto(0, 1, 0), Vetor(0, -1, 0));

        SceneData lit;   lit.globalLight.color = ColorData(0, 0, 0);
        lit.lightList.push_back(makeLight(0, 10, 0, 3.0));
        SceneData shadowed = lit;                    // mesma luz

        std::vector<ObjectData> none;
        std::vector<ObjectData> partial { makeSphereObject(0, 5, 0, 1.0, whiteDiffuse()) };

        RGB full = computeColor(hit, ray, lit, none, noMeshes);
        RGB pen  = computeColor(hit, ray, shadowed, partial, noMeshes);

        // Penumbra deve ser mais escura que o totalmente iluminado, mas não preta.
        check("penumbra_darker_than_full_but_not_black",
              pen.r > 0.0 && pen.r < full.r - 1e-6);
    }

    std::cout << "\nResultado: " << passed << " ok / " << failed << " falhou\n";
    return failed == 0 ? 0 : 1;
}
