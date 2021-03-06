<?php

/**
 * Implements hook_bundle_create_fields().
 *
 * This is a Tripal defined hook that supports integration with the
 * TripalEntity field.
 */
function tripal_analysis_interpro_bundle_fields_info($entity_type, $bundle) {
  $info = array();

  $term = tripal_load_term_entity(array('term_id' => $bundle->term_id));
  $vocab = $term->vocab;
  if ($vocab->vocabulary == 'SO') {
    $field_name = 'local__interpro_results';
    $field_type = 'local__interpro_results';
    $info[$field_name] = array(
      'field_name' => $field_name,
      'type' => $field_type,
      'cardinality' => 1,
      'locked' => FALSE,
      'storage' => array(
        'type' => 'field_chado_storage'
      ),
    );
  }
  return $info;
}

/**
 * Impelments hook_create_tripalfield_instance().
 *
 * This is a Tripal defined hook that supports integration with the
 * TripalEntity field.
 */
function tripal_analysis_interpro_bundle_instances_info($entity_type, $bundle) {
  $info = array();

  $term = tripal_load_term_entity(array('term_id' => $bundle->term_id));
  $vocab = $term->vocab;

  if ($vocab->vocabulary == 'SO') {
    $field_name = 'local__interpro_results';
    $info[$field_name] = array(
      'field_name' => $field_name,
      'entity_type' => $entity_type,
      'bundle' => $bundle->name,
      'label' => 'InterPro results',
      'description' => 'Results format for searches of the InterPro database.',
      'required' => FALSE,
      'settings' => array(
        'auto_attach' => TRUE,
        'chado_table' => 'analysis_feature',
        'chado_column' => '',
        'base_table' => 'feature',
        'term_vocabulary' => 'local',
        'term_name' => 'InterPro results',
        'term_accession' => 'interpro_results',
      ),
      'widget' => array(
        'type' => 'local__interpro_results_widget',
      ),
      'display' => array(
        'default' => array(
          'label' => 'hidden',
          'type' => 'local__interpro_results_formatter',
        ),
      ),
    );
  }
  return $info;
}


/**
 * Implements hook_bundle_fields_info_alter().
 *
 * This is a Tripal defined hook.
 */
function tripal_analysis_interpro_bundle_fields_info_alter(&$info, $bundle, $term) {
  if ($term->vocab->vocabulary == 'local' and $term->accession == 'interpro_results') {
    // We don't need the source version field.
    if (array_key_exists('iao__algorithm', $info)) {
      unset($info['iao__algorithm']);
    }
  }
}
/**
 * Implements hook_bundle_instances_info_alter().
 *
 * This is a Tripal defined hook.
 */
function tripal_analysis_interpro_bundle_instances_info_alter(&$info, $bundle, $term) {

  if ($term->vocab->vocabulary == 'local' and $term->accession == 'interpro_results') {
    // We don't need the source version field.
    if (array_key_exists('iao__algorithm', $info)) {
      unset($info['iao__algorithm']);
    }

    // Set a default for the local analysis type field
    if (array_key_exists('local__analysis_type', $info)) {
      $info['local__analysis_type']['default_value'][0]['value'] = 'interpro_analysis';
    }

    // Rename some field labels and descriptions to be more appropriate.
    if (array_key_exists('swo__software', $info)) {
      $info['swo__software']['label'] = 'InterPro Program';
      $info['swo__software']['description'] = 'The interpro program name (e.g. iprscan, InterProScan, etc).';
    }
    if (array_key_exists('iao__version_number', $info)) {
      $info['iao__version_number']['label'] = 'InterPro Version';
      $info['iao__version_number']['description'] = 'The version of InterPro used to generate the results.';
    }
    if (array_key_exists('local__source_data', $info)) {
      $info['local__source_data']['label'] = 'Query File Source';
      $info['local__source_data']['description'] = 'Please provide information, if available, regarding where the query file used for this interpro analysis was obtained.';
    }
  }
}
