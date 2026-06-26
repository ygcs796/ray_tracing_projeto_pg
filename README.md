# Entrega 1 — Raycasting com Esferas e Planos

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Entrega](https://img.shields.io/badge/entrega-1%20de%204-success)

> Parte do projeto **[Ray Tracing — Processamento Gráfico](../../tree/main)**.
> Esta branch (`entrega-1`) implementa a **primeira entrega**: *ray casting* de esferas e planos.

<p align="center"><img src="assets/entrega-1.png" width="540" alt="Três esferas renderizadas com ray casting"></p>

## O que foi feito

Câmera móvel com base ortonormal, interseção **raio-esfera** e **raio-plano**. Para cada pixel
dispara-se um raio primário e pinta-se com a cor difusa ($k_d$) do objeto mais próximo (ou preto, se o
raio não atingir nada). Ainda **não há iluminação** — daí o aspecto "chapado" das esferas acima.

Arquivos principais: `src/Camera.h`, `src/Ray.h`, `src/Intersect.h`, `src/Vetor.h`, `src/Ponto.h`.

## Como rodar

```bash
# já nesta branch (entrega-1)
g++ -std=c++17 -O2 main.cpp -o raytracer
./raytracer utils/input/entrega-1-cenarios/1-tres-esferas.json > saida.ppm
sips -s format png saida.ppm --out saida.png   # macOS (ou ImageMagick / utils/convert_ppm.py)
```

> Requisito: compilador C++17 (`g++ 8+` / `clang++ 7+`). Sem dependências externas — o parser de cena
> JSON está incluído no repositório. A saída é um PPM (texto P3) na saída padrão.

## Detalhes técnicos

### Tipos para pontos e vetores

As classes `src/Ponto` e `src/Vetor` oferecem as operações necessárias (produto escalar, produto
vetorial, normalização, subtração, multiplicação por escalar, etc.).

### Câmera

A câmera móvel é composta por:

- Ponto de localização no mundo $\small C(c_1, c_2, c_3),\; c_i \in \mathbb{R}$
- Ponto para onde a câmera aponta (centro da tela) $\small M(x, y, z),\; x,y,z \in \mathbb{R}$
- Vetor "para cima" $\small V_{up}(v_1, v_2, v_3),\; V_{up} \neq \vec{0}$
- Três vetores ortonormais $\small W, V, U \in \mathbb{R}^3$ — por convenção, **W** tem a mesma direção que $(M - C)$ mas sentido oposto
- Distância câmera-tela $\small d \in \mathbb{R}_+^*$
- Resolução $\small h_{res}, v_{res} \in \mathbb{Z}_+^*$ (largura e altura em pixels)

> **OBS:** A resolução padrão de pixel é 1×1, ou seja, a tela tem tamanho 1 no mundo e cada pixel mede $\frac{1}{h_{res}}$. O ângulo de visão é consequência direta da resolução e da distância $d$.

Cada pixel $(i, j)$ mapeia a um ponto na tela por combinação linear dos vetores da base a partir de $C$.
No repositório, `scene.camera` contém `lookfrom` ($C$), `lookat` ($M$), `up_vector` ($V_{up}$),
`screen_distance` ($d$), `image_width` ($h_{res}$) e `image_height` ($v_{res}$).

### Interseções

- **Esfera** — centro $C_\varepsilon(x,y,z)$, raio $r \in \mathbb{R}_+^*$ e cor difusa $O_d \in [0,1]^3$
- **Plano** — ponto $P_p(x,y,z)$, normal $\hat{N}(v_1,v_2,v_3)$ e cor difusa $O_d \in [0,1]^3$

Para cada pixel, dispara-se um raio e retorna-se a cor $k_d$ do objeto mais próximo (ou preto se não
houver interseção). A cena `sampleScene.json` contém um
[Cornell box](https://en.wikipedia.org/wiki/Cornell_box) com 2 esferas e 5 planos.

---

### Entregas do projeto

- **Entrega 1 — Esferas e planos** ← *você está aqui*
- [Entrega 2 — Malhas de triângulos](../../tree/entrega-2)
- [Entrega 3 — Phong + sombras](../../tree/entrega-3)
- [Entrega 4 — Reflexão + refração](../../tree/entrega-4)
- [Feature individual — Soft shadows](../../tree/feat/soft-shadow)
- [Visão geral do projeto (main)](../../tree/main)
