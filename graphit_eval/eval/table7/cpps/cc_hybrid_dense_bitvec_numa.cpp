#include <iostream> 
#include <vector>
#include <algorithm>
#include "intrinsics.h"
Graph edges; 
int  * __restrict IDs;
int  **local_IDs;
template <typename APPLY_FUNC , typename PUSH_APPLY_FUNC> VertexSubset<NodeID>* edgeset_apply_hybrid_dense_parallel_deduplicatied_from_vertexset_with_frontier_pull_frontier_bitvector(Graph & g , VertexSubset<NodeID>* from_vertexset, APPLY_FUNC apply_func, PUSH_APPLY_FUNC push_apply_func) 
{ 
    int64_t numVertices = g.num_nodes(), numEdges = g.num_edges();
    from_vertexset->toSparse();
    long m = from_vertexset->size();
    // used to generate nonzero indices to get degrees
    uintT *degrees = newA(uintT, m);
    // We probably need this when we get something that doesn't have a dense set, not sure
    // We can also write our own, the eixsting one doesn't quite work for bitvectors
    //from_vertexset->toSparse();
    {
        parallel_for (long i = 0; i < m; i++) {
            NodeID v = from_vertexset->dense_vertex_set_[i];
            degrees[i] = g.out_degree(v);
        }
    }
    uintT outDegrees = sequence::plusReduce(degrees, m);
    if (m + outDegrees > numEdges / 20) {
  VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), 0);
  bool * next = newA(bool, g.num_nodes());
  parallel_for (int i = 0; i < numVertices; i++)next[i] = 0;
    from_vertexset->toDense();
  Bitmap bitmap(numVertices);
  bitmap.reset();
  parallel_for(int i = 0; i < numVertices; i+=32){
     int start = i;
     int end = (((i + 32) < numVertices)? (i+32):numVertices);
     for(int j = start; j < end; j++){
        if (from_vertexset->bool_map_[j])
          bitmap.set_bit(j);
     }
  }
  //   parallel_for (int n = 0; n < numVertices; n++) {
  //   for (int socketId = 0; socketId < omp_get_num_places(); socketId++) {
  //     local_IDs[socketId][n] = IDs[n];
  //   }
  // }
  int numPlaces = omp_get_num_places();
    int numSegments = g.getNumSegments("s1");
    int segmentsPerSocket = (numSegments + numPlaces - 1) / numPlaces;
