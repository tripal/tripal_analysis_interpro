<?php
/**
 * Prepare interpro result for the feature shown on the page
 */
function tripal_analysis_interpro_preprocess_tripal_feature_interpro_results(&$variables) {
  $feature = $variables['node']->feature;

  // we don't know how many analysis are the old style HTML version and how many are the newer
  // XML format.  So, to be backwards compatible we should get both.
  // $feature->tripal_analysis_interpro->results->html = tripal_get_interpro_HTML_results($feature->feature_id);
  $xml_results = tripal_get_interpro_XML_results($feature->feature_id);
  if (!is_object($feature->tripal_analysis_interpro)) {
    $feature->tripal_analysis_interpro = new stdClass;
  }
  if (!is_object($feature->tripal_analysis_interpro->results)) {
    $feature->tripal_analysis_interpro->results = new stdClass;
  }
  $feature->tripal_analysis_interpro->results->xml = $xml_results;
}

/**
 *
 */
function tripal_get_interpro_XML_results($feature_id) {

  // Get the blast results stored as XML from the analysisfeatureprop table
  // the type for the property is named 'analysis_blast_output_iteration_hits'
  // and is found in the 'tripal' controlled vocabulary.  This CV term was
  // added by this module.
  $select = array(
    'analysisfeature_id' => array(
      'feature_id' => $feature_id,
    ),
    'type_id' => array(
      'name' => 'analysis_interpro_xmloutput_hit',
      'cv_id' => array(
        'name' => 'tripal'
      ),
    ),
  );
  $afeatureprops = tripal_core_chado_select('analysisfeatureprop', array('*'), $select);

  // iterate through all of the interpro XML properties for this feature
  $results = array();
  if ($afeatureprops) {
    foreach ($afeatureprops as $index => $afeatureprop) {

      // get the analysis feature record
      $analysisfeature_arr = tripal_core_chado_select('analysisfeature', array('analysis_id'),
          array('analysisfeature_id' => $afeatureprop->analysisfeature_id));
      $analysisfeature = $analysisfeature_arr[0];

      // get the analysis record and the analysis_id
      $analysis = tripal_core_generate_chado_var('analysis',
          array('analysis_id' => $analysisfeature->analysis_id));
      $analysis_id = $analysis->analysis_id;

      // parse the XML and add it to the array of results to be returned
      $interpro_xml = $afeatureprop->value;
      $orf = tripal_analysis_interpro_get_result_object($interpro_xml, $feature_id);
      $results[$analysis->analysis_id]['protein_ORFs'][] = $orf;
      $results[$analysis->analysis_id]['analysis'] = $analysis;

      // iterate through all of the protein ORFs and combine the terms into one large list
      if (!is_array($results[$analysis->analysis_id]['allterms'])) {
        $results[$analysis->analysis_id]['allterms'] = array();
      }
      if (!is_array($results[$analysis->analysis_id]['goterms'])) {
        $results[$analysis->analysis_id]['goterms'] = array();
      }
      $results[$analysis->analysis_id]['allterms'] = array_merge($results[$analysis->analysis_id]['allterms'], $orf['iprterms']);
      $results[$analysis->analysis_id]['goterms'] = array_merge($results[$analysis->analysis_id]['goterms'], $orf['goterms']);
      $i++;
    }
  }
  return $results;
}

/**
 * tripal_get_interpro_results()
 * Get interpro result from featureprop table for the feature
 */
function tripal_get_interpro_HTML_results($feature_id) {
  // Get cvterm_id for 'analysis_interpro_output_hit' which is required
  // for inserting into the analysisfeatureprop table

  $sql = "SELECT CVT.cvterm_id FROM {cvterm} CVT
          INNER JOIN {cv} ON cv.cv_id = CVT.cv_id
          WHERE CVT.name = 'analysis_interpro_output_hit'
          AND CV.name = 'tripal'";
  $type_id = chado_query($sql)->fetchField();

  // Get analysis times for the feature
  $sql = "SELECT A.analysis_id AS aid
          FROM {analysis} A
          INNER JOIN {analysisfeature} AF ON A.analysis_id = AF.analysis_id
          INNER JOIN {analysisfeatureprop} AFP ON AF.analysisfeature_id = AFP.analysisfeature_id
          WHERE feature_id = :feature_id
          AND AFP.type_id = :type_id
          AND AFP.value NOT like '%No hits reported.%'
           AND AFP.value NOT like '%parent%'
           AND AFP.value NOT like '%children%'
          GROUP BY A.analysis_id
         ";
  $hasResult = chado_query($sql, array(':feature_id' => $feature_id, ':type_id' => $type_id))->fetchField();
  $result = chado_query($sql, array(':feature_id' => $feature_id, ':type_id' => $type_id));

  // Show interpro result ORDER BY time
  if ($hasResult) { // If there is any result, show expandable box
    $content .= "<table class=\"tripal_interpro_results_table\">
                         <tr><td>";

    while ($ana = $result->fetchObject()) {
      // Show analysis date
      $sql = "SELECT name, to_char(timeexecuted, 'MM-DD-YYYY') AS time
               FROM {analysis}
               WHERE analysis_id = :analysis_id";
      $ana_details = chado_query($sql, array(':analysis_id' => $ana->aid))->fetchObject();
      // Find node id for the analysis
       
      $ana_nid = db_query("SELECT nid FROM {chado_analysis} WHERE analysis_id = :analysis_id", array(':analysis_id' => $ana->aid))->fetchField();
      $ana_url = url("node/". $ana_nid);
       

      // Show content
      $content .= "<strong>Analysis Date:</strong> $ana_details->time
      (<a href=$ana_url>$ana_details->name</a>)";

      // Show interpro results
      $sql = "SELECT AFP.value AS afpvalue
                 FROM {analysisfeatureprop} AFP
                 INNER JOIN {analysisfeature} AF on AF.analysisfeature_id = AFP.analysisfeature_id
                 WHERE AF.analysis_id = :analysis_id
                 AND AF.feature_id = :feature_id
                 AND AFP.type_id = :type_id
                 AND AFP.value NOT like '%No hits reported.%'
                 AND AFP.value NOT like '%parent%'
                 AND AFP.value NOT like '%children%'
                ";
      $interpro_results = chado_query($sql, array(':analysis_id' => $ana->aid, ':feature_id' => $feature_id, ':type_id' => $type_id));
      while ($afp = $interpro_results->fetchObject()) {
        $content .= $afp->afpvalue;
      }
    }
    $content .= '</td></tr></table>';
  }

  return $content;
}