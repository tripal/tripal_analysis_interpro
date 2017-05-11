<?php

class InterProImporter extends TripalImporter {

 /**
   * The name of this loader.  This name will be presented to the site
   * user.
   */
  public static $name = 'Chado InterProScan XML results loader';

  /**
   * The machine name for this loader. This name will be used to construct
   * the URL for the loader.
   */
  public static $machine_name = 'interpro_loader';

  /**
   * A brief description for this loader.  This description will be
   * presented to the site user.
   */
  public static $description = 'Import a InterPro XML file into Chado';

  /**
   * An array containing the extensions of allowed file types.
   */
  public static $file_types = array('xml');


  /**
   * Provides information to the user about the file upload.  Typically this
   * may include a description of the file types allowed.
   */
  public static $upload_description = 'Please provide the XML file.';

  /**
   * The title that should appear above the upload button.
   */
  public static $upload_title = 'XML File';
  /**
   * @see TripalImporter::form()
   */
  public function form($form, &$form_state) {
    // set the fedaults
    $parsego = '';
    $query_re = '';
    $query_type = '';
    $query_uniquename = '';

    // if we are re constructing the form from a failed validation or ajax callback
    // then use the $form_state['values'] values
    if (array_key_exists('values', $form_state)) {
        $parsego            = $form_state['values']['parsego'];
        $query_re           = $form_state['values']['query_re'];
        $query_type         = $form_state['values']['query_type'];
        $query_uniquename   = $form_state['values']['query_uniquename'];
    }

    $form['parsego'] = array(
        '#type' => 'checkbox',
        '#title' => t('Load GO terms to the database'),
        '#description' => t('Check the box to load GO terms to chado database'),
        '#default_value' => $parsego
    );

    $form['query_re'] = array(
        '#title' => t('Query Name RE'),
        '#type' => 'textfield',
        '#description' => t('Enter the regular expression that will extract the '.
            'feature name from the query line in the interpro results. This option '.
            'is only required when the query does not identically match a feature '.
            'in the database. For example: ^.*id=(.*?).*$'),
        '#default_value' => $query_re,
    );

    $form['query_uniquename'] = array(
        '#title' => t('Use Unique Name'),
        '#type' => 'checkbox',
        '#description' => t('Select this checkbox if the query name in the results file matches the unique name of the feature. '),
        '#default_value' => $query_uniquename,
    );

    $cv = tripal_get_cv(array('name' => 'sequence'));
    $cv_id = $cv->cv_id;
    $form['query_type'] = array(
        '#title' => t('Query Type'),
        '#type' => 'textfield',
        '#description' => t('Please enter the Sequence Ontology term (e.g. contig, polypeptide, mRNA) that describes '.
            'the query sequences in the InterProScan XML results file(s).  This is only necessary if two '.
            'or more sequences have the same name.'),
        '#default_value' => $query_type,
        '#autocomplete_path' => "admin/tripal/storage/chado/auto_name/cvterm/$cv_id",
    );
    return $form;
  }
  /**
   * @see TripalImporter::formSubmit()
   */
  public function formSubmit($form, &$form_state) {
    global $user;



    return '';
  }
  /**
   * @see TripalImporter::formValidate()
   */
  public function formValidate($form, &$form_state) {
    // trim character fields
    $form_state['values']['query_uniquename']  = trim($form_state['values']['query_uniquename']);
    $form_state['values']['query_type']        = trim($form_state['values']['query_type']);
    $form_state['values']['query_re']          = trim($form_state['values']['query_re']);


    // check the regular expression to make sure it is valid
    set_error_handler(function() {}, E_WARNING);
    $result = preg_match("/" . $form_state['values']['query_re'] . "/", null);
    restore_error_handler();
    if ($result === FALSE) {
        form_set_error('query_re', 'Invalid regular expression.');
    }
  }

  /**
   * @see TripalImporter::run()
   */
  public function run() {
    $arguments = $this->arguments['run_args'];
    $analysis_id = $arguments['analysis_id'];
    $interproxmlfile = trim($this->arguments['file']['file_path']);
    $parsego = $arguments['parsego'];
    $query_re = $arguments['query_re'];
    $query_type = $arguments['query_type'];
    $query_uniquename =$arguments['query_uniquename'];

    $this->parseXMLFile($analysis_id, $interproxmlfile,
      $parsego, $query_re, $query_type, $query_uniquename);
  }

