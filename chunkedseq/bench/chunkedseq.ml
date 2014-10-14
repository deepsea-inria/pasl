open XBase
open Params

let system = XSys.command_must_succeed_or_virtual

(*****************************************************************************)
(** Parameters *)

let arg_virtual_run = XCmd.mem_flag "virtual_run"
let arg_virtual_generate = XCmd.mem_flag "virtual_generate"
let arg_proc = XCmd.parse_or_default_int "proc" 1
let arg_timeout_opt = XCmd.parse_optional_int "timeout"
let arg_skips = XCmd.parse_or_default_list_string "skip" []
let arg_onlys = XCmd.parse_or_default_list_string "only" []
let arg_path_to_graph_data = XCmd.parse_or_default_string "path_to_graph_data" "_data"
let arg_path_to_pasl = XCmd.parse_or_default_string "path_to_pasl" "../"
let arg_mode = 
   let m = XCmd.parse_or_default_string "mode" "normal" in
   let mc = XCmd.mem_flag "complete" in
   let mr = XCmd.mem_flag "replace" in
   (* Pbench.error "incompatible mode options" => todo *)
   if mc then "complete"
   else if mr then "replace"
   else m
let arg_top_pct = XCmd.parse_or_default_float "top_pct" 0.10

let run_modes =
  Mk_runs.([
    Mode (Mk_runs.string_of_mode arg_mode);
    Virtual arg_virtual_run; ])

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

let file_tables_src exp_name =
  Printf.sprintf "tables_%s.tex" exp_name

let file_tables exp_name =
  Printf.sprintf "tables_%s.pdf" exp_name

(*****************************************************************************)
(** Chart formatters *)

let my_formatter e =
   Env.(~~ format e (
      format_values  [ "algo"; "!traversal" ]
   ))

let string_of_exectime ?(prec=2) v =
   match classify_float v with
   | FP_normal | FP_zero -> 
      if prec = 1 then sprintf "%.1fs" v
      else if prec = 2 then sprintf "%.2fs" v
      else failwith "only precision 1 and 2 are supported"
   | FP_infinite -> "$+\\infty$"
   | FP_subnormal -> "na"
   | FP_nan -> ""

let string_of_pow10 v =
  let l = log10 v in
  sprintf "$10^%d$" (int_of_float l)

(*****************************************************************************)
(** Graph file generation *)

module ExpGenerate = struct

let name = "generate"

let path = arg_path_to_pasl ^ "/graph/bench/"

let name_graphfile = "graphfile.opt2"

let prog = path ^ name_graphfile

let make()  =
   system (sprintf "mkdir -p %s" arg_path_to_graph_data) arg_virtual_generate;
   build path [name_graphfile] arg_virtual_generate

let nb_vertex_id_bits = 64

let file_friendster_snap =
  "friendster.snap"

let get_friendster_graph() =
  (* "http://snap.stanford.edu/data/com-Friendster.html" *)
  let url_friendster = "http://snap.stanford.edu/data/bigdata/communities/com-friendster.ungraph.txt.gz" in
  let file_friendster_gz = file_friendster_snap ^ ".gz" in
  let path_friendster_gz = sprintf "%s/%s" arg_path_to_graph_data file_friendster_gz in
  if not(Sys.file_exists (arg_path_to_graph_data ^ "/" ^ file_friendster_snap)) then (
    XSys.wget ~no_clobber:true url_friendster path_friendster_gz arg_virtual_generate;
    XSys.gunzip arg_path_to_graph_data file_friendster_snap arg_virtual_generate)
  else ()

let mk_graph_files_all =
  let mk_file name file bits =
    let timeout = XOption.unsome_or 10000 arg_timeout_opt in
        (mk string "outfile" (sprintf "%s/%s" arg_path_to_graph_data file))
      & (mk string "!name" name)  (* ghost parameter *)
      & (mk int "proc" arg_proc)
      & (mk int "timeout" timeout)
      & (mk int "bits" bits)
     in
   let mk_square_grid =
        (mk_file "square_grid" "square_grid.adj_bin" 64)
      & (mk string "generator" "square_grid")
      & (mk int "nb_edges_target" 400000000)
      & (mk int "source" 0)
      in
   let mk_tree =
        (mk_file "perfect_bintree" "perfect_bintree.adj_bin" 64)
      & (mk string "generator" "tree")
      & (mk int "nb_edges_target" 400000000)
      & (mk int "source" 0)
      in
   let mk_friendster =
        (mk_file "friendster" "friendster.adj_bin" 64)
      & (mk string "infile" (sprintf "%s/%s" arg_path_to_graph_data file_friendster_snap))
      & (mk int "source" 123)
      in
   Params.eval (mk_square_grid ++ mk_tree ++ mk_friendster)

