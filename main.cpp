/**
 * Lê uma cena JSON, lança um raio primário da câmera para cada pixel, testa
 * interseção com os objetos (esferas e planos) e pinta o pixel com a cor
 * difusa (kd) do objeto mais próximo atingido. Sem iluminação, sombras ou
 * reflexões — apenas ray-casting bruto.
 */

#include <iostream>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <string>

#include "src/Camera.h"
#include "src/Intersect.h"
#include "utils/Scene/sceneParser.cpp"

static const std::string DEFAULT_SCENE_PATH = "utils/input/sampleScene.json";

/** Resultado da busca do objeto mais próximo atingido por um raio. */
struct ClosestHit {
    const ObjectData* hitObject;       // ponteiro para o objeto atingido (nullptr se nenhum)
    double            distanceAlongRay; // parâmetro t da interseção (+∞ se nenhum)
};

/** Tripla de cor em [0, 255] pronta para impressão no PPM. */
struct PixelColor {
    int redByte, greenByte, blueByte;
};

/** Lê o caminho do JSON da linha de comando, ou usa o default. */
static std::string resolveScenePath(int argc, char** argv) {
    if (argc >= 2) return std::string(argv[1]);
    return DEFAULT_SCENE_PATH;
}

/** Carrega a cena do disco. Termina o programa com código 1 se falhar. */
static SceneData loadSceneOrExit(const std::string& scenePath) {
    try {
        return SceneJsonLoader::loadFile(scenePath);
    } catch (const std::exception&) {
        std::exit(1);
    }
}

/**
 * Converte componente de cor [0.0, 1.0] para inteiro [0, 255] com clamp.
 * Cores em ColorData estão em [0,1]; PPM exige inteiros em [0,255].
 */
static int normalizedColorToByte(double colorComponent) {
    int colorByte = static_cast<int>(colorComponent * 255.0);
    return std::clamp(colorByte, 0, 255);
}

/** Cor preta usada como fundo quando o raio não atinge nenhum objeto. */
static PixelColor backgroundColor() {
    return PixelColor { 0, 0, 0 };
}

/** Converte a cor difusa (kd) do objeto para a tripla de bytes do PPM. */
static PixelColor objectDiffuseColorAsPixel(const ObjectData* hitObject) {
    const ColorData& diffuseColor = hitObject->material.color;
    return PixelColor {
        normalizedColorToByte(diffuseColor.r),
        normalizedColorToByte(diffuseColor.g),
        normalizedColorToByte(diffuseColor.b)
    };
}

static void writePpmHeader(int imageWidth, int imageHeight) {
    std::cout << "P3\n" << imageWidth << " " << imageHeight << "\n255\n";
}

/** Imprime a linha "r g b" de um pixel no stdout. */
static void writePixel(const PixelColor& pixel) {
    std::cout << pixel.redByte << " " << pixel.greenByte << " " << pixel.blueByte << "\n";
}

/**
 * Varre todos os objetos da cena e retorna o mais próximo atingido pelo raio
 * (menor t > ε). Se nenhum for atingido, hitObject == nullptr.
 */
static ClosestHit findClosestHit(const Ray& ray, SceneData& scene) {
    ClosestHit result = { nullptr, std::numeric_limits<double>::infinity() };

    for (auto& candidateObject : scene.objects) {
        auto intersectionParameter = intersect(ray, candidateObject);
        if (intersectionParameter.has_value() && *intersectionParameter < result.distanceAlongRay) {
            result.hitObject = &candidateObject;
            result.distanceAlongRay = *intersectionParameter;
        }
    }
    return result;
}

/** Decide a cor do pixel com base no resultado da busca. */
static PixelColor shadePixel(const ClosestHit& hit) {
    if (hit.hitObject == nullptr) return backgroundColor();
    return objectDiffuseColorAsPixel(hit.hitObject);
}

/** Gera o raio do pixel, encontra o objeto mais próximo e devolve a cor. */
static PixelColor renderSinglePixel(const Camera& camera,
                                    SceneData& scene,
                                    int pixelColumn,
                                    int pixelRow) {
    Ray primaryRay = camera.generateRay(pixelColumn, pixelRow);
    ClosestHit hit = findClosestHit(primaryRay, scene);
    return shadePixel(hit);
}

/**
 * Percorre todos os pixels em ordem PPM (linha por linha, esquerda → direita,
 * topo → base) e escreve o resultado no stdout.
 */
static void renderSceneToPpm(const Camera& camera, SceneData& scene) {
    const int imageWidth  = camera.getImageWidth();
    const int imageHeight = camera.getImageHeight();

    writePpmHeader(imageWidth, imageHeight);

    for (int pixelRow = 0; pixelRow < imageHeight; ++pixelRow) {
        for (int pixelColumn = 0; pixelColumn < imageWidth; ++pixelColumn) {
            PixelColor pixel = renderSinglePixel(camera, scene, pixelColumn, pixelRow);
            writePixel(pixel);
        }
    }
}

int main(int argc, char** argv) {
    std::string scenePath = resolveScenePath(argc, argv);
    SceneData scene = loadSceneOrExit(scenePath);
    Camera camera(scene.camera);
    renderSceneToPpm(camera, scene);
    return 0;
}
