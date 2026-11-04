#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h> // Necessário para clock_gettime

#define INF INT_MAX

// Estrutura para representar o Grafo em formato CSR (Compressed Sparse Row)
typedef struct {
    int num_vertices;
    int num_arestas;
    int *row_ptr;   // Ponteiros de início das linhas (tamanho: num_vertices + 1)
    int *col_idx;   // Índices das colunas/destinos (tamanho: num_arestas)
    int *weights;   // Pesos das arestas (tamanho: num_arestas)
} CSRGraph;

// Função para ler o arquivo .mtx e converter para CSR
CSRGraph* ler_mtx_para_csr(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo .mtx");
        exit(EXIT_FAILURE);
    }

    char linha[256];
    // Ignorar comentários do cabeçalho Matrix Market
    while (fgets(linha, sizeof(linha), file)) {
        if (linha[0] != '%') break;
    }

    // Ler dimensões do grafo: M (linhas), N (colunas), NNZ (número de arestas)
    int rows, cols, nnz;
    if (sscanf(linha, "%d %d %d", &rows, &cols, &nnz) != 3) {
        fprintf(stderr, "Erro ao ler o cabeçalho do .mtx\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    int num_vertices = rows;
    
    // Estrutura temporária para armazenar as arestas
    typedef struct { int u, v, w; } ArestaTemp;
    ArestaTemp *arestas = malloc(2 * nnz * sizeof(ArestaTemp));
    int total_arestas = 0;

    for (int i = 0; i < nnz; i++) {
        int u, v;
        double w;
        fscanf(file, "%d %d %lf", &u, &v, &w);
        
        // Ajuste para indexação baseada em 0 (o .mtx começa em 1)
        u--; 
        v--; 
        int peso = (int)w;

        arestas[total_arestas++] = (ArestaTemp){u, v, peso};
        
        // Descomente se o grafo for não-direcionado:
        //if (u != v) arestas[total_arestas++] = (ArestaTemp){v, u, peso};
    }
    fclose(file);

    // Alocar a estrutura CSR
    CSRGraph *graph = malloc(sizeof(CSRGraph));
    graph->num_vertices = num_vertices;
    graph->num_arestas = total_arestas;
    graph->row_ptr = calloc(num_vertices + 1, sizeof(int));
    graph->col_idx = malloc(total_arestas * sizeof(int));
    graph->weights = malloc(total_arestas * sizeof(int));

    // Contar o grau de cada vértice para preencher o row_ptr
    for (int i = 0; i < total_arestas; i++) {
        graph->row_ptr[arestas[i].u + 1]++;
    }

    // Acumular para formar os offsets (prefix sum)
    for (int i = 0; i < num_vertices; i++) {
        graph->row_ptr[i + 1] += graph->row_ptr[i];
    }

    // Preencher col_idx e weights
    int *current_pos = malloc(num_vertices * sizeof(int));
    memcpy(current_pos, graph->row_ptr, num_vertices * sizeof(int));

    for (int i = 0; i < total_arestas; i++) {
        int u = arestas[i].u;
        int dest = current_pos[u]++;
        graph->col_idx[dest] = arestas[i].v;
        graph->weights[dest] = arestas[i].w;
    }

    free(arestas);
    free(current_pos);
    return graph;
}

// Algoritmo de Dijkstra Simples (baseline)
void dijkstra(CSRGraph *graph, int origem) {
    int V = graph->num_vertices;
    int *dist = malloc(V * sizeof(int));
    int *visitado = calloc(V, sizeof(int));

    for (int i = 0; i < V; i++) {
        dist[i] = INF;
    }
    dist[origem] = 0;

    for (int i = 0; i < V - 1; i++) {
        int u = -1;
        int min_dist = INF;
        for (int v = 0; v < V; v++) {
            if (!visitado[v] && dist[v] < min_dist) {
                min_dist = dist[v];
                u = v;
            }
        }

        if (u == -1 || min_dist == INF) break;
        visitado[u] = 1;

        int inicio = graph->row_ptr[u];
        int fim = graph->row_ptr[u + 1];
        for (int idx = inicio; idx < fim; idx++) {
            int v = graph->col_idx[idx];
            int peso = graph->weights[idx];

            if (!visitado[v] && dist[u] != INF && dist[u] + peso < dist[v]) {
                dist[v] = dist[u] + peso;
            }
        }
    }

    // Exibir algumas distâncias resultantes
    printf("Distancias a partir do vertice 0 (primeiros 10 vertices):\n");
    for (int i = 0; i < (V < 10 ? V : 10); i++) {
        if (dist[i] == INF) printf("Vertice %d: INF\n", i);
        else printf("Vertice %d: %d\n", i, dist[i]);
    }

    free(dist);
    free(visitado);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <arquivo.mtx>\n", argv[0]);
        return 1;
    }

    struct timespec inicio_total, fim_total;
    struct timespec inicio_alg, fim_alg;

    // Medindo o tempo total (incluindo leitura e conversão para CSR)
    clock_gettime(CLOCK_MONOTONIC, &inicio_total);

    printf("Carregando o arquivo %s e convertendo para CSR...\n", argv[1]);
    CSRGraph *graph = ler_mtx_para_csr(argv[1]);
    printf("Grafo carregado! Vértices: %d, Arestas: %d\n", graph->num_vertices, graph->num_arestas);

    // Medindo exclusivamente o tempo de resposta do algoritmo
    printf("Executando o Dijkstra...\n");
    clock_gettime(CLOCK_MONOTONIC, &inicio_alg);
    
    dijkstra(graph, 0); // Executa a partir do vértice 0
    
    clock_gettime(CLOCK_MONOTONIC, &fim_alg);

    clock_gettime(CLOCK_MONOTONIC, &fim_total);

    // Calculando os tempos em segundos
    double tempo_alg = (fim_alg.tv_sec - inicio_alg.tv_sec) + 
                       (fim_alg.tv_nsec - inicio_alg.tv_nsec) / 1e9;
                       
    double tempo_total = (fim_total.tv_sec - inicio_total.tv_sec) + 
                         (fim_total.tv_nsec - inicio_total.tv_nsec) / 1e9;

    // Logs de desempenho
    printf("\n--- METRICAS DE TEMPO ---\n");
    printf("Tempo de execucao do Dijkstra: %.6f segundos\n", tempo_alg);
    printf("Tempo total de execucao (I/O + Algoritmo): %.6f segundos\n", tempo_total);

    // Liberação de memória
    free(graph->row_ptr);
    free(graph->col_idx);
    free(graph->weights);
    free(graph);

    return 0;
}