let get_params_all key =
  ~~ List.map (Params.to_envs mk_graph_files_all) (fun e ->
    Env.get_as_string e key)

let mk_graph_infiles =
  Params.concat(~~ List.map (Params.to_envs mk_graph_files_all) (fun e ->
    let outfile = Env.get_as_string e "outfile" in
    let source = Env.get_as_int e "source" in
    mk string "infile" outfile & mk int "source" source))

let graph_filenames_all = get_params_all "outfile"

let graph_names_all = get_params_all "!name"

let mk_generated_names =
  mk_list string "name" graph_names_all

let generate_run_modes =
  Mk_runs.([
    Mode (Mk_runs.string_of_mode arg_mode);
    Virtual arg_virtual_generate; ])

let run () =
   get_friendster_graph();
   Mk_runs.(call (generate_run_modes @ [
     Output (file_results name);
      Timeout 4000;
      Args (
           mk_prog prog
         & mk_graph_files_all
       ) ] ))

let plot () =
   Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.);] ]);
      Formatter my_formatter;
      Charts mk_unit;
      Series mk_unit;
      X mk_generated_names;
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

let seq_chunkedftree = "chunkedftree"
let seq_chunkedseq = "chunkedseq"
let seq_stl_deque = "stl_deque"
let seq_stl_rope = "stl_rope"
let seq_ls_bag = "ls_bag"

let seq_info = [
  (seq_ls_bag ,        ["PBFS"; "bag"]);
  (seq_stl_deque,      ["STL"; "deque"]);
  (seq_chunkedftree,   ["Our";"chunked";"finger tree"]);
  (seq_chunkedseq,     ["Our";"bootstr.";"chunked"]);
  (seq_stl_rope,       ["STL"; "rope"]);
]

let all_sequence_names = List.map fst seq_info

let mk_sequences seqs =
  mk_list string "sequence" seqs

let mk_all_sequences =
  mk_sequences all_sequence_names

(*****************************************************************************)

let path_chunkedseq_bench = arg_path_to_pasl ^ "/chunkedseq/bench"
let name_chunkedseq_bench = "bench.exe"
let name_chunkedseq_bench_fifolifo = "do_fifo.exe_full"
(*name_chunkedseq_bench ^ "_fifolifo"*)
let name_chunkedseq_bench_filter = name_chunkedseq_bench ^ "_filter"
let name_chunkedseq_bench_splitmerge = name_chunkedseq_bench ^ "_splitmerge"
let name_chunkedseq_bench_chunksize = name_chunkedseq_bench ^ "_chunksize"
let name_chunkedseq_bench_map = name_chunkedseq_bench ^ "_map"
let prog_chunkedseq_bench_fifolifo = path_chunkedseq_bench ^ "/" ^ name_chunkedseq_bench_fifolifo
let prog_chunkedseq_bench_filter = path_chunkedseq_bench ^ "/" ^ name_chunkedseq_bench_filter
let prog_chunkedseq_bench_splitmerge = path_chunkedseq_bench ^ "/" ^ name_chunkedseq_bench_splitmerge
let prog_chunkedseq_bench_chunksize = path_chunkedseq_bench ^ "/" ^ name_chunkedseq_bench_chunksize
let prog_chunkedseq_bench_map = path_chunkedseq_bench ^ "/" ^ name_chunkedseq_bench_map

let bench_names = [
  name_chunkedseq_bench_fifolifo;
  name_chunkedseq_bench_filter;
  name_chunkedseq_bench_splitmerge;
  name_chunkedseq_bench_chunksize;
  name_chunkedseq_bench_map;
]

let make_chunkedseq_bench name () =
  let ns = if name = "" then bench_names else [name] in
  build path_chunkedseq_bench ns false

(*****************************************************************************)

let nb_table_header_lines = 3

let header_names =
  [ [""]; ["Seq.";"Length";]; ["Nb."; "Repeat"]; ]

