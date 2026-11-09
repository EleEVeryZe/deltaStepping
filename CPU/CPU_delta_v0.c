#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define INF INT_MAX
#define MAX_NODES 100
#define MAX_BUCKETS 1000

// Estrutura para uma Aresta
typedef struct {
    int destino;
    int peso;
} Aresta;

// Estrutura para a Lista de Adjacências de um Vértice
typedef struct {
    Aresta *arestas;
    int num_arestas;
    int capacidade;
} Vertice;

// Estrutura do Grafo Completo
typedef struct {
    int num_vertices;
    Vertice *vertices;
} GrafoLista;

// Nós dos Baldes (Buckets)
typedef struct BucketNode {
    int vertex;
    struct BucketNode *next;
} BucketNode;

BucketNode *buckets[MAX_BUCKETS];
int tent[MAX_NODES];
int in_bucket[MAX_NODES];

// Funções de Gerenciamento do Grafo
GrafoLista *criar_grafo(int num_vertices) {
    GrafoLista *g = malloc(sizeof(GrafoLista));
    g->num_vertices = num_vertices;
    g->vertices = malloc(num_vertices * sizeof(Vertice));
    for (int i = 0; i < num_vertices; i++) {
        g->vertices[i].num_arestas = 0;
        g->vertices[i].capacidade = 4;
        g->vertices[i].arestas = malloc(g->vertices[i].capacidade * sizeof(Aresta));
    }
    return g;
}

void adicionar_aresta(GrafoLista *g, int origem, int destino, int peso) {
    Vertice *v = &g->vertices[origem];
    if (v->num_arestas >= v->capacidade) {
        v->capacidade *= 2;
        v->arestas = realloc(v->arestas, v->capacidade * sizeof(Aresta));
    }
    v->arestas[v->num_arestas++] = (Aresta){destino, peso};
}

void liberar_grafo(GrafoLista *g) {
    for (int i = 0; i < g->num_vertices; i++) {
        free(g->vertices[i].arestas);
    }
    free(g->vertices);
    free(g);
}

// Funções dos Baldes
void bucket_insere(int v, int dist, int delta) {
    int b_idx = dist / delta;
    if (b_idx >= MAX_BUCKETS) b_idx = MAX_BUCKETS - 1;

    BucketNode *new_node = malloc(sizeof(BucketNode));
    new_node->vertex = v;
    new_node->next = buckets[b_idx];
    buckets[b_idx] = new_node;
    in_bucket[v] = b_idx;
}

