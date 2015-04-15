open XBase
open Params
let system = Pbench.system


(**
   Usage
     ./bench.sh t1 t2 [options]

   Tests "t": generate, accessible, parallel, ... and also "paper",

   Options:

   "-skip a1,a2"  to skip selected actions (actions: make, run, check, plot)
                  Note: "-skip run" automatically activates "-skip make"

   "-only a1,a2"  to perform only selected actions

   "--virtual_run"  to only show the list of commands that would be called

   "-size s1,s2"  to consider only graph of selected sizes
                  (sizes: small, medium, large, all)

   "-kind k1,k2"  to consider only graph of selected kinds (kinds:
                  grid_sq, cube, ... and also: "manual" and "generated" and "all")

   "-idempotent x1,x2"  (values: 0 or 1 or 0,1)

   "-algo x1,x2"  to consider only selected algorithms (used by sequential and parallel experiments)

   "-exp x1,x2"  to consider only selected experiments (used by cutoff and lack_parallelism experiments)

   "-proc n" to specify the number of processors that can be used in parallel runs.
                Default value is based on the host machine.

   "-timeout n" to force a specific timeout for runs
                Default values depend on the "size" argument provided.

   "-mode m"  where m is "normal" (discard all previous results) or "append"
              (append to previous results) or "replace" (discard results that are ran again)
              or "complete" (add results that are missing)

   "--complete"  is a shorthand for "-mode complete"
   "--replace"  is a shorthand for "-mode replace"

   "--our_pseudodfs_preemptive"  to enable the algorithm "our_pseudodfs_preemptive"
   "--sequential_full"   to include competition algorithms in sequential experiment
   "--nocilk"  to skip runs with cilk binaries in the lack_parallelism experiment
   "--details"  to report the execution time in the tables

   Remark: the script sometimes tries to copy the last plot generated in plots.pdf.

todo: report stddev optionally via get_mean_and_stddev_of
*)

(* ============> paper <=========================

    ./bench.sh baselines -size large


    ./bench.sh generate accessible baselines overview overheads -size large

    ./bench.sh compare -kind friendster -size large -only plot


    ===========> details <=========================
    ./bench.sh generate accessible baselines overview -size large -skip make --complete -kind tree_2_512_512,tree_2_512_1024

   
   ./bench.sh compare -skip make -kind cage15,random_arity_3 -size large -algo bfs --stat

  ./bench.sh compare -skip make -kind livejournal1,phased_524288_single,cage15 -size large -algo bfs
  ./bench.sh compare -skip make -kind cage15 -size large -algo bfs --stat
  ./bench.sh compare -skip make -kind friendster -size large -algo bfs
  ./search.opt2 -delta 50 -idempotent 0 -proc 40 -load from_file -infile /home/rainey/graphdata/cage15.adj_bin -bits 32 -source 0 -algo ls_pbfs -frontier ls_bag -ls_pbfs_cutoff 128 -ls_pbfs_loop_cutoff 128

   ./bench.sh generate -size small,medium,large
   ./bench.sh accessible baselines -size large
   ./bench.sh overview  -size large
   ./bench.sh overview  -size large

   ./bench.sh speedups_dfs speedups_bfs -size large
      ==> in the last one above, use --faster to exclude graphs with little speedups

   ---tricks:

   ./bench.sh overview -skip make -size large [-proc 30] [-algo dfs] [-kind tree_2_1024_2048] [--replace|--complete] [-timeout 10]

   ./bench.sh overview -skip make -size large -algo bfs -kind paths_8_phases_1
   
   



    ===========> more <=========================
   ./bench.sh work_efficiency lack_parallelism -size medium
   ./bench.sh generate accessible baselines -size large -kind  ,tree_2_1024_4096 --complete

 
   ./bench.sh overview -skip make -size large --replace -kind tree_2_1024_2048,tree_2_1024_4096 --virtual_run

*)


(* running the script: [TODO: update this]

from pasl/graph/bench:

** only make
./bench.sh -only make all

** without make
./bench.sh -skip make -size small generate
./bench.sh -skip make -size small accessible
./bench.sh -skip make -size small -kind phased_basic,chain,grid_sq  binaries
./bench.sh -skip make -size small sequential
./bench.sh -skip make -size small parallel
./bench.sh -skip make -size small parallel_8 -kind cage15

** two experiments at once:
./bench.sh -skip make -size large sequential parallel

** only plots:
./bench.sh -skip make -size small sequential -only plot

** virtual run:
./bench.sh -skip make -size medium parallel --virtual_run -only run

** only manuals
./bench.sh -skip make -kind manual -size medium parallel

** various sizes:
./bench.sh -skip make -size small,medium accessible

** to redo part of an experiment
./bench.sh -skip make -size small sequential --replace -kind sq_grid

** to complete an experiment when new runs are required
./bench.sh -skip make -size small sequential --complete

** open results and pdfs:
gedit results_sequential_small.txt
evince plots_sequential_small.pdf
evince plots.pdf

** to log a run
./search.log --view -idempotent 0 -delta 50 -proc 40 -load from_file -infile _data/phased_512_arity_512_large.adj_bin -bits 64 -source 0 -algo ls_pbfs -frontier ls_bag -loop_cutoff 256 -ls_pbfs_cutoff 256

** to view a run
./view.sh

** to generated a graphviz file
./graphfile.opt2 -outfile _data/phased_16384_single_tiny.dot -loop_cutoff 1000 -generator phased -generator_proc 40 -nb_phases 3 -nb_vertices_per_phase 10 -nb_per_phase_at_max_arity 1 -arity_of_vertices_not_at_max_arity 0 -bits 32 -source 0 -seed 1 -proc 40

** to build graphviz file online:
http://graphviz-dev.appspot.com/

** to build graphviz file offline
dot _data/sq_grid_tiny.dot -Tpdf > test.pdf


*)


(*****************************************************************************)
(** Parameters *)

let arg_faster = XCmd.mem_flag "faster" (* skip some slow graphs *)

let arg_onlys = XCmd.parse_or_default_list_string "only" []
let arg_skips = XCmd.parse_or_default_list_string "skip" []
let arg_virtual_run = XCmd.mem_flag "virtual_run"
let arg_nb_runs = XCmd.parse_or_default_int "runs" 1
let arg_proc = XCmd.parse_or_default_int "proc" (-1)
let arg_sizes = XCmd.parse_or_default_list_string "size" ["all"]
let arg_kinds = XCmd.parse_or_default_list_string "kind" ["all"]
let arg_algos = XCmd.parse_or_default_list_string "algo" ["all"]
let arg_timeout_opt = XCmd.parse_optional_int "timeout"
let arg_use_cilk = XCmd.mem_flag "use_cilk"
let arg_details = XCmd.mem_flag "details"
let arg_idempotent = XCmd.parse_or_default_list_int "idempotent" [0;1]
let arg_exps = XCmd.parse_or_default_list_string "exp" ["all"]
let arg_mode = Mk_runs.mode_from_command_line "mode"


let arg_proc =
   if arg_proc >= 0 then arg_proc else begin
      match Pbench.get_localhost_name() with
      | "teraram" -> 40
      | _ -> Pbench.error (sprintf "Cannot guess a value for option '-proc'")
   end

let arg_our_pbfs_cutoff = XCmd.parse_or_default_int "our_pbfs_cutoff" 1024
let arg_our_lazy_pbfs_cutoff = XCmd.parse_or_default_int "our_lazy_pbfs_cutoff" 128
let arg_ls_pbfs_cutoff = XCmd.parse_or_default_int "ls_pbfs_cutoff" 512
let arg_ls_pbfs_loop_cutoff = XCmd.parse_or_default_int "ls_pbfs_loop_cutoff" 512

let run_modes =
  Mk_runs.([
    Mode (Mk_runs.mode_of_string arg_mode);
    (* Mode arg_mode; *)
    Virtual arg_virtual_run;
    Runs arg_nb_runs; ])


(*****************************************************************************)
(** Steps *)

let select make run check plot =
   let arg_skips =
      if List.mem "run" arg_skips && not (List.mem "make" arg_skips)
         then "make"::arg_skips
         else arg_skips
      in
   Pbench.execute_from_only_skip arg_onlys arg_skips [
      "make", make;
      "run", run;
      "check", check;
      "plot", plot;
      ]

let nothing () = ()


(*****************************************************************************)
(** Files and binaries *)

let prog_parallel_cilk = "./search.cilk"
let prog_parallel_cilk_seq_init = "./search.cilk_seq_init"

let prog_parallel =
  if not arg_use_cilk then "./search.opt2"
  else prog_parallel_cilk
let prog_parallel_seq_init =
  if not arg_use_cilk then "./search.opt2_seq_init"
  else prog_parallel_cilk_seq_init
let prog_sequential = "./search.elision2"

let build_in folder bs =
  let make_cmd = "make -j " in
  let make_cmd = if folder = ""
                 then make_cmd
                 else make_cmd ^ " -C " ^ folder ^ " "
  in
  system (make_cmd ^ (String.concat " " bs))

let build bs =
  build_in "" bs

let wrap_name_size name =
   let ssize =
      match arg_sizes with
      | [x] -> x
      | _ -> "all"
   in
   let binary = if arg_use_cilk then "cilk_" else "" in
   binary ^ name ^ "_" ^ ssize

let file_results exp_name =
  Printf.sprintf "results_%s.txt" (wrap_name_size exp_name)

let file_plots exp_name =
  Printf.sprintf "plots_%s.pdf" (wrap_name_size exp_name)

let file_tables_src exp_name =
  Printf.sprintf "tables_%s.tex" (wrap_name_size exp_name)

let file_tables exp_name =
  Printf.sprintf "tables_%s.pdf" (wrap_name_size exp_name)


(*****************************************************************************)
(** Graph sizes *)

type size = Small | Medium | Large

let load = function
   | Small -> 1000000
   | Medium -> 10000000
   | Large -> 100000000
    (* todo: huge; but not for 32 bits *)

let timeout_for_size = function
   | Small -> 1
   | Medium -> 10
   | Large -> 1000

let bits_for_size = function
   | Small -> 32
   | Medium -> 32
   | Large -> 64

let string_of_size = function
   | Small -> "small"
   | Medium -> "medium"
   | Large -> "large"

let size_of_string = function
   | "small" -> Small
   | "medium" -> Medium
   | "large" -> Large
   | _ -> Pbench.error "invalid argument for argument size"

let arg_sizes =
   match arg_sizes with
   | ["all"] -> ["small"; "medium"; "large"]
   | _ -> arg_sizes

let use_sizes =
   List.map size_of_string arg_sizes

let mk_sizes =
   mk_list string "size" (List.map string_of_size use_sizes)


(*****************************************************************************)
(** Graph descriptions *)

