/**
 * Lê uma cena JSON, lança um raio primário da câmera para cada pixel, testa
 * interseção com os objetos (esferas e planos) e pinta o pixel com a cor
 * difusa (kd) do objeto mais próximo atingido. Sem iluminação, sombras ou
 * reflexões — apenas ray-casting bruto.
 */

 #include <iostream>
#include <limits>
#include <algorithm>
#include <string>

#include "src/Camera.h"
#include "src/Intersect.h"
#include "utils/Scene/sceneParser.cpp"

/**
 * Converte componente de cor [0.0, 1.0] para inteiro [0, 255] com clamp.
 * Cores em ColorData estão em [0,1]; PPM exige inteiros em [0,255].
 * O clamp protege contra entradas ligeiramente fora do intervalo.
 */
static int toByte(double c) {
    int v = static_cast<int>(c * 255.0);
    return std::clamp(v, 0, 255);
}

int main(int argc, char** argv) {
    // Argumento opcional = caminho do JSON. Default: sampleScene.json.
    std::string sceneFile = "utils/input/sampleScene.json";
    if (argc >= 2) sceneFile = argv[1];

    SceneData scene;
    try {
        scene = SceneJsonLoader::loadFile(sceneFile);
    } catch (const std::exception& e) {
        std::cerr << "Erro ao carregar cena '" << sceneFile << "': " << e.what() << "\n";
        return 1;
    }

    // Construtor da Camera calcula a base ortonormal W, U, V uma única vez.
    Camera camera(scene.camera);
    const int hres = scene.camera.image_width;
    const int vres = scene.camera.image_height;

    std::cerr << "Renderizando " << hres << "x" << vres
              << " com " << scene.objects.size() << " objetos...\n";

    // Header PPM: P3 (ASCII RGB), largura, altura, valor máximo por componente.
    std::cout << "P3\n" << hres << " " << vres << "\n255\n";

    // Y-externo, X-interno: ordem em que PPM espera os pixels (linha por linha,
    // esquerda→direita, topo→base). Permite imprimir sequencialmente sem
    // guardar a imagem toda em memória.
    for (int j = 0; j < vres; ++j) {
        // Progresso em stderr. \r sobrescreve a linha anterior no terminal.
        if (j % 10 == 0 || j == vres - 1) {
            std::cerr << "\rLinha " << (j + 1) << "/" << vres << std::flush;
        }

        for (int i = 0; i < hres; ++i) {
            Ray ray = camera.generateRay(i, j);

            // Seleção de visibilidade: varre todos os objetos e mantém o hit
            // mais próximo (menor t > ε). +∞ inicial garante que a primeira
            // interseção válida sempre atualiza closest_t.
            double closest_t = std::numeric_limits<double>::infinity();
            const ObjectData* closest_obj = nullptr;

            for (auto& obj : scene.objects) {
                auto t = intersect(ray, obj);
                if (t.has_value() && *t < closest_t) {
                    closest_t = *t;
                    closest_obj = &obj;
                }
            }

            // Sem hit → preto (fundo). Com hit → cor difusa (kd) do objeto.
            int r = 0, g = 0, b = 0;
            if (closest_obj != nullptr) {
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
