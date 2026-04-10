class Vetor:
    def __init__(self, x=0, y=0, z=0):
        self.x = x
        self.y = y
        self.z = z

    # Vetor + Vetor
    def __add__(self, other):
        return Vetor(self.x + other.x,
                      self.y + other.y,
                      self.z + other.z)

    def __str__(self):
        return f"({self.x}, {self.y}, {self.z})T"
