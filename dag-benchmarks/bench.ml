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

(*****************************************************************************)
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

let path_to_openstream = "/home/rainey/open-stream"

let thehostname = Unix.gethostname()

let is_cadmium _ = thehostname = "cadmium"

let all_procs =
  if is_cadmium()
  then [1;10;20;30;40;48]
  else [1;10;20;30;40;]

let mk_procs = mk_list int "proc" all_procs
                       
let max_proc = XList.last all_procs
               
let mk_simple_edge_algo = mk string "edge_algo" "simple"

let mk_statreeopt_edge_algo_with_no_defaults =
  mk string "edge_algo" "statreeopt"

let mk_statreeopt_edge_algo =
    mk_statreeopt_edge_algo_with_no_defaults

                                 
let mk_growabletree_edge_algo = mk string "edge_algo" "growabletree"

let mk_edge_algos =
     mk_simple_edge_algo
  ++ mk_statreeopt_edge_algo
  ++ mk_growabletree_edge_algo

let mk_direct_algo = mk string "algo" "direct"
       
let mk_direct_algos = mk_direct_algo & mk_edge_algos

let mk_portpassing_algo = mk string "algo" "portpassing"
       
let mk_algos =
     mk_direct_algos
(*  ++ mk_portpassing_algo*)
        
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

let mk_workload = mk int "workload" 2000
                     
let incounter_nb = 10000000

let mk_incounter_nb = mk int "n" incounter_nb

let mk_incounter_async_duration_base =
    mk string "cmd" "incounter_async_duration"
  & mk_nb_milliseconds

let mk_incounter_async_duration =
    mk_incounter_async_duration_base
  & mk_workload
      
let mk_incounter_async_nb_base =
    mk string "cmd" "incounter_async_nb"
  & mk_incounter_nb

let mk_incounter_async_nb =
    mk_incounter_async_nb_base
  & mk_workload
      
let mk_incounter_forkjoin_nb_base =
    mk string "cmd" "incounter_forkjoin_nb"
  & mk_incounter_nb

let mk_incounter_forkjoin_nb =
    mk_incounter_forkjoin_nb_base
  & mk_workload

let mk_mixed_nb_base =
    mk string "cmd" "mixed_nb"
  & mk_incounter_nb

let mk_mixed_nb =
    mk_mixed_nb_base
  & mk_workload
      
let mk_mixed_duration_base =
    mk string "cmd" "mixed_duration"
  & mk_nb_milliseconds

let mk_mixed_duration =
    mk_mixed_duration_base
  & mk_workload
      
let pretty_edge_algo edge_algo =
  match edge_algo with
  | "simple" -> "simple serial"
  | "statreeopt" -> "our fixed-size SNZI incounter + our outset"
  | "growabletree" -> "our growable SNZI incounter + our outset"
  | "single_buffer" -> "single buffer"
  | "perprocessor" -> "per-processor buffers"
  | _ -> "<unknown>"

let pretty_snzi s =
  match s with
  | "fixed" -> "our fixed-size SNZI tree"
  | "growable" -> "our growable SNZI tree"
  | "single_cell" -> "single-cell fetch-and-add counter"
  | _ -> "<unknown>"
           
let microbench_formatter =
  Env.format (Env.(
    [
      ("branching_factor", Format_custom (fun n -> sprintf "B=%s" n));
      ("nb_levels", Format_custom (fun n -> sprintf "D=%s" n));
      ("algo", Format_custom (fun algo -> sprintf "%s" (if algo = "portpassing" then algo else "")));
      ("edge_algo", Format_custom pretty_edge_algo);
      ("cmd", Format_custom (fun cmd -> sprintf "%s" cmd));
      ("snzi", Format_custom pretty_snzi);
    ]
  ))                
         
(*****************************************************************************)
(** SNZI-tune experiment *)

module ExpSNZITune = struct

let name = "snzi_tune"

let heights = XList.init 7 (fun i -> i)
                   
let prog_of height =
  "./bench.opt_snzi_" ^ (string_of_int height)
             
let progs = List.map prog_of heights
           
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
    & mk string "edge_algo" "statreeopt"
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
(** Incounter microbenchmark experiment *)

module ExpIncounterMixedDuration = struct

let name = "incounter_mixed_duration"

let prog = "./bench.opt"

let mk_edge_algos =
      mk_simple_edge_algo
   ++ mk_statreeopt_edge_algo

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_incounter_mixed_duration
    & mk_seed
    & mk_edge_algos
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
      Series mk_edge_algos;
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
      Series mk_edge_algos;
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
(** SNZI microbenchmark experiment *)

module ExpSNZIAlternatedDuration = struct

