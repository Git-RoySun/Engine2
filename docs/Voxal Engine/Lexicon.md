## Path-Tracing

> [!info] Path-Tracing
> Essentially, given an initial point $p$ and a direction vector $\vec v$, we start from $p$ and advance a distance $h$ in direction $v$ until we hit anything. The choice of $h$ is very important since if it is too low the algorithm is too computationally expensive and if it is too high it increases the likelihood of skipping collisions. To find the optimal choice for $h$ at every iteration, we need to know the maximum distance we can travel without hitting anything. A common technique of ensuring this is by using [[Signed Distance Fields]].

^41e157

