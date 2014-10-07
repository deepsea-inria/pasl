open Printf

(** DEPRECATED FILE, DO NOT EDIT *)



(***************************************************************)
(***************************************************************)
(***************************************************************)

(***************************************************************)
(** auxiliary functions *)

let rec int_seq a b incr =
  if a > b then [] else a :: (int_seq (a+incr) b incr)

let int_range a b =
  int_seq a b 1

let rec pow10 n =
   if n = 0 then 1 else 10 * pow10 (n-1)

let rec pow10_list n m =
   if n > m then [] else pow10 n :: pow10_list (n+1) m

let rec pow10_list_upto n =
   pow10_list 0 n

let rec pow2 n =
   if n = 0 then 1 else 2 * pow2 (n-1)

let rec pow2_list n m =
   if n > m then [] else pow2 n :: pow2_list (n+1) m

let rec pow2_list_upto n =
   pow2_list 0 n

let list_median l =
   List.nth l (List.length l / 2)

let list_max l =
   match l with 
   | [] -> assert false
   | x::ls -> List.fold_left max x ls

let list_min l =
   match l with 
   | [] -> assert false
   | x::ls -> List.fold_left min x ls

let list_foreach l f =
   List.iter f l

let list_iter = list_foreach

let list_last l =
  List.nth l (List.length l - 1)

let list_ksort cmp l =
   let cmp_fst (x,_) (y,_) = 
      cmp x y in
   List.sort cmp_fst l 


let build_list item_creator =
   let r = ref [] in
   let add x = (r := x::!r) in
   item_creator add; 
   List.rev !r

let list_prod l1 l2 =
  build_list (fun add ->
    list_foreach l1 (fun x1 ->
    list_foreach l2 (fun x2 ->
    add (x1,x2))))

let list_prod3 l1 l2 l3 =
  build_list (fun add ->
    list_foreach l1 (fun x1 ->
    list_foreach l2 (fun x2 ->
    list_foreach l3 (fun x3 ->
    add (x1,x2,x3)))))

let list_prod4 l1 l2 l3 l4 =
  build_list (fun add ->
    list_foreach l1 (fun x1 ->
    list_foreach l2 (fun x2 ->
    list_foreach l3 (fun x3 ->
    list_foreach l4 (fun x4 ->
    add (x1,x2,x3,x4))))))

let repeat n f =
   for i = 0 to pred n do f i done


let string_ends_with s str =
   let n = String.length s in
   let m = String.length str in
   if m < n 
      then false 
      else (s = String.sub str (m - n) n)

let string_starts_with s str =
   let n = String.length s in
   let m = String.length str in
   if m < n 
      then false 
      else (s = String.sub str 0 n)


let add_to_list_ref r x =
  r := x :: !r

let unix_command s =
   ignore (Unix.system s)

let file_put_contents filename str =
  let channel = open_out filename in
  output_string channel str;
  close_out channel    

let file_put_lines filename sep lines =
   file_put_contents filename (String.concat sep (lines @ [""]))

exception FileNotFound of string

let file_get_lines file = 
   if not (Sys.file_exists file)
      then raise (FileNotFound file);
   let lines = ref [] in
   let f = 
      try open_in file with End_of_file -> raise (FileNotFound file);
      in
   begin try while true do
      lines := input_line f :: !lines 
   done with End_of_file -> () end;
   close_in f;
   List.rev !lines

let file_get_contents file =
   let lines = file_get_lines file in
   (String.concat "\n" lines) ^ "\n"

let file_append_contents filename str = 
  let contents = file_get_contents filename in 
  file_put_contents filename (contents^str)

let file_get_contents_or_empty file =
   try file_get_contents file
   with FileNotFound _ -> ""

let string_split char_sep str =
   let n = String.length str in
   let words = ref [] in 
   let i = ref 0 in
   begin try while true do 
      let j = String.index_from str !i char_sep in
      let k = j - !i in
      if k > 0
        then (let word = String.sub str !i k in
              words := word :: !words);
      i := j+1;
   done with Not_found -> 
      if n - !i > 0 then
        (let rest = String.sub str !i (n - !i) in
         words := rest :: !words)
   end;
   List.rev !words

let rec take n xs = 
   match n, xs with 
        0, xs -> [] 
      | n, [] -> failwith "take" 
      | n, x::xs -> x :: take (n-1) xs

let rec drop n xs = 
   match n, xs with 
        0, xs -> xs
      | n, [] -> failwith "drop" 
      | n, x::xs -> drop (n-1) xs

