use BlockDist;

const bbox: domain(1);
const D = {1..10} dmapped new blockDist(boundingBox=bbox);
var A: [D] real;

forall a in A do
  a = here.id;

writeln(A);