void bucket_remove(int v, int b_idx) {
    BucketNode *curr = buckets[b_idx];
    BucketNode *prev = NULL;

    while (curr != NULL) {
        if (curr->vertex == v) {
            if (prev == NULL) {
                buckets[b_idx] = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    in_bucket[v] = -1;
}

// Procedure relax(w, x) (* Insert or move w in B if x < tent(w) *)
void relax(int w, int x, int delta) {
    if (x < tent[w]) {
        if (in_bucket[w] != -1) {
            bucket_remove(w, in_bucket[w]);
        }
        tent[w] = x;
        bucket_insere(w, x, delta);
    }
}

// Parser DIMACS
GrafoLista *ler_dimacs_para_lista(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo DIMACS");
        exit(EXIT_FAILURE);
    }

    char linha[256];
    int num_vertices = 0, num_arestas = 0;

    while (fgets(linha, sizeof(linha), file)) {
        if (linha[0] == 'c') continue;
        if (linha[0] == 'p') {
            sscanf(linha, "p sp %d %d", &num_vertices, &num_arestas);
            break;
        }
    }

    GrafoLista *g = criar_grafo(num_vertices);

    int u, v, w;
    while (fscanf(file, "a %d %d %d", &u, &v, &w) == 3) {
        adicionar_aresta(g, u - 1, v - 1, w);
    }
    fclose(file);

    return g;
}

int main(int argc, char *argv[]) {
    struct timespec inicio_total, fim_total;
    clock_gettime(CLOCK_MONOTONIC, &inicio_total);

    if (argc < 2) {
        printf("Uso: %s <arquivo.gr> [delta]\n", argv[0]);
        return 1;
    }

    int delta = (argc >= 3) ? atoi(argv[2]) : 15; // Delta padrão razoável

    GrafoLista *g = ler_dimacs_para_lista(argv[1]);
    printf("Grafo carregado via Lista de Adjacencias! Vertices: %d\n", g->num_vertices);

    // foreach v in V do tent(v) := infinity
    for (int i = 0; i < g->num_vertices; i++) {
        tent[i] = INF;
        in_bucket[i] = -1;
    }
    for (int i = 0; i < MAX_BUCKETS; i++) buckets[i] = NULL;

    // relax(s, 0) (* Insert source node with distance 0 *)
    relax(0, 0, delta);

    // Estrutura auxiliar para o conjunto R (Remember deleted nodes)
    int *R = malloc(g->num_vertices * sizeof(int));
    int r_size = 0;

    // while ¬isEmpty(B) do (* A phase: Some queued nodes left *)
    int i_bucket = 0;
    while (i_bucket < MAX_BUCKETS) {
        // Encontra o menor balde não vazio
        int next_non_empty = -1;
        for (int b = i_bucket; b < MAX_BUCKETS; b++) {
            if (buckets[b] != NULL) {
                next_non_empty = b;
                break;
            }
        }
        if (next_non_empty == -1) break;
        i_bucket = next_non_empty;

        r_size = 0; // Reseta o conjunto R para a fase atual

        // while B[i] != empty do (* New phase *)
        while (buckets[i_bucket] != NULL) {
            BucketNode *curr = buckets[i_bucket];
            int v = curr->vertex;

            // Remove do balde atual e adiciona ao conjunto R
            buckets[i_bucket] = curr->next;
            free(curr);
            in_bucket[v] = -1;

            R[r_size++] = v; // R := R U B[i] (* Remember deleted nodes *)

            // Req := findRequests(B[i], light) (* Create requests for light edges *)
            Vertice *vert = &g->vertices[v];
            for (int i = 0; i < vert->num_arestas; i++) {
                int neighbor = vert->arestas[i].destino;
                int weight = vert->arestas[i].peso;

                // Arestas leves (weight <= delta)
                if (weight <= delta) {
                    relax(neighbor, tent[v] + weight, delta);
                }
            }
        }

        // Req := findRequests(R, heavy) (* Create requests for heavy edges *)
        // relaxRequests(Req) (* Relaxations will not refill B[i] *)
        for (int idx = 0; idx < r_size; idx++) {
            int v = R[idx];
            Vertice *vert = &g->vertices[v];
            for (int i = 0; i < vert->num_arestas; i++) {
                int neighbor = vert->arestas[i].destino;
                int weight = vert->arestas[i].peso;

                // Arestas pesadas (weight > delta) processadas a partir do conjunto R
                if (weight > delta) {
                    relax(neighbor, tent[v] + weight, delta);
                }
            }
        }
    }

    free(R);

    printf("\nMenores distancias (Delta-Stepping completo com arestas pesadas):\n");
    for (int i = 0; i < g->num_vertices; i++) {
        if (tent[i] == INF)
            printf("Vertice %d (ID orig %d): INF\n", i, i + 1);
        else
            printf("Vertice %d (ID orig %d): %d\n", i, i + 1, tent[i]);
    }

    liberar_grafo(g);

    clock_gettime(CLOCK_MONOTONIC, &fim_total);
    double tempo_total = (fim_total.tv_sec - inicio_total.tv_sec) +
                         (fim_total.tv_nsec - inicio_total.tv_nsec) / 1e9;
    printf("\nTempo total de execucao: %.6f segundos\n", tempo_total);

    return 0;
}