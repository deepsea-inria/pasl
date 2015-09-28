open XBase
open Params

let system = XSys.command_must_succeed_or_virtual

(*****************************************************************************)
(** Parameters *)

let arg_virtual_run = XCmd.mem_flag "virtual_run"
let arg_virtual_build = XCmd.mem_flag "virtual_build"
let arg_nb_runs = XCmd.parse_or_default_int "runs" 1
let arg_mode = "replace"   (* later: investigate the purpose of "mode" *)
let arg_skips = XCmd.parse_or_default_list_string "skip" []
let arg_onlys = XCmd.parse_or_default_list_string "only" []

let run_modes =
  Mk_runs.([
    Mode (mode_of_string arg_mode);
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

let build path bs is_virtual =
   system (sprintf "make -C %s -j %s" path (String.concat " " bs)) is_virtual

let file_results exp_name =
  Printf.sprintf "results_%s.txt" exp_name

let file_plots exp_name =

  Printf.sprintf "plots_%s.pdf" exp_name

(** Evaluation functions *)

let eval_exectime = fun env all_results results ->
  Results.get_mean_of "exectime" results

let eval_nb_operations_per_second = fun env all_results results ->
  let t = eval_exectime env all_results results in
  let nb_operations = Results.get_mean_of "nb_operations" results in
  nb_operations /. t

(*****************************************************************************)
(* Fixed constants *)

let mk_algos = mk_list string "algo" ["direct"; "portpassing";]

let dflt_snzi_branching_factor = 6
let dflt_snzi_nb_levels = 3

let mk_distributed_edge_algo =
    mk string "edge_algo" "distributed"
  & mk int "branching_factor" dflt_snzi_branching_factor
  & mk int "nb_levels" dflt_snzi_nb_levels

let mk_edge_algos =
     mk string "edge_algo" "simple"
  ++ mk_distributed_edge_algo
  ++ mk string "edge_algo" "dyntree"
  ++ mk string "edge_algo" "dyntreeopt"

(*****************************************************************************)
(** Incounter microbenchmark experiment *)

module ExpIncounterMicrobench = struct

let name = "incounter_microbench"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "incounter_microbench"

let mk_incr_probs =
     ((mk int "incr_prob_a" 1) & (mk int "incr_prob_b" 2))
  ++ ((mk int "incr_prob_a" 2) & (mk int "incr_prob_b" 3))
  ++ ((mk int "incr_prob_a" 9) & (mk int "incr_prob_b" 10))

let nb_milliseconds_target = 1000

let mk_nb_milliseconds = mk int "nb_milliseconds" nb_milliseconds_target

let mk_seed = mk int "seed" 1234

let mk_branching_factors = mk_list int "branching_factor" [dflt_snzi_branching_factor;4]

let mk_nb_levels = mk_list int "nb_levels" [2;dflt_snzi_nb_levels;]

let mk_incounters =
      mk string "incounter" "simple"
   ++ (mk string "incounter" "snzi" & mk_branching_factors & mk_nb_levels)
   ++ mk string "incounter" "dyntree"
   ++ mk string "incounter" "dyntreeopt"

let mk_proc = mk_list int "proc" [1;40]

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_incr_probs
    & mk_cmd
    & mk_nb_milliseconds
    & mk_seed
    & mk_incounters
    & mk_proc)]))

let check = nothing  (* do something here *)

let incounter_microbench_formatter =
 Env.format (Env.(
    [ (*("n", Format_custom (fun n -> sprintf "fib(%s)" n)); *) ]
  ))

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter incounter_microbench_formatter;
      Charts mk_incr_probs;
      Series mk_incounters;
      X mk_proc;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Outset microbenchmark experiment *)

module ExpOutsetMicrobench = struct

let name = "outset_microbench"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "outset_microbench"

let nb_milliseconds_target = 1000

let mk_nb_milliseconds = mk int "nb_milliseconds" nb_milliseconds_target

let mk_seed = mk int "seed" 1234