let nth_or_default l n dflt =
  if n < List.length l then List.nth l n else dflt

let add_cols l add f =
  let n = List.length l in
  ~~ List.iteri l (fun i x -> (f x); if i+1 < n then add " & " else ())

let build_chunkedseq_tables name bindings =
  let nb_infos = 1 + List.length header_names in
  let nb_sequences = List.length seq_info in
  let tex_file = file_tables_src name in
  let pdf_file = file_tables name in
  Mk_table.build_table tex_file pdf_file (fun add ->
   let infos = XList.init nb_infos (fun i -> if i = 0 then "l" else "r") in
   let seqs = XList.init nb_sequences (fun i -> "r") in
   add (Latex.tabular_begin (String.concat "|" (infos @ seqs)));
   for i=1 to nb_table_header_lines do
     add_cols header_names add (fun names -> add (nth_or_default names (i-1) ""));
     add " & ";
     add_cols seq_info add (fun (seq,pretty_names) ->
                            let pretty_name = nth_or_default pretty_names (i-1) "" in
                            add (sprintf " %s" pretty_name));
     (if i = nb_table_header_lines then
        add Latex.tabular_newline
      else
        add Latex.new_line)
   done;
   ~~ List.iter bindings (fun (name,f) ->
                       let file = file_results name in
                       let results = if Sys.file_exists file then
		                       Results.from_file file
                                     else []
                       in
		       (match results with
		        | [] -> ()
                        | _ -> f add results));
   add Latex.tabular_end)

let build_chunkedseq_table name f =
  build_chunkedseq_tables name [name,f]

let list_min x xs =
  List.fold_left min (List.hd xs) (List.tl xs)

let is_in_top_pct x xs pct =
  if x = nan then 
    false
  else
    let xs' = List.filter (fun y -> y != nan) xs in
    let best = list_min x xs' in
    let worst = best +. (best *. pct) in
    x <= worst

let emit_exectime_columns mk_all_sequences results add =
  let sequence_params = Params.to_envs mk_all_sequences in
  let nb_sequences = List.length sequence_params in
  let get_exectime seq = 
    let rs = Results.filter seq results in
    match rs with
    | [] -> nan
    | _ -> Results.get_mean_of "exectime" rs
  in
  let exectimes = List.map get_exectime sequence_params in
  ~~ List.iteri exectimes (fun i t ->
       let cell_str =
         if t = nan then
           ""
         else if is_in_top_pct t exectimes arg_top_pct then
           sprintf "\\textbf{%s}" (string_of_exectime t)
         else
           string_of_exectime t
       in
       Mk_table.cell ~last:(i = nb_sequences - 1) add cell_str)

(*****************************************************************************)
(** Configuration shared by FIFO, LIFO and splitmerge *)

let mk_n =
  mk int "n" 1000000000

let mk_r_values =
  mk_list int "r" [1000000;1000;1]

let fifolifo_info = 
  let s = [seq_stl_deque; seq_chunkedftree; seq_chunkedseq;] in
  List.filter (fun (n,_) -> List.exists (fun m -> m = n) s) seq_info

let mk_fifolifo_sequences =
  mk_sequences (List.map fst fifolifo_info)

let mk_fifolifo =
     mk_n
  &  mk_r_values
  &  mk_fifolifo_sequences

let mk_scenario sscenario =
  mk string "scenario" sscenario

(*****************************************************************************)
(** Fifo experiment *)

module ExpFifo = struct

let name = "fifo"

let prog = prog_chunkedseq_bench_fifolifo

let make = make_chunkedseq_bench name_chunkedseq_bench_fifolifo

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_scenario name
     & mk_fifolifo
      ) ] ))

let emit_table_rows add results =
  let env_rows = mk_r_values in
  ~~ List.iteri (Params.to_envs env_rows) (fun i env_rows ->
    let _ = if i = 0 then add "FIFO & " 
            else add " & " in
    let results = Results.filter env_rows results in
    let len = Results.get_unique_of "length" results in
    let rval = Results.get_unique_of "r" results in
    let _ = Mk_table.cell add (string_of_pow10 len) in
    let _ = Mk_table.cell add (string_of_pow10 rval) in
    let _ = emit_exectime_columns mk_all_sequences results add in 
    add Latex.new_line)

