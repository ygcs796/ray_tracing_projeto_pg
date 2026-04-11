#ifndef VETORHEADER
#define VETORHEADER
#include <iostream>

class Vetor{
public:
    Vetor(double x=0, double y=0, double z=0): x(x), y(y), z(z) {}

    //Vetor + Vetor
    Vetor  operator+ (const Vetor&v) const{ 
        return Vetor(x+v.x, y+v.y, z+v.z); 
    }
    // Vetor * escalar
    Vetor operator* (const double&e) const {
        return Vetor(x*e, y*e, z*e);
    }
    // Vetor * Vetor (produto interno/escalar)
    Vetor operator* (const Vetor&v) const {
        return double((x*v.getX()) + (y*v.getY()) + (z*v.getZ()));
    }

    // cout << Vetor
    friend std::ostream& operator<<(std::ostream& os, const Vetor &v){ 
        return os << "(" << v.x << ", " << v.y << ", " << v.z << ")T"; 
    }
    
    double getX() const { return x; }
    double getY() const { return y; }
    double getZ() const { return z; }

private:
    double x, y, z;
};

#endif