let mk_outsets =
      mk string "outset" "simple"
   ++ mk string "outset" "dyntree"
   ++ mk string "outset" "dyntreeopt"

let mk_proc = mk_list int "proc" [1;40]

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_cmd
    & mk_nb_milliseconds
    & mk_seed
    & mk_outsets
    & mk_proc)]))

let check = nothing  (* do something here *)

let outset_microbench_formatter =
 Env.format (Env.(
    [ (*("n", Format_custom (fun n -> sprintf "fib(%s)" n)); *) ]
  ))

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter outset_microbench_formatter;
      Charts mk_unit;
      Series mk_outsets;
      X mk_proc;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Async microbenchmark experiment *)

module ExpAsyncMicrobench = struct

let name = "async_microbench"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "async_microbench"

let nb_milliseconds_target = 1000

let mk_nb_milliseconds = mk int "nb_milliseconds" nb_milliseconds_target

let mk_seed = mk int "seed" 1234

let mk_proc = mk_list int "proc" [1;40]

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_cmd
    & mk_algos
    & mk_nb_milliseconds
    & mk_seed
    & mk_edge_algos
    & mk_proc)]))

let check = nothing  (* do something here *)

let outset_microbench_formatter =
 Env.format (Env.(
    [ (*("n", Format_custom (fun n -> sprintf "fib(%s)" n)); *) ]
  ))

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter outset_microbench_formatter;
      Charts mk_unit;
      Series mk_edge_algos;
      X mk_proc;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_asyncs/second";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Edge-throughput microbenchmark experiment *)

module ExpEdgeThroughputMicrobench = struct

let name = "edge_throughput_microbench"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "edge_throughput_microbench"

let nb_milliseconds_target = 1000

let mk_nb_milliseconds = mk int "nb_milliseconds" nb_milliseconds_target

let mk_seed = mk int "seed" 1234

let mk_proc = mk_list int "proc" [1;40]

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_cmd
    & mk_algos
    & mk_nb_milliseconds
    & mk_seed
    & mk_edge_algos
    & mk_proc)]))

let check = nothing  (* do something here *)

let outset_microbench_formatter =
 Env.format (Env.(
    [ (*("n", Format_custom (fun n -> sprintf "fib(%s)" n)); *) ]
  ))

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter outset_microbench_formatter;
      Charts mk_unit;
      Series mk_edge_algos;
      X mk_proc;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_edges/second";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Gaus-Seidel benchmark experiment *)

module ExpSeidelMicrobench = struct

let name = "seidel"

let prog = "./bench.opt"

let mk_pipeline_arguments =
    mk int "pipeline_window_capacity" 128
  & mk int "pipeline_burst_rate" 32

let mk_cmd =
     mk string "cmd" "seidel_sequential"
  ++ (mk string "cmd" "seidel_parallel" & mk_pipeline_arguments)

let nb_milliseconds_target = 1000

let mk_proc = mk_list int "proc" [1;40]

let mk_N = mk int "N" 10000

let mk_numiters = mk int "numiters" 4

let mk_block_sizes = mk_list int "block_size" [2;32;64]

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_cmd
    & mk_N
    & mk_numiters
    & mk_block_sizes
    & mk_algos
    & mk_edge_algos
    & mk_proc)]))

let check = nothing  (* do something here *)

let outset_microbench_formatter =
 Env.format (Env.(
    [ (*("n", Format_custom (fun n -> sprintf "fib(%s)" n)); *) ]
  ))

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter outset_microbench_formatter;
      Charts mk_block_sizes;
      Series (mk_cmd ++ mk_edge_algos);
      X mk_proc;
      Input (file_results name);
      Output (file_plots name);
      Y_label "running time (seconds)";
      Y eval_exectime;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "incounter_microbench",           ExpIncounterMicrobench.all;
    "outset_microbench",              ExpOutsetMicrobench.all;
    "async_microbench",               ExpAsyncMicrobench.all;
    "edge_throughput_microbench",     ExpEdgeThroughputMicrobench.all;
    "seidel",                         ExpSeidelMicrobench.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
