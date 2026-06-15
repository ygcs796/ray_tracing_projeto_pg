/**
 * Testes unitários da Entrega 1: Vetor, Ponto, Ray, Camera e interseções básicas.
 *
 * Compilar: g++ -std=c++17 tests/test_entrega1.cpp -o test_entrega1
 * Rodar:    ./test_entrega1
 */

#include <iostream>
#include <cmath>
#include "../src/Vetor.h"
#include "../src/Ponto.h"
#include "../src/Ray.h"
#include "../src/Camera.h"
#include "../src/Intersect.h"

static int passed = 0, failed = 0;

static bool approx(double a, double b, double tol = 1e-9) { return std::abs(a - b) < tol; }
static bool approxV(const Vetor& v, double x, double y, double z, double tol = 1e-9) {
    return approx(v.getX(), x, tol) && approx(v.getY(), y, tol) && approx(v.getZ(), z, tol);
}
static bool approxP(const Ponto& p, double x, double y, double z, double tol = 1e-9) {
    return approx(p.getX(), x, tol) && approx(p.getY(), y, tol) && approx(p.getZ(), z, tol);
}
static void check(const std::string& name, bool ok) {
    if (ok) { std::cout << "[OK]   " << name << "\n"; ++passed; }
    else    { std::cout << "[FAIL] " << name << "\n"; ++failed; }
}

