class DeltaStepping {
  /**
   * @param {Object} graph - Adjacency list mapping node -> array of { to, weight }
   * @param {string|number} source - The source node
   * @param {number} delta - The bucket threshold parameter (Δ)
   */
  constructor(graph, source, delta) {
    this.graph = graph;
    this.source = source;
    this.delta = delta;
    this.tent = new Map(); // tent(v)
    this.B = [];           // Buckets array of Sets
  }

  run() {
    // foreach v in V do tent(v) := infinity
    for (const v of Object.keys(this.graph)) {
      this.tent.set(v, Infinity);
    }

    // relax(s, 0) (* Insert source node with distance 0 *)
    this.relax(this.source, 0);

    // while ¬isEmpty(B) do (* A phase: Some queued nodes left *)
    while (!this.isEmptyB()) {
      // i := min{j >= 0: B[j] != empty} (* Smallest nonempty bucket *)
      const i = this.minNonEmptyBucket();
      let R = new Set(); // Remember deleted nodes

      // while B[i] != empty do (* New phase *)
      while (this.B[i] && this.B[i].size > 0) {
        
        // Req := findRequests(B[i], light) (* Create requests for light edges *)
        const Req = this.findRequests(this.B[i], 'light');
        
        // R := R U B[i] (* Remember deleted nodes *)
        for (const node of this.B[i]) {
          R.add(node);
        }
        
        // B[i] := empty (* Current bucket empty *)
        this.B[i].clear();

        // relaxRequests(Req) (* Do relaxations, nodes may (re)enter B[i] *)
        this.relaxRequests(Req);
      }

      // Req := findRequests(R, heavy) (* Create requests for heavy edges *)
      const ReqHeavy = this.findRequests(R, 'heavy');

      // relaxRequests(Req) (* Relaxations will not refill B[i] *)
      this.relaxRequests(ReqHeavy);
    }

    return this.tent;
  }

  // --- Helper Functions mapped from Pseudocode ---

  /**
   * Function findRequests(V', kind : {light, heavy}) : set of Request
   * return {(w, tent(v) + c(v, w)) : v in V' ∧ (v, w) in E_kind}
   */
  findRequests(vSet, kind) {
    const requests = [];
    for (const v of vSet) {
      const neighbors = this.graph[v] || [];
      for (const edge of neighbors) {
        const isLight = edge.weight <= this.delta;
        
        // Match edge kind
        if ((kind === 'light' && isLight) || (kind === 'heavy' && !isLight)) {
          const newWeight = this.tent.get(v) + edge.weight;
          requests.push({ w: edge.to, x: newWeight });
        }
      }
    }
    return requests;
  }

  /**
   * Procedure relaxRequests(Req)
   */
  relaxRequests(reqs) {
    for (const req of reqs) {
      this.relax(req.w, req.x);
    }
  }

  /**
   * Procedure relax(w, x) (* Insert or move w in B if x < tent(w) *)
   */
  relax(w, x) {
    const currentTent = this.tent.get(w);

    // if x < tent(w) then
    if (x < currentTent) {
      // If already in a bucket, remove from old bucket: B[tent(w)/Δ] := B[tent(w)/Δ] \ {w}
      if (currentTent !== Infinity) {
        const oldBucketIndex = Math.floor(currentTent / this.delta);
        if (this.B[oldBucketIndex]) {
          this.B[oldBucketIndex].delete(w);
        }
      }

      // Insert into new bucket: B[x/Δ] := B[x/Δ] U {w}
      const newBucketIndex = Math.floor(x / this.delta);
      if (!this.B[newBucketIndex]) {
        this.B[newBucketIndex] = new Set();
      }
      this.B[newBucketIndex].add(w);

      // tent(w) := x
      this.tent.set(w, x);
    }
  }

  isEmptyB() {
    return this.B.every(bucket => !bucket || bucket.size === 0);
  }

  minNonEmptyBucket() {
    for (let j = 0; j < this.B.length; j++) {
      if (this.B[j] && this.B[j].size > 0) {
        return j;
      }
    }
    return Infinity;
  }
}




// Graph representation: Adjacency list where each node points to an array of edges { to, weight }
const graph = {
  'A': [
    { to: 'B', weight: 2 }, // Light edge (if Delta >= 2)
    { to: 'C', weight: 6 }  // Heavy edge (if Delta < 6)
  ],
  'B': [
    { to: 'C', weight: 1 },
    { to: 'D', weight: 5 }
  ],
  'C': [
    { to: 'D', weight: 2 }
  ],
  'D': []
};

// Set Delta = 3
// Edges <= 3 are 'light', edges > 3 are 'heavy'
const delta = 3;
const source = 'A';

const solver = new DeltaStepping(graph, source, delta);
const shortestPaths = solver.run();

console.log("Shortest distances from source:");
for (const [node, dist] of shortestPaths.entries()) {
  console.log(`Node ${node}: ${dist}`);
}