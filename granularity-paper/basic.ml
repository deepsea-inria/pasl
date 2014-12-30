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

(*****************************************************************************)
(** Fibonacci experiment *)

module ExpFib = struct

let name = "fib"

let prog = "./fib"

let mk_algos = mk_list string "algo" ["recursive";"cached"]

let mk_ns = mk_list int "n" [30;35;39]

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_algos
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
      Series mk_algos;
      X mk_ns;
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
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()