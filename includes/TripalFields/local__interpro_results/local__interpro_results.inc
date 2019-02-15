<?php

class local__interpro_results extends ChadoField {

  // The default lable for this field.
  public static $default_label = 'InterPro results';

  // The default description for this field.
  public static $description = 'Results for InterPro analysis.';

  // Provide a list of instance specific settings. These can be access within
  // the instanceSettingsForm.  When the instanceSettingsForm is submitted
  // then Drupal with automatically change these settings for the instnace.
  // It is recommended to put settings at the instance level whenever possible.
  // If you override this variable in a child class be sure to replicate the
  // term_name, term_vocab, term_accession and term_fixed keys as these are
  // required for all TripalFields.
  public static $default_instance_settings = [
    // The short name for the vocabulary (e.g. shcema, SO, GO, PATO, etc.).
    'term_vocabulary' => 'local',
    // The name of the term.
    'term_name' => 'InterPro results',
    // The unique ID (i.e. accession) of the term.
    'term_accession' => 'interpro_results',
    // Set to TRUE if the site admin is allowed to change the term
    // type. This will create form elements when editing the field instance
    // to allow the site admin to change the term settings above.
    'term_fixed' => FALSE,
  ];

  // The default widget for this field.
  public static $default_widget = 'local__interpro_results_widget';

  // The default formatter for this field.
  public static $default_formatter = 'local__interpro_results_formatter';

  /**
   * @see TripalField::load()
   */
  public function load($entity, $details = []) {
    $record = $entity->chado_record;

    $settings = $this->instance['settings'];
    $field_name = $this->field['field_name'];
    $field_type = $this->field['type'];
    $field_table = $this->instance['settings']['chado_table'];
    $field_column = $this->instance['settings']['chado_column'];

    // Get the terms for each of the keys for the 'values' property.
    $label_term = 'format:1341';

    // Get the list of results.
    $interpro_results = tripal_get_interpro_XML_results($record->feature_id);

    if (empty($interpro_results)) {
      unset($entity->{$field_name});
      $entity->{$field_name}['und'][0] = array(
        'value' => ''
      );
      return;
    }
    $delta = 0;

    foreach ($interpro_results as $analysis_id => $results) {
      $entity->{$field_name}['und'][$delta] = [
        'value' => 1,
        'parsed' => $results,
        'analysis_id' => $analysis_id
      ];
      $delta++;
    }
  }
}