let mk_graph_outputs_all_generated : Params.t =
   let mk_file file =
      mk string "outfile" (sprintf "_data/%s.adj_bin" file) in
   let mk_file_by_size ?bits kind mk_by_size =
      Params.concat (~~ List.map use_sizes (fun size ->
         let bits = match bits with Some b -> b | None -> bits_for_size size in
         let timeout = XOption.unsome_or (timeout_for_size size) arg_timeout_opt in
            (mk_file (kind ^ "_" ^ (string_of_size size)))
          & (mk_by_size size)
          & (mk string "!kind" kind)
          & (mk string "!size" (string_of_size size))
          & (mk int "bits" bits)
          & (mk int "source" 0)
          & (mk int "timeout" timeout)
          & (mk int "seed" 1)))
      in
   let mk_common ?(force_sequential=false) gen =
        mk string "generator" gen
      & mk int "generator_proc" (if not force_sequential then arg_proc else 0)
      in
   let mk_circular_knext =
      let build file params =
         mk_file_by_size file (fun size ->
            let (nb_vertices,k) = params size in
            mk_common "circular_knext"
          & mk int "nb_vertices" nb_vertices
          & mk int "k" k)
         in
         build "chain" (fun size ->
           let real_load = load size / 2 in
           real_load, 1)
      (* ++ build "circular_next_50" (fun size ->
            let real_load = 8 * load size in
            (real_load / 50), 50) *)
      (* ==> dropped because not interesting for traversals, and running too fast
      ++ build "complete" (fun size ->
            let real_load = (if size = Large then 2 (* 30*) else 10) * load size in
            let nb_nodes = XInt.sqrt real_load in
            nb_nodes, nb_nodes) *)
      in
   let mk_grid2 =
      let build file params =
         mk_file_by_size file (fun size ->
            let (x,y) = params size in
            mk_common "grid_2d"
          & mk int "width" x
          & mk int "height" y)
         in
        (build "grid_sq" (fun size ->
           let nb_nodes = load size / 2 in
           let nb_side = XInt.sqrt nb_nodes in
           nb_side, nb_side)
     (*
      ++ build "grid_hori_10" (fun size ->
           let nb_nodes = load size / 2 in
           (nb_nodes / 10), 10)
      ++ build "grid_verti_10" (fun size ->
           let nb_nodes = load size / 2 in
           (10, (nb_nodes / 10)))
      ++ build "grid_verti_100" (fun size ->
           let nb_nodes = load size / 2 in
           (100, (nb_nodes / 100)))
      *)
       )
      in
   let mk_grid3 =
        mk_common "cube_grid"
      & (mk_file_by_size "cube" (fun size ->
            let nb_nodes = load size / 3 in
            let nb_side = XInt.pow_float nb_nodes (1. /. 3.) in
            mk int "nb_on_side" nb_side))
      in
   let mk_phased =
       let build file params =
         mk_file_by_size file (fun size ->
           let (nb_phases,nb_per_phase,nb_at_max,arity_not) = params size in
              mk_common "phased"
            & mk int "nb_phases" nb_phases
            & mk int "nb_vertices_per_phase" nb_per_phase
            & mk int "nb_per_phase_at_max_arity" nb_at_max
            & mk int "arity_of_vertices_not_at_max_arity" arity_not)
            in
          (
          (* build "phased_200_full" (fun size ->
             let real_load = (if size = Large then (*30*) 2 else 10) * load size in
             let nb_per_phase = 200 in
             let nb_phases = XInt.sqrt (real_load / nb_per_phase / nb_per_phase) in
             (nb_phases, nb_per_phase, nb_per_phase, 0))
        ++
           build "phased_high_50" (fun size ->
             let real_load = (if size = Large then (*30*) 2 else 10) * load size in
             let nb_phases = 50 in
             let nb_per_phase = XInt.sqrt (real_load / nb_phases) in
             (nb_phases, nb_per_phase, nb_per_phase, 0))
        ++*) build "phased_low_50" (fun size ->
             let real_load = (if size = Large then 2 else 1) * load size in
             let nb_phases = 50 in
             let arity = 5 in
             let nb_per_phase = real_load / nb_phases / arity in
             (nb_phases, nb_per_phase, 0, arity))
        ++ build "phased_mix_10" (fun size ->
             let nb_phases = 10 in
             let arity = 2 in
             let nb_per_phase = (load size) / nb_phases / (1+arity) in
             (nb_phases, nb_per_phase, 1, arity))
        (* ++ build "phased_mix_2" (fun size ->
             let real_load = (if size = Large then 3 else 2) * load size in
             let nb_phases = 2 in
             let arity = 2 in
             let nb_per_phase = real_load / (2+arity) in
             (nb_phases, nb_per_phase, 1, arity)) *)
        ++ build "phased_524288_single" (fun size ->
             let real_load = (if size = Large then 2 else 2) * load size in
             let nb_per_phase = 524288 in
             let nb_phases = real_load / nb_per_phase in
             (nb_phases, nb_per_phase, 1, 0))
        (* ++ build "phased_2048_single" (fun size ->
             let real_load = (if size = Large then 2 else 2) * load size in
             let nb_per_phase = 2048 in
             let nb_phases = real_load / nb_per_phase in
             (nb_phases, nb_per_phase, 1, 0))
        ++ build "phased_4096_single" (fun size ->
             let real_load = (if size = Large then 2 else 2) * load size in
             let nb_per_phase = 4096 in
             let nb_phases = real_load / nb_per_phase in
             (nb_phases, nb_per_phase, 1, 0)) *)
        (*++ (let phased_n_n n =
              build (sprintf "phased_%d_arity_%d" n n) (fun size ->
                let real_load = (if size = Large then 2 else 2) * load size in
                let nb_per_phase = n in
                let nb_phases = max 2 (real_load / nb_per_phase / nb_per_phase) in
                (nb_phases, nb_per_phase, nb_per_phase, 0))
              in
         phased_n_n 1024
             phased_n_n 512
         ++ phased_n_n 513
         ++ phased_n_n 1024
         ++ phased_n_n 1025 ) *)
        )
      in
   let mk_parallel_paths =
       let build file params =
         mk_file_by_size file (fun size ->
           let (nb_phases,nb_paths_per_phase,nb_edges_per_path) = params size in
              mk_common "parallel_paths"
            & mk int "nb_phases" nb_phases
            & mk int "nb_paths_per_phase" nb_paths_per_phase
            & mk int "nb_edges_per_path" nb_edges_per_path
            )in
         (build "paths_8_phases_1" (fun size ->
             let real_load = load size / 2 in
             let paths = 8 in
             let nb_phases = 1 in
             let edges = real_load / paths / nb_phases  in
             (nb_phases, paths, edges))
        ++ build "paths_20_phases_1" (fun size ->
             let real_load = load size / 2 in
             let paths = 20 in
             let nb_phases = 1 in
             let edges = real_load / paths / nb_phases  in
             (nb_phases, paths, edges))
        ++ build "paths_100_phases_1" (fun size ->
             let real_load = load size / 2 in
             let paths = 100 in
             let nb_phases = 1 in
             let edges = real_load / paths / nb_phases in
             (nb_phases, paths, edges))
        ++ build "paths_524288_phases_1" (fun size ->
             let real_load = load size / 2 in
             let paths = 524288 in
             let nb_phases = 1 in
             let edges = real_load / paths / nb_phases in
             (nb_phases, paths, edges))
        (* ++ build "paths_100_phases_200" (fun size ->
             let real_load = load size / 2 in
             let paths = 100 in
             let nb_phases = 200 in
             let edges = real_load / paths / nb_phases in
             (nb_phases, paths, edges)) *)
        (*++ build "paths_30_phases_200" (fun size ->
             let real_load = load size / 2 in
             let paths = 30 in
             let nb_phases = 200 in
             let edges = real_load / paths / nb_phases in
             (nb_phases, paths, edges))*)
       (*++ build "paths_100000_len_2" (fun size ->
             let real_load = load size in
             let paths = 100000 in
             let edges = 2 in
             let nb_phases = real_load / paths / (1+edges) in
             (nb_phases, paths, edges))*)
        )
      in
    let mk_tree =
         mk_file_by_size "tree_binary" (fun size ->
            let height = XInt.log2 (load size) in
              mk_common "tree_binary"
            & mk int "branching_factor" 2
            & mk int "height" height)
          ++   
            mk_file_by_size "tree_depth_2" (fun size ->
          let real_load = (if size = Large then (* 10 *) 1 else 5) * load size in
          let branching_factor = XInt.sqrt real_load in
            mk_common "tree_depth_2"
          & mk int "branching_factor" branching_factor)
      
          (* todo: remove copy paste below *)
      (*
      ++ mk_file_by_size "tree_2_512_1024" (fun size ->
          let real_load = (if size = Large then 1 else 5) * load size in
          let branching_factor_1 = 512 in
          let branching_factor_2 = 1024 in
          let nb_phases = real_load / branching_factor_1 / branching_factor_2 in
            mk_common "tree_2"
          & mk int "branching_factor_1" branching_factor_1
          & mk int "branching_factor_2" branching_factor_2
          & mk int "nb_phases" nb_phases)
      ++ mk_file_by_size "tree_2_512_2048" (fun size ->
          let real_load = (if size = Large then  1 else 5) * load size in
          let branching_factor_1 = 512 in
          let branching_factor_2 = 2048 in
          let nb_phases = real_load / branching_factor_1 / branching_factor_2 in
            mk_common "tree_2"
          & mk int "branching_factor_1" branching_factor_1
          & mk int "branching_factor_2" branching_factor_2
          & mk int "nb_phases" nb_phases)
       *)
      ++ mk_file_by_size "tree_2_512_512" (fun size ->
          let real_load = (if size = Large then  1 else 5) * load size in
          let branching_factor_1 = 512 in
          let branching_factor_2 = 512 in
          let nb_phases = real_load / branching_factor_1 / branching_factor_2 in
            mk_common "tree_2"
          & mk int "branching_factor_1" branching_factor_1
          & mk int "branching_factor_2" branching_factor_2
          & mk int "nb_phases" nb_phases)
      ++ mk_file_by_size "tree_2_512_1024" (fun size ->
          let real_load = (if size = Large then  1 else 5) * load size in
          let branching_factor_1 = 512 in
          let branching_factor_2 = 1024 in
          let nb_phases = real_load / branching_factor_1 / branching_factor_2 in
            mk_common "tree_2"
          & mk int "branching_factor_1" branching_factor_1
          & mk int "branching_factor_2" branching_factor_2
          & mk int "nb_phases" nb_phases)
         in
    let _mk_unbalanced_tree =
       mk_file_by_size "unbalanced_tree" (fun size ->
          let real_load = load size * 6 in
          let d = float_of_int real_load *. 0.00001 in
          let frac_for_branches = 0.666 in
          let frac_for_trunk = 1.0 -. frac_for_branches in
          let depth_of_branches = int_of_float (10.0 *. d *. frac_for_branches) in
          let depth_of_trunk = int_of_float (d *. frac_for_trunk) in
          let trunk_first = 1 in
            mk_common "unbalanced_tree"
          & mk int "depth_of_trunk" depth_of_trunk
          & mk int "depth_of_branches" depth_of_branches
          & mk int "trunk_first" trunk_first)
      in
   let _mk_rmat =
        mk_common "rmat"
        & (mk_file_by_size ~bits:32 "rmat_basic" (fun size ->
          (* load needs to be limited cause only 32 bits *)
          let real_load = (if size = Large then 2 else 10) * load size in
          let nb_vertices = real_load / 15 / 100 in
          let nb_edges = 80 * nb_vertices in
          let seed = 3234230.0 in
          let a = 0.5 in
          let b = 0.1 in
          let c = 0.3 in
              mk int "tgt_nb_vertices" nb_vertices
            & mk int "nb_edges" nb_edges
            & mk float "rmat_seed" seed
            & mk float "a" a
            & mk float "b" b
            & mk float "c" c))
      in
   let mk_random =
      let build file params =
         mk_file_by_size ~bits:32 file (fun size ->
           (* load needs to be limited cause only 32 bits *)
           let (dim,degree,num_rows) = params size in
              mk_common "random"
            & mk int "dim" dim
            & mk int "degree" degree
            & mk int "num_rows" num_rows)
            in
         (build "random_arity_3" (fun size ->
             let dim = 10 in
             let degree = 3 in
             let num_rows = load size / degree in
             (dim, degree, num_rows))
       ++ build "random_arity_8" (fun size ->
             let dim = 10 in
             let degree = 8 in
             let num_rows = load size / degree in
             (dim, degree, num_rows))
       ++ build "random_arity_100" (fun size ->
             let dim = 10 in
             let degree = 100 in
             let num_rows = load size / degree in
             (dim, degree, num_rows))
       (*++ build "random_arity_1000" (fun size ->
             let real_load = (if size = Large then 1 else 15) * load size in
             let dim = 10 in
             let degree = 1000 in
             let num_rows = real_load / degree in
             (dim, degree, num_rows)) *)
         )
      in
   Params.eval (
       mk_random
    ++ (if arg_faster then (fun _ -> []) else mk_grid2 ++ mk_grid3 ++ mk_circular_knext ++ mk_parallel_paths (* ++ mk_unbalanced_tree *))
    ++ mk_phased
    ++ mk_tree
(*    ++ mk_rmat *)
    )


