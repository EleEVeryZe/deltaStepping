
 // Delta-stepping single-source shortest paths (SSSP) using CUDA and CSR.
 //
 // Input format (zero-based vertex identifiers):
 //   N_NODES N_EDGES
 //   SOURCE DESTINATION WEIGHT
 //   ...
 //
 // By default, the graph is undirected: each input edge (u, v, w) is
 // inserted as both u -> v and v -> u (only once for self-loops).
 // Pass --directed to use each input edge only as u -> v.
 //
 // Weights are non-negative finite doubles; delta must be a positive finite double.
 //
 // Usage:
 //   ./delta-stepping graph.txt source delta numBlocks numThreads [--directed|--undirected] [output.txt]
 //   (the optional output filename and graph flag may appear in either order)
 //
 // This is a baseline implementation: CSR is the only graph representation
 // optimization. Buckets are represented implicitly by floor(distance / delta).
 // Every kernel scans the vertices, avoiding additional frontier-management
 // optimizations. The CPU controls bucket progression; the GPU relaxes edges.

#include <cuda_runtime.h>

#include <chrono>
#include <cmath>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using Distance = double;
using Bucket = unsigned long long;
constexpr Distance INF = std::numeric_limits<Distance>::infinity();
constexpr Bucket NO_BUCKET = ULLONG_MAX;
// double cannot represent 2^64 - 1 exactly; test against 2^64 instead.
constexpr double BUCKET_LIMIT = 18446744073709551616.0;

#define CUDA_CHECK(call)                                                      \
    do {                                                                      \
        cudaError_t error = (call);                                           \
        if (error != cudaSuccess) {                                           \
            std::cerr << "CUDA error at " << __FILE__ << ":" << __LINE__     \
                      << ": " << cudaGetErrorString(error) << '\n';          \
            std::exit(EXIT_FAILURE);                                         \
        }                                                                     \
    } while (0)

struct Edge {
    int source;
    int destination;
    Distance weight;
};

struct CSRGraph {
    int numNodes;
    int numEdges;
    std::vector<int> rowOffsets;
    std::vector<int> columnIndices;
    std::vector<Distance> weights;
};

// Read an edge list and construct the CSR arrays without sorting the edges.
// The header's edge count describes input lines, not the number of CSR arcs.
CSRGraph readGraph(const std::string& filename, bool directed) {
    std::ifstream input(filename);
    if (!input) {
        throw std::runtime_error("Cannot open graph file: " + filename);
    }

    long long nodeCount = 0;
    long long edgeCount = 0;
    if (!(input >> nodeCount >> edgeCount) || nodeCount <= 0 || edgeCount < 0 ||
        nodeCount >= INT_MAX || edgeCount > INT_MAX) {
        throw std::runtime_error("Invalid graph header or graph too large for 32-bit CSR indices");
    }

    CSRGraph graph;
    graph.numNodes = static_cast<int>(nodeCount);
    graph.rowOffsets.assign(static_cast<size_t>(graph.numNodes) + 1, 0);

    std::vector<Edge> edges;
    edges.reserve(static_cast<size_t>(edgeCount));

    for (long long i = 0; i < edgeCount; ++i) {
        long long source = 0;
        long long destination = 0;
        Distance weight = 0.0;
        if (!(input >> source >> destination >> weight)) {
            throw std::runtime_error("Invalid edge at line " + std::to_string(i + 2));
        }
        if (source < 0 || source >= graph.numNodes ||
            destination < 0 || destination >= graph.numNodes ||
            !std::isfinite(weight) || weight < 0.0) {
            throw std::runtime_error("Out-of-range vertex or invalid weight at line " +
                                     std::to_string(i + 2));
        }

        // CSR indices use int, so even expanded undirected graphs must fit.
        const int arcsToInsert = (!directed && source != destination) ? 2 : 1;
        if (edges.size() > static_cast<size_t>(INT_MAX - arcsToInsert)) {
            throw std::runtime_error("Too many CSR arcs for 32-bit indices");
        }

        edges.push_back({static_cast<int>(source), static_cast<int>(destination), weight});
        ++graph.rowOffsets[static_cast<size_t>(source) + 1];

        if (!directed && source != destination) {
            edges.push_back({static_cast<int>(destination), static_cast<int>(source), weight});
            ++graph.rowOffsets[static_cast<size_t>(destination) + 1];
        }
    }
    graph.numEdges = static_cast<int>(edges.size());

    std::string extra;
    if (input >> extra) {
        throw std::runtime_error("Unexpected content after the declared edges");
    }

    for (int v = 0; v < graph.numNodes; ++v) {
        graph.rowOffsets[v + 1] += graph.rowOffsets[v];
    }

    graph.columnIndices.resize(static_cast<size_t>(graph.numEdges));
    graph.weights.resize(static_cast<size_t>(graph.numEdges));
    std::vector<int> cursor = graph.rowOffsets;

    for (const Edge& edge : edges) {
        const int position = cursor[edge.source]++;
        graph.columnIndices[position] = edge.destination;
        graph.weights[position] = edge.weight;
    }

    return graph;
}

