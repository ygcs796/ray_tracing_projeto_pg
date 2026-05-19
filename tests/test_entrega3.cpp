/**
 * Testes unitários da Entrega 3: Iluminação de Phong + Sombras.
 *
 * Compilar: g++ -std=c++17 tests/test_entrega3.cpp -o test_entrega3
 * Rodar:    ./test_entrega3
 *
 * Cobertura:
 *   - HitRecord / normais por tipo de objeto (esfera, plano).
 *   - Vetor de reflexão R = 2(L·N)N - L.
 *   - Componente ambiente (ka * Ia).
 *   - Componente difusa (max(L·N, 0) * IL * kd).
 *   - Componente especular (R·V^η).
 *   - Sombras: ponto bloqueado / sem obstáculo / obstáculo atrás da luz.
 *   - Clamping da cor final em [0,1].
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

/**
 * Constrói um ObjectData mínimo para uma esfera, pronto para ser usado nos
 * testes (com material default e nada de transforms).
 */
static ObjectData makeSphereObject(double cx, double cy, double cz, double radius,
                                   const MaterialData& mat) {
    ObjectData obj;
    obj.objType = "sphere";
    obj.vetorPointData["center"] = Vetor(cx, cy, cz);
    obj.numericData["radius"]    = radius;
    obj.material = mat;
    return obj;
}

/** Material "neutro" para os testes (ka=0, kd=branco, ks=0, ns=1). */
static MaterialData makeMaterial(double kdR, double kdG, double kdB,
                                 double ksR = 0, double ksG = 0, double ksB = 0,
                                 double kaR = 0, double kaG = 0, double kaB = 0,
                                 double ns  = 1) {
    MaterialData mat;
    mat.name  = "test";
    mat.color = ColorData(kdR, kdG, kdB);
    mat.ks    = ColorData(ksR, ksG, ksB);
    mat.ka    = ColorData(kaR, kaG, kaB);
    mat.kr    = ColorData(0, 0, 0);
    mat.kt    = ColorData(0, 0, 0);
    mat.ns    = ns;
    mat.ni    = 1.0;
    mat.d     = 1.0;
    return mat;
}

