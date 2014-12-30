open XBase
open Num

let rec fib_rec n =
  if n < 2 then 1 else fib_rec (n - 1) + fib_rec (n - 2)

(* This program calculates the nth fibonacci number
 * using algorithm 1B: cached binary recursion
 *
 * compiled: ocamlopt -ccopt -march=native -o f1b nums.cmxa f1b.ml
 * executed: ./f1b n
 *)



(* same double recursion as in algorith 1A, but reads from cache
 * if there is a value stored already
 *)
let fib_cache n =
  let an = abs n in
  let cache = Array.make (an+1) (Int 0) in
  let rec f = function
          | 0 -> Int 0
          | 1 -> Int 1
          | n -> if cache.(n) =/ Int 0
                 then cache.(n) <- f (n-1) +/ f (n-2);
                 cache.(n) in
  (if n<0 && -n mod 2=0 then minus_num else fun n -> n)
  (f an)

let _ =
  let n = XCmd.parse_or_default_int "n" 39 in
  let algo = XCmd.parse_or_default_string "algo" "recursive" in
  let start = Sys.time() in
  let result  = if algo = "recursive" then
                  fib_rec n
                else if algo = "cached" then
                  let Int n = fib_cache n in n
                else
                  invalid_arg "algo"
  in
  let elapsed = Sys.time() -. start in
  printf "result     %d\n" result;
  printf "exectime   %f\n" elapsed;
  ()
