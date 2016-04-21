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

let version = XCmd.parse_or_default_string "version" "fast"

let baseline = XCmd.parse_or_default_string "baseline" "self"

let n = XCmd.parse_or_default_int "n" 1000000

let extension = match version with
                | "slow" -> "virtual"
                | _ -> "virtual2"

let baseline_type = match baseline with
                   | "self" -> 0
                   | _ -> 2

let size = Printf.sprintf "%d" n

let name = "construction_speedup"

let make () =
  build "." ["rake-compress-construction.opt";"rake-compress-construction.opts"] arg_virtual_build

let mk_baseline = (mk_prog ("rake-compress-construction." ^ extension)) & (mk int "seq" baseline_type)
let mk_main = (mk_prog ("rake-compress-construction." ^ extension)) & (mk int "seq" 0)
let mk_n = mk int "n" n
let mk_graphs = (((mk string "graph" "binary_tree") & (mk_list float "fraction" [-1.0])) ++
    ((mk string "graph" "random") & (mk_list float "fraction" [0.0; 0.3; 0.6; 1.0])))
let mk_proc = mk_list int "proc" [1; 2; 4; 8; 10; 12; 15; 20; 25; 30; 39]

let run () =
  let skip_baseline = XCmd.mem_flag "skip_baseline" in
  if not skip_baseline then begin
     Mk_runs.(call [
        Runs 3;
        Output ("plots/construction/results_baseline_" ^ baseline ^ "_" ^ version ^ "_" ^ size ^ ".txt");
        Args (mk_baseline & mk_n & mk_graphs)
        ]);
  end;

  let skip_main = XCmd.mem_flag "skip_main" in
  if not skip_main then begin
    Mk_runs.(call [
      Runs 3;
      Output ("plots/construction/results_main_" ^ version ^ "_" ^ size ^ ".txt");
      Args (mk_main & mk_n & mk_proc & mk_graphs)
      ]);
  end

let check = nothing

let plot () =
  let results_baselines = Results.from_file ("plots/construction/results_baseline_" ^ baseline ^ "_" ^ version ^ "_" ^ size ^ ".txt") in
  
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
    b /. v
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
      Y_label "Speedup";
      Input ("plots/construction/results_main_" ^ version ^ "_" ^ size ^ ".txt");
      Output ("plots/construction/plot_" ^ version ^ "_" ^ baseline ^ "_" ^ size ^ ".pdf")
      ]))
(* Construction Run-time / Batch Run-time *)

let all () = select make run check plot

end

module ExpCS2 = struct

let version = XCmd.parse_or_default_string "version" "fast"

let extension = match version with
                | "slow" -> "virtual"
                | _ -> "virtual2"

let name = "construction_comparison"

let make () =
  build "." ["rake-compress-construction.opts";"rake-compress-update.opts"] arg_virtual_build

let mk_ns = mk_list int "n" [100000;200000;400000;800000;1600000;2400000;3200000]
let mk_progs = (mk_prog ("rake-compress-construction." ^ extension)) & (mk_list int "seq" [0;2])
let mk_graphs = (((mk string "graph" "binary_tree") & (mk_list float "fraction" [-1.0])) ++
    ((mk string "graph" "random") & (mk_list float "fraction" [0.0; 0.3; 0.6; 1.0])))

let run () =
  Mk_runs.(call [
    Runs 3;
    Output ("plots/construction_sizes/results_" ^ version ^ ".txt");
    Args (mk_progs & mk_ns & mk_graphs)
    ])

let check = nothing

let plot () =
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
      ));
      ("prog", Format_hidden);
      ("type", Format_hidden);
      ("seq", Format_custom (fun seq ->
        match seq with
        | "0" -> "Construction Algorithm"
        | _ -> "Static Algorithm"
      ))
    ])) in

  Mk_scatter_plot.(call ([
    Scatter_plot_opt Scatter_plot.([
      Draw_lines true; 
      X_axis [Axis.Label "Number of vertices in forest"];
      Y_axis [Axis.Lower (Some 0.)]
    ]);
    Formatter formatter;
    Charts mk_graphs;
    Series mk_progs;
    X mk_ns;
    Y_label "Construction Run-time and Static Run-time";
    Input ("plots/construction_sizes/results_" ^ version ^ ".txt");
    Output ("plots/construction_sizes/plot_" ^ version ^ ".pdf")
    ] @ (y_as_mean "exectime")
  ))

let all () = select make run check plot

end

module ExpCC = struct

let version = XCmd.parse_or_default_string "version" "fast"

let extension = match version with
                | "slow" -> "virtual"
                | _ -> "virtual2"

let name = "construction_comparison"

let make () =
  build "." ["rake-compress-construction.opts";"rake-compress-update.opts"] arg_virtual_build

let mk_n = mk int "n" 1000000
let mk_construction = ((mk_prog ("rake-compress-construction." ^ extension)) & (mk_list int "seq" [2;0]))
let mk_update = ((mk_prog ("rake-compress-update." ^ extension)) & (mk string "type" "add") & (mk_list int "seq" [0]))
let mk_graphs = (((mk string "graph" "binary_tree") & (mk_list float "fraction" [-1.0])) ++
    ((mk string "graph" "random") & (mk_list float "fraction" [0.0; 0.3; 0.6; 1.0])))

