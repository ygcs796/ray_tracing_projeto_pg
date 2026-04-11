#include <iostream>
#include "src/Ponto.h"
#include "src/Vetor.h"
using namespace std;

int main(){
    Ponto p(1, 2, 3);
    Vetor v(3, 2, 1);

    cout << p << endl;
    cout << v << endl;
    cout << p+v << endl;

    // teste da multiplicação por escalar
    cout << v*2 << endl;

    Vetor u(1, 2, 3);
    cout << v*u << endl;
}