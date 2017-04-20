<?php
/**
 * Prepare interpro result for the feature shown on the page
 */
function tripal_analysis_interpro_preprocess_tripal_feature_interpro_results(&$variables) {
    $feature = $variables['node']->feature;

    // make sure the necessary property elements exist
    if (!property_exists($feature, 'tripal_analysis_interpro')) {
        $feature->tripal_analysis_interpro = new stdClass;
    }
    if (!property_exists($feature->tripal_analysis_interpro, 'results')) {
        $feature->tripal_analysis_interpro->results = new stdClass;
    }

    $xml_results = tripal_get_interpro_XML_results($feature->feature_id);
    $feature->tripal_analysis_interpro->results->xml = $xml_results;

    // we don't know how many analysis are the old style HTML version and how many are the newer
    // XML format.  So, to be backwards compatible we should get both.
    $feature->tripal_analysis_interpro->results->html = tripal_get_interpro_HTML_results($feature->feature_id);

    // -- spf the following CSS overrides some global theme settings which
    // should not happen. commenting out 1/14/17
    //drupal_add_css("https://cdn.rawgit.com/calipho-sib/feature-viewer/v0.1.44/dist/feature-viewer.min.css", array('type' => 'external'));

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

    // Add javascript and style sheet
    drupal_add_css(drupal_get_path('module', 'tripal_analysis_interpro') . '/theme/css/tripal_analysis_interpro.css', 'theme');
    drupal_add_js( drupal_get_path('module', 'tripal_analysis_interpro') . '/theme/js/tripal_analysis_interpro.js');
}