let name = "snzi_alternated_duration"

let prog = "./bench.opt"

let mk_snzis =
  mk_list string "snzi" ["fixed"; "growable"; "single_cell";]

let mk_snzi_alternated_mixed_duration =
    mk string "cmd" name
  & mk_nb_milliseconds

let make() =
  build "." [prog] arg_virtual_build

let all_procs = XList.init (max_proc / 2) (fun i -> (i+1)*2)
let mk_procs = mk_list int "proc" all_procs
        
let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_snzi_alternated_mixed_duration
    & mk_seed
    & mk_snzis
    & mk_procs)]))

let check = nothing  (* do something here *)            
           
let plot() = begin
    Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Top_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         Y_axis [Axis.Lower (Some 0.); Axis.Upper (Some 10.0e6); Axis.Is_log false;] ]);
       Formatter microbench_formatter;
       Charts mk_unit;
      Series mk_snzis;
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
  ]));
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_unit;
      Series mk_snzis;
      X mk_procs;
      Input (file_results name);
      Output (file_plots (name^"_barplot"));
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]))
  end
    
let all () = select make run check plot

end
                                     
(*****************************************************************************)
(** Outset microbenchmark experiment *)

module ExpOutsetAddDuration = struct

let name = "outset_add_duration"

let prog = "./bench.opt"

let mk_cmd = mk string "cmd" "outset_add_duration"

let mk_edge_algos = mk_list string "edge_algo" ["simple"; "growabletree"; "perprocessor"; "single_buffer";]

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
    & mk_edge_algos
    & mk_procs)]))

let check = nothing  (* do something here *)

let plot() =
begin
    Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Bottom_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         Y_axis [Axis.Lower (Some 0.); Axis.Upper (Some 10.0e6); Axis.Is_log true;] ]);
       Formatter microbench_formatter;
       Charts mk_unit;
      Series mk_edge_algos;
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
  ]));
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter microbench_formatter;
      Charts mk_unit;
      Series mk_edge_algos;
      X mk_procs;
      Input (file_results name);
      Output (file_plots (name^"_barplot"));
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
      Y_whiskers eval_nb_operations_per_second_error;
  ]))
  end

let all () = select make run check plot

end

(*****************************************************************************)
(** Async microbenchmark experiment *)

module ExpIncounterAsyncDuration = struct

let name = "incounter_async_duration"

let prog = "./bench.opt"

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_algos
    & mk_incounter_async_duration
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

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & mk_mixed_duration
    & mk_algos
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
(** Scalability experiment *)

module ExpScalability = struct

let name = "scalability"

let prog = "./bench.opt"

let make() =
  build "." [prog] arg_virtual_build

let mk_workloads = mk_list int "workload" [0;]
(*(XList.init 4 (fun i -> i * 500)) *)
        
let mk_all_benchmarks =
     (mk_incounter_async_duration_base & mk_workloads)
  ++ (mk_incounter_async_nb_base & mk_workloads)
  ++ (mk_mixed_duration_base & mk_workloads)
  ++ (mk_mixed_nb_base & mk_workloads)
       
let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & (mk_all_benchmarks & mk_direct_algo & mk_edge_algos)
    & mk_seed
    & mk_procs)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Bottom_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         Y_axis [Axis.Lower (Some 0.); Axis.Is_log true;] ]);
       Formatter microbench_formatter;
       Charts mk_all_benchmarks;
      Series mk_edge_algos;
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Async/finish versus fork/join experiment *)

module ExpAsyncFinishVersusForkJoin = struct

let name = "async_finish_versus_fork_join"

let prog = "./bench.opt"

let make() =
  build "." [prog] arg_virtual_build

let mk_cmds = mk_incounter_async_nb ++ mk_incounter_forkjoin_nb
        
let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 1000;
    Args (
      mk_prog prog
    & (mk_cmds & mk_direct_algo & mk_growabletree_edge_algo)
    & mk_seed
    & mk_procs)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Bottom_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         Y_axis [Axis.Lower (Some 0.); Axis.Is_log true;] ]);
       Formatter microbench_formatter;
       Charts mk_unit;
      Series mk_cmds;
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Gauss-Seidel experiment *)

module ExpSeidel = struct

let name = "seidel"

let prog = "./seidel.virtual"

let mk_proc = mk int "proc" max_proc
             
let mk_params_baseline = Params.(mk string "prun_speedup" "baseline")
let mk_params_parallel = Params.(mk string "prun_speedup" "parallel")

let mk_system_pasl =
  mk string "system" "pasl"

let mk_seidel_async =
    mk string "cmd" "seidel_async"
  & mk_system_pasl
     
