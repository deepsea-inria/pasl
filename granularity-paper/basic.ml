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
let modes = XCmd.parse_or_default_list_string "modes" ["binary";"bsearch";"lbsearch";"lbinary";"sched";"standart"]

let files = List.map (fun s -> "./bench." ^ s) modes

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

(*****************************************************************************)
(** Fibonacci experiment *)

module ExpFib = struct

let name = "fib"

let prog = "./fib"

let bench = mk_list string "bench" ["fib"]

let mk_files = mk_progs files

let mk_ns = mk_list int "n" [30;35;39]

let make() =
    build "." files arg_virtual_build
(*  build "." [prog] arg_virtual_build*)

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_files
    & bench
    & mk_ns)]))

let check = nothing  (* do something here *)

let fib_formatter =
 Env.format (Env.(
   [ ("n", Format_custom (fun n -> sprintf "fib(%s)" n)); ]
  ))

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter fib_formatter;
      Charts mk_unit;
      Series mk_files;
      X mk_ns;
      Input (file_results name);
      Output (file_plots name);
      Y_label "exectime";
      Y eval_exectime;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Synthetic experiment *)

module ExpSynthetic = struct

let algo = XCmd.parse_or_default_string "algo" "parallel_for"

let name = "synthetic_" ^ algo

let prog = "./synthetic"

let bench = mk_list string "bench" ["synthetic"]

let mk_algo = mk_list string "algo" [algo]

let mk_files = mk_progs files

let mk_ns = mk_list int "n" (XCmd.parse_or_default_list_int "n" [3000;2000])
let mk_ms = mk_list int "m" (XCmd.parse_or_default_list_int "m" [3000;2000])
let mk_tries = mk_list int "tries" (XCmd.parse_or_default_list_int "tries" [10])
let mk_init = mk_list float "init" (XCmd.parse_or_default_list_float "init" [1.])
let mk_proc = mk_list int "proc" (XCmd.parse_or_default_list_int "proc" [8])

let make() =
    build "." files arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 2000;
    Args (
      mk_files
    & mk_algo
    & bench
    & mk_ns & mk_ms
    & mk_tries & mk_init
    & mk_proc
)]))

let check = nothing  (* do something here *)

let synthetic_formatter =
 Env.format (Env.(
   [ ("n", Format_custom (fun n -> sprintf "synthetic(%s)" n)); ]
  ))

let plot() =
  Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
       Formatter synthetic_formatter;
      Charts mk_unit;
      Series (mk_files & mk_tries & mk_proc & mk_init);
      X (mk_ns & mk_ms);
      Input (file_results name);
      Output (file_plots name);
      Y_label "exectime";
      Y eval_exectime;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "fib", ExpFib.all;
    "synthetic", ExpSynthetic.all
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()