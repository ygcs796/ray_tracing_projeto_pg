#include "sceneParser.cpp"
#include <iostream>
using namespace std;

int main() {
    try {
        SceneData scene = SceneJsonLoader::loadFile("../input/sampleScene.json");
        
        cerr << "Loaded " << scene.objects.size() << " objects\n";
        cerr << "Loaded " << scene.lightList.size() << " lights\n";
        cerr << "Camera image: " << scene.camera.image_width << " x " << scene.camera.image_height << "\n";

        // cout << scene << endl;
    } catch (const exception& e) {
        cerr << "Scene load error: " << e.what() << "\n";
        return 1;
    }
}