int main() {
    // ========================================================================
    // Vetor — operações básicas
    // ========================================================================
    {
        Vetor a(1, 2, 3), b(4, 5, 6);
        check("vetor_soma",     approxV(a + b, 5, 7, 9));
        check("vetor_subtracao", approxV(b - a, 3, 3, 3));
        check("vetor_negacao",  approxV(-a, -1, -2, -3));
        check("vetor_escalar",  approxV(a * 2, 2, 4, 6));
    }

    // dot product: (1,0,0)·(0,1,0) = 0; (1,2,3)·(4,5,6) = 32
    {
        check("dot_perpendicular", approx(Vetor(1,0,0).dot(Vetor(0,1,0)), 0));
        check("dot_geral",         approx(Vetor(1,2,3).dot(Vetor(4,5,6)), 32));
    }

    // cross product: (1,0,0) × (0,1,0) = (0,0,1) (regra da mão direita)
    {
        check("cross_xy_to_z",     approxV(Vetor(1,0,0).cross(Vetor(0,1,0)), 0, 0, 1));
        check("cross_anticomutativo",
              approxV(Vetor(1,0,0).cross(Vetor(0,1,0)),
                      -Vetor(0,1,0).cross(Vetor(1,0,0)).getX(),
                      -Vetor(0,1,0).cross(Vetor(1,0,0)).getY(),
                      -Vetor(0,1,0).cross(Vetor(1,0,0)).getZ()));
    }

    // magnitude e normalize: |(3,4,0)| = 5; normalize tem magnitude 1
    {
        check("magnitude_3_4_5", approx(Vetor(3,4,0).magnitude(), 5));
        Vetor n = Vetor(3, 4, 0).normalize();
        check("normalize_unitario", approx(n.magnitude(), 1.0, 1e-9));
        check("normalize_zero_safe", approxV(Vetor(0,0,0).normalize(), 0, 0, 0));
    }

    // ========================================================================
    // Ponto — semântica geométrica
    // ========================================================================
    {
        Ponto P(1, 2, 3);
        Vetor v(10, 20, 30);
        check("ponto_mais_vetor", approxP(P + v, 11, 22, 33));
        check("ponto_menos_vetor", approxP(P - v, -9, -18, -27));
        check("ponto_menos_ponto_da_vetor",
              approxV(Ponto(5,7,9) - Ponto(1,2,3), 4, 5, 6));
    }

    // ========================================================================
    // Ray::at(t) = origin + t·direction
    // ========================================================================
    {
        Ray r(Ponto(0,0,0), Vetor(1,0,0));
        check("ray_at_t0", approxP(r.at(0), 0, 0, 0));
        check("ray_at_t3", approxP(r.at(3), 3, 0, 0));
        check("ray_at_negativo", approxP(r.at(-2), -2, 0, 0));  // andar pra trás
    }

    // ========================================================================
    // Camera — base ortonormal e geração de raios
    // ========================================================================
    {
        // Câmera no Cornell Box: lookfrom=(0,0,-800), lookat=(0,0,0), Vup=(0,1,0)
        // Esperado: backward = (0, 0, -1); right = (-1, 0, 0); up = (0, -1, 0)
        // (a convenção usada inverte o cross, ver comentários em Camera.h).
        CameraData cd;
        cd.lookfrom = Ponto(0, 0, -800);
        cd.lookat   = Ponto(0, 0, 0);
        cd.upVector = Vetor(0, 1, 0);
        cd.image_width = 100;
        cd.image_height = 100;
        cd.screen_distance = 1.0;
        Camera cam(cd);

        // backward = (lookfrom - lookat).normalize() = (0,0,-800).normalize() = (0,0,-1)
        check("camera_backward", approxV(cam.getBackward(), 0, 0, -1, 1e-9));

        // Convenção das anotações da disciplina: U = Vup × W (e NÃO W × Vup).
        // right = upVector × backward = (0,1,0) × (0,0,-1)
        //       = (1*(-1) - 0*0, 0*0 - 0*(-1), 0*0 - 1*0) = (-1, 0, 0).
        check("camera_right", approxV(cam.getRight(), -1, 0, 0, 1e-9));

        // up = right × backward = (1,0,0) × (0,0,-1) = (0*(-1)-0*0, 0*0-1*(-1), 1*0-0*0) = (0, 1, 0)
        check("camera_up", approxV(cam.getUp(), 0, 1, 0, 1e-9));

        // Raio do pixel central deve apontar aproximadamente para frente (-backward).
        Ray central = cam.generateRay(50, 50);
        check("camera_raio_central_apontando_para_frente",
              central.direction.getZ() > 0.9);  // direção é +Z (-backward)
    }

    // ========================================================================
    // Interseção raio-esfera
    // ========================================================================
    {
        // Raio de (0,0,0) em direção +Z; esfera centro (0,0,5) raio 1
        // Esperado: t = 4 (entrada da esfera)
        Ray r(Ponto(0,0,0), Vetor(0,0,1));
        auto t = intersectSphere(r, Ponto(0,0,5), 1.0);
        check("esfera_hit_entrada", t.has_value() && approx(*t, 4.0, 1e-9));
    }
    {
        // Raio em direção +X mas esfera está em +Z → miss
        Ray r(Ponto(0,0,0), Vetor(1,0,0));
        auto t = intersectSphere(r, Ponto(0,0,5), 1.0);
        check("esfera_miss", !t.has_value());
    }
    {
        // Origem dentro da esfera: raio sai pelo lado oposto
        Ray r(Ponto(0,0,0), Vetor(0,0,1));
        auto t = intersectSphere(r, Ponto(0,0,0), 1.0);
        check("esfera_origem_dentro", t.has_value() && approx(*t, 1.0, 1e-9));
    }
    {
        // Esfera atrás da câmera → miss
        Ray r(Ponto(0,0,0), Vetor(0,0,1));
        auto t = intersectSphere(r, Ponto(0,0,-5), 1.0);
        check("esfera_atras_da_camera", !t.has_value());
    }

    // ========================================================================
    // Interseção raio-plano
    // ========================================================================
    {
        // Raio descendo: O=(0,5,0), D=(0,-1,0); plano y=0 com normal (0,1,0)
        // Esperado: t = 5
        Ray r(Ponto(0,5,0), Vetor(0,-1,0));
        auto t = intersectPlane(r, Ponto(0,0,0), Vetor(0,1,0));
        check("plano_hit_t5", t.has_value() && approx(*t, 5.0, 1e-9));
    }
    {
        // Raio paralelo ao plano (dot(D,N)=0) → miss
        Ray r(Ponto(0,5,0), Vetor(1,0,0));
        auto t = intersectPlane(r, Ponto(0,0,0), Vetor(0,1,0));
        check("plano_paralelo_miss", !t.has_value());
    }
    {
        // Plano atrás da câmera (raio sobe, plano em y=0 abaixo) → miss
        Ray r(Ponto(0,5,0), Vetor(0,1,0));
        auto t = intersectPlane(r, Ponto(0,0,0), Vetor(0,1,0));
        check("plano_atras_miss", !t.has_value());
    }

    std::cout << "\nResultado: " << passed << " ok / " << failed << " falhou\n";
    return failed == 0 ? 0 : 1;
}
