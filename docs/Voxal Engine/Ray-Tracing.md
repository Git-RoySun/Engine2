[[Ray-Tracing]] is a technique in 3D computer graphics that unlike traditional rasterization techniques, simulates a physic-based light rays in order to render images instead of rendering tons of tiny triangles/polygons. In [[Voxal Engine]], this is done through [[#Octree]]-accelerated path-tracing.
# Octree
If a binary tree is a tree with 2 children, then an octree is a tree with 8 children. This analogy becomes relevant when we realize that a quadtree (tree with 4 children) can represent 2D space and octrees can represent 3D space (and a binary tree can represent a 1D space).
![[Pasted image 20240413190203.png]]
A simple implementation of this data type can be
```c
struct octree{
	Octree children[8];
	Data data;
}
```

## Why Octrees?
There are 3 main benefits to using [[#Octree]]
- ### Memory:
	Octrees allows us to store voxel data without needing to store the position of every single voxel. Since positions in 3D space are 3-float component vectors, with each float taking 32 bits we are saving 96 bits per voxel!
	
	Besides position, the other information that we need to store for each voxel is the material which take usually takes 32 bits (size of a pointer). In [[Voxal Engine]], we are using 16-bit indexing for materials (we are assuming 65536 different material is enough) thus by using octrees we reduce over 85% of the original memory cost.
	[[Voxal Engine]] aims to use a maximum of 4gb of dedicated GPU memory.
	
- ### Level of Detail:
	To decrease the [[#Level of detail]] in a voxel world, we can simply increase the size of the voxel or even better, we can combine multiple voxels into a single bigger voxel. These group sizes should be a power of 8 since we are in a 3D world.
	
	In [[Voxal Engine]], every voxel is a leaf (an octree with no children) and all octrees are already grouped with their siblings in groups of 8 (thus the parent could be turned into an 8x bigger voxel). Thus the process of simplifying details is already baked into the data structure.
	
- ### Path-Tracing
	A common way of doing [[Ray-Tracing]] is to use an algorithm called [[Lexicon#^41e157|Path-Tracing]]. By using octrees, we can get the maximum distance we can travel without hitting anything efficiently: if the $p$ starts in an empty octree/leaf, we can easily find the highest parent that is still empty $t$. Thus we are guaranteed to be able to travel to the borders of $t$ without hitting anything. This helps us speed up path-tracing significantly.

##  Why not Octrees?
There exists other ways of organizing voxel data besides octrees; 2 common ones are explained in [this article](https://eisenwave.github.io/voxel-compression-docs/svo/svo.html).
- ### Voxel Array
- ### Voxel List

# Memory layout/ Serialization
Because GPU memory is not infinite (we are targeting 4Gb) we can only bind a portion of the infinitively-generated world at a time. We separate the world into chunks of 8x8x8 blocks each consisting of 16x16x16 atoms. Blocks would be the equivalent of a $1 m^3$ cube while atoms are the smallest possible voxel (1/4096 of a block).

In order to maximize render distance, we use a level of details system to make voxels that are far away appear less detailed which helps us save memory. We are targeting a render distance of 32x32x32 chunks around the camera. The resolution (or level of detail) varies by chunk (and the distance between the chunk and camera) with the highest being $128^3$ voxels while the lowest is $2^3$ voxels. [[Insert image here]]

| Distance From Center (chunks) | Width (Voxel/Chunk) | Depth |
| ----------------------------- | ------------------- | ----- |
| 16                            | 2                   | 6     |
| 14                            | 4                   | 7     |
| 12                            | 8                   | 8     |
| 10                            | 16                  | 9     |
| 8                             | 32                  | 10    |
| 6                             | 64                  | 11    |
| 4                             | 128                 | 12    |

Translated into octrees, the 32x32x32 chunk space would be the root octree with depth 0, chunks would be octrees with depth 5, blocks would be depth 8 and atoms depth 12.

| Octree Depth | Width (Chunks) | Width (Blocks) | Width (Atoms) |
| ------------ | -------------- | -------------- | ------------- |
| 0            | 32             | 256            | 4096          |
| 1            | 16             | 128            | 2048          |
| 2            | 8              | 64             | 1024          |
| 3            | 4              | 32             | 512           |
| 4            | 2              | 16             | 256           |
| 5            | 1              | 8              | 128           |
| 6            | 1/2            | 4              | 64            |
| 7            | 1/4            | 2              | 32            |
| 8            | 1/8            | 1              | 16            |
| 9            | 1/16           | 1/2            | 8             |
| 10           | 1/32           | 1/4            | 4             |
| 11           | 1/64           | 1/8            | 2             |
| 12           | 1/128          | 1/16           | 1             |

Data stored in the GPU typically cannot have pointers (unless you are using CUDA), thus the octree needs to be serialized into arrays/buffers. Serialization into SIMD arrays should be more efficient than using pointers anyways. Typically, to serialize a binary tree into a array we simply put the root at index 0 and any children at $n*2+1$ or $n*2+2$ where $n$ is index of the parent. 

This methods works but has a major drawback: empty values can still take space. This is a major problem because our implementation of the level of details systems requires sibling octrees to not have the same depth thus causing large gaps of unused memory. Therefore some modification is needed in order to fit within the 4Gb constraint.
## Buffers for each level of detail
The first modification is to group octrees that have the same level of details together, thus we would have a different buffer for each different level of detail (7 different ones in this case). The octrees within these buffers have the same depth thus the indexing rule of $n*8+i, 1\leq i\leq8$ works and occupies much less space.

There are also a few other things that we can take advantage of:
1. We know the size of each buffer ahead of time: we know the amount of memory that each chunk would take (because it depends on the level of detail) as well as the number of chunks in each buffer.
2. We can store octrees one level higher than chunks (octrees of depth 4 that i will call "chunk groups") because the level of detail changes every 2 chunks from the center. This is useful as there are only $16^3 = 4096$ octrees of depth 4.

### Indexing
The trickiest part of this implementation actually comes from determining the index of the chunks within buffers. To understand why, first notice that each buffer actually represents a layer of the root cube (like the sides of an empty box) and the indexing is like tracing a line that goes through every chunk in the layer without lifting the pen.

It would be possible to find some kind of formula that describes the indices that each chunk should go into (like a hash function), however it is quite complicated and not as flexible as simply using a mapping table:

Whenever we get to an octree of depth 4, we put it into a map with its position as key and data that describes which buffer and what index it is stored at (we simply store it at the end of the buffer thus we are just recording the number of items already in the buffer). In [[Voxal Engine]] the data is simply a 32-bit integer where the first 3 bit represent the buffer and the remaining bits represent the index.

| Detail Level (voxel/chunk) |     |
| -------------------------- | --- |
| 128                        |     |
| 64                         |     |
| 32                         |     |
| 16                         |     |
| 8                          |     |
| 4                          |     |
| 2                          |     |