let run () =
  Mk_runs.(call [
    Runs 3;
    Output ("plots/construction_update/results_" ^ version ^ ".txt");
    Args ((mk_construction ++ mk_update) & mk_n & mk_graphs);
    ])

let check = nothing

let plot () =
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
      ));
      ("prog", Format_custom (fun prog ->
        if prog = ("rake-compress-construction." ^ extension) then "pure construction"
        else if prog = ("rake-compress-update." ^ extension) then "update from scratch"
        else ""
      ));
      ("type", Format_hidden);
      ("seq", Format_custom (fun seq ->
        match seq with
        | "0" -> "parallel"
        | _ -> "sequential"
      ))
    ])) in

  Mk_bar_plot.(call ([
    Bar_plot_opt Bar_plot.([
       X_titles_dir Vertical; 
       Y_axis [Axis.Lower (Some 0.)] ]);
    Series (mk_construction ++ mk_update);
    Formatter formatter;
    X mk_graphs;
    Y_label "Execution Time";
    Input ("plots/construction_update/results_" ^ version ^ ".txt");
    Output ("plots/construction_update/plots_" ^ version ^ ".pdf");
    ] 
    @ (y_as_mean "exectime")
  ))

let all () = select make run check plot

end


module ExpCUC = struct

let version = XCmd.parse_or_default_string "version" "fast"

let scale = XCmd.parse_or_default_string "scale" "all"

let extension = match version with
                | "slow" -> "virtual"
                | _ -> "virtual2"

let scale_type = match scale with
                 | "all" -> ""
                 | _ -> "_small"

let name = "update_time_speedup"

let make () = 
  build "." ["rake-compress-update.opt";"rake-compress-update.opts"] arg_virtual_build

let mk_n = mk int "n" 1000000
let mk_construction = (mk_prog ("rake-compress-construction." ^ extension)) & (mk int "seq" 2)
let mk_update = ((mk_prog ("rake-compress-update." ^ extension)) & (mk int "seq" 0) & (mk string "type" "add"))
let mk_graphs = (((mk string "graph" "binary_tree") & (mk_list float "fraction" [-1.0])) ++
    ((mk string "graph" "random") & (mk_list float "fraction" [0.0; 0.3; 0.6; 1.0])))
let mk_ks = mk_list int "k" (match scale with
                             | "all" -> [1; 10; 100; 1000; 3000; 10000; 30000; 60000; 100000; 300000; 600000; 999999]
                             | _ -> [1; 10; 100; 300; 600; 1000; 3000; 6000; 10000; 20000; 30000; 40000]
                            )
let mk_proc = mk_list int "proc" [1; 2; 4; 8; 10; 12; 15; 20; 25; 30; 39]

let run () = 
  let skip_construction = XCmd.mem_flag "skip_construction" in
  if not skip_construction then begin
    Mk_runs.(call [
      Runs 3;
      Output ("plots/construction_update_comparison/results_construction_" ^ version ^ ".txt");
      Args (mk_construction & mk_n & mk_graphs & mk_proc);
      ]);
  end;
  let skip_update = XCmd.mem_flag "skip_update" in
  if not skip_update then begin
    Mk_runs.(call [
      Runs 3;
      Output ("plots/construction_update_comparison/results_update_" ^ version ^ scale_type ^ ".txt");
      Args (mk_update & mk_n & mk_graphs & mk_ks & mk_proc);
      ]);
  end

let check = nothing

let plot () = 
  let results_construction = Results.from_file ("plots/construction_update_comparison/results_construction_" ^ version ^ ".txt") in

  let formatter = 
    Env.format (Env.(
    [ ("graph", Format_custom (fun graph ->
        match graph with
        | "binary_tree" -> "binary tree"
        | "random" -> "random tree"
        | _ -> ""
      ));
      ("type", Format_hidden);
      ("prog", Format_hidden);
    ])) in

  let eval_relative = fun env all_results results ->
    let construction_results =  ~~ Results.filter_by_params results_construction (
         from_env (Env.filter_keys ["graph"; "fraction"; "proc"] env)
       ) in
    if construction_results = [] then Pbench.warning ("no results for construction: " ^ Env.to_string env);
    let update = Results.get_mean_of "exectime" results in
    let construction = Results.get_mean_of "exectime" construction_results in
    construction /. (max update 0.001)
    in
    Mk_scatter_plot.(call ([
      Scatter_plot_opt Scatter_plot.([
        Draw_lines true; 
        X_axis [Axis.Label "Number of inserted edges"];
        Y_axis [Axis.Lower (Some 0.); Axis.Is_log true]
      ]);
      Formatter formatter;
      Charts mk_graphs;
      Series mk_proc;
      X mk_ks;
      Y eval_relative;
      Y_label "Static Run-time / Dynamic Run-time";
      Input ("plots/construction_update_comparison/results_update_" ^ version ^ scale_type ^ ".txt");
      Output ("plots/construction_update_comparison/plot_" ^ version ^ scale_type ^ ".pdf");
      ]))

