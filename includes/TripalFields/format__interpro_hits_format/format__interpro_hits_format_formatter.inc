<?php

class format__interpro_hits_format_formatter extends ChadoFieldFormatter {

  // The default lable for this field.
  public static $default_label = 'InterPro hits format';

  // The list of field types for which this formatter is appropriate.
  public static $field_types = array('format__interpro_hits_format');

  /**
   * @see TripalFieldFormatter::view()
   */
  public function view(&$element, $entity_type, $entity, $langcode, $items, $display) {
    // Hack to ensure that jquery 3.1.0 is loaded just before calling noconflict
    drupal_add_js("https://cdnjs.cloudflare.com/ajax/libs/jquery/3.1.0/jquery.min.js", array(
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 100,
    ));
    drupal_add_js("https://cdnjs.cloudflare.com/ajax/libs/d3/3.5.17/d3.min.js", array(
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 200,
    ));
    drupal_add_js("https://cdnjs.cloudflare.com/ajax/libs/twitter-bootstrap/3.3.7/js/bootstrap.min.js", array(
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 300,
    ));
    drupal_add_js("https://cdn.rawgit.com/calipho-sib/feature-viewer/v0.1.44/dist/feature-viewer.min.js", array(
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 400,
    ));
    drupal_add_js("if (typeof feature_viewer_jquery == 'undefined') {var feature_viewer_jquery = jQuery.noConflict(true);}", array(
      'type' => 'inline',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 1000,
    ));

    // Get the settings
    $settings = $display['settings'];
    $feature = $entity->chado_record;
    $feature = chado_expand_var($feature, 'field', 'feature.residues');

    $num_results_per_page = 10;
    $feature_pager_id = 15; // increment by 1 for each database
    $i = 0;
    $list_items = '';
    foreach ($items as $delta => $item) {
      if (isset($item['parsed'])) {
        $analysis_id = $delta;
        $details = $item['parsed'];
        
        if(!isset($details['iprterms'])) continue;
        $analysis   = $details['analysis'];
        $iprterms   = $details['iprterms'];
        $format     = $details['format'];

        // ANALYSIS DETAILS
        $aname = $analysis->name;
        if (property_exists($analysis, 'nid')) {
          $aname = l($aname, 'node/' . $analysis->nid, array('attributes' => array('target' => '_blank')));
        }
        $date_performed = preg_replace("/^(\d+-\d+-\d+) .*/", "$1", $analysis->timeexecuted);
        $list_items .= "
          Analysis Name: $aname
          <br>Date Performed: $date_performed
          <div id=\"features_vis-".$analysis->analysis_id."\"></div>
          <script lang=\"javascript\">
              (function($) {
              // hack to work with multiple jquery versions
              backup_jquery = window.jQuery;
              window.jQuery = $;
              window.$ = $;
              window.ft".$analysis->analysis_id." = new FeatureViewer('".$feature->residues."',
              '#features_vis-".$analysis->analysis_id."',
              {
                  showAxis: true,
                  showSequence: true,
                  brushActive: true, //zoom
                  toolbar:true, //current zoom & mouse position
                  bubbleHelp:false,
                  zoomMax:3 //define the maximum range of the zoom
              });
              window.jQuery = backup_jquery;
              window.$ = backup_jquery;
            })(feature_viewer_jquery);
          </script>
        ";

        // ALIGNMENT SUMMARY
        $headers = array(
          'IPR Term',
          'IPR Description',
          'Source',
          'Source Term',
          'Source Description',
          'Alignment'
        );

        $rows = array();
        foreach ($iprterms as $ipr_id => $iprterm) {

          $matches  = $iprterm['matches'];
          $ipr_name = $iprterm['ipr_name'];
          $ipr_desc = $iprterm['ipr_desc'];
          $ipr_type = $iprterm['ipr_type'];

          // iterate through the evidence matches
          foreach ($matches as $match) {
            $hsp_pos = array();
            $match_id     = $match['match_id'];
            $match_name   = $match['match_name'];
            $match_dbname = $match['match_dbname'];

            $locations = $match['locations'];
            $loc_details = '';
            foreach($locations as $location){
              if ($format == 'XML4') {
                $loc_details .= 'coord: ' . $location['match_start'] . ".." . $location['match_end'];
                if($location['match_score']) {
                  $loc_details .= '<br>score: ' . $location['match_score'];
                }
              }
              if ($format == 'XML5') {
                $loc_details .= 'coord: ' . $location['match_start'] . ".." . $location['match_end'];
                if($location['match_evalue']) {
                  $loc_details .= '<br>e-value: ' . $location['match_evalue'];
                }
                if($location['match_score']) {
                  $loc_details .= '<br>score: ' . $location['match_score'];
                }
                $loc_details .= '<br>';
              }
              //$match_evidence =  $location['match_evidence'];

              $desc = '';
              if (!empty($location['match_evalue']))
                $desc = 'Expect = '.$location['match_evalue'];
              if (!empty($location['match_score']))
                if (!empty($desc))
                    $desc .= ' / ';
                $desc .= 'Score = '.$location['match_score'];
              $hsp_pos[] = array('x' => intval($location['match_start']), 'y' => intval($location['match_end']), 'description' => $desc);
            }

            // remove the trailing <br>
            $loc_details = substr($loc_details, 0, -4);

            if ($ipr_id == 'noIPR') {
              $ipr_id_link = 'None';
              $ipr_desc = 'No IPR available';
            }
            else {
              // we want to use the URL for the database
              $ipr_db = tripal_get_db(array('name' => 'INTERPRO'));
              $ipr_id_link = $ipr_id;
              if ($ipr_db and $ipr_db->urlprefix) {
                $ipr_id_link = l($ipr_id, $ipr_db->urlprefix . $ipr_id, array('attributes' => array('target' => '_blank')));
              }
            }

            // the Prosite databases are split into two libraries for InterProScan. But
            // we can just use the PROSITE database for both of them, so rename it here.
            $match_dbname = preg_replace('/(PROSITE)_.*/', '\1', $match_dbname);

            // get links for the matching databases
            $match_db = tripal_get_db(array('name' => strtoupper($match_dbname)));
            if ($match_db and $match_db->url) {
              // some databases need a prefix removed
              if ($match_dbname == "GENE3D") {
                $fixed_id = preg_replace('/G3DSA:/','', $match_id);
                $match_id = l($fixed_id, $match_db->urlprefix . $fixed_id, array('attributes' => array('target' => '_blank')));
              }
              elseif ($match_dbname == "SUPERFAMILY") {
                $fixed_id = preg_replace('/SSF/','', $match_id);
                $match_id = l($fixed_id, $match_db->urlprefix . $fixed_id, array('attributes' => array('target' => '_blank')));
              }
              // for all others, just link using the URL prefix
              else {
                $match_id = l($match_id, $match_db->urlprefix . $match_id, array('attributes' => array('target' => '_blank')));
              }
            }
            if ($match_db and $match_db->url) {
              $match_dbname = l($match_dbname, $match_db->url, array('attributes' => array('target' => '_blank')));
            }

            $rows[] = array(
              $ipr_id_link,
              $ipr_desc,
              $match_dbname,
              $match_id,
              $match_name,
              array(
                'data' => $loc_details,
                'nowrap' => 'nowrap'
              ),
            );

            $viz_name = $ipr_id;
            if (!empty($match['match_id']))
                $viz_name = $match['match_id'];

            $list_items .= "<script lang=\"javascript\">
              (function($) {
                  // hack to work with multiple jquery versions
                  backup_jquery = window.jQuery;
                  window.jQuery = $;
                  window.$ = $;
                  window.ft".$analysis->analysis_id.".addFeature({
                   data: ".json_encode($hsp_pos).",
                   name: \"".$viz_name."\",
                   className: \"ipr_match\", //can be used for styling
                   color: \"#0F8292\",
                   type: \"rect\"
               });
               window.jQuery = backup_jquery;
               window.$ = backup_jquery;
             })(feature_viewer_jquery);
            </script>";
          } // end foreach ($matches as $match) {
        } // end foreach ($iprterms as $ipr_id => $iprterm) {

        if (count($rows) == 0) {
          $rows[] = array(
            array(
              'data' => 'No results',
              'colspan' => '6',
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
        $list_items .= theme_table($table);
        $list_items .= "<br>";
        $i++;
      }
    }
    // once we have our table array structure defined, we call Drupal's theme_table()
    // function to generate the table.
    if ($i == 0) {
      $element[0] = array(
        '#type' => 'markup',
        '#markup' => 'There are no InterPro results for this feature.',
      );
    }
    else {
      // Add a list at the top of the field.
      $content = '<div class="tripal_feature-data-block-desc tripal-data-block-desc">The following InterPro results are available for this feature:</div> ';
      $content .= '<a name="interpro-results-top"></a>';
      $content .= $list_items;
      $element[0] = array(
        '#type' => 'markup',
        '#markup' => $content . $element[0]['#markup'],
      );
    }
  }
}
