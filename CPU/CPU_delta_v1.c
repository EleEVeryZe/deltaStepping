#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <time.h>
#include <immintrin.h> // Necessário para as instruções AVX2

#define INF FLT_MAX
#define MAX_NODES 250000
#define MAX_BUCKETS 1000

typedef struct {
    int destino;
    float peso; // Alterado para float para encaixar 8 lanes em 256 bits (AVX2)
} Aresta;

typedef struct {
    Aresta *arestas;
    int num_arestas;
    int capacidade;
} Vertice;

typedef struct {
    int num_vertices;
    int num_arestas;
    Vertice *vertices;
} GrafoLista;

typedef struct BucketNode {
    int vertex;
    struct BucketNode *next;
} BucketNode;

BucketNode *buckets[MAX_BUCKETS];
float tent[MAX_NODES]; // Alterado para float
int in_bucket[MAX_NODES];

GrafoLista *criar_grafo(int num_vertices, int num_arestas) {
    GrafoLista *g = malloc(sizeof(GrafoLista));
    g->num_vertices = num_vertices;
    g->num_arestas = num_arestas;
    g->vertices = malloc(num_vertices * sizeof(Vertice));
    for (int i = 0; i < num_vertices; i++) {
        g->vertices[i].num_arestas = 0;
        g->vertices[i].capacidade = 4;
        g->vertices[i].arestas = malloc(g->vertices[i].capacidade * sizeof(Aresta));
    }
    return g;
}

void adicionar_aresta(GrafoLista *g, int origem, int destino, float peso) {
    Vertice *v = &g->vertices[origem];
    if (v->num_arestas >= v->capacidade) {
        v->capacidade *= 2;
        v->arestas = realloc(v->arestas, v->capacidade * sizeof(Aresta));
    }
    v->arestas[v->num_arestas++] = (Aresta){destino, peso};
}

void liberar_grafo(GrafoLista *g) {
    if (!g) return;
    for (int i = 0; i < g->num_vertices; i++) {
        free(g->vertices[i].arestas);
    }
    free(g->vertices);
    free(g);
}