let plot() =
   build_chunkedseq_table name emit_table_rows

let all () = select make run nothing plot

end

(*****************************************************************************)
(** Lifo experiment *)

module ExpLifo = struct

let name = "lifo"

let prog = prog_chunkedseq_bench_fifolifo

let make = make_chunkedseq_bench name_chunkedseq_bench_fifolifo

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_scenario name
     & mk_fifolifo
      ) ] ))

let emit_table_rows add results =
  let env_rows = mk_r_values in
  ~~ List.iteri (Params.to_envs env_rows) (fun i env_rows ->
    let _ = if i = 0 then add "LIFO & " 
            else add " & " in
    let results = Results.filter env_rows results in
    let len = Results.get_unique_of "length" results in
    let rval = Results.get_unique_of "r" results in
    let _ = Mk_table.cell add (string_of_pow10 len) in
    let _ = Mk_table.cell add (string_of_pow10 rval) in
    let _ = emit_exectime_columns mk_all_sequences results add in 
    add Latex.new_line)

let plot() =
   build_chunkedseq_table name emit_table_rows

let all () = select make run nothing plot

end

(*****************************************************************************)
(** Chunk-size experiment *)

module ExpChunkSize = struct

let name = "chunk_size"

let prog = prog_chunkedseq_bench_chunksize

let make = make_chunkedseq_bench name_chunkedseq_bench_chunksize

let chunk_sizes = [64;128;256;512;1024;2048;4096;8192;]
let mk_chunk_sizes = mk_list int "chunk_size" chunk_sizes

let my_formatter_settings = Env.(
    ["scenario", Format_custom (fun s ->
         let f = if s = "fifo" then "FIFO" else "Split/merge + 0 push pop" in
         sprintf "%s" f)] 
  @ ["chunk_size", Format_hidden] 
  @ ["prog", Format_hidden])

let my_formatter =
   Env.format my_formatter_settings

let mk_chunk_size =
  let mk_sequence = mk string "sequence" seq_chunkedseq in
     (    mk_scenario "fifo"
        &  mk_sequence
        &  mk_n
        &  mk int "r" 1
        &  mk_chunk_sizes)
    ++
     (    mk_scenario "split_merge"
       &  mk_sequence
       &  mk_n
       &  mk int "r" 200000
       &  mk int "p" 5
       &  mk_chunk_sizes)

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_chunk_size
      ) ] ))

let plot() =
  Mk_scatter_plot.(call ([
    Scatter_plot_opt Scatter_plot.([
       Draw_lines true; 
       X_axis [Axis.Label "Chunk size";Axis.Is_log true];
       Y_axis [Axis.Lower (Some 0.)] ]);
    Formatter my_formatter;
    Charts (mk_prog prog);
    Series (mk_list string "scenario" ["fifo";"split_merge"]);
    Charts_formatter my_formatter;
    Series_formatter my_formatter;
    X mk_chunk_sizes;
    Y_label "runtime (seconds)";
    Input (file_results name);
    Output (file_plots name);
    ] 
    @ (y_as_mean "exectime")
    ))

let all () = select make run nothing plot

end

(*****************************************************************************)
(** Split-merge experiment *)

module ExpSplitMerge = struct

let name = "split_merge"

let prog = prog_chunkedseq_bench_splitmerge

let make = make_chunkedseq_bench name_chunkedseq_bench_splitmerge

let hs = [0;10;100;1000;]

let mk_hs = mk_list int "h" hs

let mk_r = mk int "r" 200000

let mk_split_merge =
     mk_scenario "split_merge"
  &  mk_list string "sequence" [seq_chunkedftree;seq_chunkedseq;seq_stl_rope]
  &  mk_n
  &  mk_r
  &  mk int "p" 5
  &  mk_hs

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_split_merge
      ) ] ))

let emit_table_rows add results =
  ~~ List.iteri (Params.to_envs mk_r) (fun i env_rows ->
    let results = Results.filter env_rows results in
    ~~ List.iteri (Params.to_envs mk_hs) (fun j env_rows ->
      let results = Results.filter env_rows results in
      let h = match (Results.get_int "h" results) with
        | [] -> -1
        | h::_ -> h in 
      let _ = if i = 0 then add (sprintf "Split/merge + %d push pop & " h)
              else add " & " in
      let len = Results.get_unique_of "length" results in
      let rval = Results.get_unique_of "r" results in
      let _ = Mk_table.cell add (string_of_pow10 len) in
      let _ = Mk_table.cell add (string_of_pow10 rval) in        
      let _ = emit_exectime_columns mk_all_sequences results add in 
      add Latex.new_line))

