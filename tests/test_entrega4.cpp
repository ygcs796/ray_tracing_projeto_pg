/**
 * Testes unitários da Entrega 4: Iluminação Recursiva (Reflexão + Refração).
 *
 * Compilar: g++ -std=c++17 tests/test_entrega4.cpp -o test_entrega4
 * Rodar:    ./test_entrega4
 *
 * Cobertura:
 *   - reflect(D, N) = D - 2(D·N)N  (diagonal, incidência perpendicular, magnitude).
 *   - refract(D, N, n1, n2) — lei de Snell:
 *       · incidência perpendicular com n1==n2 → segue reto;
 *       · ar→vidro (1→1.5) → desvia EM DIREÇÃO à normal (ângulo diminui);
 *       · vidro→ar (1.5→1) em ângulo grande → nullopt (reflexão total interna);
 *       · vidro→ar em ângulo pequeno → vetor válido.
 *   - Recursão via computeColor (caminho real findClosestHit + recursão):
 *       · reflexão: espelho mostra a cor do objeto refletido (kr·Ir);
 *       · refração: superfície transparente mostra o objeto atrás (kt·It),
 *         exercitando o ramo de orientação "entrando" da normal;
 *       · depth=0 → preto (condição de parada);
 *       · regressão: material com kr=kt=0 dá a MESMA cor que o Phong puro.
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

/** Material zerado (todos os coeficientes 0, ni=1). Cada teste seta só o que precisa. */
static MaterialData zeroMaterial() {
    MaterialData mat;
    mat.name  = "test";
    mat.color = ColorData(0, 0, 0);
    mat.ks    = ColorData(0, 0, 0);
    mat.ka    = ColorData(0, 0, 0);
    mat.kr    = ColorData(0, 0, 0);
    mat.kt    = ColorData(0, 0, 0);
    mat.ns    = 1;
    mat.ni    = 1.0;
    mat.d     = 1.0;
    return mat;
}

/** ObjectData mínimo de esfera para os testes. */
static ObjectData makeSphereObject(double cx, double cy, double cz, double radius,
                                   const MaterialData& mat) {
    ObjectData obj;
    obj.objType = "sphere";
    obj.vetorPointData["center"] = Vetor(cx, cy, cz);
    obj.numericData["radius"]    = radius;
    obj.material = mat;
    return obj;
}

/** Constrói uma direção incidente (sin θ, -cos θ, 0): ângulo θ com a normal -y. */
static Vetor incidentAt(double thetaDeg) {
    double t = thetaDeg * 3.14159265358979323846 / 180.0;
    return Vetor(std::sin(t), -std::cos(t), 0.0);
}

