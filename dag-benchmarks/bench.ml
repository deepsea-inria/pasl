open XBase
open Params

let system = XSys.command_must_succeed_or_virtual

(*****************************************************************************)
(** Parameters *)

let arg_virtual_run = XCmd.mem_flag "virtual_run"
let arg_virtual_build = XCmd.mem_flag "virtual_build"
let arg_nb_runs = XCmd.parse_or_default_int "runs" 1
let arg_mode = "normal"   (* later: investigate the purpose of "mode" *)
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
  let nb_proc = Env.get_as_float env "proc" in
  let nb_operations_per_proc = nb_operations /. nb_proc in
  nb_operations_per_proc /. t
                     
let eval_nb_operations_per_second_error = fun env all_results results ->
  let ts = Results.get Env.as_float "exectime" results in
  let nb_operations = Results.get Env.as_float "nb_operations" results in
  let nb_proc = Env.get_as_float env "proc" in
  let nb_operations_per_proc = List.map (fun nb_operations -> nb_operations /. nb_proc) nb_operations in  
  let ps = List.map (fun (x, y) -> x /. y) (List.combine nb_operations_per_proc ts) in
  let (_, stddev) = XFloat.list_mean_and_stddev ps in
  stddev

(*****************************************************************************)
(* Fixed constants *)

let dflt_snzi_branching_factor = 4
let dflt_snzi_nb_levels = 3

let mk_snzi_branching_factors = mk_list int "branching_factor" [dflt_snzi_branching_factor]
let mk_snzi_nb_levels = mk_list int "nb_levels" [dflt_snzi_nb_levels;]

let mk_simple_edge_algo = mk string "edge_algo" "simple"

let mk_distributed_edge_algo =
  mk string "edge_algo" "distributed"

let mk_dyntree_algo = mk string "edge_algo" "dyntree"

let mk_dyntreeopt_algo = mk string "edge_algo" "dyntreeopt"

let mk_edge_algos =
     mk_simple_edge_algo
  ++ mk_distributed_edge_algo
  ++ mk_dyntree_algo
  ++ mk_dyntreeopt_algo

let mk_direct_algo = mk string "algo" "direct"
       
let mk_direct_algos = mk_direct_algo & mk_edge_algos

let mk_portpassing_algo = mk string "algo" "portpassing"
       
let mk_algos =
     mk_direct_algos
  ++ mk_portpassing_algo
        
let nb_milliseconds_target = 1000
let mk_nb_milliseconds = mk int "nb_milliseconds" nb_milliseconds_target

let mk_proc = mk_list int "proc" [1;40]

let mk_seed = mk int "seed" 1234

let mk_incr_prob (a, b) =
  (mk int "incr_prob_a" a) & (mk int "incr_prob_b" b)

let incr_probs = [(1,2); (2,3); (9,10); (1,1);]

let mk_incr_probs =
  let mks = List.map mk_incr_prob incr_probs in
  List.fold_left (++) (List.hd mks) (List.tl mks)

let mk_incounter_mixed_duration =
    mk string "cmd" "incounter_mixed_duration"
  & mk_incr_probs
  & mk_nb_milliseconds
      
let mk_incounter_async_duration =
    mk string "cmd" "incounter_async_duration"
  & mk_nb_milliseconds

let mk_incounter_async_nb =
    mk string "cmd" "incounter_async_nb"
  & mk int "n" 10000000

let microbench_formatter =
  Env.format (Env.(
    [
      ("branching_factor", Format_custom (fun n -> sprintf "B=%s" n));
      ("nb_levels", Format_custom (fun n -> sprintf "D=%s" n));
      ("algo", Format_custom (fun algo -> sprintf "%s" (if algo = "portpassing" then algo else "")));
      ("edge_algo", Format_custom (fun edge_algo -> sprintf "%s" edge_algo));
      ("cmd", Format_custom (fun cmd -> sprintf "%s" cmd));
    ]
  ))                
         
(*****************************************************************************)
(** Incounter-tune experiment *)

module ExpIncounterTune = struct

let name = "incounter_tune"

let parameters = [  (4,1); (4,4); (4,8); (4,32); (4,128); (4,256); (4,512); (8,32); ]
(*(2,1); (2,8); (2,32);*)
                   
let prog_of (branching_factor, amortization_factor) =
  "./bench.opt_" ^ (string_of_int branching_factor) ^ "_" ^ (string_of_int amortization_factor)
             
let progs = List.map prog_of parameters
           
let mk_progs =
  mk_list string "prog" progs

let mk_all_benchmarks =
     mk_incounter_mixed_duration
  ++ mk_incounter_async_duration
  ++ mk_incounter_async_nb

let make() =
  build "." progs arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_progs
    & mk_all_benchmarks
    & mk_seed
    & mk string "algo" "direct"
    & mk string "edge_algo" "dyntreeopt"
    & mk_proc)]))

let check = nothing  (* do something here *)
           
let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_proc;
      Series mk_progs;
      X mk_all_benchmarks;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]))
                
