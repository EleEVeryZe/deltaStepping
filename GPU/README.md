## architecture: 
RTX 5070 (sm_120)

## Usage

```bash
Make
./delta-stepping graph.txt source-node delta numBlocks numThreads [--directed|--undirected] [output.txt]
```

### Example

```bash
./delta-stepping ../dados/RoadNetworks/california.txt 0 3 128 256 --undirected output-distances.txt
```