let graph_renaming = 
   [ "livejournal1", "livejournal";
     "paths_524288_phases_1", "para_chains_524k";
     "wikipedia-20070206", "wikipedia-2007";
     "paths_100_phases_1", "par_chains_100";
     "paths_20_phases_1", "par_chains_20";
     "paths_8_phases_1", "par_chains_8";
     "chain", "chain";
     "tree_binary", "complete_bin_tree";
     "tree_depth_2", "trees_10k_10k";
     "tree_2_512_512", "trees_512_512";
     "tree_2_512_1024", "trees_512_1024";
     "tree_2_512_512", "trees_512_512";
     "tree_2_512_1024", "trees_512_1024";
     "unbalanced_tree", "unbalanced_tree";
     "random_arity_3", "random_arity_3";
     "random_arity_8", "random_arity_8";
     "random_arity_100", "random_arity_100"; 
     "phased_low_50", "phases_50_d_5"; 
     "phased_mix_10", "phases_10_d_2_one"; 
     (* "phased_mix_2", "phases_2_arity_2_but_one"; *)
     "phased_524288_single", "trees_524k";
     "grid_sq", "square_grid";
     "cube", "cube_grid";
    ]

(*
let graph_renaming = 
   [ "livejournal1", "livejournal";
     "paths_524288_phases_1", "parallel_chains_524k";
     "wikipedia-20070206", "wikipedia-2007";
     "paths_100_phases_1", "parallel_chains_100";
     "paths_20_phases_1", "parallel_chains_20";
     "paths_8_phases_1", "parallel_chains_8";
     "chain", "chain";
     "tree_binary", "complete_binary_tree";
     "tree_depth_2", "trees_arity_10k_10k";
     "tree_2_512_512", "trees_arity_512_512";
     "tree_2_512_1024", "trees_arity_512_1024";
     "tree_2_512_512", "trees_arity_512_512";
     "tree_2_512_1024", "trees_arity_512_1024";
     "unbalanced_tree", "unbalanced_tree";
     "random_arity_3", "random_arity_3";
     "random_arity_8", "random_arity_8";
     "random_arity_100", "random_arity_100"; 
     "phased_low_50", "phases_50_arity_5"; 
     "phased_mix_10", "phases_10_arity_2_x"; 
     (* "phased_mix_2", "phases_2_arity_2_but_one"; *)
     "phased_524288_single", "trees_arity_524k";
     "grid_sq", "square_grid";
     "cube", "cube_grid";
    ]
*)

let mk_graph_outputs_all_manual : Params.t =
   let mk_file file = (* ?todo: rename "outfile" into "output_file" *)
      mk string "outfile" (sprintf "~/graphdata/%s.adj_bin" file) in
    let mk_manual file ?timeout size bits source =
       let timeout = XOption.unsome_or (XOption.unsome_or (timeout_for_size size) timeout) arg_timeout_opt in
         (mk_file file)
       & (mk string "!kind" file)  (* ghost arguments *)
       & (mk string "!size" (string_of_size size))
       & (mk int "bits" bits)
       & (mk int "source" source)
       & (mk int "timeout" timeout)
      in
   let size_medium = if arg_sizes = ["medium"] then Medium else Large in
   Params.eval (
      mk_manual "friendster" Large 64 123
   ++ mk_manual "twitter" Large 64 12
   ++ mk_manual "livejournal1" size_medium 32 0
   ++ mk_manual "wikipedia-20070206" size_medium 32 0
   (*   ++ mk_manual "cage14" size_medium 32 0 *)
   ++ mk_manual "cage15" size_medium 32 0
   (* ++ mk_manual "kkt_power" Small 32 0 *)
   (*   ++ mk_manual "Freescale1" size_medium 32 0*)
   )

let mk_graph_outputs_all =
      mk_graph_outputs_all_manual
   ++ mk_graph_outputs_all_generated

(** Build an association array from kinds to the list of keys that characterize
    a graph of this kind. E.g., "grid_sq" is bound to ["width"; "height"]. *)

let kinds_keys_assoc =
  let es = Params.to_envs mk_graph_outputs_all in
  let bs = ~~ XList.bucket es (fun e -> Env.get_as_string e "!kind") in
  let kes = ~~ List.map bs (fun (kind,es) -> (kind, List.hd es)) in
  (* todo: check all the buckets have the same params *)
  let kes = ~~ List.map kes (fun (kind,e) ->
     let ks = Env.keys e in
     let ks2 = ~~ List.filter ks (fun k ->
        not (List.mem k ["outfile"; "timeout"; "!size"; "!kind"])) in
     (kind, ks2))
     in
  kes

let keys_of_kind kind : string list =
   try List.assoc kind kinds_keys_assoc
   with Not_found -> Pbench.error ("unknow kind for keys_of_kind" ^ kind)

let kinds_all =
   XList.keys kinds_keys_assoc

let kinds_manual =
   ~~ List.map (Params.to_envs mk_graph_outputs_all_manual) (fun e ->
      Env.get_as_string e "!kind")

let kinds_generated =
   XList.substract kinds_all kinds_manual

let arg_kinds =
   match arg_kinds with
   | ["all"] -> kinds_all
   | ["generated"] -> kinds_generated
   | ["manual"] -> kinds_manual
   | ["selected"] -> ["chain";"circular_next_50";"paths_8_phases_1";"tree_depth_2";"random_arity_3";"phased_200_full"]
   | _ -> arg_kinds

let mk_kinds =
   mk_list string "kind" arg_kinds

let mk_graphs_filter_by_kinds_sizes mk_graph_params =
   Params.eval (~~ Params.filter mk_graph_params (fun e ->
         List.mem (Env.get_as_string e "!kind") arg_kinds
      && List.mem (Env.get_as_string e "!size") arg_sizes))

let mk_graph_outputs_generated =
   mk_graphs_filter_by_kinds_sizes mk_graph_outputs_all_generated

let mk_graph_outputs : Params.t =
   ~~ List.iter arg_kinds (fun kind ->
      if not (List.mem kind kinds_all)
         then Pbench.error ("unknow graph kind: " ^ kind));
   mk_graphs_filter_by_kinds_sizes mk_graph_outputs_all


let graph_descriptions_of mk_graph_outputs = (* todo: use better functions instead of this *)
   ~~ List.map (Params.to_envs mk_graph_outputs) (fun e ->
      (Env.get_as_string e "outfile"),
      (Env.get_as_string e "!kind"),
      (Env.get_as_string e "!size"),
      (Env.get_as_int e "bits"),
      (Env.get_as_int e "source"),
      (Env.get_as_int e "timeout")
      )

let graph_descriptions = 
   graph_descriptions_of mk_graph_outputs

let kinds_sizes_assoc =
   let sks = ~~ List.map graph_descriptions (fun (file,kind,size,bits,source,timeout) -> (size,kind)) in
   let s_to_sks = XList.bucket (fun (s,k) -> s) sks in
   ~~ List.map s_to_sks (fun (s,sks) ->
      let ks = List.map (fun (s,k) -> k) sks in
      (s,ks))

let kinds_of_size size =
   try List.assoc size kinds_sizes_assoc
   with Not_found -> []

let generated_kinds_of_size size =
   let ks = kinds_of_size size in
   List.filter (fun k -> List.mem k kinds_generated) ks

let mk_kind_for_size =
   mk_eval_list string "kind" (fun e -> kinds_of_size (Env.get_as_string e "size"))

let mk_generated_kind_for_size =
   mk_eval_list string "kind" (fun e -> generated_kinds_of_size (Env.get_as_string e "size"))

let mk_graph_inputs_of graph_descriptions =
   Params.eval (Params.concat (~~ List.map graph_descriptions (fun (file,kind,size,bits,source,timeout) ->
        mk string "load" "from_file"
      & mk string "infile" file
      & mk string "!kind" kind
      & mk string "!size" size
      & mk int "bits" bits
      & mk int "source" source
      & mk int "timeout" timeout)))

let mk_graph_inputs =
   mk_graph_inputs_of graph_descriptions

let mk_graph_files =
   Params.concat (~~ List.map graph_descriptions (fun (file,kind,size,bits,source,timeout) ->
      mk string "infile" file))

(*
   let es = mk_graphs Env.empty in
   let names = ~~ List.map es (fun e ->
      Env.as_string (Env.get e "outfile")) in
   mk_list string "infile" names
*)

(*****************************************************************************)
(** Maximum expected speedup *)

let max_speedup_dfs =
   [ "chain", 1;
     "paths_100_phases_1", 100;
     "paths_20_phases_1", 20;
     "paths_8_phases_1", 8;
   ]

let max_speedup_bfs =
   [ "chain", 1;
     "paths_100_phases_1", 1;
     "paths_20_phases_1", 1;
     "paths_8_phases_1", 1;
(*
     "grid_hori_10", 1;
     "grid_verti_10", 1;
     "grid_verti_100", 1;
*)
   ]


(*****************************************************************************)
(** Parameters for algorithms *)

(** Traversals *)

let mk_traversal_dfs =
   mk string "!traversal" "pseudodfs"

let mk_traversal_bfs =
   mk string "!traversal" "bfs"

let mk_traversals =
   mk_list string "!traversal" ["pseudodfs"; "bfs"]

(** Programs *)

let mk_sequential_prog =
     mk_prog prog_sequential
   & mk int "proc" 0

let mk_parallel_prog proc =
     mk_prog prog_parallel
   & mk int "proc" proc
   & mk int "delta" 50
   (* & mk string "taskset" "cas_si" *)

let mk_cilk_prog proc =
     mk_prog prog_parallel_cilk
   & mk int "proc" proc

let mk_parallel_prog_maxproc =
   mk_parallel_prog arg_proc

let mk_cilk_prog_maxproc =
   mk_cilk_prog arg_proc

(** Evaluation functions *)

let eval_exectime = fun env all_results results ->
   Results.get_mean_of "exectime" results

let eval_exectime_stddev = fun env all_results results ->
   Results.get_stddev_of "exectime" results

let eval_normalized_wrt_algo baseline = fun env all_results results ->
  let baseline_results =  ~~ Results.filter_by_params all_results (
       from_env (Env.filter_keys ["kind"; "size"] env)
     & mk string "algo" baseline
     ) in
  if baseline_results = [] then Pbench.warning ("no results for baseline: " ^ Env.to_string env);
  let v = Results.get_mean_of "exectime" results in
  let b = Results.get_mean_of "exectime" baseline_results in
  v /. b

let eval_speedup mk_seq = fun env all_results results ->
   let baseline_results =  ~~ Results.filter_by_params all_results (
       from_env (Env.filter_keys ["kind"; "size"] env)
     & mk_seq
     ) in
   if baseline_results = [] then Pbench.warning ("no results for baseline: " ^ Env.to_string env);
   let tp = Results.get_mean_of "exectime" results in
   let t1 = Results.get_mean_of "exectime" baseline_results in
   t1 /. tp

let eval_speedup_stddev mk_seq = fun env all_results results ->
   let baseline_results =  ~~ Results.filter_by_params all_results (
       from_env (Env.filter_keys ["kind"; "size"] env)
     & mk_seq
     ) in
   if baseline_results = [] then Pbench.warning ("no results for baseline: " ^ Env.to_string env);
   let t1 = Results.get_mean_of "exectime" baseline_results in
   try let times = Results.get Env.as_float "exectime" results in
       let speedups = List.map (fun tp -> t1 /. tp) times in
       XFloat.list_stddev speedups
   with Results.Missing_key _ -> nan

(** Smart constructors for algorithms *)

let mk_algo s =
   mk string "algo" s

let mk_frontier s =
   mk string "frontier" s

let mk_algo_frontier salgo sfrontier =
   (mk_algo salgo) & (mk_frontier sfrontier)

let mk_loop_cutoff =
   mk int "loop_cutoff" 2048

(** Sequential algorithms *)

let mk_sequential_dfs =
      (mk_algo "dfs_by_vertexid_array")
   ++ (mk_algo_frontier "dfs_by_vertexid_frontier" "stl_deque")
   ++ (mk_algo "dfs_by_frontier_segment")

let mk_sequential_bfs =
      (mk_algo "bfs_by_dual_arrays")
   ++ (mk_algo "bfs_by_frontier_segment")
(* deactivated by default
   ++ (mk_algo_frontier "bfs_by_dual_frontiers_and_foreach" "stl_deque")
   ++ (mk_algo_frontier "bfs_by_dual_frontiers_and_pushpop" "stl_deque")
   ++ (mk_algo "bfs_by_array")
   ++ (mk_algo_frontier "bfs_by_dynamic_array" "stl_deque")
*)

(** Parallel algorithms *)

let mk_our_parallel_dfs =
  (mk_algo "our_pseudodfs" & mk int "our_pseudodfs_cutoff" 1024)

