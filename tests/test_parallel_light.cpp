/**
 * Testes da feature extra: Iluminação de raios paralelos (fonte retangular).
 *
 * Compilar: g++ -std=c++17 tests/test_parallel_light.cpp -o test_parallel_light
 * Rodar:    ./test_parallel_light
 *
 * Cobertura:
 *   - isParallelLight / lightScalar (detecção e leitura dos campos do JSON).
 *   - parallelLightReaches:
 *       · ponto DENTRO do feixe, sem oclusor → iluminado, L = -d;
 *       · ponto FORA do feixe (além da largura/altura) → não iluminado;
 *       · ponto "rio acima" da janela (s ≤ 0) → não iluminado;
 *       · ponto dentro do feixe mas OCLUÍDO até a janela → não iluminado;
 *       · raios PARALELOS: dois pontos distintos no feixe têm o MESMO L.
 *   - computeColor: ponto no feixe recebe difusa; fora do feixe só ambiente.
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
static bool approxVec(const Vetor& v, double x, double y, double z, double tol = 1e-9) {
    return approx(v.getX(), x, tol) && approx(v.getY(), y, tol) && approx(v.getZ(), z, tol);
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

static MaterialData makeMaterial(double kd, double ka) {
    MaterialData m;
    m.name  = "test";
    m.color = ColorData(kd, kd, kd);
    m.ks    = ColorData(0, 0, 0);
    m.ka    = ColorData(ka, ka, ka);
    m.kr    = ColorData(0, 0, 0);
    m.kt    = ColorData(0, 0, 0);
    m.ns    = 1; m.ni = 1.0; m.d = 1.0;
    return m;
}

/** Luz de raios paralelos: retângulo centrado em `center`, direção `dir`, w×h. */
static LightData makeParallelLight(Ponto center, Vetor dir, double w, double h) {
    LightData L;
    L.pos   = center;
    L.color = ColorData(1, 1, 1);
    L.extraData["kind"]   = "parallel";
    L.extraData["dir_x"]  = std::to_string(dir.getX());
    L.extraData["dir_y"]  = std::to_string(dir.getY());
    L.extraData["dir_z"]  = std::to_string(dir.getZ());
    L.extraData["width"]  = std::to_string(w);
    L.extraData["height"] = std::to_string(h);
    return L;
}