void bucket_insere(int v, float dist, float delta) {
    int b_idx = (int)(dist / delta);
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

// Procedure relax(w, x)
void relax(int w, float x, float delta) {
    if (x < tent[w]) {
        if (in_bucket[w] != -1) {
            bucket_remove(w, in_bucket[w]);
        }
        tent[w] = x;
        bucket_insere(w, x, delta);
    }
}

GrafoLista *ler_grafo_decimal(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo do grafo");
        exit(EXIT_FAILURE);
    }

    int num_vertices = 0, num_arestas = 0;
    
    if (fscanf(file, "%d %d", &num_vertices, &num_arestas) != 2) {
        fprintf(stderr, "Erro ao ler o cabecalho do grafo.\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    GrafoLista *g = criar_grafo(num_vertices, num_arestas);

    int u, v;
    float peso;
    while (fscanf(file, "%d %d %f", &u, &v, &peso) == 3) {
        adicionar_aresta(g, u, v, peso);
    }
    fclose(file);

    return g;
}

int main(int argc, char *argv[]) {
    struct timespec inicio_total, fim_total;
    struct timespec inicio_leitura, fim_leitura;
    struct timespec inicio_algoritmo, fim_algoritmo;

    clock_gettime(CLOCK_MONOTONIC, &inicio_total);

    if (argc < 2) {
        printf("Uso: %s <arquivo_grafo> [delta]\n", argv[0]);
        return 1;
    }

    float delta = (argc >= 3) ? atof(argv[2]) : 0.005f;

    // Medindo o tempo de leitura do grafo
    clock_gettime(CLOCK_MONOTONIC, &inicio_leitura);
    GrafoLista *g = ler_grafo_decimal(argv[1]);
    clock_gettime(CLOCK_MONOTONIC, &fim_leitura);

    printf("Grafo carregado! Vertices: %d, Arestas: %d\n", g->num_vertices, g->num_arestas);

    // Medindo o tempo de execução do algoritmo
    clock_gettime(CLOCK_MONOTONIC, &inicio_algoritmo);

    // Inicializa distâncias com infinito
    for (int i = 0; i < g->num_vertices; i++) {
        tent[i] = INF;
        in_bucket[i] = -1;
    }
    for (int i = 0; i < MAX_BUCKETS; i++) buckets[i] = NULL;

    // relax(s, 0) - origem no vértice 0
    relax(0, 0.0f, delta);

    int *R = malloc(g->num_vertices * sizeof(int));
    int r_size = 0;

    int i_bucket = 0;
    while (i_bucket < MAX_BUCKETS) {
        int next_non_empty = -1;
        for (int b = i_bucket; b < MAX_BUCKETS; b++) {
            if (buckets[b] != NULL) {
                next_non_empty = b;
                break;
            }
        }
        if (next_non_empty == -1) break;
        i_bucket = next_non_empty;

        r_size = 0;

        // Processamento vetorizado com AVX2 (8 lanes) para arestas leves (weight <= delta)
        while (buckets[i_bucket] != NULL) {
            BucketNode *curr = buckets[i_bucket];
            int v = curr->vertex;

            buckets[i_bucket] = curr->next;
            free(curr);
            in_bucket[v] = -1;

            R[r_size++] = v; // R := R U B[i]

            Vertice *vert = &g->vertices[v];
            int i = 0;
            __m256 v_tent_v = _mm256_set1_ps(tent[v]);

            // Laço SIMD processando blocos de 8 arestas simultaneamente
            for (; i <= vert->num_arestas - 8; i += 8) {
                int dests[8];
                float w_arr[8];
                for (int k = 0; k < 8; k++) {
                    dests[k] = vert->arestas[i + k].destino;
                    w_arr[k] = vert->arestas[i + k].peso;
                }

                __m256 weights = _mm256_loadu_ps(w_arr);
                __m256i v_dests = _mm256_loadu_si256((__m256i*)dests);
                
                // Coleta vetorizada das distâncias atuais dos 8 vizinhos (Gather)
                __m256 current_tents = _mm256_i32gather_ps(tent, v_dests, sizeof(float));
                
                // Calcula novas distâncias: tent[v] + peso
                __m256 new_tents = _mm256_add_ps(v_tent_v, weights);
                
                // Compara se new_tents < current_tents
                __m256 mask = _mm256_cmp_ps(new_tents, current_tents, _CMP_LT_OS);
                int mask_int = _mm256_movemask_ps(mask);

                if (mask_int != 0) {
                    float res_new[8];
                    _mm256_storeu_ps(res_new, new_tents);
                    for (int k = 0; k < 8; k++) {
                        // Verifica se a lane melhorou a distância e se a aresta é leve
                        if ((mask_int & (1 << k)) && (w_arr[k] <= delta)) {
                            relax(dests[k], res_new[k], delta);
                        }
                    }
                }
            }

            // Resto (Tail loop) para arestas restantes (< 8)
            for (; i < vert->num_arestas; i++) {
                int neighbor = vert->arestas[i].destino;
                float weight = vert->arestas[i].peso;

                if (weight <= delta) {
                    relax(neighbor, tent[v] + weight, delta);
                }
            }
        }

        // Processamento vetorizado com AVX2 (8 lanes) para arestas pesadas (weight > delta) a partir de R
        for (int idx = 0; idx < r_size; idx++) {
            int v = R[idx];
            Vertice *vert = &g->vertices[v];
            int i = 0;
            __m256 v_tent_v = _mm256_set1_ps(tent[v]);

            for (; i <= vert->num_arestas - 8; i += 8) {
                int dests[8];
                float w_arr[8];
                for (int k = 0; k < 8; k++) {
                    dests[k] = vert->arestas[i + k].destino;
                    w_arr[k] = vert->arestas[i + k].peso;
                }

                __m256 weights = _mm256_loadu_ps(w_arr);
                __m256i v_dests = _mm256_loadu_si256((__m256i*)dests);
                __m256 current_tents = _mm256_i32gather_ps(tent, v_dests, sizeof(float));
                __m256 new_tents = _mm256_add_ps(v_tent_v, weights);
                
                __m256 mask = _mm256_cmp_ps(new_tents, current_tents, _CMP_LT_OS);
                int mask_int = _mm256_movemask_ps(mask);

                if (mask_int != 0) {
                    float res_new[8];
                    _mm256_storeu_ps(res_new, new_tents);
                    for (int k = 0; k < 8; k++) {
                        // Verifica se melhorou e se a aresta é pesada
                        if ((mask_int & (1 << k)) && (w_arr[k] > delta)) {
                            relax(dests[k], res_new[k], delta);
                        }
                    }
                }
            }

            // Tail loop para arestas pesadas restantes (< 8)
            for (; i < vert->num_arestas; i++) {
                int neighbor = vert->arestas[i].destino;
                float weight = vert->arestas[i].peso;

                if (weight > delta) {
                    relax(neighbor, tent[v] + weight, delta);
                }
            }
        }
    }

    free(R);
    clock_gettime(CLOCK_MONOTONIC, &fim_algoritmo);

    printf("\nMenores distancias calculadas (Primeiros 15 vertices):\n");
    for (int i = 0; i < (g->num_vertices < 15 ? g->num_vertices : 15); i++) {
        if (tent[i] == INF)
            printf("Vertice %d: INF\n", i);
        else
            printf("Vertice %d: %.6f\n", i, tent[i]);
    }

    liberar_grafo(g);

    clock_gettime(CLOCK_MONOTONIC, &fim_total);

    double tempo_leitura = (fim_leitura.tv_sec - inicio_leitura.tv_sec) +
                           (fim_leitura.tv_nsec - inicio_leitura.tv_nsec) / 1e9;

    double tempo_algoritmo = (fim_algoritmo.tv_sec - inicio_algoritmo.tv_sec) +
                             (fim_algoritmo.tv_nsec - inicio_algoritmo.tv_nsec) / 1e9;

    double tempo_total = (fim_total.tv_sec - inicio_total.tv_sec) +
                         (fim_total.tv_nsec - inicio_total.tv_nsec) / 1e9;

    printf("\n--- METRICAS DE TEMPO (AVX2 - 8 LANES) ---\n");
    printf("Tempo de leitura do grafo: %.6f segundos\n", tempo_leitura);
    printf("Tempo de execucao do algoritmo: %.6f segundos\n", tempo_algoritmo);
    printf("Tempo total de execucao: %.6f segundos\n", tempo_total);

    return 0;
}