let mk_our_parallel_dfs_preemptive =
  (mk_algo "our_pseudodfs_preemptive" & mk int "our_pseudodfs_cutoff" 1024)

let mk_cong_parallel_dfs =
  (mk_algo "cong_pseudodfs")

let mk_our_parallel_bfs =
  (mk_algo "our_pbfs" & mk int "our_pbfs_cutoff" arg_our_pbfs_cutoff)

let mk_our_lazy_parallel_bfs =
  (mk_algo "our_lazy_pbfs" & mk int "our_lazy_pbfs_cutoff" arg_our_lazy_pbfs_cutoff)

let mk_our_parallel_bfs_with_swap = (* deprecated *)
  (mk_algo "our_pbfs_with_swap" & mk int "our_pbfs_cutoff" 2048)

let arg_our_pseudodfs_preemptive = XCmd.mem_flag "our_pseudodfs_preemptive"

let mk_parallel_dfs =
     mk_cong_parallel_dfs
  ++ mk_our_parallel_dfs
  ++ (if arg_our_pseudodfs_preemptive
                          then mk_our_parallel_dfs_preemptive
                          else Params.from_envs []) (* todo: replace this with mk_unit *)


let mk_ls_pbfs_loop_cutoff = 
  mk int "ls_pbfs_loop_cutoff" arg_ls_pbfs_loop_cutoff

let mk_ls_bfs =
   (mk_algo_frontier "ls_pbfs" "ls_bag"
      & mk int "ls_pbfs_cutoff" arg_ls_pbfs_cutoff
      & mk_ls_pbfs_loop_cutoff)

let mk_parallel_bfs =
      mk_ls_bfs
   ++ mk_our_parallel_bfs
   ++ mk_our_lazy_parallel_bfs
   (* ++ (mk_algo_frontier "ls_pbfs" "chunkedseq_bag"
         & mk int "ls_pbfs_cutoff" arg_ls_pbfs_cutoff
         & mk_ls_pbfs_loop_cutoff) *)
   ++ (mk_algo "pbbs_pbfs"
         & mk_loop_cutoff) 

(** Filter the algorithms specified on the command line *)

let arg_algos =
   if arg_algos = ["ours"] then ["our_pbfs"; "our_pseudodfs"] 
   else if arg_algos = ["dfs"] then ["cong_pseudodfs"; "our_pseudodfs"] 
   else if arg_algos = ["bfs"] then ["ls_pbfs"; "our_pbfs"; "our_lazy_pbfs"] 
   else arg_algos

let env_in_arg_algos e =
   if arg_algos = ["all"] then true else begin
      let algo = Env.get_as_string e "algo" in
      List.mem algo arg_algos
   end

let mk_sequential_dfs = Params.eval (Params.filter env_in_arg_algos mk_sequential_dfs)
let mk_sequential_bfs = Params.eval (Params.filter env_in_arg_algos mk_sequential_bfs)
let mk_parallel_dfs = Params.eval (Params.filter env_in_arg_algos mk_parallel_dfs)
let mk_parallel_bfs = Params.eval (Params.filter env_in_arg_algos mk_parallel_bfs)


(*****************************************************************************)
(** Chart formatters *)

(** Title formatters *)

let shortname_of_prog s =
   let ext = XFile.get_extension s in
   if ext = "opt2" || ext = "opt3" then ""
   else if ext = "elision2" || ext = "elision3" then "sequential"
   else if ext = "cilk" then "cilk"
   else if ext = "cilk_seq_init" then "cilk init1"
   else if ext = "opt2_seq_init" then "init1"
   else s

let graph_renamer kind =
  try List.assoc kind graph_renaming
  with Not_found -> kind

let my_formatter_settings = Env.(
      format_values  [ "algo"; "size"; "!size"; "kind"; "!kind"; "!traversal" ]
    @ format_hiddens [ "bits"; "source"; "load"; "frontier" ]
    @ (List.map (fun k -> (k, Format_custom (fun s -> "cutoff=" ^ s)))
        [ "ls_pbfs_cutoff"; "our_pbfs_cutoff"; "our_pbfs_cutoff";
          "cong_pseudodfs_cutoff"; "our_pseudodfs_cutoff" ])
    @ ["infile", Format_custom (fun s ->
         let a = String.rindex s '/' + 1 in
         let b = String.length ".adj_bin" in
         let c = String.length s in
         let g = String.sub s a (c - a - b) in
         graph_renamer g)
         ]
     @ ["proc", Format_custom (fun s ->
         sprintf "cores=%s" s)]
     @ ["!exp_name", Format_custom (fun s ->
         sprintf "%s" s)]
     @ ["prog", Format_custom shortname_of_prog])

let my_formatter =
   Env.format my_formatter_settings

(** Table cells formatter *)

let string_of_millions ?(munit=false) v =
   let x = v /. 1000000. in
   let f = 
     if x >= 10. then sprintf "%.0f" x
     else if x >= 1. then sprintf "%.1f" x
     else if x >= 0.1 then sprintf "%.2f" x
     else sprintf "%.3f" x in
   f ^ (if munit then "m" else "")

let string_of_exp_range n = 
  let v = float_of_int n in
  if n < 1000 then sprintf "%.0f" v
  else if n < 10000 then sprintf "%.1fk" (v /. 1000.)
  else if n < 100000 then sprintf "%.0fk" (v /. 1000.)
  else if n < 1000000 then sprintf "%.0fk" (v /. 1000.)
  else if n < 10000000 then sprintf "%.1fm" (v /. 1000000.)
  else sprintf "%.0fm" (v /. 1000000.)

let optfloat_of_float v =
   match classify_float v with
   | FP_normal | FP_subnormal | FP_zero -> Some v
   | FP_infinite | FP_nan -> None

let string_of_exectime ?(prec=2) v =
   match classify_float v with
   | FP_normal | FP_zero -> 
      if prec = 1 then sprintf "%.1fs" v
      else if prec = 2 then sprintf "%.2fs" v
      else failwith "only precision 1 and 2 are supported"
   | FP_infinite -> "$+\\infty$"
   | FP_subnormal -> "na"
   | FP_nan -> "na"

let string_of_speedup ?(prec=1) v =
   match classify_float v with
   | FP_subnormal | FP_zero -> "-"
   | FP_normal -> 
      if prec = 1 then sprintf "%.1fx" v
      else if prec = 2 then sprintf "%.2fx" v
      else failwith "only precision 1 and 2 are supported"
       (* if v < 10. then sprintf "%.2fx" v else sprintf "%.1fx" v *)
   | FP_infinite -> "[timeout]"
   | FP_nan -> "na"

let string_of_percentage_value v =
    let x = 100. *. v in
    (* let sx = if abs_float x < 10. then (sprintf "%.1f" x) else (sprintf "%.0f" x)  in *)
    let sx = sprintf "%.0f" x in
    sx

let string_of_percentage ?(show_plus=false) v =
   match classify_float v with
   | FP_subnormal | FP_zero | FP_normal ->
       sprintf "%s%s%s"  (if v > 0. && show_plus then "+" else "") (string_of_percentage_value v) "%"
   | FP_infinite -> "$+\\infty$"
   | FP_nan -> "na"

let string_of_percentage_change vold vnew =
  string_of_percentage (vnew /. vold -. 1.0)

let string_of_percentage_change_bounded min_diff vold vnew =
  let p = vnew /. vold -. 1.0 in
  if abs_float p <= min_diff 
    then sprintf "$< \\pm %.0f%s$" (100. *. min_diff) "%"
    else string_of_percentage p

let report_exectime_if_required v=
  if arg_details
    then (sprintf " (%s)" (string_of_exectime v))
    else ""

(*****************************************************************************)
(** Graph generation *)

module ExpGenerate = struct

let name = "generate"

let prog = "./graphfile.opt2"

let make()  =
   system("mkdir -p _data");
   build [prog]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Timeout 400;
      Args (
           mk_prog prog
         & mk int "loop_cutoff" 1000
         & mk_graph_outputs_generated
         & mk_eval int "proc" (fun e -> Env.get_as_int e "generator_proc")
       ) ] ))

let plot () =
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.);] ]);
      Formatter my_formatter;
      Charts mk_sizes;
      Series mk_unit;
      X mk_generated_kind_for_size;
      Input (file_results name);
      Y_label "nb_edges";
      Y (fun env all_results results ->
           try
              let nb_edges =  XList.unique (Results.get_int "nb_edges" results) in
              float_of_int nb_edges
           with Results.Missing_key _ -> nan
          )
      ]))

let all () = select make run nothing plot

end


(*****************************************************************************)
(** Computing accessible *)

module ExpAccessible = struct

let name = "accessible"

let make () =
   build [prog_sequential]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (
           mk_sequential_prog
         & mk_algo_frontier "report_nb_edges_processed" "stl_deque" (* dfs_by_vertexid_array *)
         & mk_graph_inputs
       ) ] ))

let check () = ()

let plot () =
   let commons = Mk_bar_plot.([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.);] ]);
      Formatter my_formatter;
      Charts mk_sizes;
      Series mk_unit;
      X mk_kind_for_size;
      Input (file_results name);
      ]) in
   let charts1 = Mk_bar_plot.(get_charts (commons @ [
      Bar_plot_opt Bar_plot.([
         Y_axis [Axis.Upper (Some 140.) ] ]);
      Y_label "percentage of reachable vertices";
      Y (fun env all_results results ->
           let nb_vertices = Results.get_unique_of "nb_vertices" results in
           let nb_visited_vertices = Results.get_unique_of "nb_visited" results in
           100. *. nb_visited_vertices /. nb_vertices)
      ])) in
   let charts2 = Mk_bar_plot.(get_charts (commons @ [
      Y_label "percentage of reachable edges";
      Y (fun env all_results results ->
           let nb_edges = Results.get_unique_of "nb_edges" results in
           let nb_visited_edges = Results.get_unique_of "nb_edges_processed" results in
           100. *. nb_visited_edges /. nb_edges)
     ])) in
   let chart3 = Mk_bar_plot.(get_charts (commons @ [
      Y_label "nb_edges_processed";
      Y (fun env all_results results ->
            Results.get_unique_of "nb_edges_processed" results)
      ])) in
   let chart4 = Mk_bar_plot.(get_charts (commons @ [
      Y_label "nb_edges";
      Y (fun env all_results results ->
            Results.get_unique_of "nb_edges" results)
      ])) in
   Chart.build (file_plots name) (charts1 @ charts2 @ chart3 @ chart4)

let all () = select make run check plot


end

(*****************************************************************************)
(** Baselines *)

module ExpBaselines = struct

let name = "baselines"

let mk_dfs =
   mk_sequential_prog & mk string "algo" "dfs_by_vertexid_array"

let mk_bfs =
   mk_sequential_prog & mk string "algo" "bfs_by_dual_arrays"

let make () =
   build [prog_sequential]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (
            (   (mk_traversal_dfs & mk_dfs)
             ++ (mk_traversal_bfs & mk_bfs))
           & mk_graph_inputs 
           )
        ] ))

let plot () =
   ()
   (*
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         Chart_opt Chart.([Dimensions (10.,5.) ]);
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.) ] ]);
      Formatter my_formatter;
      Charts mk_sizes;
      Series (mk string "algo" "dfs_by_vertexid_array" ++ mk string "algo" "bfs_by_dual_arrays");
             (* todo: mk_dfs ++ mk_bfs *)
      X (mk_kind_for_size);
      Input (file_results name);
      Output (file_plots name);
      Y_label "exectime";
      Y eval_exectime
      ]))
   *)

let all () =
   select make run nothing plot

end


(*****************************************************************************)
(** WorkEfficiency *)

module ExpWorkEfficiency = struct

let name = "work_efficiency"

let mk_para_seq_init =
      mk_prog prog_parallel_seq_init
   & mk int "proc" 1
   & mk int "delta" 50

(* temp
let mk_sequential_prog =
     mk_prog prog_parallel_cilk_seq_init
   & mk int "proc" 0

let mk_para_seq_init =
      mk_prog prog_parallel_cilk_seq_init
   & mk int "proc" 1
*)

(* temp
let mk_base_dfs =
   mk_sequential_prog & mk string "algo" "dfs_by_vertexid_array"

let mk_base_dfs =
   mk_sequential_prog & mk string "algo" "bfs_by_dual_arrays"

let mk_base_bfs =
   mk_sequential_prog & mk string "algo" "bfs_by_dual_arrays"
*)

