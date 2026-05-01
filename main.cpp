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

/** Verifica se um arquivo existe no filesystem (sem abri-lo). */
static bool fileExistsAtPath(const std::string& filePath) {
    struct stat fileStatus;
    return stat(filePath.c_str(), &fileStatus) == 0;
}

/**
 * Resolve o caminho de um arquivo .obj referenciado pela cena.
 *
 * Os JSONs da disciplina usam caminhos inconsistentes — "inputs/cubo.obj",
 * "../input/cubo2.obj", "../input/monkey.obj" — porque foram herdados de
 * vários projetos diferentes. Tentamos múltiplas estratégias de resolução
 * antes de desistir:
 *
 *   1. O caminho literal, relativo ao diretório atual de execução.
 *   2. Resolvido em relação ao diretório onde o JSON está.
 *   3. Apenas o nome do arquivo, dentro de `utils/input/`.
 *   4. Fallback específico: `cubo2.obj` → `cubo.obj` (porque cubo2 não existe
 *      no repositório, mas várias cenas oficiais o referenciam).
 *
 * Devolve o caminho resolvido, ou string vazia se nenhuma estratégia funcionou.
 */
static std::string resolveObjPath(const std::string& pathReferencedInJson,
                                  const std::string& sceneJsonPath) {
    namespace fs = std::filesystem;

    auto tryPathOrEmpty = [](const std::string& candidatePath) -> std::string {
        if (fileExistsAtPath(candidatePath)) return candidatePath;
        return "";
    };

    // Estratégia 1: caminho literal como aparece no JSON.
    if (auto resolved = tryPathOrEmpty(pathReferencedInJson); !resolved.empty()) {
        return resolved;
    }

    // Estratégia 2: relativo ao diretório do próprio arquivo JSON.
    fs::path sceneDirectory = fs::path(sceneJsonPath).parent_path();
    if (!sceneDirectory.empty()) {
        std::string candidatePath = (sceneDirectory / pathReferencedInJson).string();
        if (auto resolved = tryPathOrEmpty(candidatePath); !resolved.empty()) {
            return resolved;
        }
    }

    // Estratégia 3: só o basename, dentro de utils/input/.
    fs::path filenameOnly = fs::path(pathReferencedInJson).filename();
    {
        std::string candidatePath = (fs::path("utils/input") / filenameOnly).string();
        if (auto resolved = tryPathOrEmpty(candidatePath); !resolved.empty()) {
            return resolved;
        }
    }

    // Estratégia 4: fallback específico para um arquivo inexistente referenciado
    // pelas cenas oficiais (cubo2.obj). Aceitamos cubo.obj como substituto.
    if (filenameOnly.string() == "cubo2.obj") {
        std::string fallbackPath = "utils/input/cubo.obj";
        if (auto resolved = tryPathOrEmpty(fallbackPath); !resolved.empty()) {
            return resolved;
        }
    }

    return "";  // nenhuma estratégia funcionou
}

/**
 * Combina a lista de transformações declaradas no JSON em uma única matriz 4x4.
 *
 * Convenção de ORDEM (importante e fonte clássica de bugs):
 *   A primeira transformação da lista é aplicada PRIMEIRO ao ponto, depois a
 *   segunda, e assim por diante. Em forma matricial:
 *
 *       matrizFinal = T_n · ... · T_2 · T_1
 *
 *   onde T_1 é a primeira da lista. Isso porque, ao aplicar `matrizFinal · ponto`,
 *   a multiplicação se associa da direita pra esquerda: T_1 atua primeiro.
 *
 * Implementação: começamos com a identidade e PRÉ-MULTIPLICAMOS cada nova
 * transformação (`composta = nova · composta`), o que faz com que a mais recente
 * fique como mais externa na composição final.
 *
 * Tipos suportados (vindos de TransformData::tType):
 *   "translation" — vetor (dx, dy, dz)
 *   "scaling"     — fatores (sx, sy, sz)
 *   "rotation"    — ângulos (rx, ry, rz) em radianos, aplicados na ordem X→Y→Z
 *                   (convenção XYZ Euler: Rz · Ry · Rx).
 */