let all () = select make run check plot

end

module ExpUT = struct

let version = XCmd.parse_or_default_string "version" "fast"

let extension = match version with
                | "slow" -> "virtual"
                | _ -> "virtual2"

let name = "update_time"

let make () = 
  build "." ["rake-compress-update.opt";"rake-compress-update.opts"] arg_virtual_build

let mk_n = mk int "n" 1000000
let mk_types = mk_list string "type" ["add";"delete"]
let mk_update = ((mk_prog ("rake-compress-update." ^ extension)) & (mk int "seq" 0))
let mk_graphs = (((mk string "graph" "binary_tree") & (mk_list float "fraction" [-1.0])) ++
    ((mk string "graph" "random") & (mk_list float "fraction" [0.0; 0.3; 0.6; 1.0])))
let mk_ks = mk_list int "k" [1; 10; 100; 1000; 3000; 10000; 30000; 60000; 100000; 140000; 160000; 200000]

let run () = 
  Mk_runs.(call [
    Runs 3;
    Output ("plots/update/results_" ^ version ^ ".txt");
    Args (mk_update & mk_types & mk_n & mk_graphs & mk_ks);
    ])

let check = nothing

let plot () = 
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
      ));
      ("prog", Format_hidden);
      ("type", Format_hidden);
      ("seq", Format_hidden);
    ])) in
    Mk_scatter_plot.(call ([
      Scatter_plot_opt Scatter_plot.([
        Draw_lines true; 
        X_axis [Axis.Label "Number of edges"];
        Y_axis [Axis.Lower (Some 0.)]
      ]);
      Formatter formatter;
      Charts mk_types;
      Series mk_graphs;
      X mk_ks;
      Y_label "Execution Time (seconds)";
      Input ("plots/update/results_" ^ version ^ ".txt");
      Output ("plots/update/plot_" ^ version ^ ".pdf")
    ] @ (y_as_mean "exectime")
  ))

let all () = select make run check plot

end

module ExpUTS = struct

let version = XCmd.parse_or_default_string "version" "fast"

let extension = match version with
                | "slow" -> "virtual"
                | _ -> "virtual2"

let name = "update_time_speedup"

let make () = 
  build "." ["rake-compress-update.opt";"rake-compress-update.opts"] arg_virtual_build

let mk_n = mk int "n" 1000000
let mk_update = ((mk_prog ("rake-compress-update." ^ extension)) & (mk int "seq" 0) & (mk string "type" "add"))
let mk_graphs = (((mk string "graph" "binary_tree") & (mk_list float "fraction" [-1.0])) ++
    ((mk string "graph" "random") & (mk_list float "fraction" [0.0; 0.3; 0.6; 1.0])))
let mk_ks = mk_list int "k" [1; 10; 100; 1000; 3000; 10000; 30000; 60000; 100000; 300000; 600000; 999999]
let mk_proc = mk_list int "proc" [1; 2; 4; 8; 10; 12; 15; 20; 25; 30; 39]

let run () = 
  Mk_runs.(call [
    Runs 3;
    Output ("plots/update_speedup/results_" ^ version ^ ".txt");
    Args (mk_update & mk_n & mk_graphs & mk_ks & mk_proc);
    ])

let check = nothing

let plot () = 
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
      ));
      ("prog", Format_hidden);
      ("type", Format_hidden);
      ("seq", Format_hidden);
      ("k", Format_custom (fun k -> sprintf "m = %s" k))
    ])) in

  let eval_relative = fun env all_results results ->
    let t1_env = Env.filter (fun k -> k <> "proc") env in
    let t1_env = Env.add t1_env "proc" (Env.Vint 1) in
    let t1_results = ~~ Results.filter all_results t1_env in
    let baseline_results =  ~~ Results.filter_by_params t1_results (
         from_env (Env.filter_keys ["graph"; "fraction"; "k"] env)
         (* here we select only the baseline runs with matching n and m arguments *)
       ) in
    if baseline_results = [] then Pbench.warning ("no results for baseline: " ^ Env.to_string env);
    let v = Results.get_mean_of "exectime" results in
    let b = Results.get_mean_of "exectime" baseline_results in
    b /. v
    in
    Mk_scatter_plot.(call ([
      Scatter_plot_opt Scatter_plot.([
        Draw_lines true; 
        X_axis [Axis.Label "Number of processors"];
        Y_axis [Axis.Lower (Some 0.)]
      ]);
      Formatter formatter;
      Charts mk_graphs;
      Series mk_ks;
      X mk_proc;
      Y eval_relative;
      Y_label "Speedup";
      Input ("plots/update_speedup/results_" ^ version ^ ".txt");
      Output ("plots/update_speedup/plot_" ^ version ^ ".pdf")
    ]
  ))


let all () = select make run check plot

end

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "construction_speedup",                 ExpCS.all;
    "construction_sizes",                   ExpCS2.all;
    "construction_comparison",              ExpCC.all;
    "construction_update_comparison",       ExpCUC.all;
    "update_time",                          ExpUT.all;
    "update_time_speedup",                  ExpUTS.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()                 