let mk_seq_dfs =
     mk_traversal_dfs
   & (   (mk_sequential_prog & (mk_sequential_dfs ++ mk_our_parallel_dfs))
      ++ (mk_para_seq_init & mk_our_parallel_dfs))

let mk_ls_bfs =
   (mk_algo_frontier "ls_pbfs" "ls_bag"
      & mk int "ls_pbfs_cutoff" 4096  (* used to be 1024 *)
      & mk_ls_pbfs_loop_cutoff)
     
let mk_seq_bfs =
     mk_traversal_bfs
   & (   (mk_sequential_prog & (mk_sequential_bfs ++ mk_our_parallel_bfs))
      ++ (mk_para_seq_init & mk_our_parallel_bfs)
      ++ (mk_para_seq_init & mk_ls_bfs))

let make () =
   build [prog_sequential; prog_parallel; prog_parallel_seq_init;
          (* temp prog_parallel_cilk_seq_init *) ]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args ( mk_graph_inputs & ((mk_seq_dfs ++ mk_seq_bfs)))
        ] ))

let plot () =
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let all_results = Results.from_file (file_results name) in
    let results = all_results in
    let env = Env.empty in
    let envs_tables = mk_sizes env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       let envs_serie = (mk_seq_dfs ++ mk_seq_bfs) env in
       let nb_series = List.length envs_serie in
       if nb_series <> 10 then Pbench.error "work_efficiency table generation needs to be modified";
       Mk_table.escape add ("All runs with a single core.");
       add Latex.new_line;
       ~~ XList.iteri envs_serie (fun i env_serie ->
          let env = Env.append env env_serie in
          (* let results = Results.filter env_serie results in
          let nb = List.length results in *)
          Mk_table.escape add (sprintf "%d = %s %s" i (shortname_of_prog (Env.get_as_string env "prog")) (Env.get_as_string env "algo") (* (Env.to_string env) *));
          add Latex.new_line;
       );
       (* add (Latex.tabular_begin (String.concat "" (XList.init (nb_series) (fun i -> "c|")))); *)
       add (Latex.tabular_begin (String.concat "" (["|l||"] @ XList.init 5 (fun i -> "c|") @ ["|"] @ XList.init 5 (fun i -> "c|"))));
       ~~ XList.iteri envs_serie (fun i env_serie ->
          let _env = Env.append env env_serie in
          add " & ";
          Mk_table.escape add (sprintf "%d" i);
       );
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let env = Env.append env env_rows in
         Mk_table.escape add (Env.get_as_string env "kind");
         ~~ XList.iteri envs_serie (fun i env_serie ->
            add " & ";
            let results = Results.filter env_serie results in
            (* let env = Env.append env env_serie in *)
            Results.check_consistent_inputs [] results;
            let v = Results.get_mean_of "exectime" results in
            (* Mk_table.escape add (string_of_exectime v) *)
            if i = 0 || i = 5 then Mk_table.escape add (string_of_exectime v) else begin
               (* let mk_base = (if i < 5 then mk_base_dfs else mk_base_bfs) in *)
               let mk_base = (if i < 5 then ExpBaselines.mk_dfs else ExpBaselines.mk_bfs) in
               let results_baseline = Results.filter (Env.concat [ env_tables ; env_rows ; Params.to_env mk_base ]) all_results in
               Results.check_consistent_inputs [] results_baseline;
               let v_baseline = Results.get_mean_of "exectime" results_baseline in
               Mk_table.escape add (string_of_percentage_change v_baseline v ^ report_exectime_if_required v);
            end;
            );
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ))

let all () =
   select make run nothing plot

end


(*****************************************************************************)
(** WorkEfficiency *)

module ExpLackParallelism = struct

let name = "lack_parallelism"

let mk_the_progs =
  let p1 = mk_prog prog_parallel_seq_init & mk int "delta" 50 in
  let p2 = mk_prog prog_parallel_cilk_seq_init in
  (if XCmd.mem_flag "no_cilk"
     then p1
     else p1 ++ p2)

let mk_graph_of_kinds kinds =
   ~~ Params.filter mk_graph_inputs (fun e ->
      let kind = Env.get_as_string e "!kind" in
      List.mem kind kinds)

let mk_the_graphs =
   let edges_cutoff = [256; 512; 1024; 2048; 4096] in
   let ours =
        mk_algo "our_pbfs"
      & mk_list int "our_pbfs_cutoff" edges_cutoff
      in
   let lack1 = (* node cutoff *)
      mk_graph_of_kinds ["paths_524288_phases_1"]
    & (   (  mk_algo_frontier "ls_pbfs" "ls_bag"
           & mk int "ls_pbfs_loop_cutoff" 1000000
           & mk_list int "ls_pbfs_cutoff" [256; 512; 1024; 2048; 4096])
       ++ (ours) )
     in
   let lack2 = (* edge cutoff *)
      mk_graph_of_kinds ["phased_524288_single"]
    & (   (  mk_algo_frontier "ls_pbfs" "ls_bag"
           & mk_list int "ls_pbfs_loop_cutoff" edges_cutoff
           & mk int "ls_pbfs_cutoff" 1000000)
       ++ (ours))
     in
   let lack3 = (* problem *)
      mk_graph_of_kinds ["tree_2_512_1024"]
    & (   (  mk_algo_frontier "ls_pbfs" "ls_bag"
           & (   (mk int "ls_pbfs_loop_cutoff" 1024 & mk int "ls_pbfs_cutoff" 512)
              (* ++ (mk int "loop_cutoff" 2048 & mk int "ls_pbfs_cutoff" 512) *) ))
        ++ (mk_algo "our_pbfs" & mk int "our_pbfs_cutoff" 1024) )
     in
   let names = XCmd.parse_or_default_list_string "lack" ["1"; "2"; "3"] in
   let mks = List.map snd (List.filter (fun (name,_) -> List.mem name names) [
      "1", lack1;
      "2", lack2;
      "3", lack3;]) in
   Params.concat mks


let make () =
   build [prog_parallel_seq_init; prog_parallel_cilk_seq_init]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (mk int "idempotent" 0
         & (mk_the_progs)
         & (mk_list int "proc" [1; arg_proc])
         & mk_the_graphs)
     ] ))

let plot () =
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let all_results = Results.from_file (file_results name) in
    let results = all_results in
    let env = Env.empty in
    let _graphs = [ "paths_524288_phases_1"; "phased_524288_single"; "phased_256_arity_256"; "phased_512_arity_512"] in
    let envs_tables = mk_unit env in
    let my_formatter = Env.format (Env.([
      "idempotent", Format_hidden;
      "infile", Format_hidden;
      "kind", Format_hidden;
      (* "!kind", Format_hidden; *)
      "frontier", Format_hidden;
      "timeout", Format_hidden;
      "ls_pbfs_cutoff", Format_key_eq_value;
      "our_pbfs_cutoff", Format_key_eq_value;
      "!size", Format_hidden;
      "proc", Format_custom (fun s ->
         sprintf "P=%s" s)
      ] )
      @ List.remove_assoc "kind" my_formatter_settings)
      in
    ~~ List.iter envs_tables (fun env_tables ->
       (* let env_tables = Env.from_assoc ["!kind", Env.Vstring graph] in *)
       (*let results = Results.filter_by_fun (fun kvs ->
       let Env.Vstring kind = List.assoc "kind" kvs in printf "%s %s\n" kind graph; kind = graph) results in*)
       let envs_rows = mk_the_graphs env in
       let envs_serie =
           ((mk_progs [prog_parallel_seq_init; prog_parallel_cilk_seq_init])
            & mk_list int "proc" [1; arg_proc]) env in
       let nb_series = List.length envs_serie in
       (* Mk_table.escape add graph;
       add Latex.new_line;*)
       add (Latex.tabular_begin (String.concat "" (["|l|"] @ XList.init (1+nb_series) (fun i -> "c|"))));
       ~~ XList.iteri envs_serie (fun i env_serie ->
          let env = Env.append env env_serie in
          add " & ";
          Mk_table.escape add (my_formatter env);
       );
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let env = Env.append env env_rows in
         Mk_table.escape add (my_formatter env);
         ~~ XList.iteri envs_serie (fun i env_serie ->
            add " & ";
            let results = Results.filter env_serie results in
            let _env = Env.append env env_serie in
            Results.check_consistent_inputs [] results;
            let v = Results.get_mean_of "exectime" results in
            Mk_table.escape add (string_of_exectime v)
            );
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ))

let all () =
   select make run nothing plot

end


(*****************************************************************************)
(** Speeedups *)

module ExpSpeedups = functor (P : sig val key : string end) -> struct
  (* key should be 'dfs' or 'bfs' *)

let key = P.key

let name = "speedups_" ^ key

let mk_traversal = if key = "dfs" then mk_traversal_dfs else mk_traversal_bfs

let mk_baseline = if key = "dfs" then ExpBaselines.mk_dfs else ExpBaselines.mk_bfs

let algo_baseline = if key = "dfs" then "dfs_by_vertexid_array" else "bfs_by_dual_arrays"

let mk_parallel = if key = "dfs" then mk_parallel_dfs else mk_parallel_bfs

let max_speedup = if key = "dfs" then max_speedup_dfs else max_speedup_bfs

let mk_prog_here = mk_parallel_prog_maxproc (* if key = "dfs" then mk_parallel_prog_maxproc else mk_cilk_prog_maxproc *)

let make () =
   build [prog_parallel (*;
          prog_parallel_cilk *) ] (* temp *)

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (mk_prog_here
          & mk int "idempotent" 0
          & mk_graph_inputs
          & (mk_traversal & mk_parallel))
      ]))

let check () =
   let results =
       Results.from_file (file_results name)
     @ Results.from_file (file_results ExpBaselines.name)
     in
   Results.check_consistent_output_filter_by_params
      "result" mk_graph_files results

let plot () =
   let results = Results.from_file (file_results name) in
   let results = Results.filter_by_params mk_traversal results in
   let results_baseline = Results.from_file (file_results ExpBaselines.name) in
   let results_baseline = Results.filter_by_params (mk_traversal & mk string "algo" algo_baseline) results_baseline in
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let env = Env.empty in
    let envs_tables = (mk_sizes) env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let results_baseline = Results.filter env_tables results_baseline in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       let envs_serie = mk_parallel env in
       let nb_series = List.length envs_serie in
       add (Env.get_as_string env "size");
       add Latex.new_line;
       add (Latex.tabular_begin (String.concat "" (["|l|"] @ XList.init (1+nb_series+1) (fun i -> "c|"))));
       add " & ";
       Mk_table.escape add algo_baseline;
       ~~ List.iter envs_serie (fun env_serie ->
          let env = Env.append env env_serie in
          add " & ";
          Mk_table.escape add (Env.get_as_string env "algo");
       );
       add " & ";
       Mk_table.escape add "max";
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let results_baseline = Results.filter env_rows results_baseline in
         let env = Env.append env env_rows in
         let kind = Env.get_as_string env "kind" in
         Mk_table.escape add kind;
         add " & ";
         Results.check_consistent_inputs [] results_baseline;
         let v_baseline = Results.get_mean_of "exectime" results_baseline in
         add (string_of_exectime v_baseline);
         ~~ List.iter envs_serie (fun env_serie ->
            add " & ";
            let results = Results.filter env_serie results in
            let _env = Env.append env env_serie in
            Results.check_consistent_inputs [] results;
            let v = Results.get_mean_of "exectime" results in
            add (string_of_speedup (v_baseline /. v) ^ report_exectime_if_required v);
            );
          add " & ";
          Mk_table.escape add (match XList.assoc_option kind max_speedup with
            | None -> " "
            | Some n -> sprintf "%dx" n);
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ));
   let yislog = false in
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         Chart_opt Chart.([Dimensions (12.,8.) ]);
         X_titles_dir Vertical;
         Y_axis [ Axis.Lower (Some (if yislog then 0.01 else 0.));
                  Axis.Is_log yislog ]
         ]);
      Formatter my_formatter;
      Charts (mk_sizes & mk_traversal);
      Series (mk_parallel);
      X mk_kind_for_size;
      Results (results @ results_baseline);
      Y_label ("speedup vs " ^ algo_baseline);
      Y (eval_speedup mk_baseline);
      Y_whiskers (eval_speedup_stddev mk_baseline);
      Output (file_plots name)
      ]))

