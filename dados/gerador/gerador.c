#include <stdio.h>
#include <stdlib.h>

/**
 * GENERATES IN THE DIMACS FORMAT
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void gerar_grafo_denso(int num_vertices, int grau_medio, const char* filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    // Garante que o grau não ultrapasse o limite de vértices
    if (grau_medio >= num_vertices) grau_medio = num_vertices - 1;
    
    long long num_arestas = (long long)num_vertices * grau_medio;
    fprintf(f, "%d %lld\n", num_vertices, num_arestas);

    srand(42);
    for (int i = 0; i < num_vertices; i++) {
        int *visitados = calloc(num_vertices, sizeof(int));
        visitados[i] = 1; // Evita self-loop

        for (int k = 0; k < grau_medio; k++) {
            int destino;
            do {
                destino = rand() % num_vertices;
            } while (visitados[destino]);

            visitados[destino] = 1;
            
            // Gera pesos decimais realistas entre 0.001 e 10.0
            double peso = ((double)rand() / RAND_MAX) * 9.999 + 0.001;

            fprintf(f, "%d %d %.6f\n", i, destino, peso);
        }
        free(visitados);
    }
    fclose(f);
    printf("Grafo denso gerado com sucesso: %d vertices, %lld arestas.\n", num_vertices, num_arestas);
}

int main() {
    // Exemplo: 50 mil vértices com grau médio de 30 arestas por nó
    gerar_grafo_denso(50000, 30, "../computado/v50ke30m.txt");
    return 0;
}
