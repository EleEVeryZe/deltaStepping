#include <stdio.h>
#include <stdlib.h>

void gerar_grafo_sintetico(int num_vertices, const char* filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    // Número de conexões por vértice não pode ser maior que (num_vertices - 1)
    int grau_saida = (num_vertices - 1 < 4) ? (num_vertices - 1) : 4;
    int num_arestas = num_vertices * grau_saida;

    fprintf(f, "p sp %d %d\n", num_vertices, num_arestas);

    srand(1337);
    for (int i = 0; i < num_vertices; i++) {
        // Usamos um array simples para controlar vizinhos já conectados neste vértice
        int *visitados = calloc(num_vertices, sizeof(int));
        visitados[i] = 1; // Não conecta em si mesmo (evita self-loop)

        for (int k = 0; k < grau_saida; k++) {
            int destino;
            do {
                // Sorteia um destino aleatório de forma mais orgânica
                destino = rand() % num_vertices;
            } while (visitados[destino]); // Garante que não há arestas paralelas

            visitados[destino] = 1;
            int peso = (rand() % 100) + 1; // Pesos entre 1 e 100

            fprintf(f, "a %d %d %d\n", i + 1, destino + 1, peso);
        }
        free(visitados);
    }
    fclose(f);
    printf("Grafo gerado sem arestas paralelas em %s\n", filename);
}
int main(int argc, char *argv[]) {
    printf("Gerando grafo de \nnumero de vertices: %d\nNome do Arquivo: %s", atoi(argv[1]), argv[2]);
    gerar_grafo_sintetico(atoi(argv[1]), argv[2]);
    printf("\nFIM");
}