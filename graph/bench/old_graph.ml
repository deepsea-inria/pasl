

(*****************************************************************************)
(** Overview with DFS as baseline *)


let plot () =
   let results = Results.from_file (file_results name) in
   let results_baseline = Results.from_file (file_results ExpBaselines.name) in
   let results_accessible = Results.from_file (file_results ExpAccessible.name) in
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let env = Env.empty in
    let envs_tables = (mk_sizes) env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let results_baseline = Results.filter env_tables results_baseline in
       let results_accessible = Results.filter env_tables results_accessible in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       (* add (Env.get_as_string env "size");
       add Latex.new_line; *)
       let nb_infos = 2 in
       let nb_bfs = 3 in
       let nb_dfs = 3 in
       add (Latex.tabular_begin (String.concat "" (["|l||"] @ XList.init nb_infos (fun i -> "@{\\,\\,}c@{\\,\\,}|") @ ["c||"] @ XList.init nb_dfs (fun i -> "c|") @ ["|"] @ XList.init nb_bfs (fun i -> "c|")  @ ["|c|"] )));
       add "graph & vertices & edges & seq & Cong. & our & runtime & seq & LS & ours & our PDFS";
       add Latex.new_line;
       add " &  &  & DFS & PDFS & PDFS & diff. & BFS & PBFS & PBFS & vs PBFS";
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let results_baseline = Results.filter env_rows results_baseline in
         let results_accessible = Results.filter env_rows results_accessible in
         let env = Env.append env env_rows in
         let kind = Env.get_as_string env "kind" in
         let nb_vertices = Results.get_unique_of "nb_vertices" results_accessible in
         let _nb_visited_vertices = Results.get_unique_of "nb_visited" results_accessible in
         let nb_edges = Results.get_unique_of "nb_edges" results_accessible in
         let _nb_visited_edges = Results.get_unique_of "nb_edges_processed" results_accessible in
         let exectime_for rs mk_base =
            let rs = Results.filter (Params.to_env mk_base) rs in
            Results.check_consistent_inputs [] rs;
            let v = Results.get_mean_of "exectime" rs in
            v
            in
         let v_dfs_seq = exectime_for results_baseline ExpBaselines.mk_dfs in
         let v_dfs_cong = exectime_for results mk_cong_parallel_dfs in
         let v_dfs_our = exectime_for results mk_our_parallel_dfs in
         let v_bfs_seq = exectime_for results_baseline ExpBaselines.mk_bfs in
         let v_bfs_ls = exectime_for results mk_ls_bfs in
         let v_bfs_our = exectime_for results mk_our_lazy_parallel_bfs in

         Mk_table.cell add (graph_renamer kind);
         Mk_table.cell add (string_of_millions nb_vertices);
         Mk_table.cell add (string_of_millions nb_edges);
         (* Mk_table.cell add (string_of_millions nb_visited_edges); *)

         Mk_table.cell add (string_of_exectime ~prec:1 v_dfs_seq);
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_dfs_cong));
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_dfs_our));
         (* Mk_table.cell add (string_of_percentage_change_bounded 0.1 v_dfs_cong v_dfs_our); *)
         Mk_table.cell add (string_of_percentage_change v_dfs_cong v_dfs_our);
         (* Mk_table.cell ~last:true add (string_of_percentage_change v_bfs_our v_dfs_our); *)

         (* Mk_table.cell add (string_of_exectime ~prec:1 v_bfs_seq);
            Mk_table.cell add (string_of_speedup (v_bfs_seq /. v_bfs_ls));
            Mk_table.cell add (string_of_speedup (v_bfs_seq /. v_bfs_our));
         *)
         Mk_table.cell add (string_of_speedup ~prec:2 (v_dfs_seq /. v_bfs_seq));
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_bfs_ls));
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_bfs_our));
         (* Mk_table.cell add (string_of_percentage_change_bounded 0.1 v_bfs_ls v_bfs_our); *)
         (* Mk_table.cell add (string_of_percentage_change v_bfs_ls v_bfs_our);*)

         Mk_table.cell ~last:true add (string_of_speedup (v_bfs_our /. v_dfs_our)); 
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ))




(*****************************************************************************)
(** OverviewOld 

   ./bench.sh overview -proc 30 -size small -skip make

   to update one column only:
   ./bench.sh overview -proc 30 -size large -skip make -algo our_pbfs --replace

*)

module ExpOverviewOld = struct

let name = "overview_old"

let mk_parallel_bfs =
      mk_ls_bfs
   ++ mk_our_lazy_parallel_bfs

let mk_parallel_bfs =
   Params.eval (Params.filter env_in_arg_algos mk_parallel_bfs)

(*
let mk_parallel_dfs =
     mk_our_parallel_dfs
  ++ mk_cong_parallel_dfs

let mk_parallel_dfs =
   Params.eval (Params.filter env_in_arg_algos mk_parallel_dfs)
*)

let prog_parallel_here = (*"./search.sta"*)  "./search.opt2" 

let mk_parallel_prog_maxproc_here =
   mk_prog prog_parallel_here
   & mk int "proc" arg_proc
   & mk int "delta" 50

let make () =
   build [prog_parallel_here] 

