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

module ExpCS = struct

let version = XCmd.parse_string "version"

let baseline = XCmd.parse_string "baseline"

let extension = match version with
                | "slow" -> "virtual"
                | _ -> "virtual2"

let baseline_type = match baseline with
                   | "self" -> 0
                   | _ -> 2

let name = "construction_speedup"

let make () =
  build "." ["rake-compress-construction.opt";"rake-compress-construction.opts"] arg_virtual_build

let mk_baseline = (mk_prog ("rake-compress-construction." ^ extension)) & (mk int "seq" baseline_type)
let mk_main = (mk_prog ("rake-compress-construction." ^ extension)) & (mk int "seq" 0)
let mk_n = mk int "n" 1000000
let mk_graphs = (((mk string "graph" "binary_tree") & (mk_list float "fraction" [-1.0])) ++
    ((mk string "graph" "random") & (mk_list float "fraction" [0.0; 0.3; 0.6; 1.0])))
let mk_proc = mk_list int "proc" [1; 2; 4; 8; 10; 12; 15; 20; 25; 30; 39]

let run () =
  let skip_baseline = XCmd.mem_flag "skip_baseline" in
  if not skip_baseline then begin
     Mk_runs.(call [
        Runs 3;
        Output ("plots/construction/results_baseline_" ^ baseline ^ "_" ^ version ^ ".txt");
        Args (mk_baseline & mk_n & mk_graphs)
        ]);
  end;

  let skip_main = XCmd.mem_flag "skip_main" in
  if not skip_main then begin
    Mk_runs.(call [
      Runs 3;
      Output ("plots/construction/results_main_" ^ version ^ ".txt");
      Args (mk_main & mk_n & mk_proc & mk_graphs)
      ]);
  end

let check = nothing

let plot () =
  let results_baselines = Results.from_file ("plots/construction/results_baseline_" ^ baseline ^ "_" ^ version ^ ".txt") in
  
  let formatter = 
    Env.format (Env.(
    [ ("graph", Format_custom (fun graph ->
        match graph with
        | "binary_tree" -> "binary tree"
        | "random" -> "random tree"
        | _ -> ""
      ));
      ("fraction", Format_custom (fun fraction ->
        if float_of_string(fraction) < 0.0 then "" else (sprintf "%s" fraction)
      ))
    ])) in

  let eval_relative = fun env all_results results ->
    let baseline_results =  ~~ Results.filter_by_params results_baselines (
         from_env (Env.filter_keys ["graph"; "fraction"] env)
         (* here we select only the baseline runs with matching n and m arguments *)
       ) in
    if baseline_results = [] then Pbench.warning ("no results for baseline: " ^ Env.to_string env);
    let v = Results.get_mean_of "exectime" results in
    let b = Results.get_mean_of "exectime" baseline_results in
    v /. b
    in
    Mk_scatter_plot.(call ([
      Scatter_plot_opt Scatter_plot.([
        Draw_lines true; 
        X_axis [Axis.Label "Number of processors"];
        Y_axis [Axis.Lower (Some 0.)]
      ]);
      Formatter formatter;
      Charts mk_unit;
      Series mk_graphs;
      X mk_proc;
      Y eval_relative;
      Y_label "relative execution time w.r.t. fastest parallel algorithm";
      Input ("plots/construction/results_main_" ^ version ^ ".txt");
      Output ("plots/construction/plot_" ^ version ^ "_" ^ baseline ^ ".pdf")
      ]))


let all () = select make run check plot

end

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "construction_speedup",                 ExpCS.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()