int main() {
    MeshLookup noMeshes;

    // -----------------------------------------------------------------------
    // Detecção e leitura de parâmetros.
    // -----------------------------------------------------------------------
    {
        LightData ponto; ponto.pos = Ponto(0, 100, 0); ponto.color = ColorData(1, 1, 1);
        LightData paral = makeParallelLight(Ponto(0, 100, 0), Vetor(0, -1, 0), 40, 40);
        check("detect_point_light_is_not_parallel", !isParallelLight(ponto));
        check("detect_parallel_light", isParallelLight(paral));
        check("scalar_read_width", approx(lightScalar(paral, "width", -1), 40.0));
        check("scalar_default_when_absent", approx(lightScalar(paral, "naoexiste", 7.0), 7.0));
    }

    // Janela em y=100 apontando para baixo, feixe quadrado 40×40 (|x|,|z| ≤ 20).
    LightData sun = makeParallelLight(Ponto(0, 100, 0), Vetor(0, -1, 0), 40, 40);

    // -----------------------------------------------------------------------
    // Ponto DENTRO do feixe, sem oclusor → iluminado; L = -d = (0,1,0).
    // -----------------------------------------------------------------------
    {
        std::vector<ObjectData> none;
        Ponto P(0, 0, 0); Vetor N(0, 1, 0); Vetor L;
        bool lit = parallelLightReaches(P, N, sun, none, noMeshes, L);
        check("inside_beam_is_lit", lit && approxVec(L, 0, 1, 0));
    }

    // -----------------------------------------------------------------------
    // Ponto FORA do feixe (x = 50 > meia-largura 20) → não iluminado.
    // -----------------------------------------------------------------------
    {
        std::vector<ObjectData> none;
        Ponto P(50, 0, 0); Vetor N(0, 1, 0); Vetor L;
        check("outside_beam_not_lit", !parallelLightReaches(P, N, sun, none, noMeshes, L));
    }

    // -----------------------------------------------------------------------
    // Ponto "rio acima" da janela (y = 150 > 100, s ≤ 0) → não iluminado.
    // -----------------------------------------------------------------------
    {
        std::vector<ObjectData> none;
        Ponto P(0, 150, 0); Vetor N(0, 1, 0); Vetor L;
        check("upstream_of_window_not_lit", !parallelLightReaches(P, N, sun, none, noMeshes, L));
    }

    // -----------------------------------------------------------------------
    // Dentro do feixe mas OCLUÍDO (esfera entre P e a janela) → não iluminado.
    // -----------------------------------------------------------------------
    {
        std::vector<ObjectData> objs { makeSphereObject(0, 50, 0, 10.0, makeMaterial(1, 0)) };
        Ponto P(0, 0, 0); Vetor N(0, 1, 0); Vetor L;
        check("inside_beam_but_occluded_not_lit",
              !parallelLightReaches(P, N, sun, objs, noMeshes, L));
    }

    // -----------------------------------------------------------------------
    // Raios PARALELOS: dois pontos distintos no feixe têm o MESMO L.
    // -----------------------------------------------------------------------
    {
        std::vector<ObjectData> none;
        Vetor L1, L2;
        parallelLightReaches(Ponto(0, 0, 0),   Vetor(0, 1, 0), sun, none, noMeshes, L1);
        parallelLightReaches(Ponto(10, 0, 10), Vetor(0, 1, 0), sun, none, noMeshes, L2);
        check("parallel_rays_same_direction",
              approxVec(L1, L2.getX(), L2.getY(), L2.getZ()) && approxVec(L1, 0, 1, 0));
    }

    // -----------------------------------------------------------------------
    // Direção inclinada: feixe diagonal ainda dá L = -d normalizado.
    // -----------------------------------------------------------------------
    {
        LightData diag = makeParallelLight(Ponto(0, 100, 0), Vetor(1, -1, 0), 400, 400);
        std::vector<ObjectData> none;
        Ponto P(0, 0, 0); Vetor N(0, 1, 0); Vetor L;
        bool lit = parallelLightReaches(P, N, diag, none, noMeshes, L);
        double s = 1.0 / std::sqrt(2.0);
        check("tilted_beam_L_is_minus_d_normalized", lit && approxVec(L, -s, s, 0, 1e-9));
    }

    // -----------------------------------------------------------------------
    // computeColor: dentro do feixe recebe difusa; fora só ambiente.
    // -----------------------------------------------------------------------
    {
        MaterialData mat = makeMaterial(/*kd*/0.7, /*ka*/0.2);
        SceneData scene; scene.globalLight.color = ColorData(0.5, 0.5, 0.5);  // Ia → ambiente 0.1
        scene.lightList.push_back(sun);
        std::vector<ObjectData> none;

        // Dentro do feixe: ambiente 0.1 + difusa 0.7·1 = 0.8.
        HitRecord hitIn{ 1.0, Ponto(0, 0, 0), Vetor(0, 1, 0), mat };
        Ray rayIn(Ponto(0, 1, 0), Vetor(0, -1, 0));   // observador acima → sem flip
        RGB cin = computeColor(hitIn, rayIn, scene, none, noMeshes);
        check("computeColor_inside_beam_has_diffuse", approx(cin.r, 0.8, 1e-9));

        // Fora do feixe: só ambiente 0.1.
        HitRecord hitOut{ 1.0, Ponto(50, 0, 0), Vetor(0, 1, 0), mat };
        Ray rayOut(Ponto(50, 1, 0), Vetor(0, -1, 0));
        RGB cout = computeColor(hitOut, rayOut, scene, none, noMeshes);
        check("computeColor_outside_beam_ambient_only", approx(cout.r, 0.1, 1e-9));
    }

    std::cout << "\nResultado: " << passed << " ok / " << failed << " falhou\n";
    return failed == 0 ? 0 : 1;
}
