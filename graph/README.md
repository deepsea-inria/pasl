Introducing the PASL graph kit             {#mainpage}
==============================

Graph-file formats  
==================

The graph-file formats include an adjacency-list format format and an
edge-list. All formats can be stored alternatively in an ascii or a
binary file. If an ascii file, the entries are delimited by any
consecutive sequence of delimiter characters: tab, space, line feed
(ascii 0x0A), and carriage return (ascii 0x0D). Files can start and
end with delimiters, which are ignored.

The edge-list and adjacency-list graph formats each start with five
fields that describe the general properties of the file:

- either the string `adjlist_` or `edglist_` to denote the type of graph described by the file
- `nb_vertices` refers to the number of vertices
- `nb_edges` refers to the number of edges
- `nb_bits_per_vertex_id` refers to the number of bits taken by a vertex id
- `is_symmetric` determines whether to generate a symmetric graph

Each of these fields are represented 1by 64 bit unsigned integers. The
subsequent fields each have the size `nb_bits_per_vertex_id` bytes. If
`is_symmetric` is nonzero, then every symmetric edge will be
generated.

Our graph formats allow duplicated edges and loops.

Every vertex id in the graph is represented by an unsigned integer
that is between zero and `nb_vertices-1`.

The total number of edges that are described by the file contents
should be equal to `nb_edges`. For example, if `is_symmetric` is
nonzero, then `nb_edges` may be larger than the number of edges in the
graph described by the file contents.

Adjacency list
--------------

The adjacency-graph format consists of a header, followed by a
sequence of offsets followed by a sequence of directed edges. The
sequence of offsets, namely `offsets`, contains `nb_vertices + 1` many
offset values: one per vertex, plus one extra value for the purpose of
bookkeeping. The offset `offsets[i]` for a vertex `v`<SUB>i</SUB>
refers to the location of the start of a contiguous block of out edges
for vertex `v`<SUB>i</SUB> in the sequence of edges, namely
`edges`. The offset `offsets[0]` of the first vertex `v`<SUB>0</SUB>
equals zero, and for any vertex `v`<SUB>i</SUB> such that `i > 0`, the
offset `offsets[i]` equals `offsets[i-1] + degree[i-1]`. The sequence
of offsets is terminated by the value `offsets[nb_vertices] =
offsets[nb_vertices-1] + degree[nb_vertices-1]`. Each block of out
edges continues until the offset of the next vertex, or the end, if
`v`<SUB>i</SUB> is the last vertex. If in ascii, all vertices and
offsets are 0 based and represented in decimal. The specific format is
as follows:

    adjlist_
    <nb_bits_per_vertex_id>
    <nb_vertices>
    <nb_edges>
    <is_symmetric>
    <offsets[0]> <offsets[1]> ... <offsets[nb_vertices-1]> <offsets[nb_vertices]>
    <edges[0][0]>              <edges[0][1]> <edges[0][2]> ... <edges[0][degree[0]-1]>
    <edges[1][0]>              <edges[1][1]> <edges[1][2]> ... <edges[1][degree[1]-1]>
    ...
    <edges[nb_vertices-1][0]>              ...                 <edges[nb_vertices-1][degree[nb_vertices-1]-1]>


Edge list {#edj_bin}
---------

The edge-graph format consists of a header followed by a sequence of
edges each being a pair of integers. Vertices are assumed to start at
0. The format is as follows:

    edglist_
    <nb_bits_per_vertex_id>
    <nb_vertices>
    <nb_edges>
    <is_symmetric>
    <edge[0].source> <edge[0].target>
    <edge[1].source> <edge[1].target>
    ...
    <edge[nb_edges-1].source> <edge[nb_edges-1].target>

Graph representations
=====================


Adjacency list {#adj_bin}
--------------

###Sequence container###

Type                | Description
--------------------|---------------------
`self_type`         | type of this container
`value_type`        | type of the contents of the container
`size_type`         | alias for `size_t`


Method                               | Description
-------------------------------------|-----------------------------------------------------------
`swap(self_type& other)`             | exchanges the contents of this bag with those of the `other` bag
`size() const`                       | returns the number of items in the bag
`operator[](size_type i) const`      | returns the item at position `i` in the bag
`alloc(size_type n)`                 | allocates space for `n` items
`data()`                             | returns a direct pointer to the memory array used internally by the sequence container to store its owned items


###Vertex###

Type                | Description
--------------------|---------------------
`vtxid_type`        | type of a vertex id
`vtxid_bag_type`    | type of a bag of vertex ids

Method                               | Description
-------------------------------------|-----------------------------------------------------------
`get_in_neighbor(vtxid_type j)`              | returns the id of the `j` th vertex in the list of incoming edges
`get_out_neighbor(vtxid_type j)`             | returns the id of the `j` th vertex in the list of outgoing edges
`get_out_neighbors()`                        | returns a direct pointer to the memory array used internally by the vertex to store its owned out neighbors
`get_in_neighbors()`                         | returns a direct pointer to the memory array used internally by the vertex to store its owned in neighbors
`get_in_degree()`                            | returns the number of incoming edges
`get_out_degree()`                           | returns the number of outgoing edges
`set_in_degree(size_type j)`                 | sets the in degree, resizing the vertex container if necessary; fails if the container is nonempty
`set_out_degree(size_type j)`                | sets the in degree, resizing the vertex container if necessary; fails if the container is nonempty
`swap_in_neighbors(vtxid_bag_type& other)`   | exchanges the contents of the bag of in neighbors by the contents of `other`
`swap_out_neighbors(vtxid_bag_type& other)`  | exchanges the contents of the bag of out neighbors by the contents of `other`


####Symmetric vertex####

Member              | Description
--------------------|---------------------
`neighbors`         | a bag of vertex ids, the contents of which denote the multiset of neighbors of the vertex


####Asymmetric vertex####

Member              | Description
--------------------|---------------------
`in_neighbors`      | a bag of vertex ids, the contents of which denote the multiset of in edges of the vertex
`out_neighbors`     | a bag of vertex ids, the contents of which denote the multiset of out edges of the vertex

####Adjacency-list graph####

Type                | Description
--------------------|---------------------
`vtxid_type`        | type of a vertex id
`vertex_type`       | type of a vertex
`adjlist_seq_type`  | type of a sequence of adjacency lists

Member              | Description
--------------------|---------------------
`nb_edges`          | number of edges in the graph
`adjlists`          | a sequence of length `nb_edges` of adjacency lists; each position in the sequence corresponds to a vertex id
`get_nb_vertices()` | returns the number of vertices in the graph


Edge list
---------

###Edge###

Type                | Description
--------------------|---------------------
`vtxid_type`        | type of a vertex id

Member                               | Description
-------------------------------------|-----------------------------------------------------------
`src`                                | vertex id of source vertex
`dst`                                | vertex id of destination vertex

###Edge-list graph###

Type                | Description
--------------------|---------------------
`vtxid_type`        | type of a vertex id
`edge_bag_type`     | type of a bag of edges

Member                               | Description
-------------------------------------|-----------------------------------------------------------
`nb_vertices`                        | number of vertices in the graph
`edges`                              | a bag of edges; the bag is of size `nb_edges`
`get_nb_edges()`                     | returns the number of edges in the graph	

`graphfile`: graph-file creator
==================

The PASL graph-file creator is an application whose purpose is to
create graphs in a format that can be processed by the PASL graph
library. The application offers two modes: graph generation and
graph-file conversion.

Graph generation
----------------

Generator                                     | Description
----------------------------------------------|---------------------
`complete`                                    | complete graph of given number of vertices _n_
`phased`                                      | todo
`parallel_paths`                              | todo
`rmat`                                        | power-law graph the resemble social networks
`square_grid`                                 | 2d _d_ x _d_ grid graph
`cube_grid`                                   | 3d _d_ x _d_ x _d_ grid graph
`chain`                                       | singly linked list of given length _n_
`star`                                        | a tree consisting of a single non-leaf node and an adjustable number _c_ of child nodes
`tree`                                        | a perfect _n_-ary tree of a given height _h_

Graph-file conversion
---------------------

File format                                    | Description
-----------------------------------------------|---------------------
`.snap`                                        | [Stanford snap format](http://snap.stanford.edu/)
`.dot`                                         | [Graphviz format](http://www.graphviz.org)
`.mmarket`                                     | [Matrix-market format](http://math.nist.gov/MatrixMarket/formats.html); currently, the only accepted mode is `matrix coordinate real symmetric`
`.adj_bin`                                     | [Adjacency-list binary format](#adj_bin)
`.edg_bin`                                     | [Edge-list binary format](#edg_bin)

Command-line interface
-----------------------

Argument                                      | Description
----------------------------------------------|---------------------
`-bits` _nb_                                  | determines the integer width to be used by the converter to represent vertex ids; _nb_ can be either 32 or 64
`-generator` _gen_                            | sets the mode to graph generator and selects _gen_ from one of the [graph generators](#generators) to generate the input graph
`-infile` _filename_                          | sets the mode to graph conversion and selects the input file _filename_ as the file from which to load the input graph; the format of the input graph is based on the file extension  (legal file extensions are defined in the list of PASL genrators above)
`-nb_edges_target` _nb_                       | sets a number of edges _nb_ to try to generate (relevant only in graph-generator mode)
`-outfile` _filename_                         | sets the file name _filename_ to which the contents of the converted graph are to be written; the format of the output graph is determined by the file extension (legal file extensions are defined in the supported file formats above)

### Examples ###

Generate a perfect binary tree with approximately one million edges
and store the results in PASL adjacency-list binary format:

    graphfile.exe -generator tree -outfile tree.adj_bin -nb_edges_target 1000000

Convert the twitter graph in snap format to the PASL adjacency-list
binary format:

    graphfile.exe -infile twitter.snap -outfile twitter.adj_bin

The contents of the `twitter.snap` file, in this case, may look like:

    # Undirected graph
    # Twitter
    # Nodes: 65608366 Edges: 1806067135
    # FromNodeId    ToNodeId
    101     102
    101     104
    101     107
    ...