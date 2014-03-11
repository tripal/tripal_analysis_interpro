<?php
$feature     = $variables['node']->feature;
$results     = $feature->tripal_analysis_interpro->results->xml;
$resultsHTML = $feature->tripal_analysis_interpro->results->html;

if(count($results) > 0){
  // Do not show Interpro result and the sidebar link if the it contains only 'noIPR'
  $emptyResult = true; 
  foreach($results as $analysis_id => $analysisprops){
    $terms = $analysisprops['allterms'];
    $protein_ORFs = $analysisprops['protein_ORFs'];
    foreach($terms as $term){
      $ipr_id = $term[0];
      $ipr_name = $term[1];
      if(strcmp($ipr_id,'noIPR') == 0){
        continue;
      } else {
        $emptyResult = false;
      }
    }
    foreach($protein_ORFs as $orf){
      $terms = $orf['terms'];
      $orf = $orf['orf'];
      foreach($terms as $term){ 
        $matches  = $term['matches'];
        $ipr_id   = $term['ipr_id'];
        $ipr_name = $term['ipr_name'];
        $ipr_type = $term['ipr_type']; 
        if(strcmp($ipr_id,'noIPR')==0){
          continue;
        } else {
          $emptyResult = false;
        }
      }
    }
    if ($emptyResult) {
      return;
    }
  }
  $i = 0;
  
  $contents = '';
  foreach($results as $analysis_id => $analysisprops){ 
    $analysis = $analysisprops['analysis'];
    $protein_ORFs = $analysisprops['protein_ORFs']; 
    $terms = $analysisprops['allterms'];
    
    
    // ANALYSIS DETAILS
    $contents .= "<div><b>Summary of Annotated Terms</b></div>";
    $aname = $analysis->name;
    if (property_exists($analysis, 'nid')) { 
      $aname = l($aname, 'node/' . $analysis->nid, array('attributes' => array('target' => '_blank')));
    } 
    $date_performed = preg_replace("/^(\d+-\d+-\d+) .*/", "$1", $analysis->timeexecuted);
    $contents .= "
      Analysis Name: $analysis_name
      <br>Date Performed: $date_performed
    ";
    
    // SUMMARY OF ANNOTATED TERMS
    // the $headers array is an array of fields to use as the colum headers.
    // additional documentation can be found here
    // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
    $headers = array('Term', 'Name');
    
    // the $rows array contains an array of rows where each row is an array
    // of values for each column of the table in that row.  Additional documentation
    // can be found here:
    // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
    $rows = array();
    foreach ($terms as $term){ 
      $ipr_id = $term[0];
      $ipr_name = $term[1];
      if(strcmp($ipr_id,'noIPR')==0){
        continue;
      }
      $rows[] = array(
        l($ipr_id, "http://www.ebi.ac.uk/interpro/entry/$ipr_id", array('attributes' => array('target' => '_blank'))),
        $ipr_name,
      );
    } 
    
    // the $table array contains the headers and rows array as well as other
    // options for controlling the display of the table.  Additional
    // documentation can be found here:
    // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
    $table = array(
      'header' => $headers,
      'rows' => $rows,
      'attributes' => array(
        'id' => 'tripal_feature-table-ipr_terms',
      ),
      'sticky' => FALSE,
      'caption' => '',
      'colgroups' => array(),
      'empty' => '',
    );
    
    // once we have our table array structure defined, we call Drupal's theme_table()
    // function to generate the table.
    $contents .= theme_table($table);
    
    
    // ANALYSIS RESULTS
    $contents .= "<div><b>Alignment Details</b><div>";
    foreach($protein_ORFs as $orf){  
      $terms = $orf['terms'];
      $orf = $orf['orf'];  
      
      //$contents .= "<br><b>ORF: " . $orf['orf_id'] . ", Length: " . $orf['orf_length'] . "</b><br>";
      foreach($terms as $term){ 
        $matches  = $term['matches'];
        $ipr_id   = $term['ipr_id'];
        $ipr_name = $term['ipr_name'];
        $ipr_type = $term['ipr_type']; 
        if(strcmp($ipr_id,'noIPR')==0){
          continue;
        }
        
        $contents .= "<div>Assigned Term: " . l($ipr_id, "http://www.ebi.ac.uk/interpro/IEntry?ac=$ipr_id", array('attributes' => array('target' => "_ipr"))) . " $ipr_name ($ipr_type)";
        $headers = array(
          array(
            'data' => 'Method',
            'width' => '15%',
          ), 
          array(
            'data' => 'Identifier',
            'width' => '15%',
          ),
          array(
            'data' => 'Description',
            'width' => '50%',
          ),
          array(
            'data' => 'Matches<sup>*</sup>',
            'width' => '30%',
          ),
        );
        $rows = array();
        
        foreach ($matches as $match){
          $match_id     = $match['match_id'];
          $match_name   = $match['match_name'];
          $match_dbname = $match['match_dbname'];
          
          $locations = $match['locations'];
          $loc_details = '';
          foreach($locations as $location){
            $loc_details .= $location['match_score']." [".$location['match_start']."-".$location['match_end']."] " . $location['match_status'] ."<br>";
            #$match_evidence =  $location['match_evidence'];
          }
          $rows[] = array(
            $match_dbname,
            $match_id,
            $match_name,
            array(
              'data' => $loc_details,
              'nowrap' => 'nowrap'
            ),
          );
        } // end foreach matches
        
        if (count($rows) == 0) {
          $rows[] = array(
            array(
              'data' => 'No results',
              'colspan' => '4',
            ),
          );
        }
        $table = array(
          'header' => $headers,
          'rows' => $rows,
          'attributes' => array(),
          'sticky' => FALSE,
          'caption' => '',
          'colgroups' => array(),
          'empty' => '',
        );
        // once we have our table array structure defined, we call Drupal's theme_table()
        // function to generate the table.
        $contents .= theme_table($table);
        $contents .= '<sup>* score [start-end] status</sup></div><br>';
      } // end foreach terms
    } // end foreach orfs 
   } // end for each analysis 
   print $contents;
} // end if


// results stored as HTML is deprecated but in the event that results may stil
// be stored in old style HTML we keep this check
if($resultsHTML){  
  // ANALYSIS DETAILS
  $aname = $analysis_name;
  if (property_exists($analysis, 'nid')) { 
    $aname = l($aname, 'node/' . $analysis->nid, array('attributes' => array('target' => '_blank')));
  } 
  $date_performed = preg_replace("/^(\d+-\d+-\d+) .*/", "$1", $analysis->timeexecuted);
  print "
    Analysis Name: $analysis_name
    <br>Date Performed: $date_performed
  "; ?>
  <div class="tripal_feature-interpro_results_subtitle">Summary of Annotated IPR terms</div> <?php 
  print $resultsHTML;?>
  </div> <?php
}