#pragma omp parallel num_threads(numPlaces) proc_bind(spread)
{
    int socketId = omp_get_place_num();
    for (int i = 0; i < segmentsPerSocket; i++) {
      int segmentId = socketId + i * numPlaces;
      if (segmentId >= numSegments) break;
      auto sg = g.getSegmentedGraph(std::string("s1"), segmentId);
#pragma omp parallel num_threads(omp_get_place_num_procs(socketId)) proc_bind(close)
{
#pragma omp for schedule(dynamic, 1024)
for ( NodeID localId=0; localId < sg->numVertices; localId++) {
      NodeID d = sg->graphId[localId];
      for (int64_t ngh = sg->vertexArray[localId]; ngh < sg->vertexArray[localId+1]; ngh++) {
        NodeID s = sg->edgeArray[ngh];
        if (bitmap.get_bit(s)) { 
          if( apply_func ( s , d , socketId) ) { 
            next[d] = 1; 
          }
        }
      } //end of loop on in neighbors
    } //end of outer for loop
} // end of per-socket parallel_for
    } // end of segment for loop
}// end of per-socket parallel region

  // parallel_for (int n = 0; n < numVertices; n++) {
  //   for (int socketId = 0; socketId < omp_get_num_places(); socketId++) {
  //     IDs[n] = min(IDs[n], local_IDs[socketId][n]);
  //   }
  // }
  next_frontier->num_vertices_ = sequence::sum(next, numVertices);
  next_frontier->bool_map_ = next;
  return next_frontier;
} else {
    if (g.flags_ == nullptr){
      g.flags_ = new int[numVertices]();
      parallel_for(int i = 0; i < numVertices; i++) g.flags_[i]=0;
    }
    VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), 0);
    if (numVertices != from_vertexset->getVerticesRange()) {
        cout << "edgeMap: Sizes Don't match" << endl;
        abort();
    }
    if (outDegrees == 0) return next_frontier;
    uintT *offsets = degrees;
    long outEdgeCount = sequence::plusScan(offsets, degrees, m);
    uintE *outEdges = newA(uintE, outEdgeCount);
      parallel_for (long i=0; i < m; i++) {
    NodeID s = from_vertexset->dense_vertex_set_[i];
    int j = 0;
    uintT offset = offsets[i];
        for(NodeID d : g.out_neigh(s)){
          if( push_apply_func ( s , d  ) && CAS(&(g.flags_[d]), 0, 1)  ) { 
            outEdges[offset + j] = d; 
          } else { outEdges[offset + j] = UINT_E_MAX; }
          j++;
        } //end of for loop on neighbors
      }
  uintE *nextIndices = newA(uintE, outEdgeCount);
  long nextM = sequence::filter(outEdges, nextIndices, outEdgeCount, nonMaxF());
  free(outEdges);
  free(degrees);
  next_frontier->num_vertices_ = nextM;
  next_frontier->dense_vertex_set_ = nextIndices;
  parallel_for(int i = 0; i < nextM; i++){
     g.flags_[nextIndices[i]] = 0;
  }
  return next_frontier;
 } //end of else
} //end of edgeset apply function 
struct updateEdge_push_ver
{
  bool operator() (NodeID src, NodeID dst) 
  {
    bool output4 ;
    bool IDs_trackving_var_3 = (bool) 0;
    IDs_trackving_var_3 = writeMin( &IDs[dst], IDs[src] ); 
    output4 = IDs_trackving_var_3;
    return output4;
  };
};
struct IDs_generated_vector_op_apply_func_0
{
  void operator() (NodeID v) 
  {
    IDs[v] = (1) ;
  };
};
struct updateEdge
{
  bool operator() (NodeID src, NodeID dst, int socketId) 
  {
    // bool output2 ;
    // bool IDs_trackving_var_1 = (bool) 0;
    // if ( ( local_IDs[socketId][dst]) > ( IDs[src]) ) { 
    //   local_IDs[socketId][dst]= IDs[src]; 
    //   IDs_trackving_var_1 = true ; 
    // } 
    // output2 = IDs_trackving_var_1;
    // return output2;
    bool output2 ;
    bool IDs_trackving_var_1 = (bool) 0;
    if ( ( IDs[dst]) > ( IDs[src]) ) {
      IDs[dst]= IDs[src];
      IDs_trackving_var_1 = true ;
    }
    output2 = IDs_trackving_var_1;
    return output2;
  };
};
struct init
{
  void operator() (NodeID v) 
  {
    IDs[v] = v;
  };
};
int main(int argc, char * argv[])
{
  edges = builtin_loadEdgesFromFile ( argv[(1) ]) ;
  edges.buildPullSegmentedGraphs("s1", 30, true);
  IDs = new int [ builtin_getVertices(edges) ];
  parallel_for (int vertexsetapply_iter = 0; vertexsetapply_iter < builtin_getVertices(edges) ; vertexsetapply_iter++) {
    IDs_generated_vector_op_apply_func_0()(vertexsetapply_iter);
  };
  local_IDs = new int *[omp_get_num_places()];
  for (int socketId = 0; socketId < omp_get_num_places(); socketId++) {
    local_IDs[socketId] = (int *)numa_alloc_onnode(sizeof(int ) * builtin_getVertices(edges) , socketId);
    parallel_for (int n = 0; n < builtin_getVertices(edges) ; n++) {
      local_IDs[socketId][n] = IDs[n];
    }
  }
  omp_set_nested(1);
  int n = builtin_getVertices(edges) ;
  for ( int trail = (0) ; trail < (10) ; trail++ )
  {
    startTimer() ;
    VertexSubset<int> *  frontier = new VertexSubset<int> ( builtin_getVertices(edges)  , n);
    parallel_for (int vertexsetapply_iter = 0; vertexsetapply_iter < builtin_getVertices(edges) ; vertexsetapply_iter++) {
      init()(vertexsetapply_iter);
    };
    while ( (builtin_getVertexSetSize(frontier) ) != ((0) ))
    {
      frontier = edgeset_apply_hybrid_dense_parallel_deduplicatied_from_vertexset_with_frontier_pull_frontier_bitvector(edges, frontier, updateEdge(), updateEdge_push_ver()); 
    }
    float elapsed_time = stopTimer() ;
    std::cout << "elapsed time: "<< std::endl;
    std::cout << elapsed_time<< std::endl;
  }
  for (int socketId = 0; socketId < omp_get_num_places(); socketId++) {
    numa_free(local_IDs[socketId], sizeof(int ) * builtin_getVertices(edges) );
  }
};

