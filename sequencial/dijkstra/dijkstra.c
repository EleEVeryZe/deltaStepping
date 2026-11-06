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

CSRGraph* ler_emaileuall_para_csr(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo email-EuAll");
        exit(EXIT_FAILURE);
    }

    char linha[256];
    int num_vertices = 0;
    int num_arestas = 0;

    // Ler o cabeçalho ignorando as linhas de comentário com '#'
    while (fgets(linha, sizeof(linha), file)) {
        if (linha[0] == '#') {
            // Opcional: podemos extrair o número de vértices e arestas do comentário se quisermos
            if (strstr(linha, "Nodes:") != NULL) {
                sscanf(strstr(linha, "Nodes:"), "Nodes: %d Edges: %d", &num_vertices, &num_arestas);
            }
            continue;
        }
        break; // A primeira linha útil após os comentários é a primeira aresta (0 1)
    }

    // Como já lemos a primeira linha de aresta (u e v), precisamos tratá-la antes de continuar o loop
    typedef struct { int u, v, w; } ArestaTemp;
    
    // Se o num_arestas foi lido do cabeçalho, usamos ele. Caso contrário, usamos um valor seguro.
    if (num_arestas == 0) num_arestas = 500000; // Alocação de segurança
    
    ArestaTemp *arestas = malloc(num_arestas * sizeof(ArestaTemp));
    int total_arestas = 0;

    int u, v;
    // Processar a primeira aresta já lida pelo while anterior
    // (Nota: se a linha lida continha "0 1", usamos sscanf nela)
    if (sscanf(linha, "%d %d", &u, &v) == 2) {
        int peso = (rand() % 10) + 1; // Atribuindo peso positivo para simular latência/distância
        arestas[total_arestas++] = (ArestaTemp){u, v, peso};
    }

    // Ler o restante das arestas do arquivo
    while (fscanf(file, "%d %d", &u, &v) == 2) {
        int peso = (rand() % 10) + 1;
        
        if (total_arestas >= num_arestas) {
            num_arestas *= 2;
            arestas = realloc(arestas, num_arestas * sizeof(ArestaTemp));
        }

        arestas[total_arestas++] = (ArestaTemp){u, v, peso};
    }
    fclose(file);

    // Se o número de vértices não foi capturado do comentário, determinamos pelo maior ID encontrado
    if (num_vertices == 0) {
        for (int i = 0; i < total_arestas; i++) {
            if (arestas[i].u > num_vertices) num_vertices = arestas[i].u;
            if (arestas[i].v > num_vertices) num_vertices = arestas[i].v;
        }
        num_vertices++; // IDs começam em 0
    }

    printf("Grafo carregado! Vertices: %d, Arestas: %d\n", num_vertices, total_arestas);

    // Construção da estrutura CSR padrão
    CSRGraph *graph = malloc(sizeof(CSRGraph));
    graph->num_vertices = num_vertices;
    graph->num_arestas = total_arestas;
    graph->row_ptr = calloc(num_vertices + 1, sizeof(int));
    graph->col_idx = malloc(total_arestas * sizeof(int));
    graph->weights = malloc(total_arestas * sizeof(int));

    for (int i = 0; i < total_arestas; i++) {
        graph->row_ptr[arestas[i].u + 1]++;
    }

    for (int i = 0; i < num_vertices; i++) {
        graph->row_ptr[i + 1] += graph->row_ptr[i];
    }

    int *current_pos = malloc(num_vertices * sizeof(int));
    memcpy(current_pos, graph->row_ptr, num_vertices * sizeof(int));

    for (int i = 0; i < total_arestas; i++) {
        int orig = arestas[i].u;
        int dest = current_pos[orig]++;
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
    CSRGraph *graph = ler_emaileuall_para_csr(argv[1]);
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