let plot() =
   build_chunkedseq_table name emit_table_rows

let all () = select make run nothing plot

end


(*****************************************************************************)
(** Filter experiment *)

module ExpFilter = struct

let name = "filter"

let prog = prog_chunkedseq_bench_filter

let make = make_chunkedseq_bench name_chunkedseq_bench_filter

let mk_r = mk_list int "r" [10000;1000;1]

let sequences = 
  [seq_stl_deque;seq_chunkedseq;seq_chunkedftree;seq_stl_rope]

let mk_filter =
     mk_scenario "filter"
  &  mk_list string "sequence" sequences
  &  mk int "n" 1000000000
  &  mk_r
  &  mk int "cutoff" 2048

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_filter
      ) ] ))

let emit_table_rows add results =
  ~~ List.iteri (Params.to_envs mk_r) (fun i env_rows ->
    let results = Results.filter env_rows results in
      let _ = if i = 0 then add "Filter & "
              else add " & " in
      let len = Results.get_unique_of "length" results in
      let rval = Results.get_unique_of "r" results in
      let _ = Mk_table.cell add (string_of_pow10 len) in
      let _ = Mk_table.cell add (string_of_pow10 rval) in        
      let _ = emit_exectime_columns mk_all_sequences results add in 
      add Latex.new_line)

let plot() =
   build_chunkedseq_table name emit_table_rows

let all () = select make run nothing plot

end

(*****************************************************************************)
(** Dynamic dictionary experiment *)

module ExpMap = struct

let name = "map"

let prog = prog_chunkedseq_bench_map

let make = make_chunkedseq_bench name_chunkedseq_bench_map

let mk_map =
    mk string "mode" "map"
  & mk_list string "map" ["stl_map";"chunkedseq_map";]
  & mk_list int "n"    [100000;1000000]
  & mk_list string "map_benchmark" ["insert";"change";"hit";"miss";"remove";]

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 4000;
     Args (
       mk_prog prog
     & mk_map
      ) ] ))

let plot = nothing

let all () = select make run nothing plot

end

(*****************************************************************************)
(* Shared definitions for graph experiments *)

let path_graph_bench = arg_path_to_pasl ^ "/graph/bench/"
let name_graph_bench = "search.all_opt2"
let prog_graph_bench = path_graph_bench ^ "/" ^ name_graph_bench

let make_graph_bench() =
  build path_graph_bench [name_graph_bench] false

let nb_vertex_id_bits = 64

let graph_benchmark_frontiers =
  [seq_stl_deque;seq_chunkedseq;seq_chunkedftree;]

let mk_all_frontiers =
  mk_list string "frontier" all_sequence_names

let mk_basic_graph_search salgo =
     mk int "bits" nb_vertex_id_bits
  &  mk string "load" "from_file"
  &  mk string "algo" salgo
  &  ExpGenerate.mk_graph_infiles

let graph_renaming = [
  "square_grid", "square grid";
  "perfect_bintree", "tree";
  "friendster", "Friendster";
]

let graph_renamer s =
  let a = String.rindex s '/' + 1 in
  let b = String.length ".adj_bin" in
  let c = String.length s in
  let g = String.sub s a (c - a - b) in
  try List.assoc g graph_renaming
  with Not_found -> g

let my_graph_formatter_settings = Env.(
  ["infile", Format_custom graph_renamer])

let my_graph_formatter =
  Env.format my_graph_formatter_settings

(*****************************************************************************)
(** DFS experiment *)

module ExpDfs = struct

let name = "dfs"

let algo = "dfs_by_vertexid_frontier"

let prog = prog_graph_bench

let make = make_graph_bench

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_basic_graph_search algo
     & mk_list string "frontier" graph_benchmark_frontiers
      ) ] ))

let emit_table_rows add results =
  ~~ List.iteri (Params.to_envs ExpGenerate.mk_graph_infiles) (fun i env_rows ->
      let results = Results.filter env_rows results in
      let infile = Env.get_as_string env_rows "infile" in
      let _ = add (sprintf "DFS %s & " (graph_renamer infile)) in
      let _ = Mk_table.cell add "" in
      let _ = Mk_table.cell add "" in        
      let _ = emit_exectime_columns mk_all_frontiers results add in 
      add Latex.new_line)