// Distances use double. CUDA has no portable atomicMin(double*) on every
// supported architecture, so implement it with a 64-bit compare-and-swap.
// Input validation and non-negative edge weights keep all finite distances >= 0.
__device__ __forceinline__ Distance atomicRead(Distance* address) {
    auto* bits = reinterpret_cast<unsigned long long*>(address);
    return __longlong_as_double(static_cast<long long>(atomicCAS(bits, 0ULL, 0ULL)));
}

__device__ __forceinline__ void atomicMinDouble(Distance* address, Distance candidate) {
    auto* bits = reinterpret_cast<unsigned long long*>(address);
    unsigned long long old = atomicCAS(bits, 0ULL, 0ULL);

    while (candidate < __longlong_as_double(static_cast<long long>(old))) {
        const unsigned long long expected = old;
        const unsigned long long replacement =
            static_cast<unsigned long long>(__double_as_longlong(candidate));
        old = atomicCAS(bits, expected, replacement);
        if (old == expected) {
            break;
        }
    }
}

// Bucket IDs are integers, even though weights, distances and delta are double.
// Returns false when the quotient is outside the 64-bit bucket index range.
__device__ __forceinline__ bool getBucket(Distance distance, Distance delta,
                                          Bucket* bucket) {
    const double quotient = floor(distance / delta);
    if (!isfinite(quotient) || quotient >= BUCKET_LIMIT) {
        return false;
    }
    *bucket = static_cast<Bucket>(quotient);
    return true;
}

// Relax light edges (weight <= delta) from vertices in the current bucket.
// processedDistance[u] records the distance last used to expand vertex u.
// A vertex must be expanded again if its distance decreases within the bucket.
__global__ void relaxLightEdges(
    int numNodes,
    const int* rowOffsets,
    const int* columnIndices,
    const Distance* weights,
    Distance* distances,
    Distance* processedDistance,
    Distance delta,
    Bucket currentBucket,
    unsigned int* activeCount) {

    const int threadId = blockIdx.x * blockDim.x + threadIdx.x;
    const int stride = blockDim.x * gridDim.x;

    for (int u = threadId; u < numNodes; u += stride) {
        const Distance du = atomicRead(&distances[u]);

        if (du == INF || du >= processedDistance[u]) {
            continue;
        }
        Bucket bucket;
        if (!getBucket(du, delta, &bucket) || bucket != currentBucket) {
            continue;
        }

        processedDistance[u] = du;
        atomicAdd(activeCount, 1U);

        for (int e = rowOffsets[u]; e < rowOffsets[u + 1]; ++e) {
            const Distance weight = weights[e];
            if (weight > delta) {
                continue;
            }

            const Distance candidate = du + weight;
            if (!isfinite(candidate)) {
                continue; // The resulting distance is not representable as double.
            }

            const int v = columnIndices[e];
            atomicMinDouble(&distances[v], candidate);
        }
    }
}