let all () = select make run check plot

end

(*****************************************************************************)
(** Scalability experiment *)

module ExpScalability = struct

let name = "scalability"

let prog = "./bench.opt"

let make() =
  build "." [prog] arg_virtual_build

let mk_all_benchmarks =
  (   mk_incounter_async_duration
      ++ mk_incounter_async_nb)
  & mk_direct_algo & mk_edge_algos

let mk_procs = mk_list int "proc" [1;4;16;40;]

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_all_benchmarks
    & mk_seed
    & mk_procs)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
      Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         (*         X_titles_dir Vertical;*)
         Y_axis [Axis.Lower (Some 0.); Axis.Is_log true;] ]);
       Formatter microbench_formatter;
      Charts  (      mk_incounter_async_duration
                  ++ mk_incounter_async_nb);
      Series mk_edge_algos;
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      (*      Y_whiskers eval_nb_operations_per_second_error;*)
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** SNZI-tune experiment *)

module ExpSNZITune = struct

let name = "snzi_tune"

let prog = "./bench.opt"
             
let branching_factors = [2;4]
let nb_levels = [2;3;5;]

let mk_configurations =
    (mk_list int "branching_factor" branching_factors)
  & (mk_list int "nb_levels" nb_levels)
          
let mk_cmd = mk string "cmd" "incounter_mixed_duration"

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
    & mk_incounter_mixed_duration
    & mk_configurations
    & mk_proc)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_proc;
      Series mk_configurations;
      X mk_incr_probs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Incounter microbenchmark experiment *)

module ExpIncounterMixedDuration = struct

let name = "incounter_mixed_duration"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "incounter_mixed_duration"

let mk_incounters =
      mk string "incounter" "simple"
   ++ (mk string "incounter" "snzi" & mk_snzi_branching_factors & mk_snzi_nb_levels)
   ++ mk string "incounter" "dyntree"
   ++ mk string "incounter" "dyntreeopt"

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

let eval_nb_operations_per_phase_per_second = fun phase env all_results results ->
  let t = Results.get_mean_of ("exectime_"^phase) results in
  let nb_operations = Results.get_mean_of ("nb_operations_"^phase) results in
  let nb_proc = Env.get_as_float env "proc" in
  let nb_operations_per_proc = nb_operations /. nb_proc in
  nb_operations_per_proc /. t
            
let plot_phase phase =
  let name2 = name^"_"^phase in
    Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_proc;
      Series mk_incounters;
      X mk_incr_probs;
      Input (file_results name);
      Output (file_plots name2);
      Y_label "nb_operations/second (per thread)";
      Y (eval_nb_operations_per_phase_per_second phase);
  ]))
            
let plot() =
  begin
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_proc;
      Series mk_incounters;
      X mk_incr_probs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]));
  plot_phase "phase1";
  plot_phase "phase2";
  end

let all () = select make run check plot

end

(*****************************************************************************)
(** Outset microbenchmark experiment *)

module ExpOutsetAddDuration = struct

let name = "outset_add_duration"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "outset_add_duration"

let mk_outsets =
      mk string "outset" "simple"
   ++ mk string "outset" "dyntree"
   ++ mk string "outset" "dyntreeopt"

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

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_proc;
      Series mk_outsets;
      X mk_unit;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Async microbenchmark experiment *)

module ExpIncounterAsyncDuration = struct

let name = "incounter_async_duration"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "incounter_async_duration"

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
    & mk_proc)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_proc;
      Series mk_algos;
      X mk_unit;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Edge-throughput microbenchmark experiment *)

module ExpMixedDuration = struct

let name = "mixed_duration"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "mixed_duration"

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
    & mk_proc)]))

let check = nothing  (* do something here *)
            
let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Chart_opt Chart.([Dimensions (10.,8.) ]);
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_proc;
      Series mk_algos;
      X mk_unit;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Gaus-Seidel benchmark experiment *)

module ExpSeidel = struct

let name = "seidel"

let prog = "./bench.opt"

let mk_pipeline_arguments =
    mk int "pipeline_window_capacity" 128
  & mk int "pipeline_burst_rate" 32

let mk_cmd =
     mk string "cmd" "seidel_sequential"
  ++ (mk string "cmd" "seidel_parallel" & mk_pipeline_arguments)

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
              
let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
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

                 (*
prun ./bench.sta -cmd incounter_async_nb -algo direct -edge_algo dyntreeopt -n 500000 -seed 1234 -proc 1,2,4,8,10,20,30,40 -workload 2000.0
                      *)

(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "incounter_tune",                 ExpIncounterTune.all;
    "snzi_tune",                      ExpSNZITune.all;
    "scalability",                    ExpScalability.all;
    "incounter_mixed_duration",       ExpIncounterMixedDuration.all;
    "outset_add_duration",            ExpOutsetAddDuration.all;
    "incounter_async_duration",       ExpIncounterAsyncDuration.all;
    "mixed_duration",                 ExpMixedDuration.all;
    "seidel",                         ExpSeidel.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