let plot() = 
   build_chunkedseq_table name emit_table_rows

let all () = select make run nothing plot

end

(*****************************************************************************)
(** BFS experiment *)

module ExpBfs = struct

let name = "bfs"

let algo = "bfs_by_dynamic_array"

let prog = prog_graph_bench

let make = make_graph_bench

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_basic_graph_search algo
     & mk_list string "frontier" graph_benchmark_frontiers
      ) ] ))

let emit_table_rows add results =
  ~~ List.iteri (Params.to_envs ExpGenerate.mk_graph_infiles) (fun i env_rows ->
      let results = Results.filter env_rows results in
      let infile = Env.get_as_string env_rows "infile" in
      let _ = add (sprintf "BFS %s & " (graph_renamer infile)) in
      let _ = Mk_table.cell add "" in
      let _ = Mk_table.cell add "" in        
      let _ = emit_exectime_columns mk_all_frontiers results add in 
      add Latex.new_line)

let plot() =
   build_chunkedseq_table name emit_table_rows

let all () = select make run nothing plot

end

(*****************************************************************************)
(** Parallel BFS experiment *)

module ExpPBfs = struct

let name = "pbfs"

let algo = "ls_pbfs"

let prog = prog_graph_bench

let make = make_graph_bench

let run() =
  Mk_runs.(call (run_modes @ [
     Output (file_results name);
     Timeout 400;
     Args (
       mk_prog prog
     & mk_basic_graph_search algo
     & mk_list string "frontier" [seq_ls_bag;seq_stl_deque;seq_chunkedseq;seq_chunkedftree;]
     & mk int "idempotent" 1
     & mk int "ls_pbfs_cutoff" 2048
      ) ] ))

let emit_table_rows add results =
  ~~ List.iteri (Params.to_envs ExpGenerate.mk_graph_infiles) (fun i env_rows ->
      let results = Results.filter env_rows results in
      let infile = Env.get_as_string env_rows "infile" in
      let _ = add (sprintf "PBFS %s & " (graph_renamer infile)) in
      let _ = Mk_table.cell add "" in
      let _ = Mk_table.cell add "" in        
      let _ = emit_exectime_columns mk_all_frontiers results add in 
      add Latex.new_line)

let plot() =
   build_chunkedseq_table name emit_table_rows

let all () = select make run nothing plot

end

(*****************************************************************************)
(** Full table report *)

module TableReport = struct

let all() = 
  let bindings = [
    ExpFifo.name,         ExpFifo.emit_table_rows;
    ExpLifo.name,         ExpLifo.emit_table_rows;
    ExpSplitMerge.name,   ExpSplitMerge.emit_table_rows;
    ExpFilter.name,       ExpFilter.emit_table_rows;
    ExpDfs.name,          ExpDfs.emit_table_rows;
    ExpBfs.name,          ExpBfs.emit_table_rows;
    ExpPBfs.name,         ExpPBfs.emit_table_rows;
  ]
  in
  build_chunkedseq_tables "paper" bindings

end

(*****************************************************************************)

let _ =
  let pasl_settings_all() =
    Pasl_settings.all arg_mode [path_chunkedseq_bench;path_graph_bench] 
  in
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "configure", pasl_settings_all;
    "generate", ExpGenerate.all;
    "fifo", ExpFifo.all;
    "lifo", ExpLifo.all;
    "chunk_size", ExpChunkSize.all;
    "split_merge", ExpSplitMerge.all;
    "filter", ExpFilter.all;
    "dfs", ExpDfs.all;
    "bfs", ExpBfs.all;
    "pbfs", ExpPBfs.all;
    "map", ExpMap.all;
    "report", TableReport.all;
  ] in
  let selected ks =
     ~~ List.iter bindings (fun (k,f) -> if List.mem k ks then f()) in
  let paper () = selected [  "generate"; "fifo"; "lifo"; "chunk_size"; "split_merge"; 
                             "filter"; "dfs"; "bfs"; "pbfs"; "report"; ] in
  Pbench.execute_from_only_skip arg_actions [] (bindings @ [
     "paper", paper;
     ]);
  ()