let all () =
   select make run check plot

end



(*****************************************************************************)
(** Comparison on BFS *)

module ExpCompare = struct

let name = "compare"

let arg_stat = XCmd.mem_flag "stat"

let prog_parallel_here = "./search.sta" (* "./search.opt2" *)

let prog_parallel =
  if arg_stat then "./search.sta" else prog_parallel_here

let mk_ls node_cutoff edge_cutoff = 
  (mk_algo_frontier "ls_pbfs" "ls_bag"
      & mk int "ls_pbfs_cutoff" node_cutoff
      & mk int "ls_pbfs_loop_cutoff" edge_cutoff)

let mk_ours_lazy lazy_cutoff =
  (mk_algo "our_lazy_pbfs" & mk int "our_lazy_pbfs_cutoff" lazy_cutoff)

let mk_ours_eager eager_cutoff =
  (mk_algo "our_pbfs" & mk int "our_pbfs_cutoff" eager_cutoff)

let mk_parallel_bfs =
     mk_ls 2048 2048
  ++ mk_ls 1024 2048
  ++ mk_ls 1024 1024
  ++ mk_ls 512 1024
  ++ mk_ls 512 512
  ++ mk_ls 512 256
  ++ mk_ls 256 256
  ++ mk_ls 256 128
  ++ mk_ours_lazy 2048
  ++ mk_ours_lazy 1024
  ++ mk_ours_lazy 512
  ++ mk_ours_lazy 256
  ++ mk_ours_lazy 128
  ++ mk_ours_lazy 64
  (*
  ++ mk_ours_lazy 4096
  ++ mk_ours_lazy 8192
  ++ mk_ours_lazy 16384
  ++ mk_ours_lazy 20000
  ++ mk_ours_lazy 20000
  ++ mk_ours_eager 4096
  ++ mk_ours_eager 20000
  *)

let mk_parallel_bfs_run =  mk_parallel_bfs
(*
     mk_ls 1024 2048
  ++ mk_ours_lazy 1024
  ++ mk_ours_lazy 2048
  ++ mk_ours_lazy 4096
  ++ mk_ours_lazy 20000
  ++ mk_ours_eager 4096
  ++ mk_ours_eager 20000
  *)

let procs = [(* 1; *) (*8; 25; 30; 35;*) 40]

let mk_procs =
  mk_list int "proc" procs

let kinds = 
   let kinds = XCmd.parse_or_default_list_string "kind" ["all"] in
   if kinds <> ["all"] then kinds else
      [ "cage15"; "livejournal1"; "paths_524288_phases_1"; "random_arity_3"; "random_arity_100"; "phased_low_50"; "paths_20_phases_1"; "phased_524288_single" (* "grid_sq" ; "twitter";*) ]

(*

cage15,livejournal1,paths_524288_phases_1,random_arity_3,random_arity_100,phased_low_50,paths_20_phases_1,phased_524288_single

*)


let mk_kinds =
   mk_list string "kind" kinds (*  (if arg_faster then kinds else arg_kinds) *)

let filter_kinds mk =
  Params.eval (~~ Params.filter mk (fun e ->
         List.mem (Env.get_as_string e "!kind") kinds))

let mk_graph_inputs = 
  filter_kinds mk_graph_inputs

(* later: mk_graph_inputs_of (graph_descriptions_of mk_graph_outputs_all) *)


let mk_traversal = mk_traversal_bfs

let mk_baseline = ExpBaselines.mk_bfs

let algo_baseline = "bfs_by_dual_arrays"

let make () =
   build [ prog_parallel ] 

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (mk_prog prog_parallel
          & mk int "delta" 50
          & mk int "idempotent" 0
          & mk_traversal_bfs
          & mk_procs
          & mk_graph_inputs
          & mk_parallel_bfs_run
          )
      ]))

let check () =
   let results =
       Results.from_file (file_results name)
     @ Results.from_file (file_results ExpBaselines.name)
     in
   Results.check_consistent_output_filter_by_params
      "result" mk_graph_files results

let plot () =
   let results = Results.from_file (file_results name) in
   let results_baseline = Results.from_file (file_results ExpBaselines.name) in
   let results_baseline = Results.filter_by_params ((* mk_traversal_bfs & *) mk string "algo" algo_baseline) results_baseline in
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let env = Env.empty in
    let envs_tables = (mk_sizes & mk_kinds) env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let results_baseline = Results.filter env_tables results_baseline in
       let env = Env.append env env_tables in
       let envs_rows = mk_parallel_bfs env in
       let envs_serie = mk_procs env in
       let nb_series = List.length envs_serie in
       (* add "\\small\n"; 
       Mk_table.escape add (Env.get_as_string env "size");  
       add Latex.new_line;
       Mk_table.escape add (Env.get_as_string env "kind");  
       add Latex.new_line;
       *)
       if arg_stat then begin
          add "speedup ; ( thousand threads ; thousand steals)";
          add Latex.new_line;
       end;
       let results_baseline = Results.filter env_tables results_baseline in
       Results.check_consistent_inputs [] results_baseline;
       let v_baseline = Results.get_mean_of "exectime" results_baseline in
       (* add ("baseline=" ^ string_of_exectime v_baseline);  
       add Latex.new_line; *)
       add (Latex.tabular_begin (String.concat "" (["|l|"] @ XList.init (1+nb_series) (fun i -> "c|"))));
       Mk_table.escape add (Env.get_as_string env "kind");
       ~~ List.iter envs_serie (fun env_serie ->
          let env = Env.append env env_serie in
          let proc = Env.get_as_string env "proc" in
          add " & ";
          Mk_table.escape add (sprintf "%s cores" proc);
          );
      let my_formatter = Env.format (Env.(
            ["ls_pbfs_cutoff", Format_custom (fun s ->
            sprintf "vertex_cutoff=%s" s)]
          @ ["ls_pbfs_loop_cutoff", Format_custom (fun s ->
            sprintf "edge_cutoff=%s" s)]
          @ ["our_lazy_pbfs_cutoff", Format_custom (fun s ->
               sprintf "cutoff=%s" s)]
          @ ["algo", Format_custom (fun s ->
               if s = "our_lazy_pbfs" then "our PBFS" else if s = "ls_pbfs" then "LS PBFS" else s)]
          @ my_formatter_settings))
            in
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let env = Env.append env env_rows in
         Mk_table.escape add (my_formatter env_rows);
         ~~ List.iter envs_serie (fun env_serie ->
            let env = Env.append env env_serie in
            let _proc = Env.get_as_string env "proc" in
            let results = Results.filter env_serie results in
            Results.check_consistent_inputs [] results;
            add " & ";
            if results = [] then add "" else begin
               let v = Results.get_mean_of "exectime" results in
               let sbaseline = 
                  if true then "" else begin
                    sprintf "(seq=%.4f) " v
                  end in
               let sspeedup = string_of_speedup (v_baseline /. v) in
               let sextra = 
                  if not arg_stat then "" else begin
                     let thread_create = Results.get_mean_of "thread_create" results in
                     let thread_send = Results.get_mean_of "thread_send" results in
                     sprintf " (%.1f/%.1f)" (thread_create /. 1000.) (thread_send /. 1000.)
                  end in
               add (sbaseline ^ sspeedup ^ sextra);
            end
            );
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
       ))

let all () =
   select make run check plot

end



(*****************************************************************************)
(** Overview

   ./bench.sh overview -proc 30 -size small -skip make

   to update one column only:
   ./bench.sh overview -proc 30 -size large -skip make -algo our_pbfs --replace

*)

module ExpOverview = struct

let name = "overview"

let mk_ligra_bfs =
  mk_algo "ligra"


let mk_parallel_bfs =
      mk_ls_bfs
      ++ mk_our_lazy_parallel_bfs
(*      ++ mk_ligra_bfs*)

let mk_parallel_bfs =
   Params.eval (Params.filter env_in_arg_algos mk_parallel_bfs)

(*
let mk_parallel_dfs =
     mk_our_parallel_dfs
  ++ mk_cong_parallel_dfs

let mk_parallel_dfs =
   Params.eval (Params.filter env_in_arg_algos mk_parallel_dfs)
*)

let prog_parallel_here = (*"./search.sta"*)  (*"./search.opt2" *) "./search.virtual"
let prog_ligra = "ligra.cilk"
let prog_ligra_here = "./" ^ prog_ligra

let mk_parallel_prog_maxproc_here =
   mk_prog prog_parallel_here
   & mk int "proc" arg_proc
   & mk int "delta" 50

let make () = (
  build [prog_parallel_here];
  build_in "../../../ligra/" [prog_ligra])

let mk_ligra = (* todo: add to mk_parallel_bfs (?) *)
    mk_prog "./search.virtual" 
  & mk_algo "ligra" 
   & mk int "proc" arg_proc

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args ( 
            mk int "idempotent" 0
          & mk_graph_inputs
          & (   (mk_parallel_prog_maxproc_here & mk_traversal_bfs & mk_parallel_bfs)
             ++ (mk_parallel_prog_maxproc_here & mk_traversal_dfs & mk_parallel_dfs)
             ++ (mk_ligra & mk_traversal_bfs)
             ))
      ]))

let check () =
   let results =
       Results.from_file (file_results name)
     @ Results.from_file (file_results ExpBaselines.name)
     in
   Results.check_consistent_output_filter_by_params
      "result" (mk_graph_files & mk_traversals) results

let plot () =
   let results = Results.from_file (file_results name) in
   let results_baseline = Results.from_file (file_results ExpBaselines.name) in
   let results_accessible = Results.from_file (file_results ExpAccessible.name) in
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let env = Env.empty in
    let envs_tables = (mk_sizes) env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let results_baseline = Results.filter env_tables results_baseline in
       let results_accessible = Results.filter env_tables results_accessible in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       (* add (Env.get_as_string env "size");
       add Latex.new_line; *)
       let nb_infos = 3 in
       let nb_bfs = 3 in
       let nb_dfs = 3 in
       add (Latex.tabular_begin (String.concat "" (["|l|"] @ XList.init nb_infos (fun i -> "@{\\,\\,}c@{\\,\\,}|") @ ["|"] @ XList.init nb_dfs (fun i -> "c|") @ ["r||"] @ XList.init nb_bfs (fun i -> "c|")  @ ["r|r||r|"] )));
       add (Latex.tabular_multicol 4 "|c||" (Latex.bold "Input graph")); add " & ";
       add (Latex.tabular_multicol 4 "|c||" (Latex.bold "DFS")); add " & ";
       add (Latex.tabular_multicol 5 "|c||" (Latex.bold "BFS")); add " & ";
       add Latex.tabular_newline;
       add "graph & verti. & edges & max & seq & our & Cong. & \\bf{Cong.} & seq & our & LS & \\bf{LS vs} & Ligra & our PBFS ";
       add Latex.new_line;
       add " &  (m) & (m) & dist & DFS & PDFS & PDFS & \\bf{vs ours} & BFS & PBFS & PBFS & \\bf{ ours} & vs ours & vs PDFS";
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let results_baseline = Results.filter env_rows results_baseline in
         let results_accessible = Results.filter env_rows results_accessible in
         let results_baseline_bfs = Results.filter_by_params mk_traversal_bfs results_baseline in
         let env = Env.append env env_rows in
         let kind = Env.get_as_string env "kind" in
         let nb_vertices = Results.get_unique_of "nb_vertices" results_accessible in
         let _nb_visited_vertices = Results.get_unique_of "nb_visited" results_accessible in
         let nb_edges = Results.get_unique_of "nb_edges" results_accessible in
         let _nb_visited_edges = Results.get_unique_of "nb_edges_processed" results_accessible in
         let max_dist = Results.get_unique_of "max_dist" results_baseline_bfs in
         let exectime_for rs mk_base =
            let rs = Results.filter_by_params mk_base rs in
            Results.check_consistent_inputs [] rs;
            let v = Results.get_mean_of "exectime" rs in
            v
            in
         let v_bfs_seq = exectime_for results_baseline ExpBaselines.mk_bfs in
         let v_bfs_ls = exectime_for results mk_ls_bfs in
         let v_bfs_ligra = exectime_for results mk_ligra in
         let v_bfs_our = exectime_for results mk_our_lazy_parallel_bfs in
         let v_dfs_seq = exectime_for results_baseline ExpBaselines.mk_dfs in
         let v_dfs_cong = exectime_for results mk_cong_parallel_dfs in
         let v_dfs_our = exectime_for results mk_our_parallel_dfs in

         Mk_table.cell add (graph_renamer kind);
         Mk_table.cell add (string_of_millions nb_vertices);
         Mk_table.cell add (string_of_millions nb_edges);
         Mk_table.cell add (string_of_exp_range (int_of_float max_dist));
         (* Mk_table.cell add (string_of_millions nb_visited_edges); * *)

         Mk_table.cell add (string_of_exectime ~prec:1 v_dfs_seq);
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_dfs_our));
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_dfs_cong));
         Mk_table.cell add (Latex.bold (string_of_percentage_change v_dfs_our v_dfs_cong));
         (*Mk_table.cell add (sprintf "%.1f" (v_dfs_cong /. v_dfs_our));*)

         Mk_table.cell add (string_of_exectime ~prec:1 v_bfs_seq);
         Mk_table.cell add (string_of_speedup (v_bfs_seq /. v_bfs_our));
         Mk_table.cell add (string_of_speedup (v_bfs_seq /. v_bfs_ls));
         Mk_table.cell add (Latex.bold (string_of_percentage_change v_bfs_our v_bfs_ls));
         Mk_table.cell add (string_of_percentage_change v_bfs_our v_bfs_ligra);