int main() {
    // -----------------------------------------------------------------------
    // Normais via HitRecord — esfera
    // -----------------------------------------------------------------------
    {
        // Raio (-2,0,0)+t(1,0,0) bate em esfera centrada na origem com raio 1
        // no ponto (-1,0,0) → N = (-1,0,0).
        MaterialData mat = makeMaterial(1, 1, 1);
        ObjectData sphere = makeSphereObject(0, 0, 0, 1.0, mat);
        Ray ray(Ponto(-2, 0, 0), Vetor(1, 0, 0));
        auto hit = intersectSphereHit(ray, sphere);
        check("sphere_hit_normal_minus_x",
              hit.has_value() && approxVec(hit->normal, -1, 0, 0));
    }
    {
        // Raio (0,2,0)+t(0,-1,0) bate em esfera (0,0,0,r=1) no ponto (0,1,0)
        // → N = (0,1,0).
        MaterialData mat = makeMaterial(1, 1, 1);
        ObjectData sphere = makeSphereObject(0, 0, 0, 1.0, mat);
        Ray ray(Ponto(0, 2, 0), Vetor(0, -1, 0));
        auto hit = intersectSphereHit(ray, sphere);
        check("sphere_hit_normal_plus_y",
              hit.has_value() && approxVec(hit->normal, 0, 1, 0));
    }

    // -----------------------------------------------------------------------
    // Normais via HitRecord — plano
    // -----------------------------------------------------------------------
    {
        // Plano y=0 com normal (0,1,0). Raio vindo de cima bate em (0,0,0).
        MaterialData mat = makeMaterial(1, 1, 1);
        ObjectData plane;
        plane.objType = "plane";
        plane.relativePos = Ponto(0, 0, 0);
        plane.vetorPointData["normal"] = Vetor(0, 1, 0);
        plane.material = mat;
        Ray ray(Ponto(0, 5, 0), Vetor(0, -1, 0));
        auto hit = intersectPlaneHit(ray, plane);
        check("plane_hit_normal_plus_y",
              hit.has_value() && approxVec(hit->normal, 0, 1, 0));
    }

    // -----------------------------------------------------------------------
    // Vetor de reflexão R = 2(L·N)N - L
    // -----------------------------------------------------------------------
    {
        // L = N → R = N (reflexão perfeita ao longo da normal).
        Vetor N(0, 1, 0);
        Vetor L = N;
        double NdotL = N.dot(L);
        Vetor R = (N * (2.0 * NdotL) - L).normalize();
        check("reflect_L_equals_N_gives_N", approxVec(R, 0, 1, 0));
    }
    {
        // L = normalize(1,1,0), N = (0,1,0).
        // 2(L·N) = 2*(1/sqrt(2)) = sqrt(2).
        // R = sqrt(2)*N - L  = (0, sqrt(2), 0) - (1/√2, 1/√2, 0)
        //   = (-1/√2, 1/√2, 0) → normalize = (-1/√2, 1/√2, 0).
        Vetor N(0, 1, 0);
        Vetor L = Vetor(1, 1, 0).normalize();
        double NdotL = N.dot(L);
        Vetor R = (N * (2.0 * NdotL) - L).normalize();
        double s = 1.0 / std::sqrt(2.0);
        check("reflect_diagonal_to_xy", approxVec(R, -s, s, 0, 1e-9));
    }

    // -----------------------------------------------------------------------
    // Componente ambiente: ka * Ia
    // -----------------------------------------------------------------------
    {
        // Cena com luz ambiente (0.1,0.1,0.1), esfera com ka=(0.5,0.5,0.5),
        // sem luzes pontuais → resultado = (0.05, 0.05, 0.05).
        MaterialData mat = makeMaterial(/*kd*/0, 0, 0,
                                        /*ks*/0, 0, 0,
                                        /*ka*/0.5, 0.5, 0.5,
                                        /*ns*/1);
        ObjectData sphere = makeSphereObject(0, 0, 0, 1.0, mat);
        std::vector<ObjectData> objects { sphere };

        SceneData scene;
        scene.globalLight.color = ColorData(0.1, 0.1, 0.1);
        // sem lightList

        Ray ray(Ponto(0, 0, -3), Vetor(0, 0, 1));
        HitRecord hit{ 2.0, Ponto(0, 0, -1), Vetor(0, 0, -1), mat };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes);
        check("ambient_only_component",
              approx(c.r, 0.05, 1e-9) && approx(c.g, 0.05, 1e-9) && approx(c.b, 0.05, 1e-9));
    }

    // -----------------------------------------------------------------------
    // Componente difusa: L alinhado com N → contribuição máxima
    // -----------------------------------------------------------------------
    {
        // Esfera em (0,0,0,r=1) com kd=(1,0,0), ka=0, ks=0. Luz pontual
        // diretamente "acima" do ponto de hit em (0,0,-1) com normal (0,0,-1):
        // colocamos a luz em (0,0,-3) — então L = (0,0,-1) = N.
        // IL = (1,1,1) → difusa esperada = 1·1·1 = 1 (canal R).
        MaterialData mat = makeMaterial(/*kd*/1, 0, 0,
                                        /*ks*/0, 0, 0,
                                        /*ka*/0, 0, 0,
                                        /*ns*/1);
        std::vector<ObjectData> objects; // sem objetos = sem sombra
        SceneData scene;
        scene.globalLight.color = ColorData(0, 0, 0);
        LightData light;
        light.pos = Ponto(0, 0, -3);
        light.color = ColorData(1, 1, 1);
        scene.lightList.push_back(light);

        Ray ray(Ponto(0, 0, -5), Vetor(0, 0, 1));
        HitRecord hit{ 4.0, Ponto(0, 0, -1), Vetor(0, 0, -1), mat };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes);
        check("diffuse_aligned_max",
              approx(c.r, 1.0, 1e-9) && approx(c.g, 0.0, 1e-9) && approx(c.b, 0.0, 1e-9));
    }

    // -----------------------------------------------------------------------
    // Componente difusa: L perpendicular a N → contribuição zero
    // -----------------------------------------------------------------------
    {
        // Mesma geometria, mas com luz em (3,0,-1) (mesmo z do hit) → L
        // perpendicular a N. Difusa deve dar 0; sem ambiente nem especular,
        // a cor final é preta.
        MaterialData mat = makeMaterial(/*kd*/1, 1, 1,
                                        /*ks*/0, 0, 0,
                                        /*ka*/0, 0, 0,
                                        /*ns*/1);
        std::vector<ObjectData> objects;
        SceneData scene;
        scene.globalLight.color = ColorData(0, 0, 0);
        LightData light;
        light.pos = Ponto(3, 0, -1);
        light.color = ColorData(1, 1, 1);
        scene.lightList.push_back(light);

        Ray ray(Ponto(0, 0, -5), Vetor(0, 0, 1));
        HitRecord hit{ 4.0, Ponto(0, 0, -1), Vetor(0, 0, -1), mat };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes);
        check("diffuse_perpendicular_zero",
              approx(c.r, 0.0, 1e-9) && approx(c.g, 0.0, 1e-9) && approx(c.b, 0.0, 1e-9));
    }

    // -----------------------------------------------------------------------
    // Componente especular: alinhamento L = V = N → brilho máximo
    // -----------------------------------------------------------------------
    {
        // Hit em (0,0,-1) com N=(0,0,-1). Câmera em (0,0,-5) → V = (0,0,-1).
        // Luz em (0,0,-5) também → L = (0,0,-1). R = N. R·V = 1.
        // Sem ambiente, sem difusa (kd=0). ks=(1,1,1), η qualquer.
        // Esperado: cor = (1,1,1).
        MaterialData mat = makeMaterial(/*kd*/0, 0, 0,
                                        /*ks*/1, 1, 1,
                                        /*ka*/0, 0, 0,
                                        /*ns*/32);
        std::vector<ObjectData> objects;
        SceneData scene;
        scene.globalLight.color = ColorData(0, 0, 0);
        LightData light;
        light.pos = Ponto(0, 0, -5);
        light.color = ColorData(1, 1, 1);
        scene.lightList.push_back(light);

        Ray ray(Ponto(0, 0, -5), Vetor(0, 0, 1));
        HitRecord hit{ 4.0, Ponto(0, 0, -1), Vetor(0, 0, -1), mat };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes);
        check("specular_aligned_full",
              approx(c.r, 1.0, 1e-9) && approx(c.g, 1.0, 1e-9) && approx(c.b, 1.0, 1e-9));
    }

    // -----------------------------------------------------------------------
    // Sombra: ponto atrás de uma esfera em relação à luz → bloqueado
    // -----------------------------------------------------------------------
    {
        // Esfera grande (r=1) centrada em (0,0,0). Ponto sombreado em (-5,0,0)
        // com normal apontando para a luz +X. Luz em (+5,0,0). A esfera está
        // ENTRE o ponto e a luz na linha x=0.
        MaterialData mat = makeMaterial(1, 1, 1);
        ObjectData blocker = makeSphereObject(0, 0, 0, 1.0, mat);
        std::vector<ObjectData> objects { blocker };

        LightData light;
        light.pos = Ponto(5, 0, 0);
        light.color = ColorData(1, 1, 1);

        Ponto P(-5, 0, 0);
        Vetor N(1, 0, 0);

        MeshLookup emptyMeshes;
        bool blocked = isInShadow(P, N, light, objects, emptyMeshes);
        check("shadow_blocked_by_sphere", blocked);
    }

    // -----------------------------------------------------------------------
    // Sombra: sem obstáculo → não em sombra
    // -----------------------------------------------------------------------
    {
        // Mesma geometria, mas sem nenhum objeto na cena.
        std::vector<ObjectData> objects;
        LightData light;
        light.pos = Ponto(5, 0, 0);
        light.color = ColorData(1, 1, 1);

        Ponto P(-5, 0, 0);
        Vetor N(1, 0, 0);

        MeshLookup emptyMeshes;
        bool blocked = isInShadow(P, N, light, objects, emptyMeshes);
        check("shadow_no_blocker", !blocked);
    }

    // -----------------------------------------------------------------------
    // Sombra: objeto ATRÁS da luz → não bloqueia
    // -----------------------------------------------------------------------
    {
        // Ponto em P=(-5,0,0) com N=(1,0,0). Luz em (1,0,0). Obstáculo seria
        // em (5,0,0) — atrás da luz. t do raio de sombra até a esfera ≈ 5,
        // mas a luz está a t=6 (medida de P+ε*N até a luz), espera, vamos
        // recalcular: distLuz = |luz - P_off| ≈ 6. distEsfera ≈ 9 (centro a 10
        // de P, raio 1). Como 9 > 6, a esfera está ATRÁS da luz → NÃO bloqueia.
        MaterialData mat = makeMaterial(1, 1, 1);
        ObjectData behindLight = makeSphereObject(5, 0, 0, 1.0, mat);
        std::vector<ObjectData> objects { behindLight };

        LightData light;
        light.pos = Ponto(1, 0, 0);
        light.color = ColorData(1, 1, 1);

        Ponto P(-5, 0, 0);
        Vetor N(1, 0, 0);

        MeshLookup emptyMeshes;
        bool blocked = isInShadow(P, N, light, objects, emptyMeshes);
        check("shadow_object_behind_light_does_not_block", !blocked);
    }

    // -----------------------------------------------------------------------
    // Clamping: cor estourada deve ser truncada em 1.0
    // -----------------------------------------------------------------------
    {
        // Forçamos um cenário de overflow: ka=2, Ia=1 → ambiente = 2 (passa
        // de 1). O computeColor deve clampar em 1.0.
        MaterialData mat = makeMaterial(/*kd*/0, 0, 0,
                                        /*ks*/0, 0, 0,
                                        /*ka*/2.0, 2.0, 2.0,
                                        /*ns*/1);
        std::vector<ObjectData> objects;
        SceneData scene;
        scene.globalLight.color = ColorData(1, 1, 1);

        Ray ray(Ponto(0, 0, -5), Vetor(0, 0, 1));
        HitRecord hit{ 4.0, Ponto(0, 0, -1), Vetor(0, 0, -1), mat };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes);
        check("clamp_overflow_to_one",
              approx(c.r, 1.0, 1e-9) && approx(c.g, 1.0, 1e-9) && approx(c.b, 1.0, 1e-9));
    }

    std::cout << "\nResultado: " << passed << " ok / " << failed << " falhou\n";
    return failed == 0 ? 0 : 1;
}
