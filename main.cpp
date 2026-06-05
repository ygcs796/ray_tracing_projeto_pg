/**
 * Lê uma cena JSON, lança um raio primário da câmera para cada pixel, testa
 * interseção com os objetos (esferas, planos e malhas) e pinta o pixel pela
 * iluminação local de Phong (ambiente + difusa + especular + sombras).
 *
 * Reflexões e refrações (Entrega 4) ainda não entram.
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
#include "src/Phong.h"
#include "utils/Scene/sceneParser.cpp"

static const std::string DEFAULT_SCENE_PATH = "utils/input/sampleScene.json";

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

/** Converte uma cor RGB em ponto flutuante [0,1] para a tripla de bytes
 *  [0,255] esperada pelo PPM. */
static PixelColor rgbToPixel(const RGB& color) {
    return PixelColor {
        normalizedColorToByte(color.r),
        normalizedColorToByte(color.g),
        normalizedColorToByte(color.b)
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
 * Profundidade máxima da recursão de reflexão/refração. Cada raio
 * secundário (refletido ou refratado) decrementa esse contador; ao chegar a 0,
 * computeColor devolve preto. 3 níveis é a recomendação do monitor — suficiente
 * para ver reflexos mútuos decrescentes sem custo alto.
 */
static constexpr int MAX_DEPTH = 3;

/**
 * Decide a cor do pixel: se o raio não atingiu nada, devolve o fundo;
 * caso contrário, avalia a equação de Phong (ambiente + difusa + especular,
 * com sombras + reflexão e refração recursivas) e converte para bytes [0,255].
 *
 * `findClosestHit` mora em Intersect.h (é compartilhada com os raios secundários
 * recursivos do Phong) e recebe o vetor de objetos diretamente.
 */
static PixelColor shadePixel(const Ray& primaryRay,
                             const std::optional<HitRecord>& hit,
                             SceneData& scene,
                             const MeshLookup& meshes) {
    if (!hit.has_value()) return backgroundColor();
    RGB color = computeColor(*hit, primaryRay, scene, scene.objects, meshes, MAX_DEPTH);
    return rgbToPixel(color);
}

/** Gera o raio do pixel, encontra o objeto mais próximo e devolve a cor. */
static PixelColor renderSinglePixel(const Camera& camera,
                                    SceneData& scene,
                                    const MeshLookup& meshes,
                                    int pixelColumn,
                                    int pixelRow) {
    Ray primaryRay = camera.generateRay(pixelColumn, pixelRow);
    std::optional<HitRecord> hit = findClosestHit(primaryRay, scene.objects, meshes);
    return shadePixel(primaryRay, hit, scene, meshes);
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

    // Estratégia 4: fallbacks específicos para arquivos referenciados nos JSONs
    // do monitor mas que têm nome diferente no nosso repositório.
    if (filenameOnly.string() == "cubo2.obj") {
        std::string fallbackPath = "utils/input/cubo.obj";
        if (auto resolved = tryPathOrEmpty(fallbackPath); !resolved.empty()) {
            return resolved;
        }
    }
    if (filenameOnly.string() == "macaco.obj") {
        std::string fallbackPath = "utils/input/monkey.obj";
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
 *   "rotation"    — ângulos (rx, ry, rz) em GRAUS, aplicados na ordem X→Y→Z
 *                   (convenção XYZ Euler: Rz · Ry · Rx). Convertemos para
 *                   radianos antes de chamar as factories da Matrix4x4.
 */
static Matrix4x4 composeTransforms(const std::vector<TransformData>& transformList) {
    constexpr double PI = 3.14159265358979323846;

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
            // (rx, ry, rz) em GRAUS — convertemos para radianos.
            double angleXRad = currentTransform.data.getX() * PI / 180.0;
            double angleYRad = currentTransform.data.getY() * PI / 180.0;
            double angleZRad = currentTransform.data.getZ() * PI / 180.0;
            // Composição XYZ Euler: aplica X primeiro, depois Y, depois Z,
            // o que vira Rz·Ry·Rx em notação matricial.
            Matrix4x4 rotationXMatrix = Matrix4x4::rotationX(angleXRad);
            Matrix4x4 rotationYMatrix = Matrix4x4::rotationY(angleYRad);
            Matrix4x4 rotationZMatrix = Matrix4x4::rotationZ(angleZRad);
            stepMatrix = rotationZMatrix * rotationYMatrix * rotationXMatrix;
        }
        // Pré-multiplica: a transformação que acabamos de ler vira a MAIS EXTERNA,
        // o que é correto porque a primeira da lista deve ser a MAIS INTERNA
        // (aplicada primeiro a um ponto).
        composedMatrix = stepMatrix * composedMatrix;
    }
    return composedMatrix;
}

// Constrói MaterialData a partir do .mtl lido pelo objReader.
static MaterialData buildMaterialFromMtl(objReader& reader) {
    Vetor kd = reader.getKd();
    Vetor ks = reader.getKs();
    Vetor ka = reader.getKa();
    Vetor ke = reader.getKe();

    MaterialData mat;
    mat.name  = "from_mtl";
    mat.color = ColorData(kd.getX(), kd.getY(), kd.getZ());
    mat.ks    = ColorData(ks.getX(), ks.getY(), ks.getZ());
    mat.ka    = ColorData(ka.getX(), ka.getY(), ka.getZ());
    mat.kr    = ColorData(ke.getX(), ke.getY(), ke.getZ());
    mat.kt    = ColorData(0, 0, 0);
    mat.ns    = reader.getNs();
    mat.ni    = reader.getNi();
    mat.d     = reader.getD();
    return mat;
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

        // Carrega o .obj e pega o material do .mtl associado.
        // Material da mesh sempre vem do .mtl, nunca do JSON da cena.
        objReader meshReader(resolvedObjPath);
        MaterialData materialFromMtl = buildMaterialFromMtl(meshReader);
        currentObject.material = materialFromMtl;

        std::unique_ptr<TriangleMesh> meshForCurrentObject =
            std::make_unique<TriangleMesh>(meshReader, materialFromMtl);

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

/**
 * Aplica as transformações declaradas no JSON ao centro e raio de uma ESFERA.
 *
 * Suporte (conforme orientação do monitor):
 *   - Translação: soma (dx, dy, dz) ao centro.
 *   - Rotação:    aplica a matriz ao centro como ponto. (Visualmente, rotacionar
 *                 uma esfera em torno do próprio centro não muda nada — mas se
 *                 a rotação for em torno da origem do mundo, o centro se move.)
 *   - Escala:     uniforme. Pegamos o primeiro componente do vetor `factors`
 *                 e aplicamos ao raio. Centro também é escalado como ponto.
 *
 * Como a escala é uniforme, dá pra compor T·R·S em uma matriz só, aplicar ao
 * centro e tratar o raio separadamente (multiplicando pelo fator escalar).
 */
static void applyTransformsToSphere(ObjectData& sphereObject) {
    if (sphereObject.transforms.empty()) return;

    // Calcula o fator escalar uniforme (primeiro componente de qualquer "scaling").
    double uniformScaleFactor = 1.0;
    for (const TransformData& currentTransform : sphereObject.transforms) {
        if (currentTransform.tType == "scaling") {
            uniformScaleFactor *= currentTransform.data.getX();
        }
    }

    // Aplica T·R·S ao centro como ponto (a escala uniforme aqui é equivalente
    // a multiplicar todas as coordenadas pelo mesmo fator, então casa com a
    // multiplicação do raio que faremos depois).
    Matrix4x4 composedTransform = composeTransforms(sphereObject.transforms);
    Ponto originalCenter        = sphereObject.getPonto("center");
    Ponto transformedCenter     = composedTransform.transformPoint(originalCenter);

    // Escreve o novo centro de volta no ObjectData (o parser guarda como Vetor).
    sphereObject.vetorPointData["center"] = Vetor(
        transformedCenter.getX(),
        transformedCenter.getY(),
        transformedCenter.getZ());

    // Aplica o fator escalar ao raio.
    double originalRadius = sphereObject.getNum("radius");
    sphereObject.numericData["radius"] = originalRadius * uniformScaleFactor;
}

/**
 * Aplica as transformações declaradas no JSON ao ponto-de-referência e à
 * normal de um PLANO.
 *
 * Suporte (conforme orientação do monitor):
 *   - Translação: translada o ponto do plano (relativePos). A normal não muda.
 *   - Rotação:    rotaciona a normal e também o ponto.
 *   - Escala:     IGNORADA. Escalar um plano infinito não muda nada visualmente.
 *
 * Implementação: filtramos as transformações da lista para descartar `scaling`,
 * compomos o resto em uma única matriz, e aplicamos ao ponto e à normal.
 */
static void applyTransformsToPlane(ObjectData& planeObject) {
    if (planeObject.transforms.empty()) return;

    // Filtra fora as transformações de escala (planos não escalam).
    std::vector<TransformData> filteredTransforms;
    for (const TransformData& currentTransform : planeObject.transforms) {
        if (currentTransform.tType != "scaling") {
            filteredTransforms.push_back(currentTransform);
        }
    }
    if (filteredTransforms.empty()) return;

    Matrix4x4 composedTransform = composeTransforms(filteredTransforms);

    // Transforma o ponto do plano (que vive em relativePos para "type":"plane").
    Ponto transformedPointOnPlane = composedTransform.transformPoint(planeObject.relativePos);
    planeObject.relativePos = transformedPointOnPlane;

    // Transforma a normal. Como só temos translação + rotação aqui (escala foi
    // filtrada), a matriz é ortogonal na parte 3x3 e usar transformVector é
    // equivalente a (M⁻¹)ᵀ — mais simples e barato.
    Vetor originalNormal      = planeObject.getVetor("normal");
    Vetor transformedNormal   = composedTransform.transformVector(originalNormal).normalize();
    planeObject.vetorPointData["normal"] = transformedNormal;
}

/**
 * Itera sobre os objetos da cena aplicando transformações em esferas e planos
 * (malhas são tratadas separadamente em `loadMeshes` porque exigem carregar
 * o .obj antes de transformar os vértices).
 */
static void applyTransformsToSpheresAndPlanes(SceneData& scene) {
    for (ObjectData& currentObject : scene.objects) {
        if      (currentObject.objType == "sphere") applyTransformsToSphere(currentObject);
        else if (currentObject.objType == "plane")  applyTransformsToPlane(currentObject);
    }
}

int main(int argc, char** argv) {
    std::string scenePath = resolveScenePath(argc, argv);
    SceneData scene = loadSceneOrExit(scenePath);
    Camera camera(scene.camera);

    // Etapa da Entrega 2: antes de começar o loop de render, processamos todas
    // as transformações declaradas no JSON.
    //   - Esferas e planos: alteramos centro/raio/ponto/normal in-place.
    //   - Malhas: carregamos o .obj, construímos TriangleMesh, e aplicamos as
    //     transformações aos vértices.
    applyTransformsToSpheresAndPlanes(scene);
    std::vector<std::unique_ptr<TriangleMesh>> meshStorage;
    MeshLookup meshLookup = loadMeshes(scene, scenePath, meshStorage);

    renderSceneToPpm(camera, scene, meshLookup);
    return 0;
}