let mk_seidel_cilk =
  mk string "system" "cilk"

let mk_seidel_openstream =
  mk string "system" "openstream"
     
let doit id (mk_numiters, mk_seidel_params) =
  let name = name^"_"^id in
  
  let mk_seidel_config =
      mk_prog prog
    & mk_numiters
    & mk_seidel_params
  in
  
  let mk_seidel_sequential =
      mk_seidel_config
    & mk string "cmd" "seidel_sequential"
    & mk_system_pasl
    & mk_params_baseline
  in
  
  let mk_parallel_shared =
      mk_seidel_config
    & mk_proc
    & mk_params_parallel
  in
        
  let mk_parallels =
      mk_parallel_shared
    & ( (mk_seidel_async & mk_algos) ++ mk_seidel_cilk ++ mk_seidel_openstream )
  in
  
  let path_to_openstream_seidel = path_to_openstream ^ "/examples/seidel" in
                             
  let make() = (
    build "." [prog] arg_virtual_build;
    build path_to_openstream_seidel ["stream_seidel";"cilk_seidel";] arg_virtual_build;
  )
  in
  
  let run() =
    Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Timeout 1000;
      Args (mk_seidel_sequential ++ mk_parallels)]))
  in
              
  let check = nothing  (* do something here *)
  in

  let formatter =
    Env.format (Env.(
    [
      ("branching_factor", Format_custom (fun n -> sprintf "B=%s" n));
      ("nb_levels", Format_custom (fun n -> sprintf "D=%s" n));
      ("algo", Format_custom (fun algo -> sprintf "%s" (if algo = "portpassing" then algo else "")));
      ("edge_algo", Format_custom pretty_edge_algo);
      ("N", Format_custom (fun n -> sprintf "size %s" n));
      ("block_size_lg", Format_custom (fun n -> sprintf "tile %d" (1 lsl (int_of_string n))));
      ("system", Format_custom (fun n -> sprintf "%s" n));
    ]
  ))                
  in
  
  let plot() =
    let eval_y env all_results results = 
      let results = ~~ Results.filter_by_params results mk_params_parallel in
      let baseline_results = ~~ Results.filter_by_params all_results mk_params_baseline in
      let baseline_env = ~~ Env.filter env (fun k -> List.mem k ["N";"numiters";"block_size_lg";]) in
      let baseline_results = ~~ Results.filter baseline_results baseline_env in
      if baseline_results = [] then Pbench.warning ("no results for baseline: " ^ Env.to_string env);
      let tp = Results.get_mean_of "exectime" results in
      let tb = Results.get_mean_of "exectime" baseline_results in
      tb /. tp 
      in
      Mk_scatter_plot.(call ([
      Chart_opt Chart.([
        Legend_opt Legend.([Legend_pos Bottom_right]);
        ]);
        Scatter_plot_opt Scatter_plot.([
           Draw_lines true; 
           Y_axis [Axis.Lower (Some 0.); Axis.Is_log true;] ]);
         Formatter formatter;
        Charts (mk_seidel_params);
        Series ((mk_seidel_async & mk_algos) ++ mk_seidel_cilk ++ mk_seidel_openstream);
        X mk_numiters;
        Input (file_results name);
        Output (file_plots name);
        Y_label "speedup";
        Y (eval_y);
    ]))
  in
  
  select make run check plot

let mk_seidel_params_small =
  let mk_numiters =
    let nb = 20 in
    let ns = XList.init nb (fun i -> (i+1) * 100) in
    mk_list int "numiters" ns
  in
  (mk_numiters, (mk int "N" 256) & (mk int "block_size_lg" 6))
      
let mk_seidel_params_large =
  let mk_numiters =
    let nb = 20 in
    let ns = XList.init nb (fun i -> (i+1) * 10) in
    mk_list int "numiters" ns
  in
  (mk_numiters, (mk int "N" 1024) & (mk int "block_size_lg" 7))

let all () = (
  doit "small" mk_seidel_params_small;
  doit "large" mk_seidel_params_large)
                      
end

(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "snzi_tune",                      ExpSNZITune.all;
    "scalability",                    ExpScalability.all;
    "incounter_mixed_duration",       ExpIncounterMixedDuration.all;
    "outset_add_duration",            ExpOutsetAddDuration.all;
    "incounter_async_duration",       ExpIncounterAsyncDuration.all;
    "mixed_duration",                 ExpMixedDuration.all;
    "async_finish_versus_fork_join",  ExpAsyncFinishVersusForkJoin.all;
    "seidel",                         ExpSeidel.all;
    "snzi_alternated_duration",       ExpSNZIAlternatedDuration.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