(*
         Mk_table.cell add (sprintf "%.1f" (v_bfs_ls /. v_bfs_our));
         Mk_table.cell add (sprintf "%.1f" (v_bfs_ligra /. v_bfs_our));
*)
         (* Mk_table.cell add (string_of_speedup (v_bfs_seq /. v_bfs_ligra)); *)
         (* Mk_table.cell add (string_of_percentage_change_bounded 0.1 v_bfs_ls v_bfs_our); *)

         Mk_table.cell ~last:true add (string_of_percentage_change v_dfs_our v_bfs_our ); 
         (*  Mk_table.cell ~last:true add (sprintf "%.1f" (v_bfs_our /. v_dfs_our)); *)
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ))

let all () =
   select make run check plot

end



(*****************************************************************************)
(** Overheads *)

module ExpOverheads = struct

let name = "overheads"

let mk_prog_here =
     mk_prog prog_parallel
   & mk int "proc" 1
   & mk int "delta" 50

let make () =
   build [prog_parallel ] 

let mk_parallel =
     (mk_traversal_bfs & mk_parallel_bfs) 
  ++ (mk_traversal_dfs & mk_parallel_dfs)

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (mk_prog_here
          & mk int "idempotent" 0
          & mk_graph_inputs
          & mk_parallel )
      ]))

let check () = ()

let plot () =
   let results = Results.from_file (file_results name) in
   let results_baseline = Results.from_file (file_results ExpBaselines.name) in
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let env = Env.empty in
    let envs_tables = (mk_sizes) env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let results_baseline = Results.filter env_tables results_baseline in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       (* add (Env.get_as_string env "size"); add Latex.new_line; *)
       let nb_bfs = 2 in
       let nb_dfs = 2 in
       add (Latex.tabular_begin (String.concat "" (["|l||"] @ XList.init nb_bfs (fun i -> "c|") @ ["|"] @ XList.init nb_dfs (fun i -> "c|") )));
       add "graph & LS & our & Cong. & our";
       add Latex.new_line;
       add " & PBFS & PBFS & PDFS & PDFS";
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let results_baseline = Results.filter env_rows results_baseline in
         let env = Env.append env env_rows in
         let kind = Env.get_as_string env "kind" in
         let exectime_for rs mk_base =
            let rs = Results.filter (Params.to_env mk_base) rs in
            Results.check_consistent_inputs [] rs;
            let v = Results.get_mean_of "exectime" rs in
            v
            in
         let v_bfs_seq = exectime_for results_baseline ExpBaselines.mk_bfs in
         let v_bfs_ls = exectime_for results mk_ls_bfs in
         let v_bfs_our = exectime_for results mk_our_lazy_parallel_bfs in
         let v_dfs_seq = exectime_for results_baseline ExpBaselines.mk_dfs in
         let v_dfs_cong = exectime_for results mk_cong_parallel_dfs in
         let v_dfs_our = exectime_for results mk_our_parallel_dfs in
         Mk_table.cell add (graph_renamer kind);
         Mk_table.cell add (string_of_percentage_change v_bfs_seq v_bfs_ls);
         Mk_table.cell add (string_of_percentage_change v_bfs_seq v_bfs_our);
         Mk_table.cell add (string_of_percentage_change v_dfs_seq v_dfs_cong);
         Mk_table.cell ~last:true add (string_of_percentage_change v_dfs_seq v_dfs_our);
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ))

let all () =
   select make run check plot

end



(*****************************************************************************)
(** Idempotence *)

module ExpIdempotence = struct

let name = "idempotence"

let mk_idempotent_all =
   mk_list int "idempotent" [0;1]

let make () =
   build [prog_parallel]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (mk_parallel_prog_maxproc
          & mk_idempotent_all
          & mk_graph_inputs
          & (mk_our_parallel_dfs (* ++ mk_our_parallel_bfs *))
          )
        ] ))

let plot () =
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let all_results = Results.from_file (file_results name) in
    let results = all_results in
    let env = Env.empty in
    let envs_tables = (mk_sizes & (mk_our_parallel_dfs ++ mk_our_parallel_bfs)) env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       let envs_serie = mk_idempotent_all env in
       let nb_series = List.length envs_serie in
       Mk_table.escape add (Env.get_as_string env "algo");
       add Latex.new_line;
       add (Latex.tabular_begin (String.concat "" (["|l|"] @ XList.init (nb_series) (fun i -> "c|"))  ));
       ~~ List.iter envs_serie (fun env_serie ->
          let env = Env.append env env_serie in
          add " & ";
          Mk_table.escape add (sprintf "idempotent=%s" (Env.get_as_string env "idempotent"));
       );
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let results_baseline = Results.filter (Env.concat [ env_tables ; env_rows ; Params.to_env (mk int "idempotent" 0) ]) all_results in
         let env = Env.append env env_rows in
         Mk_table.escape add (Env.get_as_string env "kind");
         Results.check_consistent_inputs [] results_baseline;
         let v_baseline = Results.get_mean_of "exectime" results_baseline in
         ~~ List.iter envs_serie (fun env_serie ->
            add " & ";
            let results = Results.filter env_serie results in
            let env = Env.append env env_serie in
            Results.check_consistent_inputs [] results;
            let v = Results.get_mean_of "exectime" results in
            let is_first_col = (Env.get_as_string env "idempotent") = "0" in
            if is_first_col
               then Mk_table.escape add (string_of_exectime v)
               else Mk_table.escape add (string_of_percentage_change v_baseline v ^ report_exectime_if_required v);
            );
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ))

let all () =
   select make run nothing plot

end

(* LATER: here and above experiments
let check () =
   Results.check_consistent_output_filter_by_params_from_file
      "result" mk_graph_files (file_results name)
*)


(*****************************************************************************)
(** ExpLigra *)

module ExpLigra = struct

let name = "ligra"

let mk_programs_ours = (mk_prog "./search.opt2" & mk_our_lazy_parallel_bfs)
let mk_programs_ligra = (mk_prog "./search.virtual" & mk_algo "ligra")
let mk_programs_algos = mk_programs_ours ++ mk_programs_ligra

(* todo: let mk_procs = mk int "proc" arg_proc*)
let mk_procs = mk_list int "proc" [1;arg_proc] 

let make () =
  build [
    "../../../ligra/ligra.cilk_32";
    "../../../ligra/ligra.cilk_64";
    "search.opt2" ]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (
            mk_procs
          & mk_traversal_bfs
          & mk_graph_inputs
          & mk int "idempotent" 0
          & mk_programs_algos
       ) ] ))


let check () =
   Results.check_consistent_output_filter_by_params_from_file
     "result" mk_graph_files (file_results name)

let plot () =
   (* table *)
   let results = Results.from_file (file_results name) in
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let env = Env.empty in
    let envs_tables = mk_sizes env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       add (Env.get_as_string env "size"); add Latex.new_line; 
       add (Latex.tabular_begin "|l|c|c||c|c|c||c|c|c|");
       add "graph & max-dist & BFS & our PBFS & ligra & diff & our PBFS & ligra & diff";
       add Latex.tabular_newline;
       add "  & & seq & 1core & 1core & 1core & Ncore & Ncore & Ncore";
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let env = Env.append env env_rows in
         let kind = Env.get_as_string env "kind" in
         let exectime_for rs mk_base =
            let rs = Results.filter (Params.to_env mk_base) rs in
            Results.check_consistent_inputs [] rs;
            let v = Results.get_mean_of "exectime" rs in
            v
            in
         let results_baseline_bfs = Results.filter_by_params (mk_traversal_bfs & mk int "proc" 0) results in
         let max_dist = Results.get_unique_of "max_dist" results_baseline_bfs in
         let v_base = exectime_for results (mk_algo "bfs_by_dual_arrays" & mk int "proc" 0) in
         let v_our1 = exectime_for results (mk_programs_ours & mk int "proc" 1) in
         let v_ligra1 = exectime_for results (mk_programs_ligra & mk int "proc" 1) in
         let v_our = exectime_for results (mk_programs_ours & mk int "proc" arg_proc) in
         let v_ligra = exectime_for results (mk_programs_ligra & mk int "proc" arg_proc) in
         Mk_table.cell add (graph_renamer kind);
         Mk_table.cell add (string_of_exp_range (int_of_float max_dist));
         Mk_table.cell add (string_of_exectime v_base);

         Mk_table.cell add (string_of_percentage_change v_base v_our1);
         Mk_table.cell add (string_of_percentage_change v_base v_ligra1);
         Mk_table.cell add (string_of_percentage_change v_our1 v_ligra1);

         Mk_table.cell add (string_of_exectime v_our);
         Mk_table.cell add (string_of_exectime v_ligra);
         Mk_table.cell ~last:true add (string_of_percentage_change v_our v_ligra);
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ));
   (* barplot *)
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         Chart_opt Chart.([Dimensions (10.,5.) ]);
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.) ] ]);
      Formatter my_formatter;
      Charts (mk_sizes & mk_procs);
      Series mk_programs_algos;
      X mk_kind_for_size;
      Input (file_results name);
      Output (file_plots name);
      Y_label "exectime";
      Y eval_exectime
      ]))


let all () = select make run check plot

end



(*****************************************************************************)
(** OLD Binaries                Mk_table.escape add (string_of_percentage_change v_baseline v ^ report_exectime_if_required v);performances *)

module ExpOldBinaries = struct

let name = "binaries"

let mk_proc_0 = mk int "proc" 0

let mk_programs =
      (mk_prog "./search.elision2")
   ++ (mk_prog "./search.elision3")
   ++ (mk_prog "./search.opt2" & mk_proc_0)
   ++ (mk_prog "./search.opt3" & mk_proc_0)

let mk_algos = mk_sequential_dfs ++ mk_sequential_bfs

let make () =
   build (~~ Params.map_envs mk_programs (fun e -> Env.get_as_string e "prog"))

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (
           mk_programs
          & mk_graph_inputs
          & mk_algos
       ) ] ))

let check () = ()

let plot () =
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         Chart_opt Chart.([Dimensions (10.,5.) ]);
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.) ] ]);
      Formatter my_formatter;
      Charts (mk_sizes & mk_kind_for_size);
      Series mk_programs;
      X mk_algos;
      Input (file_results name);
      Output (file_plots name);
      Y_label "exectime";
      Y eval_exectime
      ]))

let all () = select make run check plot

end


(*****************************************************************************)
(** OLD Sequential algorithms *)

module ExpOldSequential = struct

let name = "sequential"

let arg_sequential_full = XCmd.mem_flag "sequential_full"


let make () =
   build [prog_sequential; prog_parallel]

let mk_par =
   (mk_parallel_prog 1 ++ mk_parallel_prog_maxproc)

let mk_par_dfs =
   if arg_sequential_full then mk_parallel_dfs else mk_our_parallel_dfs

