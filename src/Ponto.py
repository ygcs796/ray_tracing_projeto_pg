class Ponto:
    def __init__(self, x=0, y=0, z=0):
        self.x = x
        self.y = y
        self.z = z

    # Ponto + Vetor → Ponto
    def __add__(self, v):
        return Ponto(self.x + v.x,
                     self.y + v.y,
                     self.z + v.z)

    # Ponto - Ponto → Vetor
    def __sub__(self, other):
        from src.Vetor import Vetor
        return Vetor(self.x - other.x,
                     self.y - other.y,
                     self.z - other.z)

    def __str__(self):
        return f"({self.x}, {self.y}, {self.z})"