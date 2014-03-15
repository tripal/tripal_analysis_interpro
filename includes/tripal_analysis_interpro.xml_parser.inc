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
  if (preg_match('/^protein-matches/', $xml->getname())) {
    tripal_analysis_interpro_parseXML5($xml, $analysis_id, $filename,
      $parsego, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
      $file_no, $total_files);
  }
  elseif (preg_match('/^nucleotide-sequence-matches/', $xml->getname())) {
    tripal_analysis_interpro_parseXML5($xml, $analysis_id, $filename,
      $parsego, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
      $file_no, $total_files);
  } 
  // the  remaining are for InterPro v4 XML
  elseif (preg_match('/^EBIInterProScanResults/', $xml->getname())) {
    // results are from the EBI InterProScan Website Output
    tripal_analysis_interpro_parseXML4($xml, $analysis_id, $filename,
      $parsego, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
      $file_no, $total_files);
  }
  elseif (preg_match('/^interpro_matches/', $xml->getname())) {
    // rseults are from the command-line InterPrScan version
    tripal_analysis_interpro_parseXML4($xml, $analysis_id, $filename,
      $parsego, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
      $file_no, $total_files);
  }
  else {
    watchdog('tr_ipr_parse', "ERROR: cannot parse XML file format is not recognized", array(), WATCHDOG_ERROR);
    return;
  }
}
/**
 * 
 * @param $xml
 * @param $analysis_id
 * @param $filename
 * @param $parsego
 * @param $query_re
 * @param $query_type
 * @param $query_uniquename
 * @param $job_id
 * @param $uptate_status
 * @param $file_no
 * @param $total_files
 */
function tripal_analysis_interpro_parseXML5($xml, $analysis_id, $filename,
  $parsego, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
  $file_no, $total_files) {

  // Count the number of sequences to be processed
  $no_iterations = 0;
  foreach ($xml as $entity) {
    $no_iterations++;
  }
  $interval = intval($no_iterations * 0.01);
  if ($interval == 0) {
    $interval = 1;
  }
  $idx_iterations = 0;
  
  // Processed each nucelotide sequence entry
  foreach ($xml as $entity) {
    
    // Set job status
    $idx_iterations ++;
    if ($idx_iterations % $interval == 0) {
      $percentage = sprintf("%.4f", ($idx_iterations / $no_iterations) * ($file_no/$total_files) * 100 + (($file_no)/$total_files)*100);
      tripal_job_set_progress($job_id, $percentage);
      print "Parsing file, $filename, $file_no of $total_files  ($percentage%). Memory: " . number_format(memory_get_usage()) . " bytes\r";
    }
    
    // get details about this sequence
    $children = $entity->children();
    foreach ($children as $child) {
      $childname = $child->getname();
      if ($childname == 'xref') {
        $attrs   = $child->attributes();
        $seqid   = (string) $attrs['id'];
        $seqname = (string) $attrs['name'];
        break;
      }
    }

    // is the sequence id is 'submitted' then the
    // results do not contain the original sequence names.  The only
    // option we have is to use the filename.  This will work in the case of
    // Blast2GO which stores the XML for each sequence in a file with the
    // the filename the name of the sequence
    if (id == "Submitted") {
      $filename = preg_replace('/^.*\/(.*).xml$/', '$1', $interproxmlfile);
      watchdog('tr_ipr_parse', "Sequence name for results is not specific, using filename: %filename as the sequence name\n", array('%filename' => $filename), WATCHDOG_NOTICE);
      $seqid = $filename;
    }
  
    // match the name of the feature in the XML file to a feature in Chado
    $feature_id = tripal_analysis_interpro_match_feature($seqid, $query_re, $query_uniquename);
    if (!$feature_id) {
      continue;
    }
    
    // create an entry in the analysisfeature table and add the XML for this feature
    // to the analysisfeatureprop table
    $analysisfeature_id = tripal_analysis_interpro_add_analysis_feature($feature_id, $analysis_id, $entity);
    if (!$analysisfeature_id) {
      continue;
    }

    // parse the XML
    $ipr_array = tripal_analysis_interpro_parse_feature_XML($entity->asXML(), $feature_id);
    
    // add in the IPR terms
    $iprterms = $ipr_array['iprterms'];
    tripal_analysis_interpro_load_iprterms($iprterms, $feature_id, $analysisfeature_id);
    
    // get the DB id for the GO database
    $parsego = tripal_analysis_get_property($analysis_id, 'analysis_interpro_parsego');
    $go_db_id = chado_query("SELECT db_id FROM {db} WHERE name = 'GO'")->fetchField();
    if ($parsego and !$go_db_id) {
      watchdog('tr_ipr_parse', 'GO schema not installed in chado. GO terms are not processed.', array(), WATCHDOG_WARNING);
    }
    if ($parsego and $go_db_id) {
      $goterms = $ipr_array['goterms'];
      tripal_analysis_interpro_load_goterms($goterms, $feature_id, $analysisfeature_id, $go_db_id);
    }
  } // end foreach ($xml as $entity)
}