let rec list_take_drop n l =
  if n = 0 then ([], l) else
  match l with
  | [] -> failwith "invalid argument for list_take_drop"
  | x::l' -> let (h,t) = list_take_drop (n-1) l' in
             (x::h, t)

let rec list_pairing l =
  match l with
  | [] -> []
  | [x] -> failwith "invalid argument for list_pairing"
  | x::y::l' -> (x,y) :: (list_pairing l')

let list_mean l =
  let n = float_of_int (List.length l) in
  let s = List.fold_left (+.) 0. l in
  s /. n

let float_sq x =
  x *. x

let list_mean_and_stddev l =  
  let mean = list_mean l in
  let variance = list_mean (List.map (fun x -> float_sq (x -. mean)) l) in
  mean, sqrt variance

let string_cmp x y =
  if x < y then -1 else if x = y then 0 else 1

let list_foreach l f =
   List.iter f l

let list_for_all l f =
   List.for_all f l

let list_exists l f =
   List.exists f l

let list_iteri f l =
  let rec aux i = function
    | [] -> ()
    | h::t -> (f i h); (aux (i+1) t)
    in
  aux 0 l

let list_foreachi l f =
   list_iteri f l

let list_map l f =
   List.map f l

let list_mem l f =
   List.mem f l

let list_filter l f =
   List.filter f l

let list_remove x l1 =
   List.filter (fun y -> y <> x) l1

let list_substract l1 l2 =
   List.filter (fun x -> not (List.mem x l2)) l1

let list_inter l1 l2 =
   List.filter (fun x -> List.mem x l2) l1

let list_included l1 l2 =
   list_for_all l1 (fun x -> List.mem x l2)

let list_of_option = function
   | None -> []
   | Some x -> [x]

let list_of_list_option = function
   | None -> []
   | Some x -> x



let list_mapi f l =
  let rec aux i = function
    | [] -> []
    | h::t -> (f i h)::(aux (i+1) t)
    in
  aux 0 l

let list_filter_every n l =
  if n <= 0 then failwith "invalid arg for list_filter";
  let rec aux acc k = function
      | [] -> acc
      | x::l' ->
          if k = 0 then aux (x::acc) (n-1) l'
                   else aux acc (k-1) l'
     in
  List.rev (aux [] (n-1) l)


(* Same as (k, v)::(List.remove_assoc k l), but preserves order of bindings *)

let list_assoc_replace k v l =
   let rec aux = function
      | [] -> raise Not_found
      | (k',v')::l' -> if k = k' then (k,v)::l' else (k',v')::(aux l')
      in
   aux l


let list_assoc_replace_pair (k,v) l =
     try
   list_assoc_replace k v l
  with Not_found -> List.iter (fun (a,b) -> printf "%s\n" a) l; failwith  ("failed to find ")

let unsome = function
  | None -> failwith "unsome of none"
  | Some x -> x

let unsome_or default = function
  | None -> default
  | Some x -> x



let string_quote s =
   "\"" ^ s ^ "\""

let list_concat_map f l =
  List.concat (List.map f l)

let string_of_list sep string_of_item l =
   String.concat sep (List.map string_of_item l)


(***************************************************************)
(** imperative finite maps *)