let mk_par_bfs =
   if arg_sequential_full then mk_parallel_bfs else mk_our_parallel_bfs

let mk_dfs =
     mk_traversal_dfs
   & (   (mk_sequential_prog & (mk_sequential_dfs ++ mk_par_dfs))
      ++ (mk_par & mk_par_dfs))

let mk_bfs =
     mk_traversal_bfs
   & (   (mk_sequential_prog & (mk_sequential_bfs ++ mk_par_bfs))
      ++ (mk_par & mk_par_bfs))

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (mk_graph_inputs & (mk_dfs ++ mk_bfs))
      ]))

let check () =
   Results.check_consistent_output_filter_by_params_from_file
     "result" mk_graph_files (file_results name)

let plot () =
   let get_charts mk_charts mk_series (ylabel,eval) =
      Mk_bar_plot.(get_charts ([
         (* Chart_opt Chart.([
            Legend_opt Legend.([
               Legend_pos Outside_below
               ])]); *)
         Bar_plot_opt Bar_plot.([
            Chart_opt Chart.([Dimensions (18.,8.) ]);
            X_titles_dir Vertical;
            Y_axis [Axis.Lower (Some 0.)] ]);
         Formatter my_formatter;
         Charts (mk_sizes & mk_charts);
         Series mk_series;
         X mk_kind_for_size;
         Input (file_results name);
         Y_label ylabel;
         Y eval;
         ])) in
    let show_normalized baseline =
      (sprintf "exectime normalized vs %s" baseline, eval_normalized_wrt_algo baseline) in
    let show_absolute =
      ("exectime", eval_exectime) in
    Chart.build (file_plots name) (
        get_charts mk_unit mk_dfs (show_normalized "dfs_by_vertexid_array")
      @ get_charts mk_unit mk_bfs (show_normalized "bfs_by_dual_arrays")
      @ if arg_sequential_full then [] else (
      get_charts mk_unit (mk_dfs ++ mk_bfs) (show_normalized "dfs_by_vertexid_array")
      @ get_charts mk_unit (mk_dfs ++ mk_bfs) show_absolute )
     )

let all () = select make run check plot

end


(*****************************************************************************)
(** OLD Parallel traversal algorithms baseline *)

module ExpOldBaselines = struct

let name = "parallel_baseline"

let mk_dfs =
   mk_sequential_prog & mk string "algo" "dfs_by_vertexid_array"

let mk_bfs =
   mk_sequential_prog & mk string "algo" "bfs_by_dual_arrays"

let make () =
   build [prog_sequential]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args ((mk_dfs ++ mk_bfs) & mk_graph_inputs
       ) ] ))

let plot () =
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         Chart_opt Chart.([Dimensions (10.,5.) ]);
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.) ] ]);
      Formatter my_formatter;
      Charts mk_sizes;
      Series (*  (mk_dfs ++ mk_bfs); *) (mk string "algo" "dfs_by_vertexid_array" ++ mk string "algo" "bfs_by_dual_arrays");
      X (mk_kind_for_size);
      Input (file_results name);
      Output (file_plots name);
      Y_label "exectime";
      Y eval_exectime
      ]))

let all () =
   select make run nothing plot

end


(*****************************************************************************)
(** OLD Parallel traversal algorithms *)

module ExpOldParallel = functor (P : sig val proc : int end) -> struct

let name = sprintf "parallel_%d" P.proc

let mk_idempotent =
   mk_list int "idempotent" arg_idempotent

let make () =
   build [prog_parallel]

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args (
           mk_parallel_prog P.proc
         & mk_idempotent
         & mk_graph_inputs
         & (    (mk_traversal_dfs & mk_parallel_dfs)
             ++ (mk_traversal_bfs & mk_parallel_bfs))
      )]))

let check () =
   Results.check_consistent_output_filter_by_params_from_file
      "result" mk_graph_files (file_results name)
   (* todo: check consistency with baseline by merging result files *)

let results_append rs e =
   ~~ List.map rs (fun (inputs,env) -> (inputs, Env.append env e))

let plot () =
   let results_all =
        Results.from_file (file_results name)
      @ Results.from_file (file_results ExpOldBaselines.name)
      (* (~~ XList.concat_map (Params.to_envs mk_idempotent) (fun e -> results_append (Results.from_file (file_results ExpOldBaselines.name)) e)) *)
      in
   (* TODO: allow other source for baseline *)
   let get_charts mk_charts mk_series (ylabel,yislog,eval) =
      Mk_bar_plot.(get_charts ([
         Bar_plot_opt Bar_plot.([
            Chart_opt Chart.([Dimensions (12.,8.) ]);
            X_titles_dir Vertical;
            Y_axis [ Axis.Lower (Some (if yislog then 0.01 else 0.));
                     Axis.Is_log yislog ]
            ]);
         Formatter my_formatter;
         Charts (mk_sizes & mk_charts);
         Series (mk_series);
         X mk_kind_for_size;
         Results results_all;
         Y_label ylabel;
         Y eval;
         ]))
         in
      let get_speedup mk_seq =
         let env_seq = Params.to_env mk_seq in
         let algo = Env.get_as_string env_seq "algo" in
         (("speedup vs " ^ algo), false (* true *), (eval_speedup mk_seq)) in
      let get_exectime = ("exectime", false, eval_exectime) in
      let charts =  (* ExpOldBaselines.mk_dfs ++  ExpOldBaselines.mk_bfs ++ *)
            get_charts (mk_idempotent & mk_traversal_dfs) (mk_parallel_dfs) (get_speedup ExpOldBaselines.mk_dfs)
          @ get_charts (mk_traversal_dfs) (mk_idempotent & mk_our_parallel_dfs) (get_speedup ExpOldBaselines.mk_dfs)
          @ get_charts (mk_idempotent & mk_traversal_bfs) (mk_parallel_bfs) (get_speedup ExpOldBaselines.mk_bfs)
          @ get_charts (mk_traversal_bfs) (mk_idempotent & mk_our_parallel_bfs) (get_speedup ExpOldBaselines.mk_bfs)
          @ get_charts (mk int "idempotent" 1) (mk_our_parallel_dfs ++ mk_our_parallel_bfs) (get_speedup ExpOldBaselines.mk_dfs)
          @ get_charts (mk_traversal_dfs) (mk_idempotent & mk_parallel_dfs) get_exectime
          @ get_charts (mk_traversal_bfs) (mk_idempotent & mk_parallel_bfs) get_exectime
         in
      Chart.build (file_plots name) charts

let all () =
   select make run check plot

end


(*****************************************************************************)
(** OLD Cutoff experiment *)

module ExpOldCutoff = struct

let name = "cutoff"

let mk_loop_cutoff =
   mk int "loop_cutoff" 1024

let edges_cutoffs = [128;256;512;1024;2048]

let vertices_cutoffs = [512;1024;2048;4096]

let experiments = [  (* todo; apply "mk_list int k vs" directly here *)
   ("our_pbfs_cutoff",
     (mk_algo "our_pbfs",
     "our_pbfs_cutoff", edges_cutoffs));
   ("our_pseudodfs_cutoff",
     (mk_algo "our_pseudodfs",
     "our_pseudodfs_cutoff", edges_cutoffs));
   ("ls_pbfs_cutoff_with_loop_cutoff_1024",
     (mk_algo_frontier "ls_pbfs" "ls_bag" & mk int "ls_pbfs_loop_cutoff" 1024,
     "ls_pbfs_cutoff", vertices_cutoffs));
   ("ls_loop_cutoff_with_pbfs_cutoff_512",
     (mk_algo_frontier "ls_pbfs" "ls_bag" & mk int "ls_pbfs_cutoff" 512,
     "ls_pbfs_loop_cutoff", edges_cutoffs));
   (* ("ls_chunkedbag_cutoff_with_loop_cutoff_1024",
     (mk_algo_frontier "ls_pbfs" "chunkedseq_bag" & mk int "loop_cutoff" 1024,
     "ls_pbfs_cutoff", vertices_cutoffs));*)
   ("pbfs_cutoff",
     (mk_algo "pbbs_pbfs",
     "loop_cutoff", vertices_cutoffs));
   (* ("cong_cutoff",
    (mk_algo_frontier "cong_pseudodfs" "stl_deque",
    "cong_pseudodfs_cutoff", [128;256;512;1014;2048;4096;8012])); *)
   ]

let experiments =
   match arg_exps with
   | ["all"] -> experiments
   | exps -> ~~ List.filter experiments (fun (name,_) -> List.mem name exps)

let mk_experiments =
  Params.concat (~~ List.map experiments (fun (name,(m,k,vs)) ->
      mk string "!exp_name" name
    & m
    & mk_list int k vs))

let mk_experiments_name =
   mk_list string "!exp_name" (XList.keys experiments)

let kvs_for_name name =
   let (m,k,vs) =
      try List.assoc name experiments
      with Not_found -> Pbench.error "failure in exp cutoff"
      in
   (k,vs)

let make () =
   build [prog_parallel]

let run () =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Args (
         mk_parallel_prog_maxproc
       & mk int "idempotent" 1
       & mk_graph_inputs
       & mk_experiments
     ) ] ))

let check () =
   Results.check_consistent_output_filter_by_params_from_file
      "result" mk_graph_files (file_results name)

let cutoff_formatter =
   let f = ~~ XList.assoc_replaces my_formatter_settings
      (List.map (fun k -> (k, Env.Format_key_eq_value))
        [ "ls_pbfs_cutoff"; "our_pbfs_cutoff"; "our_pbfs_cutoff";
          "cong_pseudodfs_cutoff"; "our_pseudodfs_cutoff" ])
    in
   Env.format f

let plot () =
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
      Formatter cutoff_formatter;
      Charts (mk_experiments_name & mk_sizes);
      Series (fun e ->
         let name = Env.get_as_string e "!exp_name" in
         let (k,vs) = kvs_for_name name in
         mk_list int k vs e);
      X mk_kind_for_size;
      Input (file_results name);
      Output (file_plots name);
      Y_label "exectime";
      Y eval_exectime;
      ]))

let all () =
   select make run check plot

end



(*****************************************************************************)
(** Main *)

let _ =
   let arg_actions = XCmd.get_others() in
   let bindings = [
      "generate", ExpGenerate.all;
      "accessible", ExpAccessible.all;
      "baselines", ExpBaselines.all;
      "work_efficiency", ExpWorkEfficiency.all;
      "lack_parallelism", ExpLackParallelism.all;
      "speedups_dfs", (let module E = ExpSpeedups(struct let key = "dfs" end) in E.all);
      "speedups_bfs", (let module E = ExpSpeedups(struct let key = "bfs" end) in E.all);
      "compare", ExpCompare.all;
      "overview", ExpOverview.all;
      "overheads", ExpOverheads.all;
      "idempotence", ExpIdempotence.all;
      "ligra", ExpLigra.all;
      "old_binaries", ExpOldBinaries.all;
      "old_sequential", ExpOldSequential.all;
      "old_parallel_baseline", ExpOldBaselines.all;
      "old_parallel", (let module E = ExpOldParallel(struct let proc = arg_proc end) in E.all);
      "old_parallel_8", (let module E = ExpOldParallel(struct let proc = 8 end) in E.all);
      "old_cutoff", ExpOldCutoff.all;
      ] in
   let selected ks =
      ~~ List.iter bindings (fun (k,f) -> if List.mem k ks then f()) in
   let _all () = selected (XList.keys bindings) in
   let paper () = selected [ "generate"; "baselines"; "overview" (*"work_efficiency"; "lack_parallelism";  "speedups_dfs"; "speedups_bfs";  *) ] in 
   Pbench.execute_from_only_skip arg_actions [] (bindings @ [
      (* "all", all; *)
      "paper", paper;
      ]);
   match arg_actions with
   | [] -> Pbench.warning "did no specify any experiment to perform"
   | ks -> let k = XList.last ks in if not (List.mem k ["generate"]) then begin
              (* let k = if k = "parallel" then "parallel_" ^ (string_of_int arg_proc) else k in *)
              let cmd1 = sprintf "cp %s plots.pdf" (file_plots k) in
              let cmd2 = sprintf "cp %s tables.pdf" (file_tables k) in
              ~~ List.iter [ cmd1; cmd2] (fun cmd ->
                 let s = XSys.command_as_bool cmd in
                 if s then Pbench.info cmd)
           end