/**
 * 
 */
function tripal_analysis_interpro_add_analysis_feature($feature_id, $analysis_id, $xml_obj) {
  
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
  $args = array(':analysisfeature_id' => $analysisfeature_id, ':type_id' => $type_id); 
  $result = chado_query($sql, $args)->fetchObject();
  $rank = 0;
  if ($result) {
    $rank = $result->rank + 1;
  }
  
  $values = array(
    'analysisfeature_id' => $analysisfeature_id,
    'type_id' => $type_id,
    'value' => $xml_obj->asXML(),
    'rank' => $rank,
  );
  $success = tripal_core_chado_insert('analysisfeatureprop', $values);
  if (!$success) {
    watchdog('tr_ipr_parse', 'ERROR: cannot add the XML for the feature into the analysisfeatureprop table',
    array(), WATCHDOG_ERROR);
  }
  
  return $analysisfeature_id;
}
/**
 * @param $xml
 */
function tripal_analysis_interpro_parseXML4($xml, $analysis_id, $filename,
      $parsego, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
      $file_no, $total_files) {

  // If there is an EBI header then we need to skip that
  // and set our proteins array to be the second element of the array. This
  // occurs if results were generated with the online InterProScan tool.
  // if the XML starts in with the results then this happens when InterProScan
  // is used command-line and we can just use the object as is
  if (preg_match('/^EBIInterProScanResults/', $xml->getname())) {
    $children = $xml->children();
    $header = $children[0];
    $proteins = $children[1];
  }
  // if the XML starts with the <interpro_matches> tag
  elseif (preg_match('/^interpro_matches/', $xml->getname())) {
    $proteins = $xml;
  }

  // Count the number of entries to be processed
  $no_iterations = 0;
  foreach ($proteins as $protein) {
    $no_iterations++;
  }
  $interval = intval($no_iterations * 0.01);
  if ($interval == 0) {
    $interval = 1;
  }
  $idx_iterations = 0;

  // Processed each protein
  foreach ($proteins as $protein) {
    
    // Set job status
    $idx_iterations ++;    
    if ($idx_iterations % $interval == 0) {
      $percentage = sprintf("%.4f", ($idx_iterations / $no_iterations) * ($file_no / $total_files) * 100 + (($file_no) /$total_files) * 100);
      tripal_job_set_progress($job_id, $percentage);
      print "Parsing file, $filename, $file_no of $total_files  ($percentage%). Memory: " . number_format(memory_get_usage()) . " bytes\r";
    }

    // match the protein id with the feature name
    $feature_id = 0;
    $attrs = $protein->attributes();
    $seqname = $attrs['id'];

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
    
    // match the name of the feature in the XML file to a feature in Chado
    $feature_id = tripal_analysis_interpro_match_feature($seqname, $query_re, $query_uniquename);
    if (!$feature_id) {
      continue;
    }

    // create an entry in the analysisfeature table and add the XML for this feature
    // to the analysisfeatureprop table
    $analysisfeature_id = tripal_analysis_interpro_add_analysis_feature($feature_id, $analysis_id, $protein);
    if (!$analysisfeature_id) {
      continue;
    }
     
    // parse the XML
    $ipr_array = tripal_analysis_interpro_parse_feature_XML($protein->asXML(), $feature_id);
    
    // add in the IPR terms
    $iprterms = $ipr_array['iprterms'];
    tripal_analysis_interpro_load_iprterms($iprterms, $feature_id, $analysisfeature_id);
    
    // get the DB id for the GO database
    $parsego = tripal_analysis_get_property($analysis_id, 'analysis_interpro_parsego');
    $go_db_id = chado_query("SELECT db_id FROM {db} WHERE name='GO'")->fetchField();
    if ($parsego and !$go_db_id) {
      watchdog('tr_ipr_parse', 'GO schema not installed in chado. GO terms are not processed.', array(), WATCHDOG_WARNING);
    }
    // add in the GO Terms
    if ($parsego and $go_db_id) {
      $goterms = $ipr_array['goterms'];
      tripal_analysis_interpro_load_goterms($goterms, $feature_id, $analysisfeature_id, $go_db_id);
    }
  } // end foreach ($proteins as $protein) 
     
  return;
}
/**
 * Attempts to find a feature in Chado that matches the sequence id as listed
 * in the InterProScan XML file
 * 
 * @param $seqid
 *   The sequence name provided by the InterProXML file
 * @param $query_re
 *   The regular expression provided by the user when the chado_analysis_interpro
 *   node was created by the user
 * @param $query_uniquename
 *   The value provided by the user when the chado_analysis_interpro node was
 *   created that indicates TRUE if the $seqid is the uniquename. Otherwise
 *   it is assumed to just be the feature name.
 *   
 * @return
 *   The feature_id of the matching feature or NULL if not found.  
 */