// After light-edge closure, relax each heavy edge (weight > delta) once
// from every vertex belonging to the completed bucket.
__global__ void relaxHeavyEdges(
    int numNodes,
    const int* rowOffsets,
    const int* columnIndices,
    const Distance* weights,
    Distance* distances,
    Distance delta,
    Bucket currentBucket) {

    const int threadId = blockIdx.x * blockDim.x + threadIdx.x;
    const int stride = blockDim.x * gridDim.x;

    for (int u = threadId; u < numNodes; u += stride) {
        const Distance du = atomicRead(&distances[u]);
        if (du == INF) {
            continue;
        }
        Bucket bucket;
        if (!getBucket(du, delta, &bucket) || bucket != currentBucket) {
            continue;
        }

        for (int e = rowOffsets[u]; e < rowOffsets[u + 1]; ++e) {
            const Distance weight = weights[e];
            if (weight <= delta) {
                continue;
            }

            const Distance candidate = du + weight;
            if (!isfinite(candidate)) {
                continue; // The resulting distance is not representable as double.
            }

            const int v = columnIndices[e];
            atomicMinDouble(&distances[v], candidate);
        }
    }
}

// Find the smallest nonempty bucket beyond the current one.
// Buckets are implicit: a finite distance d belongs to bucket floor(d/delta).
__global__ void findNextBucket(
    int numNodes,
    const Distance* distances,
    Distance delta,
    Bucket currentBucket,
    Bucket* nextBucket,
    unsigned int* bucketOverflow) {

    const int threadId = blockIdx.x * blockDim.x + threadIdx.x;
    const int stride = blockDim.x * gridDim.x;

    for (int u = threadId; u < numNodes; u += stride) {
        const Distance du = distances[u];
        if (du == INF) {
            continue;
        }

        Bucket bucket;
        if (!getBucket(du, delta, &bucket)) {
            atomicExch(bucketOverflow, 1U);
            continue;
        }
        if (bucket > currentBucket) {
            atomicMin(nextBucket, bucket);
        }
    }
}

unsigned long long parseUnsigned(const char* value, const char* argumentName) {
    const std::string text(value);
    if (text.empty() || text[0] == '-') {
        throw std::runtime_error(std::string("Invalid ") + argumentName + ": " + text);
    }

    size_t consumed = 0;
    unsigned long long result = 0;
    try {
        result = std::stoull(text, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("Invalid ") + argumentName + ": " + text);
    }
    if (consumed != text.size()) {
        throw std::runtime_error(std::string("Invalid ") + argumentName + ": " + text);
    }
    return result;
}

Distance parsePositiveDouble(const char* value, const char* argumentName) {
    const std::string input(value);
    size_t consumed = 0;
    Distance parsed = 0.0;
    try {
        parsed = std::stod(input, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("Invalid ") + argumentName + ": " + input);
    }
    if (consumed != input.size() || !std::isfinite(parsed) || parsed <= 0.0) {
        throw std::runtime_error(std::string("Invalid ") + argumentName + ": " + input);
    }
    return parsed;
}