module Imap :
sig
   type ('a,'b) t
   val from_list : ('a * 'b) list -> ('a, 'b) t
   val to_list : ('a, 'b) t -> ('a * 'b) list
   val create : unit -> ('a,'b) t
   val is_empty : ('a,'b) t -> bool
   val filter : ('a -> 'b -> bool) -> ('a,'b) t -> ('a,'b) t
   val mem : 'a -> ('a,'b) t -> bool
   val find : 'a -> ('a,'b) t -> 'b
   val find_or_create : (unit -> 'b) -> 'a -> ('a,'b) t -> 'b
   val iter : ('a -> 'b -> unit) -> ('a,'b) t -> unit
   val foreach : ('a,'b) t -> ('a -> 'b -> unit) -> unit
   val map : ('a * 'b -> 'c * 'd) -> ('a,'b) t -> ('c,'d) t
   val fold : ('c -> 'a -> 'b -> 'c) -> 'c -> ('a,'b) t -> 'c
   val ksort : ('a -> 'a -> int) -> ('a,'b) t -> unit
end
= 
struct

   type ('a,'b) t = (('a * 'b) list) ref 

   let from_list m =
      ref m

   let to_list m =
      !m

   let create () = 
      ref []

   let is_empty m =
     !m = []

   let filter f m =
     from_list (List.filter (fun (k,v) -> f k v) !m)

   let mem k m =
      List.mem_assoc k !m

   let find k m = 
      List.assoc k !m

   let find_or_create c k m =
      try find k m 
      with Not_found -> 
        let v = c() in
        m := (k,v)::!m; 
        v

   let iter f m =
       List.iter (fun (k,v) -> f k v) !m

   let foreach m f = 
      iter f m

   let map f m =
      from_list (List.map f !m)

   let fold f i m =
      List.fold_left (fun acc (k,v) -> f acc k v) i !m
   
   let ksort cmp m =
      m := List.sort (fun (k1,_) (k2,_) -> cmp k1 k2) !m
end



(***************************************************************)
(** * Command-line management *)

(** Extract list of strings from coma-separated values *)

let split_args s = 
  Str.split (Str.regexp_string ",") s

(** Same and store result in a reference *)

let string_args_to_ref r s =
  r := split_args s

(** Extract list of integers from coma-separated values *)

let split_int_args s = 
  List.map int_of_string (Str.split (Str.regexp_string ",") s)

(** Same and store result in a reference *)

let int_args_to_ref r s =
  r := split_int_args s

(***************************************************************)
(** * Dynamically-typed values *)

module Dyn = struct

type t =
  | Bool of bool
  | Int of int
  | Float of float
  | String of string
  | List of t list
  | Obj of Obj.t

exception TypeError of string * t

let type_name = function
  | Bool _ -> "bool"
  | Int _ -> "int"
  | Float _ -> "float"
  | String _ -> "string"
  | List _ -> "list"
  | Obj _ -> "obj"

let string_of_dyn = function
  | Bool b -> if b then "true" else "false"
  | Int n -> string_of_int n
  | Float f -> string_of_float f
  | String s -> s (* "\"" ^ s ^ "\"" *)
  | List l -> "[list]"
  | Obj o -> "[obj]"

let rec equal d1 d2 = 
  match d1,d2 with
  | Bool b1, Bool b2 -> b1 = b2
  | Int n1, Int n2 -> n1 = n2
  | Float f1, Float f2 -> f1 = f2
  | String s1, String s2 -> s1 = s2
  | List l1, List l2 ->
      begin try List.for_all2 equal l1 l2
      with Invalid_argument _ -> false end
  | Obj o1, Obj o2 -> o1 = o2 
  | _, _ -> false

let string_of_error s v =
  sprintf "dyn-error: expected type %s, but found value %s"
     s (string_of_dyn v)

let throw_type_error s v =
   failwith (string_of_error s v)

let get_bool = function
  | Bool b -> b
  | v -> throw_type_error "bool" v

let get_int = function
  | Int n -> n
  | v -> throw_type_error "int" v

let get_float = function
  | Float b -> b
  | v -> throw_type_error "float" v

let get_string = function
  | String s -> s
  | v -> throw_type_error "string" v

let get_list = function
  | List l -> l
  | v -> throw_type_error "list" v

let get_obj : 'a = function
  | Obj o -> o
  | v -> throw_type_error "obj" v

let get_as_int = function
  | Float b -> int_of_float b
  | Int n -> n
  | String s -> int_of_string s
  | v -> throw_type_error "get_as_float" v

let get_as_float = function
  | Float b -> b
  | Int n -> float_of_int n 
  | String s -> float_of_string s
  | v -> throw_type_error "get_as_float" v

let get_as_string = function
  | Int n -> string_of_int n
  | Float f -> string_of_float f
  | String s -> s
  | v -> throw_type_error "get_as_string" v

end 

type dyn = Dyn.t
type dyns = dyn list

(********)

let list_assoc_option k l =
   try Some (List.assoc k l)
   with Not_found -> None


let unsome_or_failwith msg = function
  | None -> failwith msg
  | Some x -> x


(****************************************************)
(****************************************************)
(****************************************************)

(** The first letter of the keyword indicates the type to be used for parsing the rest of the line:
    - i : for an int 
    - f : for a float 
    - s : for a string
    *)

exception CannotParse of string

type param = (string * dyn) list

let params_from_file filename =
   let lines = file_get_lines filename in
   build_list (fun add ->
      list_foreachi lines (fun id_line line ->
         try 
            if line = "" || line = "===" then begin
               ()
            end else begin
               let space_index = String.index line ' ' in
               let keyword = Str.string_before line space_index in
               let text = Str.string_after line (space_index+1) in
               let first_letter = keyword.[0] in
               let value = match first_letter with
                  | 'i' -> Dyn.Int (Scanf.sscanf text "%d" (fun x -> x))
                  | 'f' -> Dyn.Float (Scanf.sscanf text "%f" (fun x -> x))
                  | 's' -> Dyn.String text
                  | x -> failwith (sprintf "unrecognized keyword prefix: %c" x)
                  in
               add (keyword,value)
            end
          with _ -> raise (CannotParse (sprintf "line %d:\n%s\n" (id_line+1) line))
         );
      )

exception Param_not_found of string

let find_param key params =
   try List.assoc key params
   with Not_found -> raise (Param_not_found key)

let find_param_int key params =
   Dyn.get_int (find_param key params)

let find_param_float key params =
   Dyn.get_float (find_param key params)

let find_param_string key params =
   Dyn.get_string (find_param key params)

let find_param_as_float key params =
   Dyn.get_as_float (find_param key params)

let find_param_as_int key params =
   Dyn.get_as_int (find_param key params)






let rec int_log2 n =
   if n <= 1 then 0 else 1 + (int_log2 (n/2))

let float_log2 n =
   log (float_of_int n) /. log 2.


open Random 

(* get a random gaussian using a Box-Muller transform *)

let rec random_normalized_gaussian () = 
   (* Generate two uniform numbers from -1 to 1 *) 
   let x = Random.float 2.0 -. 1.0 in 
   let y = Random.float 2.0 -. 1.0 in 
   let s = x*.x +. y*.y in 
   if s > 1.0 then random_normalized_gaussian () 
   else x *. sqrt (-2.0 *. (log s) /. s) 

let random_gaussian mean stddev = 
   let x = random_normalized_gaussian() in
   mean +. stddev *. x


let list_sorted_insert cmp x l =
   let rec ins = function
      | [] -> [x]
      | y::q -> if cmp x y <= 0 then x::y::q else y::(ins q)
      in
   ins l

let cmp_fst (x1,y1) (x2,y2) = 
   x1 - x2

let cmp_snd (x1,y1) (x2,y2) = 
   y1 - y2

let cmp_fst_desc (x1,y1) (x2,y2) = 
   x2 - x1

let cmp_snd_desc (x1,y1) (x2,y2) = 
   y2 - y1

exception Break


let unsome_or_failwith msg = function
  | None -> failwith msg
  | Some x -> x

let array_iteri t f =
   Array.iteri f t

let array_iter t f =
   Array.iter f t

let array_map t f =
   Array.map f t

let array_mapi t f =
   Array.mapi f t

let option_map o r f =
   match o with
   | None -> r
   | Some x -> f x

let option_iter o f =
   match o with
   | None -> ()
   | Some x -> f x

let char_is_digit c =
     int_of_char '0' <= int_of_char c
  && int_of_char c <= int_of_char '9'


let repeait n f =
   for i = 0 to pred n do f i done

let repeat n f = (* todo: update code *)
   for i = 0 to pred n do f () done


let add_to_int_ref r x =
  r := x + !r
let add_to_float_ref r x =
  r := x +. !r

let create_int_counter () =
   let r = ref 0 in
   let get () = !r in
   let incr () = (r := !r +1) in
   (get,incr)

let create_int_sum i =
   let r = ref i in
   let get () = !r in
   let add x = (r := !r + x) in
   (get,add)

let while_true f =
   let on = ref true in
   while !on do f() done 


let list_init n f =
   let rec aux acc i =
      if i = n then acc else aux (f i :: acc) (i+1)
      in
   List.rev (aux [] 0)


let bool_of_int x =
   (x <> 0)

let nat_range n =
   int_range 0 (n-1)

let nat_foreach n f =
   for i = 0 to pred n do f i done

let list_pop_last l =
   let rec aux acc = function
      | [] -> failwith "empty list"
      | [x] -> (x, List.rev acc)
      | x::l' -> aux (x::acc) l'
      in
   aux [] l
   
let rec list_extract_nth n l = (* not tail rec *)
   match l with
   | [] -> failwith "list_extract invalid arg"
   | x::q -> 
      if n = 0 then (x,q) else
      let (y,q') = list_extract_nth (n-1) q in
      (y,x::q')   

let list_schuffle l = (* quadratic complexity, not tail rec *)
   let rec aux q =
      let n = List.length q in
      if n = 0 then [] else begin
         let k = Random.int n in
         let (x,q') = list_extract_nth k q in
         x :: (aux q')
      end
      in
   aux l



let list_take_head = function
   | [] -> raise Not_found
   | x::xs -> x, xs


let list_take_last l =
   let rec aux acc = function
      | [] -> raise Not_found
      | [x] -> x, (List.rev acc)
      | x::xs -> aux (x::acc) xs
   in
   aux [] l


let list_rev_notrec l =
   let res = ref [] in
   let cur = ref l in
   begin try while true do
      match !cur with
      | [] -> raise Break
      | x::xs -> 
         res := x::!res;
         cur := xs
   done with Break -> () end;
   !res

let list_filter_notrec p l =
  let rec find accu = function
    | [] -> list_rev_notrec accu
    | x :: l -> if p x then find (x :: accu) l else find accu l 
  in
  find [] l

let list_map_notrec f l =
   list_rev_notrec (List.rev_map f l) 

(* changed to avoid overflow *)
let file_get_lines file = 
   if not (Sys.file_exists file)
      then raise (FileNotFound file);
   let lines = ref [] in
   let f = 
      try open_in file with End_of_file -> raise (FileNotFound file);
      in
   begin try while true do
      lines := input_line f :: !lines 
   done with End_of_file -> () end;
   close_in f;
   list_rev_notrec !lines


(* changed to avoid overflow *)
let build_list item_creator =
   let r = ref [] in
   let add x = (r := x::!r) in
   item_creator add; 
   list_rev_notrec !r

let append_notrec l1 l2 =
   let res = ref l2 in
   let cur = ref (list_rev_notrec l1) in
   begin try while true do
      match !cur with
      | [] -> raise Break
      | x::xs -> 
         res := x::!res;
         cur := xs
   done with Break -> () end;
   !res
   (* todo: use a transfer list ref to list ref function *)
   
let list_concat_map_ l f =
  list_concat_map f l

let list_prod_2 = list_prod 

let list_prod_3 l1 l2 l3 =
   list_prod l1 (list_prod l2 l3)

let list_prod_4 l1 l2 l3 l4 =
   list_prod l1 (list_prod_3 l2 l3 l4)

let list_prod_5 l1 l2 l3 l4 l5 =
   list_prod l1 (list_prod_4 l2 l3 l4 l5)


(***************************************************************)

let complexity_pow a x =
   a ** x

let complexity_pow2 x =
   2. ** x

let complexity_logx x =
   log x

let complexity_log2x x =
   log x *. log x

let complexity_xlogx x =
   x *. log x

let complexity_xlog2x x =
   x *. log x *. log x

let complexity_x2logx x =
   x *. log x *. log x

let complexity_x x =
   x

let complexity_x2 x =
   x *. x

let complexity_x3 x =
   x *. x *. x

let complexity_x4 x =
   x *. x *. x *. x

let rec facto n =
   if n <= 1 then 1 else n * facto (n-1)

let complexity_facto x =
   float_of_int (facto (int_of_float x))

(***************************************************************)

exception Break

(***************************************************************)


let list_rev_notrec l =
   let res = ref [] in
   let cur = ref l in
   begin try while true do
      match !cur with
      | [] -> raise Break
      | x::xs -> 
         res := x::!res;
         cur := xs
   done with Break -> () end;
   !res

let log2 x =
   log x /. log 2.

(***************************************************************)

let bool_of_string s =
  (int_of_string s != 0)

(***************************************************************)

module Cmdline = struct

type kind = 
   | Bool
   | Int
   | Float
   | String
   | List of kind

exception Argument_not_found of string
exception Illegal_command_line of string

let ordered_args,args,flags,others =
   let ordered_args = ref [] in
   let args = Hashtbl.create 10 in
   let flags = Hashtbl.create 10 in
   let others = ref [] in
   let argv = Sys.argv in
   let n = Array.length argv in
   let rec aux i =
      if i = n then () else begin
         let arg = argv.(i) in
         if arg.[0] <> '-' then begin
            add_to_list_ref others arg;
            aux (i+1)
         end else if arg.[1] = '-' then begin
            let key = Str.string_after arg 2 in       
            Hashtbl.add flags key true;
            aux (i+1)
         end else begin
            let key = Str.string_after arg 1 in
            if i+1 = n then raise (Illegal_command_line ("missing argument after " ^ key));
            let value = argv.(i+1) in
            add_to_list_ref ordered_args (key,value);
            Hashtbl.add args key value;
            aux (i+2)
         end 
      end in
   aux 1;
   !ordered_args, args, flags, !others

let remove_flag name =
   Hashtbl.remove flags name

let remove_arg name =
   Hashtbl.remove args name

let iter_args f =
   Hashtbl.iter f args  

let iter_flags f =
   Hashtbl.iter (fun k v -> f k) flags

let iter_others f =
   List.iter f others

let map_args f =
   build_list (fun add -> iter_args (fun x v -> add (f x v)))

let map_flags f =
   build_list (fun add -> iter_flags (fun x -> add (f x)))

let get_args () =
   map_args (fun x v -> (x,v)) (*ordered_args *)

let get_flags () =
   map_flags (fun x -> x)

let get_others () =
   others

let mem_flag name =
   try Hashtbl.find flags name
   with Not_found -> false

let find_value name = 
   try Hashtbl.find args name
   with Not_found -> raise (Argument_not_found name)

let mem_value name = 
   Hashtbl.mem args name 


let rec format kind value =
   match kind with
   | Bool -> Dyn.Bool (bool_of_string value)
   | Int -> Dyn.Int (int_of_string value)
   | Float -> Dyn.Float (float_of_string value)
   | String -> Dyn.String value
   | List kind' -> 
      let values = split_args value in
      Dyn.List (List.map (format kind) values)

let parse_dyn kind name =
   let value = find_value name in
   format kind value

let parse_bool name =
   bool_of_string (find_value name)

let parse_int name =
   int_of_string (find_value name)

let parse_float name =
   float_of_string (find_value name)

let parse_string name =
   (find_value name)

let parse_list_string name = 
  split_args (find_value name)

let parse_list_int name =
   List.map int_of_string (parse_list_string name)

let parse_list_float name =
   List.map float_of_string (parse_list_string name)

let parse_or_default parsing name default =
   try parsing name with Argument_not_found _ -> default

let parse_or_default_bool =
   parse_or_default parse_bool

let parse_or_default_int =
   parse_or_default parse_int

let parse_or_default_float =
   parse_or_default parse_float

let parse_or_default_string =
   parse_or_default parse_string

let parse_or_default_list_string =
   parse_or_default parse_list_string

let parse_or_default_list_int =
   parse_or_default parse_list_int

let parse_or_default_list_float =
   parse_or_default parse_list_float


let parse_optional parsing name =
   try Some (parsing name) with Argument_not_found _ -> None

let parse_optional_bool =
   parse_optional parse_bool

let parse_optional_int =
   parse_optional parse_int

let parse_optional_float =
   parse_optional parse_float

let parse_optional_string =
   parse_optional parse_string

let parse_optional_list_string =
   parse_optional parse_list_string

end


(****************************************************)

let read_int32 ch =
   let ch1 = input_byte ch in
   let ch2 = input_byte ch in
   let ch3 = input_byte ch in
   let ch4 = input_byte ch in
   let small = Int32.of_int (ch1 lor (ch2 lsl 8) lor (ch3 lsl 16)) in
   Int32.logor (Int32.shift_left (Int32.of_int ch4) 24) small

let read_int64 ch =
   let small = Int64.of_int32 (read_int32 ch) in
   let big = Int64.of_int32 (read_int32 ch) in
   Int64.logor (Int64.shift_left big 32) small

let read_int ch =
   Int64.to_int (read_int64 ch)

(* todo: does not work *)
let read_double ch =
	Int64.float_of_bits (read_int64 ch)

let read_char ch =
  Char.chr (read_int ch)

let read_string ch =
  let len = read_int ch in
  let str = String.make len ' ' in
  for i = 0 to pred len do
     str.[i] <- read_char ch
  done;
  str


(* DEBUG
   print_int ch1;print_newline();
   print_int ch2;print_newline();
   print_int ch3;print_newline();
   print_int ch4; print_newline();

let _ =
   let ch = open_in_bin "LOG_BIN" in
   let a = read_double ch in
   Printf.printf "%f\n" a;
   let b = read_int64 ch in
   Printf.printf "%s\n" (Int64.to_string b);
   let c = read_int64 ch in
   Printf.printf "%s\n" (Int64.to_string c);
   close_in ch;
   exit 0
*)

(****************************************************)

let msg str =
   print_string str; flush stdout

let warning str =
   print_string ("Warning: " ^ str ^ "\n"); flush stdout
