<?php

/**
 * @file
 * The interpro XML parser
 */
function tripal_analysis_interpro_parseXMLFile($analysis_id, $interproxmlfile,
  $parsego, $query_re, $query_type, $query_uniquename, $job_id) {
  
  // clear out the anslysisfeature table for this analysis before getting started
  tripal_core_chado_delete('analysisfeature', array('analysis_id' => $analysis_id));

  // If user input a file(e.g. interpro.xml)
  if (is_file($interproxmlfile)) {
    tripal_analysis_interpro_parseSingleXMLFile($analysis_id, $interproxmlfile,
        $parsego, $query_re, $query_type, $query_uniquename, $job_id, 1);
  }
  else {
    $dir_handle = @opendir($interproxmlfile) or die("Unable to open $interproxmlfile");
    $pattern = sql_regcase($interproxmlfile . "/*.xml");
    $total_files = count(glob($pattern));
    print "$total_files file(s) to be parsed.\n";

    $interval = intval($total_files * 0.01);
    if ($interval == 0) {
      $interval = 1;
    }
    $no_file = 0;

    // Parsing all files in the directory
    while ($file = readdir($dir_handle)) {
      if (preg_match("/^.*\.xml/i", $file)) {
        tripal_analysis_interpro_parseSingleXMLFile($analysis_id, "$interproxmlfile/$file",
          $parsego, $query_re, $query_type, $query_uniquename, $job_id, 0, $no_file, $total_files);        
      }
      $no_file ++;
    }
  }

  print "\nDone\n";     
}
/**
 *
 */
