use Itertools;

//follower
var A: [1..10] int;
forall (id, el) in zip(1..10, cycle((1, 2, 3, 4))) with (ref A) do A[id] = el;
writeln(A);
forall (id, el) in zip(1..10, cycle('ABC')) with (ref A) do A[id] = el.toByte();
writeln(A);

var B: [2..11] int;
forall (id, el) in zip(2..11, cycle([1, 2, 3])) with (ref B) do B[id] = el;
writeln(B);

//leader
var C: [1..20] atomic int;
C.write(0);
forall (el, id) in zip(cycle((1,2,3,4,5), 4), 1..20) with (ref C) do C[id].add(el);
writeln(C);

C.write(0);
forall (el, id) in zip(cycle('ABCDE', 4), 1..20) with (ref C) do C[id].add(el.toByte());
writeln(C);

var D: [2..21] int;
forall (el, id) in zip(cycle([1,2,3,4,5], 4), 2..21) with (ref D) do D[id]=el;
writeln(D);
