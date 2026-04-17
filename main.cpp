/**
 * main.cpp — Entrega 1 (Ray-Caster Básico)
 *
 * Lê uma cena JSON, lança raios primários da câmera para cada pixel,
 * testa interseção com os objetos (esferas e planos) e pinta cada pixel
 * com a cor difusa (kd) do objeto mais próximo atingido.
 *
 * Sem iluminação, sombras ou recursão — apenas ray-casting bruto.
 *
 * Uso:
 *     g++ -std=c++17 -O2 main.cpp -o raytracer
 *     ./raytracer > imagem.ppm
 *     python utils/convert_ppm.py imagem.ppm imagem.png
 *
 * O caminho da cena pode ser passado como argumento; se omitido, usa sampleScene.json.
 */

#include <iostream>
#include <limits>
#include <algorithm>
#include <string>

#include "src/Camera.h"
#include "src/Intersect.h"
#include "utils/Scene/sceneParser.cpp"

/** Converte um componente de cor em [0,1] para [0,255] com clamping. */
static int toByte(double c) {
    int v = static_cast<int>(c * 255.0);
    return std::clamp(v, 0, 255);
}

int main(int argc, char** argv) {
    // Cena default = sampleScene (Cornell Box)
    std::string sceneFile = "utils/input/sampleScene.json";
    if (argc >= 2) sceneFile = argv[1];

    SceneData scene;
    try {
        scene = SceneJsonLoader::loadFile(sceneFile);
    } catch (const std::exception& e) {
        std::cerr << "Erro ao carregar cena '" << sceneFile << "': " << e.what() << "\n";
        return 1;
    }

    Camera camera(scene.camera);
    const int hres = scene.camera.image_width;
    const int vres = scene.camera.image_height;

    std::cerr << "Renderizando " << hres << "x" << vres
              << " com " << scene.objects.size() << " objetos...\n";

    // Header PPM (formato P3, ASCII)
    std::cout << "P3\n" << hres << " " << vres << "\n255\n";

    for (int j = 0; j < vres; ++j) {
        // Progresso na stderr (não polui a saída PPM que vai para stdout)
        if (j % 10 == 0 || j == vres - 1) {
            std::cerr << "\rLinha " << (j + 1) << "/" << vres << std::flush;
        }

        for (int i = 0; i < hres; ++i) {
            Ray ray = camera.generateRay(i, j);

            // Varre todos os objetos e mantém o hit mais próximo (menor t > 0)
            double closest_t = std::numeric_limits<double>::infinity();
            const ObjectData* closest_obj = nullptr;

            for (auto& obj : scene.objects) {
                auto t = intersect(ray, obj);
                if (t.has_value() && *t < closest_t) {
                    closest_t = *t;
                    closest_obj = &obj;
                }
            }

            int r = 0, g = 0, b = 0;
            if (closest_obj != nullptr) {
                // Cor bruta do objeto: kd (cor difusa armazenada em material.color)
                const ColorData& c = closest_obj->material.color;
                r = toByte(c.r);
                g = toByte(c.g);
                b = toByte(c.b);
            }
            std::cout << r << " " << g << " " << b << "\n";
        }
    }

    std::cerr << "\nConcluído.\n";
    return 0;
}