int main(int argc, char** argv) {
    if (argc < 6 || argc > 8) {
        std::cerr << "Usage: " << argv[0]
                  << " graph.txt source delta numBlocks numThreads"
                  << " [--directed|--undirected] [output.txt]\n";
        return EXIT_FAILURE;
    }

    try {
        const std::string filename = argv[1];
        const unsigned long long sourceArgument = parseUnsigned(argv[2], "source");
        const Distance delta = parsePositiveDouble(argv[3], "delta");
        const unsigned long long blocksArgument = parseUnsigned(argv[4], "numBlocks");
        const unsigned long long threadsArgument = parseUnsigned(argv[5], "numThreads");

        bool directed = false; // Default: undirected graph.
        bool hasGraphFlag = false;
        std::string outputFilename;
        for (int i = 6; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--directed" || argument == "--undirected") {
                if (hasGraphFlag) {
                    throw std::runtime_error("Specify only one graph direction flag");
                }
                directed = (argument == "--directed");
                hasGraphFlag = true;
            } else if (!argument.empty() && argument[0] == '-') {
                throw std::runtime_error("Unknown option: " + argument);
            } else if (outputFilename.empty()) {
                outputFilename = argument;
            } else {
                throw std::runtime_error("Multiple output filenames provided");
            }
        }

        if (blocksArgument == 0 || threadsArgument == 0 ||
            blocksArgument > INT_MAX || threadsArgument > INT_MAX) {
            throw std::runtime_error("numBlocks and numThreads must be positive");
        }

        const int numBlocks = static_cast<int>(blocksArgument);
        const int numThreads = static_cast<int>(threadsArgument);
        const CSRGraph graph = readGraph(filename, directed);

        if (sourceArgument >= static_cast<unsigned long long>(graph.numNodes)) {
            throw std::runtime_error("Source vertex is outside the graph");
        }
        const int source = static_cast<int>(sourceArgument);

        cudaDeviceProp deviceProperties{};
        CUDA_CHECK(cudaGetDeviceProperties(&deviceProperties, 0));
        if (numThreads > deviceProperties.maxThreadsPerBlock ||
            numBlocks > deviceProperties.maxGridSize[0]) {
            throw std::runtime_error("The requested CUDA launch configuration is not supported");
        }

        const size_t vertexBytes = static_cast<size_t>(graph.numNodes) * sizeof(Distance);
        const size_t rowBytes = (static_cast<size_t>(graph.numNodes) + 1) * sizeof(int);
        const size_t columnBytes = static_cast<size_t>(graph.numEdges) * sizeof(int);
        const size_t weightBytes = static_cast<size_t>(graph.numEdges) * sizeof(Distance);

        int* dRowOffsets = nullptr;
        int* dColumnIndices = nullptr;
        Distance* dWeights = nullptr;
        Distance* dDistances = nullptr;
        Distance* dProcessedDistance = nullptr;
        unsigned int* dActiveCount = nullptr;
        Bucket* dNextBucket = nullptr;
        unsigned int* dBucketOverflow = nullptr;

        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dRowOffsets), rowBytes));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dColumnIndices),
                              columnBytes == 0 ? sizeof(int) : columnBytes));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dWeights),
                              weightBytes == 0 ? sizeof(Distance) : weightBytes));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dDistances), vertexBytes));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dProcessedDistance), vertexBytes));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dActiveCount), sizeof(unsigned int)));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dNextBucket), sizeof(Bucket)));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dBucketOverflow), sizeof(unsigned int)));

        std::vector<Distance> distances(static_cast<size_t>(graph.numNodes), INF);
        std::vector<Distance> processedDistance(static_cast<size_t>(graph.numNodes), INF);
        distances[source] = 0.0;

        CUDA_CHECK(cudaMemcpy(dRowOffsets, graph.rowOffsets.data(), rowBytes,
                              cudaMemcpyHostToDevice));
        if (graph.numEdges > 0) {
            CUDA_CHECK(cudaMemcpy(dColumnIndices, graph.columnIndices.data(), columnBytes,
                                  cudaMemcpyHostToDevice));
            CUDA_CHECK(cudaMemcpy(dWeights, graph.weights.data(), weightBytes,
                                  cudaMemcpyHostToDevice));
        }
        CUDA_CHECK(cudaMemcpy(dDistances, distances.data(), vertexBytes,
                              cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(dProcessedDistance, processedDistance.data(), vertexBytes,
                              cudaMemcpyHostToDevice));

        const auto startTime = std::chrono::steady_clock::now();
        Bucket currentBucket = 0;
        unsigned long long completedBuckets = 0;
        unsigned long long lightRounds = 0;

        while (true) {
            // Light-edge closure for the current bucket.
            while (true) {
                CUDA_CHECK(cudaMemset(dActiveCount, 0, sizeof(unsigned int)));
                relaxLightEdges<<<numBlocks, numThreads>>>(
                    graph.numNodes, dRowOffsets, dColumnIndices, dWeights,
                    dDistances, dProcessedDistance, delta, currentBucket,
                    dActiveCount);
                CUDA_CHECK(cudaGetLastError());

                unsigned int activeCount = 0;
                CUDA_CHECK(cudaMemcpy(&activeCount, dActiveCount, sizeof(unsigned int),
                                      cudaMemcpyDeviceToHost));
                if (activeCount == 0) {
                    break;
                }
                ++lightRounds;
            }

            // Heavy edges are processed once after light-edge closure.
            relaxHeavyEdges<<<numBlocks, numThreads>>>(
                graph.numNodes, dRowOffsets, dColumnIndices, dWeights,
                dDistances, delta, currentBucket);
            CUDA_CHECK(cudaGetLastError());
            ++completedBuckets;

            // Advance directly to the next nonempty bucket.
            CUDA_CHECK(cudaMemset(dNextBucket, 0xFF, sizeof(Bucket)));
            CUDA_CHECK(cudaMemset(dBucketOverflow, 0, sizeof(unsigned int)));
            findNextBucket<<<numBlocks, numThreads>>>(
                graph.numNodes, dDistances, delta, currentBucket,
                dNextBucket, dBucketOverflow);
            CUDA_CHECK(cudaGetLastError());

            Bucket nextBucket = NO_BUCKET;
            CUDA_CHECK(cudaMemcpy(&nextBucket, dNextBucket, sizeof(Bucket),
                                  cudaMemcpyDeviceToHost));
            unsigned int bucketOverflow = 0;
            CUDA_CHECK(cudaMemcpy(&bucketOverflow, dBucketOverflow,
                                  sizeof(unsigned int), cudaMemcpyDeviceToHost));
            if (bucketOverflow != 0) {
                throw std::runtime_error("Distance/delta exceeds the 64-bit bucket index range");
            }
            if (nextBucket == NO_BUCKET) {
                break;
            }
            currentBucket = nextBucket;
        }

        CUDA_CHECK(cudaDeviceSynchronize());
        const auto endTime = std::chrono::steady_clock::now();

        CUDA_CHECK(cudaMemcpy(distances.data(), dDistances, vertexBytes,
                              cudaMemcpyDeviceToHost));

        std::ofstream outputFile;
        std::ostream* output = &std::cout;
        if (!outputFilename.empty()) {
            outputFile.open(outputFilename);
            if (!outputFile) {
                throw std::runtime_error("Cannot open output file: " + outputFilename);
            }
            output = &outputFile;
        }

        *output << std::setprecision(std::numeric_limits<Distance>::max_digits10);
        *output << "node distance\n";
        for (int node = 0; node < graph.numNodes; ++node) {
            *output << node << ' ';
            if (distances[node] == INF) {
                *output << "INF";
            } else {
                *output << distances[node];
            }
            *output << '\n';
        }

        const double elapsedMs =
            std::chrono::duration<double, std::milli>(endTime - startTime).count();
        std::cerr << "Nodes: " << graph.numNodes << ", CSR arcs: " << graph.numEdges << '\n'
                  << "Graph: " << (directed ? "directed" : "undirected") << '\n'
                  << "Source: " << source << ", delta: " << delta << '\n'
                  << "Blocks: " << numBlocks << ", threads per block: " << numThreads << '\n'
                  << "Completed buckets: " << completedBuckets << '\n'
                  << "Light relaxation rounds: " << lightRounds << '\n'
                  << "Processing time (ms): " << elapsedMs << '\n';

        CUDA_CHECK(cudaFree(dRowOffsets));
        CUDA_CHECK(cudaFree(dColumnIndices));
        CUDA_CHECK(cudaFree(dWeights));
        CUDA_CHECK(cudaFree(dDistances));
        CUDA_CHECK(cudaFree(dProcessedDistance));
        CUDA_CHECK(cudaFree(dActiveCount));
        CUDA_CHECK(cudaFree(dNextBucket));
        CUDA_CHECK(cudaFree(dBucketOverflow));
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
