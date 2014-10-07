<?php

/*
Usage:  php filterobjdfs.php $build_folder $source_base
  where, e.g., $build_folder is _build/exe
  and where, e.g., $source_base is "myprog" (assuming myprog.cpp contains the main)

Output:  on stdout, a list of path to *.ok files.
   Note that all these object files have path of the form $build_bolder/*.o
   The *.o files are computed by following the dependencies between
   the *.ok files, using a depth-first search exploration.
   These dependencies are stored in the foo.*.d files located in the 
   folder $build_folder.
*/

// for debugging calls: fwrite(STDERR, "call on ".$argv[1]." ".$argv[2]."\n");

function error($s) {
   fwrite(STDERR, $s."\n");
   exit(1);
}

if (count($argv) != 3) 
   error("not the right number of arguments");
$build_folder = $argv[1]; // used as global
$source_base = $argv[2];

function parse_targets($file, $base) {
   global $build_folder;
   if (! file_exists($file))
      return array();
   $subject = file_get_contents($file);
   $head = preg_quote($build_folder.'/'.$base.'.ok: '.$build_folder.'/');
   $pattern = '`'.$head.'(.*)\\.ok`U';
   $matches = array();
   preg_match_all($pattern, $subject, $matches);
   return $matches[1];
}

function get_targets($base) {
   global $build_folder;
   $pattern = $build_folder.'/'.$base.'.*.d';
   $files = glob($pattern);
   if (empty($files)) {
      error("Warning: did not find any file named $pattern.");
      return array();
   }
   $all_results = array();
   foreach ($files as $file) {
      $results = parse_targets($file, $base);
      $all_results = array_merge($all_results, $results);
   }
   return $all_results;
}

function explore_from($node, & $visited) {
   if (isset($visited[$node]))
      return;
   $visited[$node] = true;
   $edges = get_targets($node);
   foreach ($edges as $target)
      explore_from($target, $visited);
}

function compute_list_of_bases($build_folder, $source_base) {
   $visited = array();
   explore_from($source_base, $visited);
   $results = array();
   foreach ($visited as $base => $dummy) 
      $results[] = $build_folder.'/'.$base.'.ok';
   return $results;
}

$results = compute_list_of_bases($build_folder, $source_base);
echo implode(' ', $results);


?>