  /**
   * @file
   * The interpro XML parser
   */
  function parseXMLFile($analysis_id, $interproxmlfile,
      $parsego, $query_re, $query_type, $query_uniquename) {
        
        $transaction = db_transaction();
        try {
          
          // clear out the anslysisfeature table for this analysis before getting started
          chado_delete_record('analysisfeature', array('analysis_id' => $analysis_id));
          
          // If user input a file(e.g. interpro.xml)
          if (is_file($interproxmlfile)) {
            $this->parseSingleXMLFile($analysis_id, $interproxmlfile,
                $parsego, $query_re, $query_type, $query_uniquename, 1);
          }
          else {
            // Parsing all files in the directory
            $dir_handle = @opendir($interproxmlfile) or die("Unable to open $interproxmlfile");
            $files_to_parse = [];
            while ($file = readdir($dir_handle)) {
              if (preg_match("/^.*\.xml/i", $file)) {
                $files_to_parse[] = $file;
              }
            }
            
            $total_files = count($files_to_parse);
            $no_file = 0;
            $this->setTotalItems($total_files);
            $this->logMessage(t("$total_files file(s) to be parsed.\n"));
            foreach ($files_to_parse as $file) {
              $no_file ++;
              $this->logMessage(t("File $no_file of $total_files: $file                       \n"));
              $this->parseSingleXMLFile($analysis_id, "$interproxmlfile/$file",
                  $parsego, $query_re, $query_type, $query_uniquename, 0, $no_file, $total_files);
              $this->setItemsHandled($no_file);
            }
          }
        }
        catch (Exception $e) {
          $transaction->rollback();
          $this->logMessage(t("\nFAILED: Rolling back database changes...\n"));
        }
        $this->logMessage(t("\nDone\n"));
  }
  /**
   *
   */
  function parseSingleXMLFile($analysis_id, $interproxmlfile,
      $parsego, $query_re, $query_type, $query_uniquename, $uptate_status = 1,
      $file_no = 0, $total_files = 1) {
        
        $filename = preg_replace('/^.*\/(.*)$/' , '$1', $interproxmlfile);
        
        // Load the XML file
        $xml =  simplexml_load_file($interproxmlfile);
        if ($xml === FALSE) {
          watchdog('tripal_interpro', "Cannot open XML. Please check that it is valid XML.", NULL, WATCHDOG_ERROR);
          return;
        }
        
        // If starts with 'protein-matches' or 'nucleotide-sequence-matches' then this is InterPro v5 XML
        if (preg_match('/^protein-matches/', $xml->getname())) {
          $this->parseXML5($xml, $analysis_id, $filename,
              $parsego, $query_re, $query_type, $query_uniquename, $uptate_status,
              $file_no, $total_files);
        }
        elseif (preg_match('/^nucleotide-sequence-matches/', $xml->getname())) {
          $this->parseXML5($xml, $analysis_id, $filename,
              $parsego, $query_re, $query_type, $query_uniquename, $uptate_status,
              $file_no, $total_files);
        }
        // the  remaining are for InterPro v4 XML
        elseif (preg_match('/^EBIInterProScanResults/', $xml->getname())) {
          // results are from the EBI InterProScan Website Output
          $this->parseXML4($xml, $analysis_id, $filename,
              $parsego, $query_re, $query_type, $query_uniquename, $uptate_status,
              $file_no, $total_files);
        }
        elseif (preg_match('/^interpro_matches/', $xml->getname())) {
          // rseults are from the command-line InterPrScan version
          $this->parseXML4($xml, $analysis_id, $filename,
              $parsego, $query_re, $query_type, $query_uniquename, $uptate_status,
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
   * @param $uptate_status
   * @param $file_no
   * @param $total_files
   */
  function parseXML5($xml, $analysis_id, $filename,
      $parsego, $query_re, $query_type, $query_uniquename, $uptate_status,
      $file_no, $total_files) {
        
        // Count the number of sequences to be processed
        $no_iterations = 0;
        foreach ($xml as $entity) {
          $no_iterations++;
        }
        $this->setTotalItems($no_iterations);
        $idx_iterations = 0;
        
        // Processed each nucelotide sequence entry
        foreach ($xml as $entity) {
          
          // Set job status
          $idx_iterations ++;
          $this->setItemsHandled($idx_iterations);
          
          // get details about this sequence
          $children = $entity->children();
          foreach ($children as $child) {
            $childname = $child->getname();
            
            // the <xref> tag holds the sequence name and description.  There could
            // be multiple sequences that have the same protein and hence the same
            // matches. So, for each one in the file, we want to re-parse the matches
            if ($childname == 'xref') {
              $attrs   = $child->attributes();
              $seqid   = (string) $attrs['id'];
              $seqname = (string) $attrs['name'];
              
              // match the name of the feature in the XML file to a feature in Chado
              $feature_id = $this->matchFeature($seqid, $seqname,
                  $query_re, $query_uniquename, $query_type);
              if (!$feature_id) {
                continue;
              }
              
              // create an entry in the analysisfeature table and add the XML for this feature
              // to the analysisfeatureprop table
              $analysisfeature_id = $this->addAnalysisFeature($feature_id, $analysis_id, $entity);
              if (!$analysisfeature_id) {
                continue;
              }
              
              // parse the XML
              $ipr_array = $this->parseFeatureXML($entity->asXML(), $feature_id);
              
              // add in the IPR terms
              $iprterms = $ipr_array['iprterms'];
              $this->loadIprterms($iprterms, $feature_id, $analysisfeature_id);
              
              // get the DB id for the GO database
              $parsego = chado_get_property(
                  array('table' => 'analysis', 'id' => $analysis_id),
                  array('type_name' => 'analysis_interpro_parsego', 'cv_name' => 'tripal')
                  );
              $go_db_id = chado_query("SELECT db_id FROM {db} WHERE name = 'GO'")->fetchField();
              if ($parsego and !$go_db_id) {
                watchdog('tr_ipr_parse', 'GO schema not installed in chado. GO terms are not processed.', array(), WATCHDOG_WARNING);
              }
              if ($parsego and $go_db_id) {
                $goterms = $ipr_array['goterms'];
                $this->loadGoterms($goterms, $feature_id, $analysisfeature_id, $go_db_id);
              }
            } // end if ($childname == 'xref') {
          } // end foreach ($children as $child) {
        } // end foreach ($xml as $entity)
  }
  
  /**
   *
   */
  function addAnalysisFeature($feature_id, $analysis_id, $xml_obj) {
    
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
    $analysisfeature = chado_select_record('analysisfeature', array('*'), $values);
    if (count($analysisfeature) == 0) {
      $analysisfeature = chado_insert_record('analysisfeature', $values);
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
    $success = chado_insert_record('analysisfeatureprop', $values);
    if (!$success) {
      watchdog('tr_ipr_parse', 'ERROR: cannot add the XML for the feature into the analysisfeatureprop table',
          array(), WATCHDOG_ERROR);
    }
    
    return $analysisfeature_id;
  }
  /**
   * @param $xml
   */
  function parseXML4($xml, $analysis_id, $filename,
      $parsego, $query_re, $query_type, $query_uniquename, $uptate_status,
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
        $this->setTotalItems($no_iterations);
        $idx_iterations = 0;
        
        // Processed each protein
        foreach ($proteins as $protein) {
          
          // Set job status
          $idx_iterations ++;
          $this->setItemsHandled($idx_iterations);
          
          // match the protein id with the feature name
          $feature_id = 0;
          $attrs = $protein->attributes();
          $seqid = $attrs['id'];
          
          // is the sequence name a generic name (i.e. 'Sequence_1') then the
          // results do not contain the original sequence names.  The only
          // option we have is to use the filename.  This will work in the case of
          // Blast2GO which stores the XML for each sequence in a file with the
          // the filename the name of the sequence
          if (preg_match('/Sequence_\d+/', $seqid)) {
            $filename = preg_replace('/^.*\/(.*).xml$/', '$1', $interproxmlfile);
            watchdog('tr_ipr_parse', "Sequence name for results is not specific, using filename: %filename as the sequence name\n", array('%filename' => $filename), WATCHDOG_NOTICE);
            $seqid = $filename;
          }
          
          // Remove _ORF from the sequence name
          $seqid = preg_replace('/^(.+)_\d+_ORF\d+.*/', '$1', $seqid);
          
          // match the name of the feature in the XML file to a feature in Chado
          $feature_id = $this->matchFeature($seqid, '', $query_re,
              $query_uniquename, $query_type);
          if (!$feature_id) {
            continue;
          }
          
          // create an entry in the analysisfeature table and add the XML for this feature
          // to the analysisfeatureprop table
          $analysisfeature_id = $this->addAnalysisFeature($feature_id, $analysis_id, $protein);
          if (!$analysisfeature_id) {
            continue;
          }
          
          // parse the XML
          $ipr_array = $this->parseFeatureXML($protein->asXML(), $feature_id);
          
          // add in the IPR terms
          $iprterms = $ipr_array['iprterms'];
          $this->loadIprterms($iprterms, $feature_id, $analysisfeature_id);
          
          // get the DB id for the GO database
          $parsego = chado_get_property(
              array('table' => 'analysis', 'id' => $analysis_id),
              array('type_name' => 'analysis_interpro_parsego', 'cv_name' => 'tripal')
              );
          $go_db_id = chado_query("SELECT db_id FROM {db} WHERE name='GO'")->fetchField();
          if ($parsego and !$go_db_id) {
            watchdog('tr_ipr_parse', 'GO schema not installed in chado. GO terms are not processed.', array(), WATCHDOG_WARNING);
          }
          // add in the GO Terms
          if ($parsego and $go_db_id) {
            $goterms = $ipr_array['goterms'];
            $this->loadGoterms($goterms, $feature_id, $analysisfeature_id, $go_db_id);
          }
        } // end foreach ($proteins as $protein)
        
        return;
  }
  /**
   * Attempts to find a feature in Chado that matches the sequence id as listed
   * in the InterProScan XML file
   *
   * @param $seqid
   *   The sequence id provided by the InterProXML file
   * @param $seqname
   *   The sequence name provided by the InterProXML file (may not always be present)
   * @param $query_re
   *   The regular expression provided by the user when the chado_analysis_interpro
   *   node was created by the user
   * @param $query_uniquename
   *   The value provided by the user when the chado_analysis_interpro node was
   *   created that indicates TRUE if the $seqid is the uniquename. Otherwise
   *   it is assumed to just be the feature name.
   * @param $query_type
   *   The feature type name (e.g. mRNA).
   *
   * @return
   *   The feature_id of the matching feature or NULL if not found.
   */
  function matchFeature($seqid, $seqname = '',
      $query_re, $query_uniquename, $query_type) {
        
        $feature = '';
        
        // if a regular expression is provided then pick out the portion requested
        $matches = array();
        if ($query_re) {
          
          // first try the seqid if the regex works then use it
          if (preg_match("/$query_re/", $seqid, $matches)) {
            $feature = $matches[1];
          }
          // second, try the seqname
          elseif ($seqname and preg_match("/$query_re/", $seqname, $matches)) {
            $feature = $matches[1];
          }
          // if no match then generate an error
          else {
            watchdog('tr_ipr_parse', "Failed: Cannot find feature for '%$seqid' using the regular expression: %query_re",
                array('%$seqid' => $seqid, '%query_re' => $query_re));
            return FALSE;
          }
        }
        // If no reqular expression provided then get everything up to the first space
        elseif (preg_match('/^(.*?)\s.*$/',  $seqid, $matches)) {
          $feature = $matches[1];
        }
        else {
          $feature =  $seqid;
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
        
        $feature_arr = chado_select_record('feature', array('feature_id'), $select);
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
   *   An array of terms where each array of the following format:
   *     $iprterms['IPRXXXXXX']['ipr_name']
   *     $iprterms['IPRXXXXXX']['ipr_desc']
   *     $iprterms['IPRXXXXXX']['ipr_type']
   * @param $feature_id
   *   The feature to which the terms should be assocaited
   * @param $analysisfeature_id
   *   The analysisfeature to which the terms should be listed as properties
   *
   */
  function loadIprterms($iprterms, $feature_id, $analysisfeature_id) {
    
    // iterate through each othe IPR terms
    foreach ($iprterms as $ipr_id => $iprterm) {
      
      if ($iprterm['ipr_name'] and $iprterm['ipr_name'] != 'noIPR') {
        
        // currently there is no InterPro Ontology OBO file so we can't
        // load the IPR terms that way, we need to just add them
        // as we encounter them. If the term already exists
        // we do not want to update it.
        $ipr_cvterm = tripal_insert_cvterm(array(
          'id' => "INTERPRO:$ipr_id",
          'name' => $iprterm['ipr_name'],
          'def'  => $iprterm['ipr_desc'],
          'namespace' => 'INTERPRO',
          'is_obsolete' => 1,
          'cv_name' => 'INTERPRO',
          'db_name' => 'INTERPRO'
        ));
        if (!$ipr_cvterm) {
          watchdog('tr_ipr_parse', "Failed: Cannot find cvterm: %name '%value'",
              array('%name' => $ipr_id, '%value' =>  $iprterm['ipr_name']), WATCHDOG_ERROR);
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
        $cvterm = chado_select_record('feature_cvterm', $columns, $values);
        if (count($cvterm) == 0) {
          $success = chado_insert_record('feature_cvterm', $values);
          if (!$success) {
            watchdog('tr_ipr_parse', "Failed: Cannot add cvterm: %name '%value'",
                array('%name' => $ipr_id, '%value' =>  $iprterm['ipr_name']), WATCHDOG_ERROR);
            continue;
          }
        }
        
        // Insert IPR terms into the analysisfeatureprop table but only if it
        // doesn't already exist
        $values = array(
          'analysisfeature_id' => $analysisfeature_id,
          'type_id' => $ipr_cvterm->cvterm_id,
          'rank' => 0,
          'value' => $ipr_id
        );
        $options = array('statement_name' => 'sel_analysisfeatureprop_antyrava');
        $results = chado_select_record('analysisfeatureprop', array('analysisfeatureprop_id'), $values, $options);
        if (count($results) == 0 ) {
          $options = array('statement_name' => 'ins_analysisfeatureprop_antyrava');
          $success = chado_insert_record('analysisfeatureprop', $values, $options);
          if (!$success) {
            watchdog('tr_ipr_parse', "Failed:  Cannot add cvterm as an analysis feature property for the interpro term: %name '%value'",
                array('%name' => $ipr_id, '%value' =>  $iprterm['ipr_name']), WATCHDOG_ERROR);
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
   *   An array containin each GO terms in the following format:
   *     $goterms['GO:XXXXXXX']['category']
   *     $goterms['GO:XXXXXXX']['name']
   * @param $feature_id
   *   The feature to which the terms should be assocaited
   * @param $analysisfeature_id
   *   The analysisfeature to which the terms should be listed as properties
   * @param $go_db_id
   *   The db_id for the GO (Gene Ontology)
   */
  function loadGoterms($goterms, $feature_id, $analysisfeature_id, $go_db_id) {
    
    // cycle through the GO terms and add them to the database
    foreach ($goterms as $go_id => $goterm) {
      
      // seperate the 'GO:' from the term
      if (preg_match("/^.*?GO:(\d+).*$/", $go_id, $matches)) {
        
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
        $feature_cvterm = chado_select_record('feature_cvterm', array('*'), $values);
        if (sizeof($feature_cvterm) == 0) {
          $feature_cvterm = chado_insert_record('feature_cvterm', $values);
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
        $analysisfeatureprop = chado_select_record('analysisfeatureprop', array('*'), $values);
        if (sizeof($analysisfeatureprop) == 0) {
          $values['value'] = $matches[1];
          $analysisfeatureprop = chado_insert_record('analysisfeatureprop', $values);
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
   *   An array with the following fields:
   *
   *     An arrray containing all of the IPR terms mapped to this feature. Each
   *     IPR term is an array with 3 elements. The first element is the IPR
   *     accession, the second is the name and the third is the description
   *       $terms['iprterms']
   *
   *     A string indicating the XML format that was parsed. Valid values
   *     are XML4 or XML5
   *       $terms['format']
   *
   *     An array of terms, where GO:XXXXXXXX idicates a GO accession that
   *     is used as a key for the array.  All GO terms for all matches are stored here.
   *       $terms['goterms']['GO:XXXXXXX']['category']
   *       $terms['goterms']['GO:XXXXXXX']['name']
   *
   *     An array of terms. The variable IPRXXXXXX indicates an IPR accession
   *     that is used as a key for the array.
   *       $terms['iprterms']
   *       $terms['iprterms']['IPRXXXXXX']['ipr_name']
   *       $terms['iprterms']['IPRXXXXXX']['ipr_desc']
   *       $terms['iprterms']['IPRXXXXXX']['ipr_type']
   *
   *     Each term may have one or more matches.  The variable $j indicates
   *     an index variable for iterating through the matches.
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_id']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_name']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_desc']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_dbname']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['evalue']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['score']
   *
   *     An array of terms, where GO:XXXXXXXX idicates a GO accession that
   *     is used as a key for the array.  GO terms are stored a second time
   *     here to associate them with the proper IPR.
   *       $terms['iprterms']['IPRXXXXXX']['goterms']['GO:XXXXXXX']['category']
   *       $terms['iprterms']['IPRXXXXXX']['goterms']['GO:XXXXXXX']['name']
   *
   *     Each match can have multiple start and stop locations. The variable $k
   *     indicates an index variable for iterating through the locations
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_start']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_end']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_score']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_status']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_evalue']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_level']
   *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_evidence']
   */
  function parseFeatureXML($interpro_xml, $feature_id) {
    
    // Load the XML into an object
    $xml_obj = simplexml_load_string($interpro_xml);
    
    // we can distinguish between XML4 and XML5 by looking at the name and attributes
    $name  = $xml_obj->getname();
    $attrs = $xml_obj->attributes();
    
    if ($name == 'nucleotide-sequence') {
      return $this->parseFeatureXML5Nucelotide($xml_obj, $feature_id);
    }
    if ($name == 'protein') {
      // XML 5 protein key has no attributes XML4 does
      if (count($attrs) == 0) {
        return $this->parseFeatureXML5Protein($xml_obj, $feature_id);
      }
      return $this->parseFeatureXML4($xml_obj, $feature_id);
    }
  }
  /**
   * Parses the Interpro XML for a single feature and returns an array
   * containing the results.  This function should be used when IPR XMLv5 is
   * being parsed for a protein input sequence or for the protein subsection
   * of XML from a nucleotide input sequence.
   *
   * @param $xml_obj
   *   The SimpleXMLElement object for a single feature.
   * @param $feature_id
   *   The feature ID to which this blast XML belongs.
   *
   * @return
   *  An array as specified by the tripal_analysis_interpro_parse_feature_XML()
   *  function
   */
  function parseFeatureXML5Protein($xml_obj, $feature_id) {
    
    $terms  = array();
    $terms['format'] = 'XML5';
    $terms['iprterms'] = array();
    $terms['goterms'] = array();
    
    // iterate through each element of the 'protein' children
    foreach ($xml_obj->children() as $element) {
      
      if($element->getname() == 'matches') {
        // iterate through the match elements
        foreach ($element->children() as $match_element) {
          // initalize the $match array where we store details about this match
          $match = array();
          
          // sometimes an alignment is made but there is no corresponding IPR term
          // so we default the match IPR term to 'noIPR'
          $match_ipr_id = 'noIPR';
          $match_ipr_type = '';
          $match_ipr_desc = '';
          $match_ipr_name = '';
          
          // iterate through the children of this match to collect the details
          foreach ($match_element->children() as $match_details) {
            
            // the <signature> tag contains information about the match in the
            // member database (e.g. GENE3D, PFAM, etc).
            if ($match_details->getname() == 'signature') {
              
              // get the details about this match
              $attrs = $match_details->attributes();
              $match['match_name'] = (string) $attrs['name']; // the name of the match
              $match['match_desc'] = (string) $attrs['desc']; // the match description
              $match['match_id']   = (string) $attrs['ac'];   // the library accession number
              
              // find the IPR term and GO Terms associated with this match
              foreach ($match_details->children() as $sig_element) {
                
                // the <entry> tag contains the IPR term entry that corresponds to this match
                if ($sig_element->getname() == 'entry') {
                  $attrs = $sig_element->attributes();
                  $match_ipr_id   = (string) $attrs['ac'];
                  $match_ipr_type = (string) $attrs['type']; // e.g. DOMAIN, SITE
                  $match_ipr_name = (string) $attrs['name']; // the match name
                  $match_ipr_desc = (string) $attrs['desc']; // the match description
                  
                  // initialize the term sub array and matches if they haven't already been added.
                  if (!array_key_exists($match_ipr_id, $terms['iprterms'])) {
                    $terms['iprterms'][$match_ipr_id] = array();
                    $terms['iprterms'][$match_ipr_id]['matches'] = array();
                    $terms['iprterms'][$match_ipr_id]['goterms'] = array();
                  }
                  
                  // get the GO terms which are children of the <entry> element
                  foreach ($sig_element->children() as $entry_element) {
                    if ($entry_element->getname() == 'go-xref') {
                      $attrs = $entry_element->attributes();
                      $go_id = (string) $attrs['id'];
                      $goterm = array();
                      $goterm['category'] = (string) $attrs['category'];
                      $goterm['name']     = (string) $attrs['name'];
                      
                      // GO terms are stored twice. Once with the IPR term to which they were found
                      // and second as first-level element of the $terms array where all terms are present
                      $terms['iprterms'][$match_ipr_id]['goterms'][$go_id] = $goterm;
                      $terms['goterms'][$go_id] = $goterm;
                    }
                  }
                }
                
                // add in the match library and version information
                if ($sig_element->getname() == 'signature-library-release') {
                  $attrs = $sig_element->attributes();
                  $match['match_dbname']  = (string) $attrs['library'];
                  $match['match_version'] = (string) $attrs['version'];
                }
              }
            } // end if($match_element->getname() == 'signature') {
            
            // the <locations> tag lists the alignment locations for this match
            if($match_details->getname() == 'locations') {
              $k = 0;
              foreach ($match_details->children() as $loc_element) {
                $attrs = $loc_element->attributes();
                $match['locations'][$k]['match_start']  = (string) $attrs['start'];
                $match['locations'][$k]['match_end']    = (string) $attrs['end'];
                $match['locations'][$k]['match_score']  = (string) $attrs['score'];
                $match['locations'][$k]['match_evalue'] = (string) $attrs['evalue'];
                $match['locations'][$k]['match_level']  = (string) $attrs['level'];
                $k++;
              }
            } // end if($match_details->getname() == 'locations') {
          } // end foreach ($match_element->children() as $match_details) {
          
          $attrs = $match_element->attributes();
          $match['evalue'] = (string) $attrs['evalue'];
          $match['score']  = (string) $attrs['score'];
          
          // add this match to the IPR term key to which it is associated
          $terms['iprterms'][$match_ipr_id]['matches'][] = $match;
          $terms['iprterms'][$match_ipr_id]['ipr_type'] = $match_ipr_type;
          $terms['iprterms'][$match_ipr_id]['ipr_name'] = $match_ipr_name;
          $terms['iprterms'][$match_ipr_id]['ipr_desc'] = $match_ipr_desc;
          
          // make sure we have a goterms array in the event that none were found
          if (!array_key_exists('goterms', $terms['iprterms'][$match_ipr_id])) {
            $terms['iprterms'][$match_ipr_id]['goterms'] = array();
          }
          
        } // end foreach ($element->children() as $match_element) {
      } // end if($element->getname() == 'matches') {
    } // end foreach ($xml_obj->children() as $element) {
    
    //if (count($terms['iprterms']) > 0) {
    //  dpm($terms);
    //}
    return $terms;
  }
  /**
   * Parses the Interpro XML for a single feature and returns an array
   * containing the results.  This function should be used when IPR XMLv5 is
   * being parsed for a nucleotide input sequence
   *
   * @param $xml_obj
   *   The SimpleXMLElement object for a single feature.
   * @param $feature_id
   *   The feature ID to which this blast XML belongs.
   *
   * @return
   *  An array as specified by the tripal_analysis_interpro_parse_feature_XML()
   *  function
   */
  function parseFeatureXML5Nucelotide($xml_obj, $feature_id) {
    
    $results  = array();
    $results['format'] = 'XML5';
    $results['iprterms'] = array();
    $results['goterms'] = array();
    
    // iterate through each element of the 'nucleotide-sequence' children
    foreach ($xml_obj->children() as $element) {
      if($element->getname() == 'orf') {
        foreach ($element->children() as $sub_element) {
          
          // the <protein> tag is an ORF in the nucleotide sequence
          if ($sub_element->getname() == 'protein') {
            $terms = $this->parseFeatureXML5Protein($sub_element, $feature_id);
            
            // becase there are multiple ORFs per sequence we want to combine the
            // results into a single set to be returned by this function
            foreach ($terms['iprterms'] as $ipr_id => $iprterm) {
              
              // consolidate the IPR
              $results['iprterms'][$ipr_id]['ipr_desc'] = $iprterm['ipr_desc'];
              $results['iprterms'][$ipr_id]['ipr_name'] = $iprterm['ipr_name'];
              $results['iprterms'][$ipr_id]['ipr_type'] = $iprterm['ipr_type'];
              
              // merge the matches
              if (!array_key_exists('matches', $results['iprterms'][$ipr_id])) {
                $results['iprterms'][$ipr_id]['matches'] = array();
              }
              $results['iprterms'][$ipr_id]['matches'] = array_merge($results['iprterms'][$ipr_id]['matches'], $iprterm['matches']);
              
              // merge the goterms
              if (!array_key_exists('goterms', $results['iprterms'][$ipr_id])) {
                $results['iprterms'][$ipr_id]['goterms'] = array();
              }
              $results['iprterms'][$ipr_id]['goterms'] = array_merge($results['iprterms'][$ipr_id]['goterms'], $iprterm['goterms']);
              
              // merge the go terms
              foreach ($iprterm['goterms'] as $go_id => $goterm) {
                $results['goterms'][$go_id]['name']     = $goterm['name'];
                $results['goterms'][$go_id]['category'] = $goterm['category'];
              }
            }
          }
        }
      }
    }
    return $results;
  }
  
  /**
   * Parses the Interpro XML for a single feature and returns an array
   * containing the results.
   *
   * @param $xml_obj
   *   The SimpleXMLElement object for a single feature.
   * @param $feature_id
   *   The feature ID to which this blast XML belongs.
   *
   * @return
   *  An array as specified by the tripal_analysis_interpro_parse_feature_XML()
   *  function
   */
  function parseFeatureXML4($xml_obj, $feature_id) {
    
    $terms  = array();
    $terms['format'] = 'XML4';
    $terms['iprterms'] = array();
    $terms['goterms'] = array();
    
    // get the properties of this result
    $attr = $xml_obj->attributes();
    $protein['orf_id']     = (string) $attr["id"];
    $protein['orf_length'] = (string) $attr["length"];
    $protein['orf_crc64']  = (string) $attr["crc64"];
    
    // iterate through each interpro results for this protein
    foreach ($xml_obj->children() as $intepro) {
      
      // get the interpro term for this match
      $attr = $intepro->attributes();
      $ipr_id = (string) $attr["id"];
      
      $terms['iprterms'][$ipr_id]['ipr_name'] = (string) $attr["name"];
      $terms['iprterms'][$ipr_id]['ipr_desc'] = (string) $attr["name"];
      $terms['iprterms'][$ipr_id]['ipr_type'] = (string) $attr["type"];
      $terms['iprterms'][$ipr_id]['matches']  = array();
      $terms['iprterms'][$ipr_id]['goterms']  = array();
      
      // iterate through the elements of the interpro result
      $match_count = 0;
      foreach ($intepro->children() as $level1) {
        $element_name = $level1->getName();
        if ($element_name == 'match') {
          
          $match = array();
          
          // get the match name for this match
          $attr = $level1->attributes();
          $match['match_id']     = (string) $attr["id"];
          $match['match_name']   = (string) $attr["name"];
          $match['match_dbname'] = (string) $attr["dbname"];
          
          // get the location information for this match
          $k = 0;
          foreach ($level1->children() as $level2) {
            $element_name = $level2->getName();
            if ($element_name == 'location') {
              $attr = $level2->attributes();
              $match['locations'][$k]['match_start']    = (string) $attr["start"];
              $match['locations'][$k]['match_end']      = (string) $attr["end"];
              $match['locations'][$k]['match_score']    = (string) $attr["score"];
              $match['locations'][$k]['match_status']   = (string) $attr["status"];
              $match['locations'][$k]['match_evidence'] = (string) $attr["evidence"];
              $k++;
            }
          }
          
          // add this match to the IPR term key to which it is associated
          $terms['iprterms'][$ipr_id]['matches'][] = $match;
        }
        
        // get the GO terms for this match
        if ($element_name == 'classification') {
          $attrs = $level1->attributes();
          if ($attrs['class_type'] == 'GO') {
            $go_id = (string) $attrs['id'];
            $goterm = array();
            $goterm['category'] = (string) $level1->category;
            $goterm['name']     = (string) $level1->description;
            
            // GO terms are stored twice. Once with the IPR term to which they were found
            // and second as first-level element of the $terms array where all terms are present
            $terms['iprterms'][$ipr_id]['goterms'][$go_id] = $goterm;
            $terms['goterms'][$go_id] = $goterm;
          }
        }
      }
    }
    
    //if (count($terms['iprterms']) > 0) {
    //  dpm($terms);
    //}
    return $terms;
  }
}
