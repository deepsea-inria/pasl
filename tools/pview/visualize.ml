open Shared
open Scanf
open Printf
open Graphics

let my_draw_string size col (x,y) str =
   set_text_size size;
   set_color col;
   moveto x y;
   draw_string str

let my_fill_rect col (x,y) (w,h) =
   set_color col;
   fill_rect x y w h
   (* debug: try 
   with _ -> failwith (Printf.sprintf "invalid my_fill_rect %d %d %d %d" x y w h)
   *)

let my_line col (x1,y1) (x2,y2) =
   set_color col;
   moveto x1 y1;
   lineto x2 y2

let shift_point (x,y) (a,b) =
   (x+a, y+b)

let shift_point_x (x,y) a =
   (x+a,y)

let shift_point_y (x,y) b =
   (x,y+b)

let shift_point_ref r (a,b) =
   r := shift_point !r (a,b)

let shift_point_x_ref r a =
   r := shift_point_x !r a

let shift_point_y_ref r b =
   r := shift_point_y !r b

let white = rgb 255 255 255
let black = rgb 0 0 0 
let red = rgb 255 0 0 
let yellow = rgb 255 255 0 
let blue = rgb 0 0 255
let green = rgb 0 255 0 
let gray = rgb 200 200 200 


(****************************************************)
(* Parsing *)

(** Parsing of a list of items from a file *)

let parse_item_list_from_file parse_item filename =
   let ch = open_in_bin filename in
   let items = build_list (fun add ->
      try while true do
         add (parse_item ch)
      done with End_of_file -> ()
      ) in
   close_in ch;
   items

(****************************************************)
(** Display settings *)

(** Data type to store settings *)

type gopt = 
  { gopt_width_full : int;
    gopt_height_full : int; 
    gopt_proc_height : int; }

(** Parsing of display settings from command-line arguments *)

let gopt = ref
  { gopt_width_full = Cmdline.parse_or_default_int "width" 800; 
    gopt_height_full = Cmdline.parse_or_default_int "height" 800;
    gopt_proc_height = Cmdline.parse_or_default_int "proc_height" (-1) } 

let cached_proc_height = 
   ref (-1)

let compute_proc_height nbproc =
  if !gopt.gopt_proc_height <> -1 then !gopt.gopt_proc_height else begin 
    let h = min 30 ((!gopt.gopt_height_full - 50) / nbproc) in
    if h < 2 then failwith "window is not tall enough to show all processors";
    h
  end

let get_proc_height () =
   assert (!cached_proc_height > 0);
   !cached_proc_height


(****************************************************)
(** Number of processors *)

(** Keep track of the number of processors; The function
    set_nbproc must be called on every update *)

let nbproc_ref = ref (-1)

let get_nbproc () =
  assert (!nbproc_ref >= 0);
  !nbproc_ref

let set_nbproc nb =
   nbproc_ref := nb;
   cached_proc_height := compute_proc_height nb


(****************************************************)
(** Graphics window *)

(** Open a manually-synchronized graphic window at the appropriate size *)

let open_graphic_window () =
   open_graph (sprintf " %dx%d" !gopt.gopt_width_full !gopt.gopt_height_full);
   auto_synchronize false

(** Synchronize the content of the graphic window *)

let synchronize_graphic_window () =
   synchronize()

(** Close the graphic window *)

let close_graphic_window () =
   close_graph()

(****************************************************)

(** Precomputes rainbow colors *)

let rainbow_table_size = 
   (255*6)

let rainbow_table = 
   lazy begin
   let data = Array.make rainbow_table_size black in
   let r = ref 255 in
   let g = ref 0 in
   let b = ref 0 in
   let i = ref 0 in
   let save () =
      data.(!i) <- rgb !r !g !b;
      incr i
      in
   let increase c =
      for k = 1 to 255 do
         c := k;
         save();
      done
      in
   let decrease c =
      for k = 254 downto 0 do
         c := k;
         save();
      done
      in
   increase g;
   decrease r;
   increase b;
   decrease g;
   increase r;
   decrease b;
   data
   end