static Matrix4x4 composeTransforms(const std::vector<TransformData>& transformList) {
    Matrix4x4 composedMatrix = Matrix4x4::identity();
    for (const TransformData& currentTransform : transformList) {
        Matrix4x4 stepMatrix = Matrix4x4::identity();
        if (currentTransform.tType == "translation") {
            stepMatrix = Matrix4x4::translation(currentTransform.data.getX(),
                                                currentTransform.data.getY(),
                                                currentTransform.data.getZ());
        } else if (currentTransform.tType == "scaling") {
            stepMatrix = Matrix4x4::scaling(currentTransform.data.getX(),
                                            currentTransform.data.getY(),
                                            currentTransform.data.getZ());
        } else if (currentTransform.tType == "rotation") {
            // (rx, ry, rz) em radianos, composição XYZ Euler: aplica X primeiro,
            // depois Y, depois Z, o que vira Rz·Ry·Rx em notação matricial.
            Matrix4x4 rotationXMatrix = Matrix4x4::rotationX(currentTransform.data.getX());
            Matrix4x4 rotationYMatrix = Matrix4x4::rotationY(currentTransform.data.getY());
            Matrix4x4 rotationZMatrix = Matrix4x4::rotationZ(currentTransform.data.getZ());
            stepMatrix = rotationZMatrix * rotationYMatrix * rotationXMatrix;
        }
        // Pré-multiplica: a transformação que acabamos de ler vira a MAIS EXTERNA,
        // o que é correto porque a primeira da lista deve ser a MAIS INTERNA
        // (aplicada primeiro a um ponto).
        composedMatrix = stepMatrix * composedMatrix;
    }
    return composedMatrix;
}

/**
 * Carrega todas as malhas referenciadas pela cena, aplica as transformações
 * declaradas no JSON, e devolve uma tabela `ObjectData* → TriangleMesh*` que o
 * dispatcher de interseção usa em runtime.
 *
 * Ownership: as malhas em si vivem em `meshStorageOwner` (vector de unique_ptrs).
 * O lookup retornado só guarda ponteiros não-donos. O chamador precisa manter
 * `meshStorageOwner` vivo enquanto o lookup for usado — caso contrário, os
 * ponteiros viram "dangling".
 */
static MeshLookup loadMeshes(SceneData& scene,
                             const std::string& scenePath,
                             std::vector<std::unique_ptr<TriangleMesh>>& meshStorageOwner) {
    MeshLookup objectToMeshLookup;

    for (ObjectData& currentObject : scene.objects) {
        bool isNotMesh = currentObject.objType != "mesh";
        if (isNotMesh) continue;

        std::string resolvedObjPath = resolveObjPath(currentObject.getProperty("path"), scenePath);
        bool objFileNotFound = resolvedObjPath.empty();
        if (objFileNotFound) {
            std::clog << "[mesh] Aviso: arquivo .obj não encontrado para "
                      << currentObject.getProperty("path")
                      << " — objeto ignorado.\n";
            continue;
        }

        // Carrega o .obj e constrói a malha (vértices, faces, normais).
        objReader meshReader(resolvedObjPath);
        std::unique_ptr<TriangleMesh> meshForCurrentObject =
            std::make_unique<TriangleMesh>(meshReader, currentObject.material);

        // Aplica a sequência de transformações do JSON (translação/escala/rotação).
        bool hasExplicitTransforms = !currentObject.transforms.empty();
        if (hasExplicitTransforms) {
            Matrix4x4 composedTransform = composeTransforms(currentObject.transforms);
            meshForCurrentObject->applyTransform(composedTransform);
        }

        // `relativePos` é o campo padrão de "posição" usado por esferas e planos;
        // para malhas, o tratamos como uma translação adicional aplicada DEPOIS
        // das transformações da lista. Mantemos para compatibilidade com cenas
        // que usam relativePos como atalho.
        bool relativePositionIsNonZero =
            currentObject.relativePos.getX() != 0.0 ||
            currentObject.relativePos.getY() != 0.0 ||
            currentObject.relativePos.getZ() != 0.0;
        if (relativePositionIsNonZero) {
            Matrix4x4 relativePosTranslation = Matrix4x4::translation(
                currentObject.relativePos.getX(),
                currentObject.relativePos.getY(),
                currentObject.relativePos.getZ());
            meshForCurrentObject->applyTransform(relativePosTranslation);
        }

        objectToMeshLookup[&currentObject] = meshForCurrentObject.get();
        meshStorageOwner.push_back(std::move(meshForCurrentObject));
    }

    return objectToMeshLookup;
}

int main(int argc, char** argv) {
    std::string scenePath = resolveScenePath(argc, argv);
    SceneData scene = loadSceneOrExit(scenePath);
    Camera camera(scene.camera);

    // Etapa da Entrega 2: antes de começar o loop de render, carregamos todas
    // as malhas .obj e aplicamos as transformações declaradas no JSON. Os
    // unique_ptrs em `meshStorage` são donos da memória; `meshLookup` só guarda
    // ponteiros não-donos para consulta rápida durante a interseção.
    std::vector<std::unique_ptr<TriangleMesh>> meshStorage;
    MeshLookup meshLookup = loadMeshes(scene, scenePath, meshStorage);

    renderSceneToPpm(camera, scene, meshLookup);
    return 0;
}
