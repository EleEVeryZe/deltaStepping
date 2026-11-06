SE O GRAFO NAO FOR NAO-DIRECIONADO, DESCOMENTE A LINHA 60


##How to execute
    gcc -O3 dijkstra.c -o dijkstra
    ./dijkstra ../dados/emailEu/Email-EuAll.txt

##  How to execute in root
    gcc -O3 ./sequencial/dijkstra/dijkstra.c -o ./sequencial/dijkstra/dijkstra
    
    Sobre a base:
    ./sequencial/dijkstra/dijkstra ./dados/emailEu/Email-EuAll.txt 


## Execute with PERF
``
perf stat -e cache-misses,cache-references,instructions,cycles,L1-dcache-load-misses ./sequencial/dijkstra/dijkstra ./dados/emailEu/Email-EuAll.txt
``

--- METRICAS DE TEMPO ---
Tempo de execucao do Dijkstra: 15.064798 segundos
Tempo total de execucao (I/O + Algoritmo): 15.111656 segundos