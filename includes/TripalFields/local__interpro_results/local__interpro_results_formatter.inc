<?php

/**
 *
 */
class local__interpro_results_formatter extends ChadoFieldFormatter {

  /**
   * The default lable for this field.
   */
  public static $default_label = 'InterPro results';

  /**
   * The list of field types for which this formatter is appropriate.
   */
  public static $field_types = ['local__interpro_results'];

  /**
   * @see TripalFieldFormatter::view()
   */
  public function view(&$element, $entity_type, $entity, $langcode, $items, $display) {
    // Hack to ensure that jquery 3.1.0 is loaded just before calling noconflict.
    drupal_add_js("https://cdnjs.cloudflare.com/ajax/libs/jquery/3.1.0/jquery.min.js", [
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 100,
    ]);
    drupal_add_js("https://cdnjs.cloudflare.com/ajax/libs/d3/3.5.17/d3.min.js", [
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 200,
    ]);
    drupal_add_js("https://cdnjs.cloudflare.com/ajax/libs/twitter-bootstrap/3.3.7/js/bootstrap.min.js", [
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 300,
    ]);

    drupal_add_js("if (typeof feature_viewer_jquery == 'undefined') {var feature_viewer_jquery = jQuery.noConflict(true);}", [
      'type' => 'inline',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 1000,
    ]);

    // Can all of the above add_js calls be removed?
    drupal_add_js("https://cdn.rawgit.com/calipho-sib/feature-viewer/v1.0.0/dist/feature-viewer.bundle.js", [
      'type' => 'external',
      'scope' => 'header',
      'group' => 15,
      'every_page' => TRUE,
      'weight' => 500,
    ]);

    // We want JQuery UI support for the accordion.
    drupal_add_library('system', 'ui.accordion');

    // Get the settings.
    $settings = $display['settings'];
    $feature = $entity->chado_record;
    $feature = chado_expand_var($feature, 'field', 'feature.residues');

    $content = "<p>Click a tab below to view interpro alignments.</p>";
    $content .= '<div id="tripal-interpro-analysis-accordian"></div>';

    $i = 0;
    $list_items = '';
    foreach ($items as $delta => $item) {
      if (isset($item['parsed'])) {
        $details = $item['parsed'];

        if (!isset($details['iprterms'])) {
          continue;
        }
        $analysis = $details['analysis'];
        $iprterms = $details['iprterms'];
        $format = $details['format'];

        // ANALYSIS DETAILS.
        $aname = $analysis->name;
        if (property_exists($analysis, 'nid')) {
          $aname = l($aname, 'node/' . $analysis->nid, ['attributes' => ['target' => '_blank']]);
        }
        $date_performed = preg_replace("/^(\d+-\d+-\d+) .*/", "$1", $analysis->timeexecuted);

        // Add the div box for each results.
        $content .= '
        <h3 id="analysis-' . $analysis->analysis_id . '">' . $analysis->name . '</h3>
          <p>
            Performed on: ' . preg_replace("/^(\d+-\d+-\d+) .*/", "$1", $analysis->timeexecuted) . '
            <br>Display provided by the ' . l('neXtProt Feature Viewer', 'https://github.com/calipho-sib/feature-viewer') . '
            <div
              id="features_ipr_vis-' . $analysis->analysis_id . '"
              class="interpro-alignment-viewer"
              analysis_id="' . $analysis->analysis_id . '" residues="' . $feature->residues . '"
              width="500px"></div>
            <br>
            </p>
      ';

        // ALIGNMENT SUMMARY.
        $headers = [
          'IPR Term',
          'IPR Description',
          'Source',
          'Source Term',
          'Source Description',
          'Alignment',
        ];

        $rows = [];
        $j = 0;
        foreach ($iprterms as $ipr_id => $iprterm) {

          $matches = $iprterm['matches'];
          $ipr_name = $iprterm['ipr_name'];
          $ipr_desc = $iprterm['ipr_desc'];
          $ipr_type = $iprterm['ipr_type'];

          // Iterate through the evidence matches.
          foreach ($matches as $match) {
            $hsp_pos = [];
            $match_id = $match['match_id'];
            $match_name = $match['match_name'];
            $match_dbname = $match['match_dbname'];

            $locations = $match['locations'];
            $loc_details = '';
            foreach ($locations as $location) {
              if ($format == 'XML4') {
                $loc_details .= 'coord: ' . $location['match_start'] . ".." . $location['match_end'];
                if ($location['match_score']) {
                  $loc_details .= '<br>score: ' . $location['match_score'];
                }
              }
              if ($format == 'XML5') {
                $loc_details .= 'coord: ' . $location['match_start'] . ".." . $location['match_end'];
                if ($location['match_evalue']) {
                  $loc_details .= '<br>e-value: ' . $location['match_evalue'];
                }
                if ($location['match_score']) {
                  $loc_details .= '<br>score: ' . $location['match_score'];
                }
                $loc_details .= '<br>';
              }
              $desc = '';
              if (!empty($location['match_evalue'])) {
                $desc = 'Expect = ' . $location['match_evalue'];
              }
              if (!empty($location['match_score'])) {
                if (!empty($desc)) {
                  $desc .= ' / ';
                }
              }
              $desc .= 'Score = ' . $location['match_score'];
              $hsp_pos[] = [
                'x' => intval($location['match_start']),
                'y' => intval($location['match_end']),
                'description' => $desc,
              ];
            }

            // Remove the trailing <br>.
            $loc_details = substr($loc_details, 0, -4);

            if ($ipr_id == 'noIPR') {
              $ipr_id_link = 'None';
              $ipr_desc = 'No IPR available';
            }
            else {
              // We want to use the URL for the database.
              $ipr_db = tripal_get_db(['name' => 'INTERPRO']);
              $ipr_id_link = $ipr_id;
              if ($ipr_db and $ipr_db->urlprefix) {
                $ipr_id_link = l($ipr_id, $ipr_db->urlprefix . $ipr_id, ['attributes' => ['target' => '_blank']]);
              }
            }

            // The Prosite databases are split into two libraries for InterProScan. But
            // we can just use the PROSITE database for both of them, so rename it here.
            $match_dbname = preg_replace('/(PROSITE)_.*/', '\1', $match_dbname);

            // Get links for the matching databases.
            $match_db = tripal_get_db(['name' => strtoupper($match_dbname)]);
            if ($match_db and $match_db->url) {
              // Some databases need a prefix removed.
              if ($match_dbname == "GENE3D") {
                $fixed_id = preg_replace('/G3DSA:/', '', $match_id);
                $match_id = l($fixed_id, $match_db->urlprefix . $fixed_id, ['attributes' => ['target' => '_blank']]);
              }
              elseif ($match_dbname == "SUPERFAMILY") {
                $fixed_id = preg_replace('/SSF/', '', $match_id);
                $match_id = l($fixed_id, $match_db->urlprefix . $fixed_id, ['attributes' => ['target' => '_blank']]);
              }
              // For all others, just link using the URL prefix.
              else {
                $match_id = l($match_id, $match_db->urlprefix . $match_id, ['attributes' => ['target' => '_blank']]);
              }
            }
            if ($match_db and $match_db->url) {
              $match_dbname = l($match_dbname, $match_db->url, ['attributes' => ['target' => '_blank']]);
            }
            $feature_viewer_data = [];
            foreach($hsp_pos as $pos){
              $feature_viewer_data[] = [
                'x' => intval($pos['x']),
                'y' => intval($pos['y']),
                'id' => 'tripal-ipr-domain-' . $analysis->analysis_id . '-' . $j,
                'description' => $pos['description'],
              ];
              $j++;

            }
            // Build feature viewer data array.


            $rows[] = [
              'data' => [
                $ipr_id_link,
                $ipr_desc,
                $match_dbname,
                $match_id,
                $match_name,
                [
                  'data' => $loc_details,
                  'nowrap' => 'nowrap',
                ],
              ],
              // Additional info for the visualization but not the table.
              'class' => ['tripal-analysis-ipr-fv-' . $analysis->analysis_id],
              'fv_data' => json_encode($feature_viewer_data),
              'name' => $match_name,

            ];

            $viz_name = $ipr_id;
            if (!empty($match['match_id'])) {
              $viz_name = $match['match_id'];
            }

          } // end foreach ($matches as $match) {
        } // end foreach ($iprterms as $ipr_id => $iprterm) {

        if (count($rows) == 0) {
          $rows[] = [
            [
              'data' => 'No results',
              'colspan' => '6',
            ],
          ];
        }
        $table = [
          'header' => $headers,
          'rows' => $rows,
          'attributes' => [],
          'sticky' => FALSE,
          'caption' => '',
          'colgroups' => [],
          'empty' => '',
          'attributes' => [
            'id' => 'tripal-analysis-interpro-table-' . $analysis->analysis_id,
            'class' => ['tripal-analysis-interpro-table'],
          ],
        ];
        // Once we have our table array structure defined, we call Drupal's theme_table()
        // function to generate the table.
        $list_items .= theme_table($table);
        $list_items .= "<br>";
        $i++;
      }
    }
    if ($i == 0) {
      $element[0] = [
        '#type' => 'markup',
        '#markup' => 'There are no InterPro results for this feature.',
      ];
    }
    else {
      // Add a list at the top of the field.
      $content .= $list_items;
      if (isset($element[0]['#markup'])) {
        $content .= $element[0]['#markup'];
      }
      $element[0] = [
        '#type' => 'markup',
        '#markup' => $content,
      ];
    }

    // Add the visualization.
    drupal_add_js(drupal_get_path('module',
        'tripal_analysis_interpro') . '/theme/js/tripal_analysis_interpro_field.js');
  }

}
