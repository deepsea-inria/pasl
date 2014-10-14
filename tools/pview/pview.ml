open Shared
open Scanf
open Printf
open Graphics
open Visualize

(* Command line argument (with default values):
    -width 800
    -height 800
    -proc_height 10
    -input LOG_BIN 

Commands:
  -'f' for full view
  -click to zoom in
  -'o' for zoom out
  -'q' for quit
  -'r' for reload file
*)


(****************************************************)
(* Type of events *)

type evttype = 
  | Evt_enter_launch
  | Evt_exit_launch
  | Evt_enter_algo
  | Evt_exit_algo
  | Evt_enter_wait
  | Evt_exit_wait
  | Evt_communicate
  | Evt_interrupt
  | Evt_algo_phase
  | Evt_locality_start of int
  | Evt_locality_stop of int
  (*
  | Evt_task_exec
  | Evt_poll_and_deal
  | Evt_task_create
  | Evt_task_schedule
  | Evt_task_pop
  | Evt_task_send
  *)
  | Evt_other

(****************************************************)
(* Parsing of events *)

let parse_event ch =
   let time = (float_of_int) (read_int ch) (* read_double ch *) in
   let proc = read_int ch in
   let code = read_int ch in
   let evttype = match code with
      | 0 -> Evt_enter_launch
      | 1 -> Evt_exit_launch
      | 2 -> Evt_enter_algo
      | 3 -> Evt_exit_algo
      | 4 -> Evt_enter_wait
      | 5 -> Evt_exit_wait
      | 6 -> Evt_communicate
      | 7 -> Evt_interrupt
      | 8 -> Evt_algo_phase
      | 9 -> Evt_locality_start (read_int ch)
      | 10 -> Evt_locality_stop (read_int ch)
      | _ -> Evt_other
      in
   (time,proc,evttype)


(****************************************************)
(* Printing of events (for debug purposes only) *)

let string_of_evttype = function
   | Evt_enter_launch -> "enter_launch"
   | Evt_exit_launch -> "exit_launch"
   | Evt_enter_algo -> "enter_algo"
   | Evt_exit_algo -> "exit_algo "
   | Evt_enter_wait -> "enter_wait"
   | Evt_exit_wait -> "exit_wait"
   | Evt_communicate -> "communicate"
   | Evt_interrupt -> "interrupt"
   | Evt_algo_phase -> "algo_phase"
   | Evt_locality_start pos -> sprintf "locality_start\t%d" pos
   | Evt_locality_stop pos -> sprintf "locality_stop\t%d" pos
   | Evt_other -> "other"

let print_event evt =
   let (time,proc,evttype) = evt in
   printf "%f %d %s\n" time proc (string_of_evttype evttype)

let print_events evts =
   List.iter print_event evts


(****************************************************)
(* Display of events *)

let idle_time_color = red

let drawing events range =
   let subblock = Cmdline.parse_or_default_int "sub" 8 in
   let not_idle = -1. in
   let not_working = -1. in
   let nbproc = get_nbproc() in
   let idle_since = Array.make nbproc not_idle in
   let working_since = Array.make nbproc not_working in
   let last_locality = Array.make nbproc (-1) in
   let height = get_proc_height() - 2 in
   let draw_launch timestamp =
      draw_phase blue range 15 timestamp in
   let draw_algo timestamp =
      draw_phase black range 30 timestamp in
   list_foreach events (fun (timestamp, proc, eventtype) ->
      match eventtype with 
      | Evt_enter_launch -> draw_launch timestamp
      | Evt_exit_launch -> draw_launch timestamp
      | Evt_enter_algo -> draw_algo timestamp
      | Evt_exit_algo -> draw_algo timestamp
      | Evt_enter_wait ->
         if idle_since.(proc) <> not_idle
            then warning "two consecutive enter_wait";
         idle_since.(proc) <- timestamp
      | Evt_exit_wait ->
         if idle_since.(proc) = not_idle then
            warning "exit_wait without prior enter_wait"
         else begin
            draw_box range proc idle_time_color 1 (height-1) idle_since.(proc) timestamp;
            idle_since.(proc) <- not_idle
         end
      | Evt_communicate ->
         draw_box range proc black 0 (height/4) (timestamp+.1.) (timestamp+.1.)
      | Evt_interrupt ->
         draw_box range proc black 0 (height/2) timestamp timestamp
      | Evt_algo_phase -> 
         draw_launch timestamp
      | Evt_locality_start pos -> 
         if working_since.(proc) <> not_working
            then warning "two consecutive locality_start";
         working_since.(proc) <- timestamp;
         last_locality.(proc) <- pos
      | Evt_locality_stop pos ->
          if working_since.(proc) = not_working then
             warning "locality_stop without prior locality_start"
          else begin
            let pos_start = last_locality.(proc) in
            let pos_stop = pos in
            (* the constant 60 comes from locality_range_t::init() in task.cpp *)
            (*
            let span = pos_stop - pos_start in
            let height_approx = int_of_float (float_log2 span /. 60. *. (float_of_int height)) in
            let height_real = min (height-1) (max 5 height_approx) in
            *)
            let pos_mid = (pos_start + pos_stop) / 2 in
            let progress1 = (float_of_int pos_mid) /. ((float_of_int) (1 lsl 60)) in
            (* printf "%.8f\n" progress;*)
            (* let progress = Random.float 1.0 in *)
            let color1 = rainbow_color progress1 in
            let step = (float_of_int pos_mid) /. ((float_of_int) (1 lsl (60-subblock))) in
            let progress2 = step -. floor step in
            let color2 = rainbow_color progress2 in
            let startstamp = working_since.(proc) in
            let height1 = (height-1) / 2 in
            let height2 = (height-1) / 4 in
            draw_box range proc color1 1 height1 startstamp timestamp;
            draw_box range proc color2 (1+height1) height2 startstamp timestamp;
            draw_box range proc black 1 5 timestamp timestamp;
            working_since.(proc) <- not_working
          end

      | Evt_other -> 
         draw_box range proc gray 0 1 timestamp timestamp
      );
   nat_foreach nbproc (fun p ->
     if (idle_since.(p) <> not_idle) 
        then warning "missing exit_wait events")


(****************************************************)
(* Pre-processing of events *)

let preprocess_events events =
   let init_range = 
      let dummy_time = -1. in
      let select_evt join evt =
         List.fold_left (fun acc (t,p,s) ->
           if s <> evt then acc else if acc = dummy_time then t else join acc t) dummy_time events in
      let t1 = select_evt min Evt_enter_algo in
      let t2 = select_evt max Evt_exit_algo in
      if t1 = dummy_time || t2 = dummy_time
         then failwith "cannot find enter_algo or exit_algo";
      (t1,t2)
      in   
   let full_range = extract_full_range events in
   (* Printf.printf "algo start  %f  algo stop %f \n" (fst init_range) (snd init_range);
   Printf.printf "all start  %f  all stop %f \n" (fst full_range) (snd full_range); *)
   (events, extract_nb_proc events, init_range, full_range)


(****************************************************)
(* Main loop *)

let _ =
   let logfile = Cmdline.parse_or_default_string "input" "LOG_BIN" in
   let parse_events () =
      parse_item_list_from_file parse_event logfile in
   main_loop parse_events preprocess_events drawing


