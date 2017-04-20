<?php

class format__interpro_hits_format_widget extends ChadoFieldWidget {

  // The default lable for this field.
  public static $default_label = 'InterPro hits format';

  // The list of field types for which this formatter is appropriate.
  public static $field_types = array('format__interpro_hits_format');


  /**
   * @see TripalFieldWidget::form()
   */
  public function form(&$widget, &$form, &$form_state, $langcode, $items, $delta, $element) {
    parent::form($widget, $form, $form_state, $langcode, $items, $delta, $element);
  }

  /**
   * @see TripalFieldWidget::validate()
   */
  public function validate($element, $form, &$form_state, $langcode, $delta) {

  }
}
