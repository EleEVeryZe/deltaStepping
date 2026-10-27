# Delta-Stepping em CPU


## Estado atual do projeto

Este diretório contém a implementação e os dados para avaliação do algoritmo Delta-Stepping em CPU, com foco em desempenho e otimização de memória. O trabalho atual busca comparar uma linha de base sequencial com uma versão otimizada para uso de SIMD e melhor localidade de cache.

A estrutura do projeto foi simplificada para refletir o estado real da implementação: há uma versão em C++ para processamento do grafo e um dataset em Matrix Market para benchmarking.

## Estrutura atual do diretório

```text
CPU/
├── README.md
├── delta_std.cpp
└── road-roadNet-CA/
    ├── readme.html
    └── road-roadNet-CA.mtx
```

- `delta_std.cpp`: implementação principal do algoritmo Delta-Stepping em C++.
- `road-roadNet-CA/road-roadNet-CA.mtx`: grafo de entrada em formato Matrix Market usado nos experimentos.
- `road-roadNet-CA/readme.html`: documentação do dataset disponibilizado junto com o grafo.
- `README.md`: descrição do projeto, estrutura e objetivos.

## Visão geral do algoritmo

O Delta-Stepping é uma variação do algoritmo de caminhos mínimos com origem única (SSSP) que separa as arestas em duas classes:

- arestas leves: peso <= delta;
- arestas pesadas: peso > delta.

A ideia central é processar relaxamentos em buckets, reduzindo o número de operações em grafos esparsos e permitindo melhor organização do trabalho computacional.

## Versões contempladas

### 1) Versão sequencial de referência

A versão sequencial continua importante como baseline para:

- medir o custo estrutural do algoritmo;
- comparar desempenho bruto com a versão otimizada;
- validar correção funcional;
- quantificar ganhos de SIMD, cache e organização de dados.

Ela deve permanecer disponível como referência para comparação posterior.

### 2) Versão otimizada para SIMD e cache

A implementação em desenvolvimento prioriza:

- processamento em blocos para melhor aproveitamento de SIMD;
- melhoria de localidade de referência;
- redução de acessos irregulares e espalhados em memória;
- menor overhead em estruturas de bucket e relaxamento;
- melhor uso do cache em grafos grandes.

O objetivo principal é explorar o desempenho do algoritmo em CPU com foco em vetorização e eficiência de memória, sem perder a referência sequencial para comparação.

## Como compilar

No diretório `CPU`, a compilação pode ser feita com:

```bash
g++ -O3 -std=c++17 -Wall -Wextra delta_std.cpp -o delta_std
```

Para explorar vetorização mais agressiva, pode-se usar:

```bash
g++ -O3 -std=c++17 -mavx2 -mfma delta_std.cpp -o delta_std
```

## Como executar

A execução usa um grafo em formato Matrix Market, como o dataset localizado em:

```bash
./delta_std road-roadNet-CA/road-roadNet-CA.mtx
```

O programa atual é um protótipo de benchmark e pode ser ajustado para receber parâmetros adicionais como:

- valor de delta;
- número de origem (source node);
- limite de instâncias ou repetições;
- seleção do tipo de execução (sequencial ou otimizada).

## Executando o GAP Benchmark Suite (GAPBS)

Embora o foco principal do projeto seja a implementação e a otimização do Delta-Stepping em CPU, a seção abaixo documenta o fluxo de uso do benchmark GAPBS como referência de pipeline de avaliação e comparação de desempenho. A ideia é manter a linha de base sequencial do projeto e, quando necessário, comparar com uma implementação mais madura do estado da arte em SSSP.

### 1) Clone e compilação do GAPBS

Se o GAPBS ainda não estiver disponível no ambiente, faça o clone do repositório e compile os binários:

```bash
git clone https://github.com/sbeamer/gapbs.git
cd gapbs
make
```

### 2) Conversão do dataset

Para reduzir a sobrecarga de leitura do grafo durante a execução, converta o arquivo Matrix Market para o formato binário do GAPBS:

```bash
./converter -f ../CPU/road-roadNet-CA/road-roadNet-CA.mtx -b ../CPU/road-roadNet-CA/road-roadNet-CA.sg
```

### 3) Execução sequencial como linha de base

Para capturar a execução sequencial de referência, force a execução com uma única thread:

```bash
OMP_NUM_THREADS=1 ./sssp -f ../CPU/road-roadNet-CA/road-roadNet-CA.sg -n 10
```

### 4) Execução paralela com todas as threads disponíveis

Para avaliar o comportamento em hardware multicore, execute o benchmark com o scheduler padrão:

```bash
./sssp -f ../CPU/road-roadNet-CA/road-roadNet-CA.sg -n 10
```

O parâmetro `-n 10` indica que o benchmark seleciona dez fontes aleatórias dentro do grafo, mede o tempo de execução de cada uma e calcula uma média, reduzindo ruído devido ao aquecimento do cache e a variações iniciais de execução.

### 5) Papel do GAPBS neste projeto

O GAPBS serve como referência externa de desempenho em SSSP para comparação posterior com a implementação local em CPU. A estrutura sequencial do trabalho continua sendo o ponto de comparação principal, enquanto a versão otimizada em SIMD/cache busca reduzir o custo de memória e melhorar a eficiência por núcleo.

## Observações sobre o estado do código

- A implementação atual já demonstra a estrutura do algoritmo Delta-Stepping em C++.
- O código ainda está em evolução para incorporar otimizações explícitas de memória e SIMD.
- A versão sequencial deve continuar disponível como referência para futuras comparações de desempenho.
- O foco principal do projeto é a melhoria de eficiência em CPU, e não a criação de um pipeline completo de benchmark automatizado.

## Próximos passos esperados

- separar claramente a versão sequencial da versão otimizada;
- introduzir estruturas mais cache-friendly para arestas e buckets;
- aplicar vetorização explícita para relaxamentos de borda;
- medir speedup em relação à linha de base sequencial;
- documentar resultados para diferentes valores de delta e grafos.

## Referências conceituais

- Meyer, U. and Sanders, P. (2003). Delta-stepping: a parallelizable shortest path algorithm.
- Beamer, S., Asanović, K., and Patterson, D. (2015). The GAP benchmark suite.

Este README reflete o estado atual do diretório e o foco do trabalho em otimizações de SIMD e cache, preservando a referência sequencial para comparação posterior.