function tripal_analysis_interpro_match_feature($seqid, $query_re, $query_uniquename) {

  $feature = '';
  
  // if a regular expression is provided then pick out the portion requested
  $matches = array();
  if ($query_re and preg_match("/$query_re/", $seqid, $matches)) {
    $feature = $matches[1];
  }
  // If no match by the regular expression then get everything up to the first space
  else {
    if (preg_match('/^(.*?)\s.*$/',  $seqid, $matches)) {
      $feature = $matches[1];
    }
    // if no match up to the first space then just use the entire string
    else {
      $feature =  $seqid;
    }
  }
  
  if (!$feature and $query_re) {
    watchdog('tr_ipr_parse', "Failed: Cannot find feature for '%seqname' using the regular expression: %query_re",
      array('%seqname' => $seqname, '%query_re' => $query_re));
    return NULL;
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
    return NULL;
  }
  if (count($feature_arr) == 0) {
    watchdog('tr_ipr_parse', "Failed: cannot find a matching feature for '%feature' in the database.",
    array('%feature' => $feature), WATCHDOG_ERROR);
    return NULL;
  }
  $feature_id = $feature_arr[0]->feature_id;
  
  return $feature_id;
}
/**
 * Adds the InterProScan IPR terms to the analysisfeatureprop table and the
 * feature_cvterm table for the feature.  If the IPR terms do not exist in 
 * the 'INTERPRO' vacabulary then they are automatically added
 * 
 * @param $ipterms
 *   An array of terms with each term being an array if three elements where
 *   element 0 is the IPR accession, element 1 is the name of the term and
 *   element 3 is the description 
 * @param $feature_id
 *   The feature to which the terms should be assocaited
 * @param $analysisfeature_id
 *   The analysisfeature to which the terms should be listed as properties 
 *
 */
