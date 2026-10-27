#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>

const double INF = std::numeric_limits<double>::infinity();

// Structure to represent an edge
struct Edge {
    int to;
    double weight;
};

// Represents a relaxation request
struct Request {
    int to;
    double dist;
};

void delta_stepping(const std::vector<std::vector<Edge>>& graph, int source, double delta) {
    int num_nodes = graph.size();
    std::vector<double> tent(num_nodes, INF);
    tent[source] = 0;

    // Vector of buckets: buckets[i] contains a vector of nodes currently assigned there
    // Using a vector of vectors as a simple bucket-queue abstraction
    std::vector<std::vector<int>> buckets;
    
    // Lambda to find or dynamically resize buckets as needed
    auto insert_into_bucket = [&](int node, double dist) {
        size_t bucket_idx = static_cast<size_t>(dist / delta);
        if (bucket_idx >= buckets.size()) {
            buckets.resize(bucket_idx + 1);
        }
        buckets[bucket_idx].push_back(node);
    };

    insert_into_bucket(source, 0);
    size_t current_bucket_idx = 0;

    while (current_bucket_idx < buckets.size()) {
        // Skip empty buckets
        if (buckets[current_bucket_idx].empty()) {
            current_bucket_idx++;
            continue;
        }

        std::vector<int> R; // To track nodes deleted from this bucket

        // Inner loop: process light edges inside the current bucket phase
        while (!buckets[current_bucket_idx].empty()) {
            // Copy and clear the current bucket entries for this sub-phase
            std::vector<int> B_i = std::move(buckets[current_bucket_idx]);
            buckets[current_bucket_idx].clear();

            std::vector<Request> light_requests;

            for (int u : B_i) {
                // To prevent processing duplicate legacy entries in the same bucket
                if (std::find(R.begin(), R.end(), u) == R.end()) {
                    R.push_back(u);
                }

                for (const auto& edge : graph[u]) {
                    if (edge.weight <= delta) {
                        light_requests.push_back({edge.to, tent[u] + edge.weight});
                    }
                }
            }

            // Relax light requests
            for (const auto& req : light_requests) {
                if (req.dist < tent[req.to]) {
                    tent[req.to] = req.dist;
                    insert_into_bucket(req.to, req.dist);
                }
            }
        }

        // Current bucket is now completely settled. Process deferred heavy requests.
        std::vector<Request> heavy_requests;
        for (int u : R) {
            for (const auto& edge : graph[u]) {
                if (edge.weight > delta) {
                    heavy_requests.push_back({edge.to, tent[u] + edge.weight});
                }
            }
        }

        // Relax heavy requests
        for (const auto& req : heavy_requests) {
            if (req.dist < tent[req.to]) {
                tent[req.to] = req.dist;
                insert_into_bucket(req.to, req.dist);
            }
        }

        current_bucket_idx++;
    }

    // Output verification for first few nodes
    std::cout << "Node\tDistance\n";
    for (int i = 0; i < std::min(num_nodes, 10); ++i) {
        std::cout << i << "\t" << tent[i] << "\n";
    }
}

int main() {
    // Example: 5 nodes graph representation matching an Adjacency List pattern
    int num_nodes = 5;
    std::vector<std::vector<Edge>> graph(num_nodes);

    graph[0].push_back({1, 1.0});
    graph[0].push_back({2, 5.0});
    graph[1].push_back({2, 2.0});
    graph[1].push_back({3, 7.0});
    graph[2].push_back({3, 1.0});
    graph[3].push_back({4, 3.0});

    double delta = 2.0;
    delta_stepping(graph, 0, delta);

    return 0;
}