function tripal_analysis_interpro_parseSingleXMLFile($analysis_id, $interproxmlfile,
    $parsego, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status = 1, 
    $file_no = 1, $total_files = 1) {
      
  $filename = preg_replace('/^.*\/(.*)$/' , '$1', $interproxmlfile);

  // Load the XML file
  $xml =  simplexml_load_file($interproxmlfile);
  if (!$xml) {
    watchdog('tripal_interpro', "Cannot open XML. Please check that it is valid XML.", NULL, WATCHDOG_ERROR);
    return;
  }
  
  // If starts with 'protein-matches' or 'nucleotide-sequence-matches' then this is InterPro v5 XML
  $is_xml4 = 0;
  $is_xml5 = 0;
  if (preg_match('/^protein-matches/', $xml->getname())) {
    $is_xml5 = 1;
  }
  elseif (preg_match('/^nucleotide-sequence-matches/', $xml->getname())) {
    $is_xml5 = 1;
  }
  
  // the  remaining are for InterPro v4 XML
  
  // If there is an EBI header then we need to skip that
  // and set our proteins array to be the second element of the array. This
  // occurs if results were generated with the online InterProScan tool.
  // if the XML starts in with the results then this happens when InterProScan
  // is used command-line and we can just use the object as is
  elseif (preg_match('/^EBIInterProScanResults/', $xml->getname())) {
    $children = $xml->children();
    $header = $children[0];
    $proteins = $children[1];
    $is_xml4 = 1;
  }
  // if the XML starts with the <interpro_matches> tag
  elseif (preg_match('/^interpro_matches/', $xml->getname())) {
    $proteins = $xml;
    $is_xml4 = 1;
  }
  else {
    watchdog('tr_ipr_parse', "ERROR: cannot parse XML file format is not recognized", array(), WATCHDOG_ERROR);
    return;
  }
  
  
  // Get cvterm_id for 'analysis_interpro_xmloutput_hits' which is required
  // for inserting into the analysisfeatureprop table
  $sql = "
    SELECT
      CVT.cvterm_id
    FROM {cvterm} CVT
      INNER JOIN {cv} ON cv.cv_id = CVT.cv_id
    WHERE
      CVT.name = 'analysis_interpro_xmloutput_hit' AND
      CV.name = 'tripal'
  ";
  $type_id = chado_query($sql)->fetchField();

  // Count the number of entires to be processed
  $no_iterations = 0;
  foreach ($proteins as $protein) {
    $no_iterations ++;
  }
  $interval = intval($no_iterations * 0.01);
  if ($interval == 0) {
    $interval = 1;
  }
  $idx_iterations = 0;

  // get the DB id for the GO database
  $parsego = tripal_analysis_get_property($analysis_id, 'analysis_interpro_parsego');
  $go_db_id = chado_query("SELECT db_id FROM {db} WHERE name='GO'")->fetchField();
  if ($parsego and !$go_db_id) {
    watchdog('tr_ipr_parse', 'GO schema not installed in chado. GO terms are not processed.', array(), WATCHDOG_WARNING);
  }

  // Processed each protein
  foreach ($proteins as $protein) {
    
    // Set job status
    $idx_iterations ++;    
    if ($idx_iterations % $interval == 0) {
      $percentage = sprintf("%.4f", ($idx_iterations / $no_iterations) * ($file_no/$total_files) * 100 + (($file_no)/$total_files)*100);
      tripal_job_set_progress($job_id, $percentage);
      print "Parsing file, $filename, $file_no of $total_files  ($percentage%). Memory: " . number_format(memory_get_usage()) . " bytes\r";
    }
           
    
    // match the protein id with the feature name
    $feature_id = 0;
    $attr = $protein->attributes();
    $seqname =$attr ['id'];

    // is the sequence name a generic name (i.e. 'Sequence_1') then the
    // results do not contain the original sequence names.  The only
    // option we have is to use the filename.  This will work in the case of
    // Blast2GO which stores the XML for each sequence in a file with the
    // the filename the name of the sequence
    if (preg_match('/Sequence_\d+/', $seqname)) {
      $filename = preg_replace('/^.*\/(.*).xml$/', '$1', $interproxmlfile);
      watchdog('tr_ipr_parse', "Sequence name for results is not specific, using filename: %filename as the sequence name\n", array('%filename' => $filename), WATCHDOG_NOTICE);
      $seqname = $filename;
    }

    // Remove _ORF from the sequence name
    $seqname = preg_replace('/^(.+)_\d+_ORF\d+.*/', '$1', $seqname);
     
    // if a regular expression is provided then pick out the portion requested
    if ($query_re and preg_match("/$query_re/", $seqname, $matches)) {
      $feature = $matches[1];
    }
    // If no match by the regular expression then get everything up to the first space
    else {
      if (preg_match('/^(.*?)\s.*$/',  $seqname, $matches)) {
        $feature = $matches[1];
      }
      // if no match up to the first space then just use the entire string
      else {
        $feature =  $seqname;
      }
    }

    if (!$feature and $query_re) {
      watchdog('tr_ipr_parse', "Failed: Cannot find feature for '%seqname' using the regular expression: %query_re",
        array('%seqname' => $seqname, '%query_re' => $query_re));
      continue;
    }

    // now find the feature in chado
    $select = array();
    if ($query_uniquename) {
      $select['uniquename'] = $feature;
    } 
    else {
      $select['name'] = $feature;
    }
    if ($query_type) {
      $select['type_id'] = array(
          'cv_id' => array(
              'name' => 'sequence'
          ),
          'name' => $query_type,
      );
    }

    $feature_arr = tripal_core_chado_select('feature', array('feature_id'), $select);
    if (count($feature_arr) > 1) {
      watchdog('tr_ipr_parse', "Ambiguous: '%feature' matches more than one feature and is being skipped.",
        array('%feature' => $feature), WATCHDOG_WARNING);
      continue;
    }
    if (count($feature_arr) == 0) {
      watchdog('tr_ipr_parse', "Failed: cannot find a matching feature for '%feature' in the database.",
        array('%feature' => $feature), WATCHDOG_ERROR);
      continue;
    }
    $feature_id = $feature_arr[0]->feature_id;

    // Successfully matched. Add analysis_id and
    // feature_id to analysisfeature. Add the table as XML to analysisfeatureprop
    if ($feature_id) {

      // Insert into analysisfeature table only if it doesn't already exist
      $values = array('feature_id' => $feature_id, 'analysis_id' => $analysis_id);
      $analysisfeature = tripal_core_chado_select('analysisfeature', array('*'), $values);
      if (count($analysisfeature) == 0) {
        $analysisfeature = tripal_core_chado_insert('analysisfeature', $values);
        $analysisfeature_id = $analysisfeature['analysisfeature_id'];
      } 
      else {
        $analysisfeature_id = $analysisfeature[0]->analysisfeature_id;      
      }

      // Insert interpro xml results into analysisfeatureprop table
      // but first check to see if we have an existing entry, if we do, then
      // increment the rank
      $sql = "
         SELECT analysisfeatureprop_id, rank
         FROM {analysisfeatureprop}
         WHERE analysisfeature_id = :analysisfeature_id AND type_id = :type_id
         ORDER BY rank DESC
      ";
      $result = chado_query($sql, array(':analysisfeature_id' => $analysisfeature_id, ':type_id' => $type_id))->fetchObject();
      $rank = 0;
      if ($result) {
        $rank = $result->rank + 1;
      }
      
      $values = array(
        'analysisfeature_id' => $analysisfeature_id,
        'type_id' => $type_id,
        'value' => $protein->asXML(),
        'rank' => $rank,
      );
      $options = array('statement_name' => 'ins_analysisfeatureprop_antyvara');
      $success = tripal_core_chado_insert('analysisfeatureprop', $values, $options);
      if (!$success) {
        watchdog('tr_ipr_parse', 'ERROR: cannot add the XML for the feature into the analysisfeatureprop table',
          array(), WATCHDOG_ERROR);
      }
       
      // parse the XML and add the IPR terms individually to the analysisfeatureprop table
      $ipr_obj = tripal_analysis_interpro_get_result_object($protein->asXML(), $feature_id);
      $iprterms = $ipr_obj['iprterms'];
      foreach ($iprterms as $iprterm) {
        if (strcmp($iprterm[0], 'noIPR')!=0) {
          // add the cvterm
          $term = array(
              'id' => "INTERPRO:$iprterm[0]",
              'name' => $iprterm[1],
              'def'  => $iprterm[1],
              'namespace' => 'INTERPRO',
              'is_obsolete' => 1,
          );
          // currently there is no InterPro Ontology OBO file so we can't
          // load the IPR terms that way, we need to just add them now
          // as we encounter them. If the term already exists
          // we do not want to update it.
          $ipr_cvterm = tripal_cv_add_cvterm($term, NULL, 0, 0);
          if (!$ipr_cvterm) {
            watchdog('tr_ipr_parse', "Failed: Cannot find cvterm: %name '%value'",
              array('%name' => $iprterm[0], '%value' =>  $iprterm[1]), WATCHDOG_ERROR);
            continue;
          }
           
          // Insert IPR terms into the feature_cvterm table
          // the default pub_id of 1 (NULL) is used. if the cvterm already exists then just skip adding it
          $values = array(
              'feature_id' => $feature_id,
              'cvterm_id' => $ipr_cvterm->cvterm_id,
              'pub_id' => 1,
          );
          $columns = array('*');
          $cvterm = tripal_core_chado_select('feature_cvterm', $columns, $values);
          if (count($cvterm) == 0) {
            $success = tripal_core_chado_insert('feature_cvterm', $values);
            if (!$success) {
              watchdog('tr_ipr_parse', "Failed: Cannot add cvterm: %name '%value'",
                array('%name' => $iprterm[0], '%value' =>  $iprterm[1]), WATCHDOG_ERROR);
              continue;
            }
          }
           
          // Insert IPR terms into the analysisfeatureprop table but only if it
          // doesn't already exist
          $values = array(
              'analysisfeature_id' => $analysisfeature_id,
              'type_id' => $ipr_cvterm->cvterm_id,
              'rank' => 0,
              'value' => $iprterm[0]
          );
          $options = array('statement_name' => 'sel_analysisfeatureprop_antyrava'); 
          $results = tripal_core_chado_select('analysisfeatureprop', array('analysisfeatureprop_id'), $values, $options);
          if (count($results) == 0 ) {
            $options = array('statement_name' => 'ins_analysisfeatureprop_antyrava'); 
            $success = tripal_core_chado_insert('analysisfeatureprop', $values, $options);
            if (!$success) {
              watchdog('tr_ipr_parse', "Failed:  Cannot add cvterm as an analysis feature property for the interpro term: %name '%value'",
                array('%name' => $iprterm[0], '%value' =>  $iprterm[1]), WATCHDOG_ERROR);
              continue;
            }
          }
        }
      }

      // parse the XML for each protein if GO terms are requested
      if ($parsego and $go_db_id) {
        $goterms = $ipr_obj['goterms'];

        // cycle through the GO terms and add them to the database
        foreach ($goterms as $goterm) {
           
          // seperate the 'GO:' from the term
          if (preg_match("/^.*?GO:(\d+).*$/", $goterm, $matches)) {

            // Find cvterm_id for the matched GO term
            $sql = "SELECT cvterm_id
            FROM {cvterm} CVT
            INNER JOIN {dbxref} DBX ON CVT.dbxref_id = DBX.dbxref_id
            WHERE DBX.accession = :accession AND DBX.db_id = :db_id";
            $goterm_id = chado_query($sql, array(':accession' => $matches[1], ':db_id' => $go_db_id))->fetchField();
            if (!$goterm_id) {
              watchdog('tr_ipr_parse', "Cannot find GO cvterm: 'GO:%term'. skipping.", 
                array('%term' => $matches[1]), WATCHDOG_WARNING);
              continue;
            }

            // Insert GO terms into feature_cvterm table. Default pub_id = 1 (NULL) was used. But
            // only insert if not already there
            $values = array(
                'feature_id' => $feature_id,
                'cvterm_id' => $goterm_id,
                'pub_id' => 1);
            $feature_cvterm = tripal_core_chado_select('feature_cvterm', array('*'), $values);
            if (sizeof($feature_cvterm) == 0) {
              $feature_cvterm = tripal_core_chado_insert('feature_cvterm', $values);
              if (!$feature_cvterm) {
                watchdog('tr_ipr_parse', "ERROR:  Cannot add cvterm to the feature for term: %goterm_id'\n",
                  array('%goterm_id' => $goterm_id), WATCHDOG_ERROR);
                continue;
              }
            }

            // Insert GO terms into analysisfeatureprop table
            $values = array('analysisfeature_id' => $analysisfeature_id,
                'type_id' => $goterm_id,
                'rank' => 0);
            $analysisfeatureprop = tripal_core_chado_select('analysisfeatureprop', array('*'), $values);
            if (sizeof($analysisfeatureprop) == 0) {
              $values['value'] = $matches[1];
              $analysisfeatureprop = tripal_core_chado_insert('analysisfeatureprop', $values);
            }
          } // end if preg_match
        } // end for each goterm
      } // end if ($parsego and $go_db_id)
    } // end if ($feature_id)
  } // end foreach ($proteins as $protein)  
  return;
}
/*******************************************************************************
 * Parses the Interpro XML for a single feature and returns an object
* containing the results.
*
* @param $interpro_xml
*   The XML results for a single feature.
* @param $feature_id
*   The feature ID to which this blast XML belongs.
*
* @return
*   An object with the following fields:
*
*      Details about interpro match for the feature
*        $ipr_obj['orf']
*        $ipr_obj['orf']['orf_id']
*        $ipr_obj['orf']['orf_length']
*        $ipr_obj['orf']['orf_crc64']
*
*      An arrray containing all of the IPR terms mapped to this feature
*        $ipr_obj['iprterms']
*
*      An array containing all of the GO terms mapped to the feature
*        $ipr_obj['goterms']
*
*      An array of terms. The variable $i indicates an index variable for
*      iterating through the terms
*        $ipr_obj['terms']
*        $ipr_obj['terms'][$i]['ipr_id']
*        $ipr_obj['terms'][$i]['ipr_name']
*        $ipr_obj['terms'][$i]['ipr_type']
*
*      Each feature may have one or more matches.  The variable $j indicates
*      an index variable for iterating through the matches.
*        $ipr_obj['terms'][$i]['matches'][$j]['match_id']
*        $ipr_obj['terms'][$i]['matches'][$j]['match_name']
*        $ipr_obj['terms'][$i]['matches'][$j]['match_dbname']
*        $ipr_obj['terms'][$i]['matches'][$j]['go_terms'][]
*
*      Each match can have multiple start and stop locations. The variable $k
*      indicates an index variable for iterating through the locations
*        $ipr_obj['terms'][$i]['matches'][$j]['locations'][$k]['match_start']
*        $ipr_obj['terms'][$i]['matches'][$j]['locations'][$k]['match_end']
*        $ipr_obj['terms'][$i]['matches'][$j]['locations'][$k]['match_score']
*        $ipr_obj['terms'][$i]['matches'][$j]['locations'][$k]['match_status']
*        $ipr_obj['terms'][$i]['matches'][$j]['locations'][$k]['match_evidence']
*
*/
function tripal_analysis_interpro_get_result_object($interpro_xml, $feature_id) {

  // Load the XML into an object
  $xml_obj = simplexml_load_string($interpro_xml);

  // iterate through each interpro results for this protein
  $results = array();
  $terms = array();
  $protein = array();
  $iprterms = array();
  $goterms = array();

  $term_count = 0;
  $match_count = 0;
   
  // get the properties of this result
  $attr = $xml_obj->attributes();
  $protein['orf_id'] = (string) $attr["id"];
  $protein['orf_length'] = (string) $attr["length"];
  $protein['orf_crc64'] = (string) $attr["crc64"];
   
  foreach ($xml_obj->children() as $intepro) {
    // get the interpro term for this match
    $attr = $intepro->attributes();
    $terms[$term_count]['ipr_id'] = (string) $attr["id"];
    $terms[$term_count]['ipr_name'] = (string) $attr["name"];
    $terms[$term_count]['ipr_type'] = (string) $attr["type"];
    $iprterms[] = array($terms[$term_count]['ipr_id'], $terms[$term_count]['ipr_name']);

    // iterate through the elements of the interpro result
    $matches[$term_count]['matches'] = array();
    $match_count = 0;
    foreach ($intepro->children() as $level1) {
      $element_name = $level1->getName();
      if ($element_name == 'match') {
        // get the match name for this match
        $attr = $level1->attributes();
        $terms[$term_count]['matches'][$match_count]['match_id'] = (string) $attr["id"];
        $terms[$term_count]['matches'][$match_count]['match_name'] = (string) $attr["name"];
        $terms[$term_count]['matches'][$match_count]['match_dbname'] = (string) $attr["dbname"];

        // get the location information for this match
        $loc_count = 0;
        foreach ($level1->children() as $level2) {
          $element_name = $level2->getName();
          if ($element_name == 'location') {
            $attr = $level2->attributes();
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_start'] = (string) $attr["start"];
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_end'] = (string) $attr["end"];
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_score'] = (string) $attr["score"];
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_status'] = (string) $attr["status"];
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_evidence'] = (string) $attr["evidence"];
            $loc_count++;
          }
        }
        $match_count++;
      }

      if ($element_name == 'classification') {
        $attr = $level1->attributes();
        if ($attr['class_type'] == 'GO') {
          $terms[$term_count]['matches'][$match_count]['go_terms'][] = (string) $attr['id'];
          $goterms[] = (string) $attr['id'];
        }
      }
    }
    $term_count++;
  }
  $results['terms'] = $terms;
  $results['orf'] = $protein;
  $results['iprterms'] = $iprterms;
  $results['goterms'] = $goterms;
  return $results;
}
