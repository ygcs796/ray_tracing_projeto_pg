#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <map>

/*
Classe de leitura de arquivos .mtl, que guarda cores e propriedades de materiais.

A saber que:
    - kd = Difuso (Cor do objeto)
    - ks = Specular (Reflexivo)
    - ke = Emissivo 
    - ka = Ambiente
    - ns = Brilho
    - ni = Índice de refração
    - d = Opacidade

A classe precisa ser instânciada passando o caminho do arquivo .mtl correspondente
*/


using namespace std;

struct MaterialProperties {
    Vetor  kd;     // Difuso      (kd)
    Vetor  ks;     // Especular   (ks)
    Vetor  ka;     // Ambiente    (ka)
    Vetor  kr;     // Reflexivo   (kr)  [Emissivo  (ke)] considere ke = kr
    Vetor  ke;     // Emissivo    (ke)  [Reflexivo (kr)] considere kr = ke  //normalmente o ke é o mais usado
    Vetor  kt;     // Transmicivo (kt)
    double ns;     // Rugosidade  (eta) [Brilho]
    double ni;     // Refração 
    double d;      // Opacidade

    MaterialProperties() : kd(0, 0, 0), ks(0, 0, 0), ke(0, 0, 0), ka(0, 0, 0), ns(0), ni(0), d(0) {}
};

class colormap {

public:
    map<string, MaterialProperties> mp;

    //Construtor    
    colormap(){};
    colormap(string input){

        // construtor: lê arquivo cores.mtl e guarda valores RGB associados a cada nome

        std::ifstream mtlFile(input);

        if (!mtlFile.is_open()) {
            std::cerr << "erro abrindo arquivo cores.mtl\n";
        }

        string line, currentMaterial;

        while (std::getline(mtlFile, line)) {
            std::istringstream iss(line);
            std::string keyword;
            iss >> keyword;

            if (keyword == "newmtl") {
                iss >> currentMaterial;
                if (!currentMaterial.empty()) {
                    mp[currentMaterial] = MaterialProperties();
                }
            } else if (keyword == "Kd") {
                double kdR, kdG, kdB;
                iss >> kdR >> kdG >> kdB;
                if (!currentMaterial.empty()) {
                    mp[currentMaterial].kd = Vetor(kdR, kdG, kdB);
                }
            } else if (keyword == "Ks") {
                double ksR, ksG, ksB;
                iss >> ksR >> ksG >> ksB;
                if (!currentMaterial.empty()) {
                    mp[currentMaterial].ks = Vetor(ksR, ksG, ksB);
                }
            } else if (keyword == "Ke") {
                double keR, keG, keB;
                iss >> keR >> keG >> keB;
                if (!currentMaterial.empty()) {
                    mp[currentMaterial].ke = Vetor(keR, keG, keB);
                }
            } else if (keyword == "Kr") {
                double krR, krG, krB;
                iss >> krR >> krG >> krB;
                if (!currentMaterial.empty()) {
                    mp[currentMaterial].kr = Vetor(krR, krG, krB);
                }
            }
            else if (keyword == "Ka") {
                double kaR, kaG, kaB;
                iss >> kaR >> kaG >> kaB;
                if (!currentMaterial.empty()) {
                    mp[currentMaterial].ka = Vetor(kaR, kaG, kaB);
                }
            } else if (keyword == "Ns") {
                iss >> mp[currentMaterial].ns;
            } else if (keyword == "Ni") {
                iss >> mp[currentMaterial].ni;
            } else if (keyword == "d") {
                iss >> mp[currentMaterial].d;
            }
        }

        mtlFile.close();
    }

    Vetor getColor(string& s){
        if (mp.find(s) != mp.end()) {
            return mp[s].kd;
        } else {
            cerr << "Error: cor " << s << " indefinida no arquivo .mtl\n";
            return Vetor(0,0,0);
        }
    }

    MaterialProperties getMaterialProperties(string& s){
        if (mp.find(s) != mp.end()) {
            return mp[s];
        } else {
            cerr << "Error: Cor " << s << " indefinida no arquivo .mtl\n";
            return MaterialProperties();
        }
    }

};