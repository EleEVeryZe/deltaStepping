SE O GRAFO NAO FOR NAO-DIRECIONADO, DESCOMENTE A LINHA 60


##How to execute
    gcc -O3 dijkstra.c -o dijkstra
    ./dijkstra ../dados/road-roadNet-CA/road-roadNet-CA.mtx

##  How to execute in root
    gcc -O3 ./sequencial/dijkstra/dijkstra.c -o ./sequencial/dijkstra/dijkstra
    
    Sobre a base:
    ./sequencial/dijkstra/dijkstra ./dados/road-roadNet-CA/road-roadNet-CA.mtx
    ./sequencial/dijkstra/dijkstra ./dados/email-enron/email-enron.mtx

## Execute with PERF
``
perf stat -e cache-misses,cache-references,instructions,cycles,L1-dcache-load-misses ./sequencial/dijkstra/dijkstra ./dados/road-roadNet-CA/road-roadNet-CA.mtx
``
