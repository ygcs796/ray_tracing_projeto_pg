# Entrega 2 — Raycasting com Malhas de Triângulos

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Entrega](https://img.shields.io/badge/entrega-2%20de%204-success)

> Parte do projeto **[Ray Tracing — Processamento Gráfico](../../tree/main)**.
> Esta branch (`entrega-2`) implementa a **segunda entrega**: malhas de triângulos e transformações afins.

<p align="center"><img src="assets/entrega-2.png" width="540" alt="Malha de triângulos (Suzanne) renderizada"></p>

## O que foi feito

Suporte a **malhas de triângulos** carregadas de arquivos `.obj` (interseção raio-triângulo de
**Möller–Trumbore**) e a **transformações afins** — translação, escala e rotação — via matrizes
homogêneas 4×4. Escalas não-uniformes corrigem a normal por $(M^{-1})^\top$. As facetas visíveis na malha
do macaco "Suzanne" acima evidenciam a geometria triangular.

Arquivos principais: `src/Matrix.h` (matrizes 4×4) e `src/Mesh.h` (malha + Möller–Trumbore), além das
interseções de esfera/plano herdadas da [Entrega 1](../../tree/entrega-1).

## Como rodar

```bash
# já nesta branch (entrega-2)
g++ -std=c++17 -O2 main.cpp -o raytracer
./raytracer utils/input/entrega-2-cenarios/6-monkey-em-cena.json > saida.ppm
sips -s format png saida.ppm --out saida.png   # macOS (ou ImageMagick / utils/convert_ppm.py)
```

> Há outras cenas em `utils/input/entrega-2-cenarios/` demonstrando cada transformação
> (`1-cubo-base`, `2-cubo-translacao`, `3-cubo-escala-nao-uniforme`, `4-cubo-rotacao`,
> `5-composicao-completa`).

## Detalhes técnicos

### Malha de triângulos

Uma malha é definida por:

- $n_\triangle \in \mathbb{N}$ — número de triângulos
- $n_\circ \in \mathbb{N},\; n_\circ \geq 3$ — número de vértices
- Lista de vértices (pontos), tamanho $n_\circ$
- Lista de triplas de índices de vértices (uma por triângulo), tamanho $n_\triangle$
- Lista de normais de triângulos (vetores), tamanho $n_\triangle$
- Lista de normais de vértices — cada uma é a média das normais dos triângulos que compartilham aquele vértice, tamanho $n_\circ$
- Cor difusa $O_d \in [0,1]^3$

A malha é carregada de um arquivo `.obj` usando o `ObjReader` do repositório; o material vem do `.mtl`.

### Transformações afins

- Matrizes de doubles (4×4) aplicáveis a pontos e vetores
- `translation`, `scaling` e `rotation` (ângulos em graus no JSON)
- Não há animação; renderizar antes e depois de uma transformação é suficiente

A lista de transformações de cada objeto está em `obj.transforms` — **a ordem importa**.

---

### Entregas do projeto

- [Entrega 1 — Esferas e planos](../../tree/entrega-1)
- **Entrega 2 — Malhas de triângulos** ← *você está aqui*
- [Entrega 3 — Phong + sombras](../../tree/entrega-3)
- [Entrega 4 — Reflexão + refração](../../tree/entrega-4)
- [Feature individual — Soft shadows](../../tree/feat/soft-shadow)
- [Visão geral do projeto (main)](../../tree/main)