function tripal_analysis_interpro_load_iprterms($iprterms, $feature_id, $analysisfeature_id) {
  
  // iterate through each othe IPR terms
  foreach ($iprterms as $iprterm) {
    
    if (strcmp($iprterm[0], 'noIPR')!=0) {
      
      // add all terms but the 'noIPR' term
      $term = array(
        'id' => "INTERPRO:$iprterm[0]",
        'name' => $iprterm[1],
        'def'  => $iprterm[2],
        'namespace' => 'INTERPRO',
        'is_obsolete' => 1,
      );
      // currently there is no InterPro Ontology OBO file so we can't
      // load the IPR terms that way, we need to just add them
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
}
/**
 * Adds GO terms to the analysisfeatureprop  table and the feature_cvterm
 * table for the feature.
 *  
 * @param $goterms
 *   An array containin each GO terms in teh form GO:XXXXXX
 * @param $feature_id
 *   The feature to which the terms should be assocaited
 * @param $analysisfeature_id
 *   The analysisfeature to which the terms should be listed as properties 
 * @param $go_db_id
 *   The db_id for the GO (Gene Ontology)
 */
function tripal_analysis_interpro_load_goterms($goterms, $feature_id, $analysisfeature_id, $go_db_id) {
  
  // cycle through the GO terms and add them to the database
  foreach ($goterms as $goterm) {
     
    // seperate the 'GO:' from the term
    if (preg_match("/^.*?GO:(\d+).*$/", $goterm, $matches)) {
  
      // Find cvterm_id for the matched GO term
      $sql = "
        SELECT cvterm_id
        FROM {cvterm} CVT
          INNER JOIN {dbxref} DBX ON CVT.dbxref_id = DBX.dbxref_id
        WHERE DBX.accession = :accession AND DBX.db_id = :db_id
      ";
      $args = array(':accession' => $matches[1], ':db_id' => $go_db_id);
      $goterm_id = chado_query($sql, $args)->fetchField();
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
        'pub_id' => 1
      );
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
      $values = array(
        'analysisfeature_id' => $analysisfeature_id,
        'type_id' => $goterm_id,
        'rank' => 0
      );
      $analysisfeatureprop = tripal_core_chado_select('analysisfeatureprop', array('*'), $values);
      if (sizeof($analysisfeatureprop) == 0) {
        $values['value'] = $matches[1];
        $analysisfeatureprop = tripal_core_chado_insert('analysisfeatureprop', $values);
      }
    } // end if preg_match
  } // end for each goterm
}
/**
 * Parses the Interpro XML for a single feature and returns an array
 * containing the results.
 *
 * @param $interpro_xml
 *   The XML results for a single feature.
 * @param $feature_id
 *   The feature ID to which this blast XML belongs.
 *
 * @return
 *   An array whose structure varies depending if XML is v5 or v4. See the
 *   return value from these functions for specific structure: 
 *     tripal_analysis_interpro_parse_feature_XML5_protein()
 *     tripal_analysis_interpro_parse_feature_XML5_nucleotide()
 *     tripal_analysis_interpro_parse_feature_XML4()
 *   
 *
 */
function tripal_analysis_interpro_parse_feature_XML($interpro_xml, $feature_id) {

  // Load the XML into an object
  $xml_obj = simplexml_load_string($interpro_xml);
  
  // we can distinguish between XML4 and XML5 by looking at the name and attributes
  $name  = $xml_obj->getname();
  $attrs = $xml_obj->attributes();
  
  if ($name == 'nucleotide-sequence') {
   return tripal_analysis_interpro_parse_feature_XML5_nucelotide($xml_obj, $feature_id);
  }
  if ($name == 'protein') {
    // XML 5 protein key has no attributes XML4 does
    if (count($attrs) == 0) {
      return tripal_analysis_interpro_parse_feature_XML5_protein($xml_obj, $feature_id);
    }
    return tripal_analysis_interpro_parse_feature_XML4($xml_obj, $feature_id);
  }
}
/**
 * Parses the Interpro XML for a single feature and returns an array
 * containing the results.
 *
 * @param $interpro_xml
 *   The XML results for a single feature.
 * @param $feature_id
 *   The feature ID to which this blast XML belongs.
 *
 * @return
 *   An array in the following structure
 *   
 *      The format of this array
 *        $protein['format'] = 'XML5';
 *
 *      Details about interpro match for the feature
 *        $protein['orf']
 *        $protein['orf']['start']
 *        $protein['orf']['end']
 *        $protein['orf']['strand']
 *
 *      An arrray containing all of the IPR terms mapped to this feature. Each
 *      IPR term is an array with 3 elements. The first element is the IPR
 *      accession, the second is the name and the third is the description
 *        $protein['iprterms']
 *
 *      An array containing all of the GO terms mapped to the feature. Each GO
 *      term is simply a string of the format GO:XXXXXX
 *        $protein['goterms']
 *
 *      An array of terms. The variable $i indicates an index variable for
 *      iterating through the terms
 *        $protein['matches']
 *        $protein['matches'][$i]['ipr_id']
 *        $protein['matches'][$i]['ipr_name']
 *        $protein['matches'][$i]['ipr_type']
 *        $protein['matches'][$i]['match_id']
 *        $protein['matches'][$i]['match_name']
 *        $protein['matches'][$i]['match_dbname']
 *        $protein['matches'][$i]['match_version']
 *        $protein['matches'][$i]['go_terms'][]
 *
 *      Each match can have multiple start and stop locations. The variable $k
 *      indicates an index variable for iterating through the locations
 *        $protein['matches'][$i]['locations'][$k]['match_start']
 *        $protein['matches'][$i]['locations'][$k]['match_end']
 *        $protein['matches'][$i]['locations'][$k]['match_score']
 *        $protein['matches'][$i]['locations'][$k]['match_evalue']
 *        $protein['matches'][$i]['locations'][$k]['match_level']
 *
 */
function tripal_analysis_interpro_parse_feature_XML5_protein($xml_obj, $feature_id) {

  $protein = array();

  // iterate through each element of the 'protein' children
  foreach ($xml_obj->children() as $element) {
    
    if($element->getname() == 'matches') {
      // iterate through the match elements
      $i = 0;
      foreach ($element->children() as $match_element) {
        
        $attrs = $match_element->attributes();
        $protein['matches'][$i]['evalue']    = (string) $attrs['evalue'];
        $protein['matches'][$i]['score']     = (string) $attrs['score'];

        foreach ($match_element->children() as $match_details) {

          if ($match_details->getname() == 'signature') {
            $attrs = $match_details->attributes();
            $protein['matches'][$i]['match_name'] = (string) $attrs['name']; // the name of the match
            $protein['matches'][$i]['match_desc'] = (string) $attrs['desc']; // the match description
            $protein['matches'][$i]['match_id']   = (string) $attrs['ac'];   // the library accession number
            
            foreach ($match_details->children() as $sig_element) {
              if ($sig_element->getname() == 'entry') {
                $attrs = $sig_element->attributes();
                $protein['matches'][$i]['ipr_type'] = (string) $attrs['type']; // e.g. DOMAIN, SITE
                $protein['matches'][$i]['ipr_name'] = (string) $attrs['name']; // the match name
                $protein['matches'][$i]['ipr_desc'] = (string) $attrs['desc']; // the match description
                $protein['matches'][$i]['ipr_id']   = (string) $attrs['ac'];   // the IPR accession number
                $protein['iprterms'][] = array((string) $attrs['ac'], (string) $attrs['name'], (string) $attrs['desc']);
                
                // get the GO terms which are children of the <entry> element
                foreach ($sig_element->children() as $entry_element) {
                  if ($entry_element->getname() == 'go-xref') {
                    $attrs = $entry_element->attributes();
                    $protein['matches'][$i]['go_terms'][] = (string) $attrs['id'];
                    $protein['goterms'][] = (string) $attrs['id'];
                  }
                }
              }
              if ($sig_element->getname() == 'models') {
                // ignore the model we don't need it
              }
              if ($sig_element->getname() == 'signature-library-release') {
                $attrs = $sig_element->attributes();
                $protein['matches'][$i]['match_dbname']  = (string) $attrs['library'];
                $protein['matches'][$i]['match_version'] = (string) $attrs['version'];
              }
            }
          } // end if($match_element->getname() == 'signature') {
          if($match_details->getname() == 'locations') {
            $k = 0;
            foreach ($match_details->children() as $loc_element) {
              $attrs = $loc_element->attributes();
              $protein['matches'][$i]['locations'][$k]['match_start']  = (string) $attrs['start'];
              $protein['matches'][$i]['locations'][$k]['match_end']    = (string) $attrs['end'];
              $protein['matches'][$i]['locations'][$k]['match_score']  = (string) $attrs['score'];
              $protein['matches'][$i]['locations'][$k]['match_evalue'] = (string) $attrs['evalue'];
              $protein['matches'][$i]['locations'][$k]['match_level']  = (string) $attrs['level'];
              $k++;
            }
          }
        } // end foreach ($match_element->children() as $match_details) {
      } // end foreach ($element->children() as $match_element) {
      $i++;
    } // end if($element->getname() == 'matches') {
  } // end foreach ($xml_obj->children() as $element) {
  
  // proteins don't have orfs but nucleotide-sequences do. We just initialize it here
  // as this array is returned by both XML5 functions and it needs to be the same
  $protein['format']        = 'XML5';
  $protein['orf']['name']   = '';
  $protein['orf']['start']  = '';
  $protein['orf']['end']    = '';
  $protein['orf']['strand'] = '';
  $protein['orf']['num_matches'] = $i;
  return $protein;
}
/**
 * Parses the Interpro XML for a single feature and returns an array
 * containing the results.
 *
 * @param $interpro_xml
 *   The XML results for a single feature.
 * @param $feature_id
 *   The feature ID to which this blast XML belongs.
 *   
 * @return
 *   An array of proteins.  Each protein is an array with the same format 
 *   as returned by the tripal_analysis_interpro_parse_feature_XML5_protein() function
 */
function tripal_analysis_interpro_parse_feature_XML5_nucelotide($xml_obj, $feature_id) {

  $proteins = array();
  $iprterms = array();
  $goterms  = array();
  
  // iterate through each element of the 'nucleotide-sequence' children
  foreach ($xml_obj->children() as $element) {
    if($element->getname() == 'orf') {
      
      foreach ($element->children() as $sub_element) {
        if ($sub_element->getname() == 'protein') {
          $orf = tripal_analysis_interpro_parse_feature_XML5_protein($sub_element, $feature_id);
        }
        // add in the ORF information to the protein
        $attr = $element->attributes();
        $orf['orf']['end']    = (string) $attr["end"];
        $orf['orf']['start']  = (string) $attr["start"];
        $orf['orf']['strand'] = (string) $attr["strand"];
        
        // concatenate all the iprterms for all orfs
        if (array_key_exists('iprterms', $orf)) {
          foreach ($orf['iprterms'] as $iprterm) {
            $iprterms[] = $iprterm;
          }
        }
        
        // concatenenate all the go terms for all orfs
        if (array_key_exists('iprterms', $orf)) {
          foreach ($orf['goterms'] as $goterm) {
            $goterms[] = $goterm;
          }
        }
      }
    }
  }
  return array(
    'proteins' => $proteins,
    'goterms'  => $goterms,
    'iprterms' => $iprterms,
  );
}

/**
 * Parses the Interpro XML for a single feature and returns an array
 * containing the results.
 *
 * @param $interpro_xml
 *   The XML results for a single feature.
 * @param $feature_id
 *   The feature ID to which this blast XML belongs.
 *
 * @return
 *   An array with the following fields:
 *
 *      Details about interpro match for the feature
 *        $ipr_array['orf']
 *        $ipr_array['orf']['orf_id']
 *        $ipr_array['orf']['orf_length']
 *        $ipr_array['orf']['orf_crc64']
 *
 *      An arrray containing all of the IPR terms mapped to this feature. Each
 *      IPR term is an array with 3 elements. The first element is the IPR
 *      accession, the second is the name and the third is the description
 *        $ipr_array['iprterms']
 *
 *      An array containing all of the GO terms mapped to the feature
 *        $ipr_array['goterms']
 *
 *      An array of terms. The variable $i indicates an index variable for
 *      iterating through the terms
 *        $ipr_array['terms']
 *        $ipr_array['terms'][$i]['ipr_id']
 *        $ipr_array['terms'][$i]['ipr_name']
 *        $ipr_array['terms'][$i]['ipr_type']
 * 
 *       Each feature may have one or more matches.  The variable $j indicates
 *      an index variable for iterating through the matches.
 *        $ipr_array['terms'][$i]['matches'][$j]['match_id']
 *        $ipr_array['terms'][$i]['matches'][$j]['match_name']
 *        $ipr_array['terms'][$i]['matches'][$j]['match_dbname']
 *        $ipr_array['terms'][$i]['matches'][$j]['go_terms'][]
 *
 *      Each match can have multiple start and stop locations. The variable $k
 *      indicates an index variable for iterating through the locations
 *        $ipr_array['terms'][$i]['matches'][$j]['locations'][$k]['match_start']
 *        $ipr_array['terms'][$i]['matches'][$j]['locations'][$k]['match_end']
 *        $ipr_array['terms'][$i]['matches'][$j]['locations'][$k]['match_score']
 *        $ipr_array['terms'][$i]['matches'][$j]['locations'][$k]['match_status']
 *        $ipr_array['terms'][$i]['matches'][$j]['locations'][$k]['match_evidence']
 *
 */
function tripal_analysis_interpro_parse_feature_XML4($xml_obj, $feature_id) {

  $results  = array();
  $terms    = array();
  $protein  = array();
  $iprterms = array();
  $goterms  = array();

  $term_count = 0;
  $match_count = 0;
   
  // get the properties of this result
  $attr = $xml_obj->attributes();
  $protein['orf_id']     = (string) $attr["id"];
  $protein['orf_length'] = (string) $attr["length"];
  $protein['orf_crc64']  = (string) $attr["crc64"];

  // iterate through each interpro results for this protein
  foreach ($xml_obj->children() as $intepro) {
    // get the interpro term for this match
    $attr = $intepro->attributes();
    $terms[$term_count]['ipr_id']   = (string) $attr["id"];
    $terms[$term_count]['ipr_name'] = (string) $attr["name"];
    $terms[$term_count]['ipr_type'] = (string) $attr["type"];
    $iprterms[] = array($terms[$term_count]['ipr_id'], $terms[$term_count]['ipr_name'], $terms[$term_count]['ipr_name']);

    // iterate through the elements of the interpro result
    $matches[$term_count]['matches'] = array();
    $match_count = 0;
    foreach ($intepro->children() as $level1) {
      $element_name = $level1->getName();
      if ($element_name == 'match') {
        // get the match name for this match
        $attr = $level1->attributes();
        $terms[$term_count]['matches'][$match_count]['match_id']     = (string) $attr["id"];
        $terms[$term_count]['matches'][$match_count]['match_name']   = (string) $attr["name"];
        $terms[$term_count]['matches'][$match_count]['match_dbname'] = (string) $attr["dbname"];

        // get the location information for this match
        $loc_count = 0;
        foreach ($level1->children() as $level2) {
          $element_name = $level2->getName();
          if ($element_name == 'location') {
            $attr = $level2->attributes();
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_start']    = (string) $attr["start"];
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_end']      = (string) $attr["end"];
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_score']    = (string) $attr["score"];
            $terms[$term_count]['matches'][$match_count]['locations'][$loc_count]['match_status']   = (string) $attr["status"];
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
  $results['terms']    = $terms;
  $results['orf']      = $protein;
  $results['iprterms'] = $iprterms;
  $results['goterms']  = $goterms;
  return $results;
}
