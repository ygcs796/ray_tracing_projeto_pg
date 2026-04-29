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
#include <vector>
#include <memory>
#include <filesystem>
#include <sys/stat.h>

#include "src/Camera.h"
#include "src/Intersect.h"
#include "src/Mesh.h"
#include "src/Matrix.h"
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
static ClosestHit findClosestHit(const Ray& ray, SceneData& scene, const MeshLookup& meshes) {
    // tMax ( dinâmico )
    ClosestHit result = { nullptr, std::numeric_limits<double>::infinity() };

    for (auto& candidateObject : scene.objects) {
        auto intersectionParameter = intersect(ray, candidateObject, meshes);
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
                                    const MeshLookup& meshes,
                                    int pixelColumn,
                                    int pixelRow) {
    Ray primaryRay = camera.generateRay(pixelColumn, pixelRow);
    ClosestHit hit = findClosestHit(primaryRay, scene, meshes);
    return shadePixel(hit);
}

/**
 * Percorre todos os pixels em ordem PPM (linha por linha, esquerda → direita,
 * topo → base) e escreve o resultado no stdout.
 */
static void renderSceneToPpm(const Camera& camera, SceneData& scene, const MeshLookup& meshes) {
    const int imageWidth  = camera.getImageWidth();
    const int imageHeight = camera.getImageHeight();

    writePpmHeader(imageWidth, imageHeight);

    for (int pixelRow = 0; pixelRow < imageHeight; ++pixelRow) {
        for (int pixelColumn = 0; pixelColumn < imageWidth; ++pixelColumn) {
            PixelColor pixel = renderSinglePixel(camera, scene, meshes, pixelColumn, pixelRow);
            writePixel(pixel);
        }
    }
}

// ============================================================================
// CARREGAMENTO E TRANSFORMAÇÃO DE MALHAS (Entrega 2)
// ============================================================================

/** True se `path` existe no filesystem. */
static bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

/**
 * Resolve o caminho de um .obj referenciado pela cena.
 *
 * Os arquivos JSON usam caminhos como "inputs/cubo.obj", "../input/cubo2.obj",
 * "../input/monkey.obj" — relativos ao diretório do JSON, OU à raiz do projeto.
 * Tentamos várias resoluções na ordem mais provável.
 *
 */
static std::string resolveObjPath(const std::string& referencedPath, const std::string& sceneJsonPath) {
    namespace fs = std::filesystem;

    auto tryPath = [](const std::string& p) -> std::string {
        if (fileExists(p)) return p;
        return "";
    };

    // 1. Caminho literal (relativo ao cwd).
    if (auto r = tryPath(referencedPath); !r.empty()) return r;

    // 2. Relativo ao diretório do JSON.
    fs::path sceneDir = fs::path(sceneJsonPath).parent_path();
    if (!sceneDir.empty()) {
        std::string p = (sceneDir / referencedPath).string();
        if (auto r = tryPath(p); !r.empty()) return r;
    }

    // 3. Apenas o basename em utils/input/.
    fs::path basename = fs::path(referencedPath).filename();
    {
        std::string p = (fs::path("utils/input") / basename).string();
        if (auto r = tryPath(p); !r.empty()) return r;
    }

    if (basename.string() == "cubo2.obj") {
        std::string p = "utils/input/cubo.obj";
        if (auto r = tryPath(p); !r.empty()) return r;
    }

    return "";  // não encontrado
}

/**
 * Compõe a sequência de transformações de um objeto numa única matriz 4x4.
 *
 * Convenção: a primeira transformação na lista (índice 0) é aplicada PRIMEIRO
 * ao ponto, depois a segunda, e assim por diante. Em forma matricial isso é:
 *     M = T_n · ... · T_2 · T_1
 * Aplicar M·P calcula T_n(...T_2(T_1(P))).
 *
 * Suporta TransformData::tType ∈ {"translation", "scaling", "rotation"}.
 * Para "rotation", o vetor `data` traz (rx, ry, rz) em radianos; aplicamos na
 * ordem X→Y→Z (composição padrão de Euler).
 */
static Matrix4x4 composeTransforms(const std::vector<TransformData>& transforms) {
    Matrix4x4 composed = Matrix4x4::identity();
    for (const auto& t : transforms) {
        Matrix4x4 step = Matrix4x4::identity();
        if (t.tType == "translation") {
            step = Matrix4x4::translation(t.data.getX(), t.data.getY(), t.data.getZ());
        } else if (t.tType == "scaling") {
            step = Matrix4x4::scaling(t.data.getX(), t.data.getY(), t.data.getZ());
        } else if (t.tType == "rotation") {
            // (rx, ry, rz) em radianos. Composição X→Y→Z: Rz · Ry · Rx.
            Matrix4x4 rx = Matrix4x4::rotationX(t.data.getX());
            Matrix4x4 ry = Matrix4x4::rotationY(t.data.getY());
            Matrix4x4 rz = Matrix4x4::rotationZ(t.data.getZ());
            step = rz * ry * rx;
        }
        // Pré-multiplica: nova transformação fica MAIS EXTERNA na composição.
        composed = step * composed;
    }
    return composed;
}

/**
 * Carrega todas as malhas referenciadas pela cena, aplica suas transformações
 * e devolve a tabela {ObjectData* → TriangleMesh*} usada pelo dispatcher.
 *
 * Os TriangleMesh ficam em `meshStorage` (que mantém ownership) — o lookup só
 * armazena ponteiros não-donos. O caller deve manter `meshStorage` vivo enquanto
 * o lookup for usado.
 */
static MeshLookup loadMeshes(SceneData& scene,
                             const std::string& scenePath,
                             std::vector<std::unique_ptr<TriangleMesh>>& meshStorage) {
    MeshLookup lookup;

    for (auto& obj : scene.objects) {
        if (obj.objType != "mesh") continue;

        std::string objPath = resolveObjPath(obj.getProperty("path"), scenePath);
        if (objPath.empty()) {
            std::clog << "[mesh] Aviso: .obj não encontrado para "
                      << obj.getProperty("path") << " — objeto ignorado.\n";
            continue;
        }

        objReader reader(objPath);
        auto mesh = std::make_unique<TriangleMesh>(reader, obj.material);

        // Aplica transformações declaradas na cena (translação/escala/rotação).
        if (!obj.transforms.empty()) {
            Matrix4x4 composed = composeTransforms(obj.transforms);
            mesh->applyTransform(composed);
        }

        // relativePos da cena também desloca a malha (translação implícita).
        if (obj.relativePos.getX() != 0.0 || obj.relativePos.getY() != 0.0 || obj.relativePos.getZ() != 0.0) {
            Matrix4x4 t = Matrix4x4::translation(obj.relativePos.getX(),
                                                 obj.relativePos.getY(),
                                                 obj.relativePos.getZ());
            mesh->applyTransform(t);
        }

        lookup[&obj] = mesh.get();
        meshStorage.push_back(std::move(mesh));
    }

    return lookup;
}

int main(int argc, char** argv) {
    std::string scenePath = resolveScenePath(argc, argv);
    SceneData scene = loadSceneOrExit(scenePath);
    Camera camera(scene.camera);

    // Carrega as malhas e aplica suas transformações antes do loop de renderização.
    std::vector<std::unique_ptr<TriangleMesh>> meshStorage;
    MeshLookup meshes = loadMeshes(scene, scenePath, meshStorage);

    renderSceneToPpm(camera, scene, meshes);
    return 0;
}