int main() {
    // =======================================================================
    // reflect(D, N) = D - 2(D·N)N
    // =======================================================================
    {
        // Bola batendo no chão em 45°: D=(1,-1,0)/√2 reflete para (1,1,0)/√2.
        Vetor D = Vetor(1, -1, 0).normalize();
        Vetor N(0, 1, 0);
        Vetor R = reflect(D, N);
        Vetor expected = Vetor(1, 1, 0).normalize();
        check("reflect_diagonal_bounce",
              approxVec(R, expected.getX(), expected.getY(), expected.getZ()));
    }
    {
        // Incidência perpendicular: D=(0,-1,0) reflete de volta para (0,1,0).
        Vetor R = reflect(Vetor(0, -1, 0), Vetor(0, 1, 0));
        check("reflect_perpendicular_back", approxVec(R, 0, 1, 0));
    }
    {
        // A reflexão preserva a magnitude do vetor incidente.
        Vetor D(1, -1, 0);            // magnitude √2
        Vetor R = reflect(D, Vetor(0, 1, 0));
        check("reflect_preserves_magnitude", approx(R.magnitude(), D.magnitude()));
    }

    // =======================================================================
    // refract(D, N, n1, n2) — lei de Snell
    // =======================================================================
    {
        // Incidência perpendicular com n1==n2: o raio segue reto, sem desvio.
        Vetor D(0, -1, 0);
        Vetor N(0, 1, 0);             // aponta contra D (lado de onde D vem)
        auto T = refract(D, N, 1.5, 1.5);
        check("refract_perpendicular_straight",
              T.has_value() && approxVec(*T, 0, -1, 0));
    }
    {
        // Ar → vidro (1 → 1.5) a 45°: o raio refratado desvia EM DIREÇÃO à
        // normal, ou seja o seno (componente horizontal) diminui.
        Vetor D = incidentAt(45.0);   // |D.x| = sin45 ≈ 0.7071
        Vetor N(0, 1, 0);
        auto T = refract(D, N, 1.0, 1.5);
        bool bendsTowardNormal = T.has_value()
            && std::abs(T->getX()) < std::abs(D.getX()) - 1e-6  // ângulo menor
            && T->getY() < 0.0;                                  // continua descendo
        check("refract_air_to_glass_bends_toward_normal", bendsTowardNormal);
    }
    {
        // Vidro → ar (1.5 → 1) a 60° (acima do ângulo crítico ≈ 41.8°):
        // reflexão total interna → nullopt.
        Vetor D = incidentAt(60.0);
        Vetor N(0, 1, 0);
        auto T = refract(D, N, 1.5, 1.0);
        check("refract_glass_to_air_total_internal_reflection", !T.has_value());
    }
    {
        // Vidro → ar (1.5 → 1) a 20° (abaixo do crítico): raio válido.
        Vetor D = incidentAt(20.0);
        Vetor N(0, 1, 0);
        auto T = refract(D, N, 1.5, 1.0);
        check("refract_glass_to_air_small_angle_valid", T.has_value());
    }

    // =======================================================================
    // Recursão de reflexão via computeColor (kr · Ir)
    // =======================================================================
    {
        // Espelho perfeito (kr=1, demais 0) no ponto (0,0,0) com N=(0,0,-1)
        // virado para a câmera em (0,0,-5). O raio refletido volta em -z e
        // atinge uma esfera VERMELHA por ambiente em (0,0,-3). O pixel do
        // espelho deve mostrar exatamente a cor refletida (0.5, 0, 0).
        MaterialData mirror = zeroMaterial();
        mirror.kr = ColorData(1, 1, 1);

        MaterialData redAmbient = zeroMaterial();
        redAmbient.ka = ColorData(0.5, 0.0, 0.0);

        ObjectData redSphere = makeSphereObject(0, 0, -3, 1.0, redAmbient);
        std::vector<ObjectData> objects { redSphere };

        SceneData scene;
        scene.globalLight.color = ColorData(1, 1, 1);   // Ia = branco

        Ray ray(Ponto(0, 0, -5), Vetor(0, 0, 1));
        HitRecord hit{ 5.0, Ponto(0, 0, 0), Vetor(0, 0, -1), mirror };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes, 3);
        check("reflection_mirror_shows_reflected_color",
              approx(c.r, 0.5, 1e-9) && approx(c.g, 0.0, 1e-9) && approx(c.b, 0.0, 1e-9));
    }

    // =======================================================================
    // Recursão de refração via computeColor (kt · It) — ramo "entrando"
    // =======================================================================
    {
        // Superfície transparente (kt=1, ni=1 → sem desvio) em (0,0,0) com
        // N=(0,0,-1) virada para a câmera em (0,0,-5). O raio refratado segue
        // reto em +z e atinge uma esfera AZUL por ambiente em (0,0,3). O pixel
        // deve mostrar a cor transmitida (0, 0, 0.5). Como D·N < 0, este teste
        // exercita o ramo de orientação "entrando" (n1=1, n2=ni, N mantida).
        MaterialData glass = zeroMaterial();
        glass.kt = ColorData(1, 1, 1);
        glass.ni = 1.0;

        MaterialData blueAmbient = zeroMaterial();
        blueAmbient.ka = ColorData(0.0, 0.0, 0.5);

        ObjectData blueSphere = makeSphereObject(0, 0, 3, 1.0, blueAmbient);
        std::vector<ObjectData> objects { blueSphere };

        SceneData scene;
        scene.globalLight.color = ColorData(1, 1, 1);

        Ray ray(Ponto(0, 0, -5), Vetor(0, 0, 1));
        HitRecord hit{ 5.0, Ponto(0, 0, 0), Vetor(0, 0, -1), glass };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes, 3);
        check("refraction_transparent_shows_object_behind",
              approx(c.r, 0.0, 1e-9) && approx(c.g, 0.0, 1e-9) && approx(c.b, 0.5, 1e-9));
    }

    // =======================================================================
    // Condição de parada: depth = 0 → preto imediato
    // =======================================================================
    {
        MaterialData mirror = zeroMaterial();
        mirror.kr = ColorData(1, 1, 1);
        mirror.ka = ColorData(1, 1, 1);   // mesmo com ambiente alto, depth=0 ignora

        std::vector<ObjectData> objects;
        SceneData scene;
        scene.globalLight.color = ColorData(1, 1, 1);

        Ray ray(Ponto(0, 0, -5), Vetor(0, 0, 1));
        HitRecord hit{ 5.0, Ponto(0, 0, 0), Vetor(0, 0, -1), mirror };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes, 0);
        check("recursion_depth_zero_returns_black",
              approx(c.r, 0.0) && approx(c.g, 0.0) && approx(c.b, 0.0));
    }

    // =======================================================================
    // Regressão: kr=kt=0 → idêntico ao Phong puro da Entrega 3
    // =======================================================================
    {
        // Material só com ambiente (ka=0.5), kr=kt=0. Com Ia=0.1 a cor é 0.05
        // exatamente como na Entrega 3 — a recursão não muda nada.
        MaterialData mat = zeroMaterial();
        mat.ka = ColorData(0.5, 0.5, 0.5);

        std::vector<ObjectData> objects;
        SceneData scene;
        scene.globalLight.color = ColorData(0.1, 0.1, 0.1);

        Ray ray(Ponto(0, 0, -3), Vetor(0, 0, 1));
        HitRecord hit{ 2.0, Ponto(0, 0, -1), Vetor(0, 0, -1), mat };

        MeshLookup emptyMeshes;
        RGB c = computeColor(hit, ray, scene, objects, emptyMeshes, 3);
        check("regression_opaque_identical_to_phong",
              approx(c.r, 0.05, 1e-9) && approx(c.g, 0.05, 1e-9) && approx(c.b, 0.05, 1e-9));
    }

    std::cout << "\nResultado: " << passed << " ok / " << failed << " falhou\n";
    return failed == 0 ? 0 : 1;
}