(** Turns a number in the range (0.0,1.0) into a index in the rainbow table.
    Values outside the range (0.0,1.0) will be truncated. *)

let rainbow_index progress =
   let index_unsafe = int_of_float ((float_of_int rainbow_table_size) *. progress) in
   let index = max 0 (min (rainbow_table_size-1) index_unsafe) in
   index

(** Turns a number in the range (0.0,1.0) into a color, using the rainbow table *)

let rainbow_color progress =
   let table = Lazy.force rainbow_table in
   table.(rainbow_index progress)

(** Display the rainbow of colors *)

let print_rainbow (x,y) (w,h) =
   if (w <= 0) then failwith "print_rainbow requires a positive width";
   for pos = 0 to pred w do
      let progress = (float_of_int pos) /. (float_of_int w) in
      my_fill_rect (rainbow_color progress) (x+pos, y) (1, h);
   done



(****************************************************)

(** Checks whether a given timestamp belongs to a given time-frame *)

let inrange (boxtime1,boxtime2) timestamp =
   boxtime1 <= timestamp && timestamp <= boxtime2

(** Relative position of a x-coordinate into the window *)

let relative_of_xcoord x =
   float_of_int x /. float_of_int !gopt.gopt_width_full 

(** Convert a x-coordinate into a timestamp *)

let timestamp_of_xcoord (boxtime1,boxtime2) x =
   boxtime1 +. (relative_of_xcoord x) *. (boxtime2 -. boxtime1)

(** Ensure a coordinate fits in the display panel *)

let bound_xcoord x =
   min (max 0 x) (!gopt.gopt_width_full-1)

let bound_ycoord y =
    min (max 0 y) (!gopt.gopt_height_full-1)

(** Convert a given timestamp into a valid x-coordinate in the windows,
    based on its relative position in the given time-frame *)

let xcoord_or_timestamp (boxtime1,boxtime2) timestamp =
   let progress = (timestamp -. boxtime1) /. (boxtime2 -. boxtime1) in
   let x = int_of_float (progress *. (float_of_int !gopt.gopt_width_full)) in
   bound_xcoord x

(** Return the height at which to draw information for a given processor *)

let proc_y proc =
   0 + proc * (get_proc_height())

(** Draw horizontal lines to separate processors *)

let draw_proc_lines () =
   nat_foreach (get_nbproc()) (fun proc ->
      my_fill_rect black (0, proc_y proc) (!gopt.gopt_width_full, 0))

(** Draws a colored rectangle of a given height for a given processor,
    bounded by two given timestamp, w.r.t. a given time-frame *)

let draw_box range proc col bottom height timestamp1 timestamp2 =
   let (boxtime1, boxtime2) = range in
   if height < 0 then failwith "drawbox with negative height";
   let timestamp1 = max boxtime1 timestamp1 in
   let timestamp2 = min boxtime2 timestamp2 in
   if timestamp1 <= timestamp2 then begin
      let x1 = xcoord_or_timestamp range timestamp1 in
      let x2 = xcoord_or_timestamp range timestamp2 in
      let w = max 1 (x2 - x1) in
      my_fill_rect col (x1, proc_y proc + 1 + bottom) (w, height)
   end

(** Draw a vertical line across all processors, at a given timestamp
    w.r.t. a given time-frame. *)

let draw_phase col range extraheight timestamp =
   if inrange range timestamp then begin
      let x = xcoord_or_timestamp range timestamp in
      my_fill_rect col (x, 0) (1, extraheight + proc_y (get_nbproc()))
   end
   (* at top only: my_fill_rect col (x-1, 1 * 10 + proc_y (get_nbproc())) (1, 10) *)


(****************************************************)
(** Exceptions for control flow *)

exception Restart
exception Reload
exception Exit


(****************************************************)
(** Print information about the current scale *)

(** Convert a time into a readable form *)

let convert_time t =
  if t >= 1000000. then
      sprintf "%.1f seconds" (t /. 1000000.) 
  else if t >= 100000. then
      sprintf "%.2f seconds" (t /. 1000000.) 
  else if t >= 10000. then
      sprintf "%.1f milliseconds" (t /. 1000.) 
  else if t >= 1000. then
      sprintf "%.2f milliseconds" (t /. 1000.) 
  else 
     sprintf "%d microseconds" (int_of_float t) 