let run () =
   Mk_runs.(call (run_modes @ [
      Output (file_results name);
      Args ( 
            mk int "idempotent" 0
          & mk_graph_inputs
          & (   (mk_parallel_prog_maxproc_here & mk_traversal_bfs & mk_parallel_bfs)
             ++ (mk_parallel_prog_maxproc_here  & mk_traversal_dfs & mk_parallel_dfs)))
      ]))

let check () =
   let results =
       Results.from_file (file_results name)
     @ Results.from_file (file_results ExpBaselines.name)
     in
   Results.check_consistent_output_filter_by_params
      "result" (mk_graph_files & mk_traversals) results

let plot () =
   let results = Results.from_file (file_results name) in
   let results_baseline = Results.from_file (file_results ExpBaselines.name) in
   let results_accessible = Results.from_file (file_results ExpAccessible.name) in
   let tex_file = file_tables_src name in
   let pdf_file = file_tables name in
   Mk_table.build_table tex_file pdf_file (fun add ->
    let env = Env.empty in
    let envs_tables = (mk_sizes) env in
    ~~ List.iter envs_tables (fun env_tables ->
       let results = Results.filter env_tables results in
       let results_baseline = Results.filter env_tables results_baseline in
       let results_accessible = Results.filter env_tables results_accessible in
       let env = Env.append env env_tables in
       let envs_rows = mk_kind_for_size env in
       (* add (Env.get_as_string env "size");
       add Latex.new_line; *)
       let nb_infos = 2 in
       let nb_bfs = 4 in
       let nb_dfs = 4 in
       add (Latex.tabular_begin (String.concat "" (["|l||"] @ XList.init nb_infos (fun i -> "@{\\,\\,}c@{\\,\\,}|") @ ["|"] @ XList.init nb_bfs (fun i -> "c|") @ ["|"] @ XList.init nb_dfs (fun i -> "c|")  @ ["|c|"] )));
       add "graph & vertices & edges & seq & LS & ours & runtime & seq & Cong. & our & runtime & our PDFS";
       add Latex.new_line;
       add " &  &  & BFS & PBFS & PBFS & diff & DFS & PDFS & PDFS & diff. & vs PBFS";
       add Latex.tabular_newline;
       ~~ List.iter envs_rows (fun env_rows ->
         let results = Results.filter env_rows results in
         let results_baseline = Results.filter env_rows results_baseline in
         let results_accessible = Results.filter env_rows results_accessible in
         let env = Env.append env env_rows in
         let kind = Env.get_as_string env "kind" in
         let nb_vertices = Results.get_unique_of "nb_vertices" results_accessible in
         let _nb_visited_vertices = Results.get_unique_of "nb_visited" results_accessible in
         let nb_edges = Results.get_unique_of "nb_edges" results_accessible in
         let _nb_visited_edges = Results.get_unique_of "nb_edges_processed" results_accessible in
         let exectime_for rs mk_base =
            let rs = Results.filter (Params.to_env mk_base) rs in
            Results.check_consistent_inputs [] rs;
            let v = Results.get_mean_of "exectime" rs in
            v
            in
         let v_bfs_seq = exectime_for results_baseline ExpBaselines.mk_bfs in
         let v_bfs_ls = exectime_for results mk_ls_bfs in
         let v_bfs_our = exectime_for results mk_our_lazy_parallel_bfs in
         let v_dfs_seq = exectime_for results_baseline ExpBaselines.mk_dfs in
         let v_dfs_cong = exectime_for results mk_cong_parallel_dfs in
         let v_dfs_our = exectime_for results mk_our_parallel_dfs in
         Mk_table.cell add (graph_renamer kind);
         Mk_table.cell add (string_of_millions nb_vertices);
         Mk_table.cell add (string_of_millions nb_edges);
         (* Mk_table.cell add (string_of_millions nb_visited_edges); *)
         Mk_table.cell add (string_of_exectime ~prec:1 v_bfs_seq);
         Mk_table.cell add (string_of_speedup (v_bfs_seq /. v_bfs_ls));
         Mk_table.cell add (string_of_speedup (v_bfs_seq /. v_bfs_our));
         (* Mk_table.cell add (string_of_percentage_change_bounded 0.1 v_bfs_ls v_bfs_our); *)
         Mk_table.cell add (string_of_percentage_change v_bfs_ls v_bfs_our);
         Mk_table.cell add (string_of_exectime ~prec:1 v_dfs_seq);
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_dfs_cong));
         Mk_table.cell add (string_of_speedup (v_dfs_seq /. v_dfs_our));
         (* Mk_table.cell add (string_of_percentage_change_bounded 0.1 v_dfs_cong v_dfs_our); *)
         Mk_table.cell add (string_of_percentage_change v_dfs_cong v_dfs_our);
         (* Mk_table.cell ~last:true add (string_of_percentage_change v_bfs_our v_dfs_our); *)
         Mk_table.cell ~last:true add (string_of_speedup (v_bfs_our /. v_dfs_our)); 
         add Latex.tabular_newline;
         );
       add Latex.tabular_end;
       add Latex.new_page;
      ))

let all () =
   select make run check plot

end

