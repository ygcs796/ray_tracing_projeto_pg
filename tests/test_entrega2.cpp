/**
 * Testes unitários da Entrega 2: Matrix4x4 e interseção raio-triângulo.
 *
 * Compilar: g++ -std=c++17 tests/test_entrega2.cpp -o test_entrega2
 * Rodar:    ./test_entrega2
 */

#include <iostream>
#include <cmath>
#include <cstdlib>
#include "../src/Matrix.h"
#include "../src/Ponto.h"
#include "../src/Vetor.h"
#include "../src/Ray.h"
#include "../src/Intersect.h"

static int passed = 0, failed = 0;

static bool approx(double a, double b, double tol = 1e-9) {
    return std::abs(a - b) < tol;
}

static bool approxPoint(const Ponto& p, double x, double y, double z, double tol = 1e-9) {
    return approx(p.getX(), x, tol) && approx(p.getY(), y, tol) && approx(p.getZ(), z, tol);
}

static bool approxVec(const Vetor& v, double x, double y, double z, double tol = 1e-9) {
    return approx(v.getX(), x, tol) && approx(v.getY(), y, tol) && approx(v.getZ(), z, tol);
}

static void check(const std::string& name, bool ok) {
    if (ok) { std::cout << "[OK]   " << name << "\n"; ++passed; }
    else    { std::cout << "[FAIL] " << name << "\n"; ++failed; }
}

int main() {
    // Matrix4x4 — fundamentos
    {
        Matrix4x4 I = Matrix4x4::identity();
        check("identity_point", approxPoint(I.transformPoint(Ponto(3, -2, 7)), 3, -2, 7));
    }
    {
        Matrix4x4 T = Matrix4x4::translation(1, 2, 3);
        check("translation_point", approxPoint(T.transformPoint(Ponto(0, 0, 0)), 1, 2, 3));
    }
    {
        Matrix4x4 T = Matrix4x4::translation(1, 2, 3);
        check("translation_vector_unchanged", approxVec(T.transformVector(Vetor(1, 0, 0)), 1, 0, 0));
    }
    {
        Matrix4x4 S = Matrix4x4::scaling(2, 2, 2);
        check("scaling_point", approxPoint(S.transformPoint(Ponto(1, 1, 1)), 2, 2, 2));
    }
    {
        Matrix4x4 R = Matrix4x4::rotationZ(M_PI / 2.0);
        check("rotation_z_90", approxPoint(R.transformPoint(Ponto(1, 0, 0)), 0, 1, 0, 1e-9));
    }
    {
        Matrix4x4 M = Matrix4x4::translation(1, 2, 3) * Matrix4x4::scaling(2, 3, 4) * Matrix4x4::rotationY(0.7);
        Matrix4x4 P = M * M.inverse();
        bool ok = true;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (!approx(P.at(i, j), (i == j ? 1.0 : 0.0), 1e-9)) ok = false;
        check("M_times_inverse_is_identity", ok);
    }
    {
        Matrix4x4 M = Matrix4x4::translation(1, 0, 0) * Matrix4x4::scaling(2, 2, 2);
        check("composition_T_then_S", approxPoint(M.transformPoint(Ponto(1, 0, 0)), 3, 0, 0));
    }
    {
        Matrix4x4 S = Matrix4x4::scaling(2, 1, 1);
        check("normal_under_nonuniform_scale", approxVec(S.transformNormal(Vetor(1, 0, 0)), 1, 0, 0, 1e-9));
    }

    // Möller-Trumbore
    {
        Ray r(Ponto(0, 0, -1), Vetor(0, 0, 1));
        Ponto v0(-1, -1, 0), v1(1, -1, 0), v2(0, 1, 0);
        auto hit = intersectTriangle(r, v0, v1, v2);
        check("triangle_hit_t1", hit.has_value() && approx(hit->distanceAlongRay, 1.0, 1e-9));
        if (hit.has_value()) {
            check("triangle_hit_baricentricas_validas",
                  hit->alpha >= 0 && hit->beta >= 0 && hit->gamma >= 0 &&
                  approx(hit->alpha + hit->beta + hit->gamma, 1.0, 1e-9));
        }
    }
    {
        Ray r(Ponto(5, 5, -1), Vetor(0, 0, 1));
        Ponto v0(-1, -1, 0), v1(1, -1, 0), v2(0, 1, 0);
        check("triangle_miss", !intersectTriangle(r, v0, v1, v2).has_value());
    }
    {
        Ray r(Ponto(0, 0, -1), Vetor(1, 0, 0));
        Ponto v0(-1, -1, 0), v1(1, -1, 0), v2(0, 1, 0);
        check("triangle_parallel", !intersectTriangle(r, v0, v1, v2).has_value());
    }

    // Extensões
    {
        Matrix4x4 S = Matrix4x4::scalingAroundPoint(2, 2, 2, Ponto(5, 0, 0));
        check("scaling_around_point_pivot_fixed",
              approxPoint(S.transformPoint(Ponto(5, 0, 0)), 5, 0, 0));
        check("scaling_around_point_stretched",
              approxPoint(S.transformPoint(Ponto(7, 0, 0)), 9, 0, 0));
    }
    {
        Matrix4x4 R1 = Matrix4x4::rotationAroundAxis(M_PI / 2.0, Vetor(0, 1, 0));
        Matrix4x4 R2 = Matrix4x4::rotationY(M_PI / 2.0);
        bool ok = true;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (!approx(R1.at(i, j), R2.at(i, j), 1e-9)) ok = false;
        check("rotation_axis_matches_rotationY", ok);
    }
    {
        // Rotação 120° em torno do eixo (1,1,1) cicla os eixos: x → y → z → x
        Matrix4x4 R = Matrix4x4::rotationAroundAxis(2.0 * M_PI / 3.0, Vetor(1, 1, 1));
        Ponto p1 = R.transformPoint(Ponto(1, 0, 0));
        Ponto p3 = R.transformPoint(R.transformPoint(p1));
        check("rotation_axis_arbitrary_120deg", approxPoint(p1, 0, 1, 0, 1e-9));
        check("rotation_axis_three_steps_full_turn", approxPoint(p3, 1, 0, 0, 1e-9));
    }
    {
        // Afim 3 correspondências reproduzindo translação (1,2,3)
        Ponto A(0, 0, 0), B(1, 0, 0), C(0, 1, 0);
        Ponto A2(1, 2, 3), B2(2, 2, 3), C2(1, 3, 3);
        Matrix4x4 M = Matrix4x4::affineFromCorrespondences(A, B, C, A2, B2, C2);
        check("affine_3pts_recreates_translation",
              approxPoint(M.transformPoint(Ponto(0, 0, 0)), 1, 2, 3, 1e-7));
    }
    {
        // Afim 3 correspondências reproduzindo escala (2,3,1)
        Ponto A(0, 0, 0), B(1, 0, 0), C(0, 1, 0);
        Ponto A2(0, 0, 0), B2(2, 0, 0), C2(0, 3, 0);
        Matrix4x4 M = Matrix4x4::affineFromCorrespondences(A, B, C, A2, B2, C2);
        check("affine_3pts_recreates_scaling",
              approxPoint(M.transformPoint(Ponto(1, 1, 0)), 2, 3, 0, 1e-7));
    }

    std::cout << "\nResultado: " << passed << " ok / " << failed << " falhou\n";
    return failed == 0 ? 0 : 1;
}