(** Display the current scale at a given x-position at the top of the window *)

let display_scale title t xpos =
  let str = convert_time t in
  my_draw_string 20 black (xpos,!gopt.gopt_height_full-50) (title ^ str)
  
(** Compute the width of a range *)

let width_of_range (t1,t2) =
  t2 -. t1

let display_rainbow () =
   print_rainbow (0,!gopt.gopt_height_full-10) (!gopt.gopt_width_full,10)


(****************************************************)
(** Extract infos from events *)

(** Get the range of a list of events *)

let extract_full_range events =
   let t1 = List.fold_left (fun acc (t,_,s) -> min acc t) infinity events in
   let t2 = List.fold_left (fun acc (t,_,s) -> max acc t) neg_infinity events in
   assert (t1 <> infinity && t2 <> neg_infinity);
   (t1,t2)

(** Get the number of processors *)

let extract_nb_proc events =
   1 + List.fold_left (fun acc (_,p,_) -> max p acc) 0 events 


(****************************************************)
(** Main display loop that controls ranges *)

(** - maintains a stack of ranges for zooming in and out
    - supports measure of the width of an interval
    - handle all key and mouse events 
  *)

let display_at_range draw init_range full_range =  
   let range_stack = ref [init_range] in
   let no_pos = (-1,-1) in
   let last_down_pos = ref no_pos in
   let measure_to_display = ref None in
   while true do
      assert (!range_stack <> []);
      let range = List.hd (!range_stack) in
      clear_graph();
      draw_proc_lines();
      draw range;
      (* display_rainbow(); *)
      display_scale "Runtime: " (width_of_range range) 0;
      begin match !measure_to_display with
         | Some r -> display_scale "Time range: " r 200
         | None -> ()
      end;
      measure_to_display := None;
      synchronize_graphic_window();
      let e = wait_next_event [ Button_down; Button_up; Key_pressed ] in
      let mouse_pos = (e.mouse_x, e.mouse_y) in
      if e.keypressed then begin
         match e.key with
         | 'q' -> raise Exit 
         | 'r' -> raise Reload
         | 's' -> raise Restart
         | 'f' -> range_stack := [full_range]
         | 'i' -> range_stack := [init_range]
         | ' ' -> range_stack := [init_range]
         | 'o' -> 
            begin match !range_stack with
            | [] -> ()
            | [r] -> ()
            | r::rs -> range_stack := rs
            end
         | _ -> ()
      end else if e.button then begin
         (* mouse down *)
         last_down_pos := mouse_pos;
      end else begin
         (* mouse up *)
         if mouse_pos <> !last_down_pos then begin
            (* mouse move *)
            let t1 = timestamp_of_xcoord range (fst !last_down_pos) in
            let t2 = timestamp_of_xcoord range (fst mouse_pos) in
            measure_to_display := Some (abs_float (t2 -. t1));
         end else begin
            (* mouse didn't move *)
            let progress = relative_of_xcoord (fst mouse_pos) in
            let (range1, range2) = range in
            let newspan = (range2 -. range1) /. 2. in
            let newrange1 =
               if progress < 0.25 
                  then range1
               else if progress > 0.75
                  then range2 -. newspan
               else
                  range1 +. (progress -. 0.25) *. 2. *. newspan 
               in
            let newrange = (newrange1, newrange1 +. newspan) in
            add_to_list_ref range_stack newrange;
         end
       end
   done

(****************************************************)

let main_loop parse_events preprocess_events drawing =
   open_graphic_window();
   begin try while true do
      try 
         let parsed_events = parse_events() in
         let (events, nbproc, init_range, full_range) = 
            preprocess_events parsed_events in
         set_nbproc nbproc;
         while true do 
            try 
               display_at_range (drawing events) init_range full_range
            with Restart -> () 
         done 
      with Reload -> ()
   done with Exit -> () end;
   close_graphic_window()
