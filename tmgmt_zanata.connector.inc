<?php
/**
 * @file
 * Connector to Zanata server.
 */

/**
 * Handles operations relating to Zanata URLs and translation job requests.
 */
class TmgmtZanataConnector {

  private $useSegmentation;

  private $segmenter;

  private $serverUrl;
  private $projectId;
  private $versionId;
  private $username;
  private $apiKey;

  private $job;
  private $translator;
  private $controller;

  /**
   * Create a new connector for the given job.
   *
   * A separate connector should be used for each job.
   */
  public function __construct(TMGMTJob $job) {

    $translator = $job->getTranslator();

    $this->serverUrl = $translator->getSetting('server');
    $this->projectId = $translator->getSetting('project');
    $this->versionId = $translator->getSetting('version');
    $this->username = $translator->getSetting('username');
    $this->apiKey = $translator->getSetting('api_key');

    $segmentation = $translator->getSetting('segmentation');
    $this->useSegmentation = $segmentation != 'NONE';
    // TODO only create if $useSegmentation
    $this->segmenter = new TmgmtZanataSegmenter($segmentation);


    $this->job = $job;
    $this->translator = $translator;
    $this->controller = $translator->getController();
  }

  /**
   * Generate the URL for the translation editor in Zanata.
   *
   * This will be the URL to the document list in the editor, ready for a node
   * to be selected.
   */
  public function getEditorUrl() {
    $locale = $this->getLocale();
    return "{$this->serverUrl}/webtrans/Application.seam?project={$this->projectId}&iteration={$this->versionId}&localeId={$locale}&locale=en";
  }

  /**
   * Generate a base URL for documents in a project/version.
   *
   * @return string
   *   the base REST URL for documents in the configured project/version
   */
  private function getBaseUrl() {
    return "{$this->serverUrl}/rest/projects/p/{$this->projectId}/"
           . "iterations/i/{$this->versionId}/r";
  }

  /**
   * Generate a document ID for a given job item.
   *
   * @return string
   *   the unencoded document id on Zanata for the given job item
   */
  private function getDocId($job_item) {
    return "{$job_item->item_type}/{$job_item->item_id}";
  }

  /**
   * Generate a REST URL for a given job item.
   *
   * @return string
   *   the REST URL for the given job item
   */
  private function getItemUrl(TMGMTJobItem $job_item) {
    $encoded_id = strtr($this->getDocId($job_item), '/', ',');
    return "{$this->getBaseUrl()}/{$encoded_id}";
  }

  /**
   * Generate a REST URL for translations of the given job item.
   *
   * @return the REST URL for the translations of the given job item for the
   *         given locale.
   */
  private function getItemTranslationUrl(TMGMTJobItem $job_item) {
    return $this->getItemUrl($job_item) . '/translations/' . $this->getLocale() . '?ext=comment';
  }

  /**
   * Check for and download available translations from Zanata server.
   */
  public function pollTranslations() {
    $job = $this->job;
    $base_options = $this->getBaseOptions();

    $count_items_with_changes = 0;
    $count_strings_changed = 0;

    foreach ($job->getItems() as $job_item) {
      $url = $this->getItemTranslationUrl($job_item);
      $options = $base_options;

      /* TODO consider skipping any item that is marked as completed, to avoid
       * excess network traffic. This would miss translations that have been
       * updated since the last check.
       */
      $this->setEtagHeaderFromItem($options, $job_item);


      $response = drupal_http_request($url, $options);

      switch ($response->code) {
        case 200:
          $data = drupal_json_decode($response->data);
          $targets = $data['textFlowTargets'];

          if ($this->useSegmentation) {
            $translated = $this->processTranslatedSegments($job_item, $targets);
          }
          else {
            $translated = $this->processUnsegmentedTranslations($targets);
          }

          $expanded_translated = tmgmt_unflatten_data($translated);

          $count_finished_before = $this->countFinishedTranslations($job_item);
          $job_item->addTranslatedData($expanded_translated);
          $count_finished_after = $this->countFinishedTranslations($job_item);

          $delta_finished = $count_finished_after - $count_finished_before;
          if ($delta_finished !== 0) {
            $job_item->addMessage(format_plural($delta_finished,
              '1 new translation fetched.',
              '@count new translations fetched.'));
            $count_items_with_changes++;
            $count_strings_changed += $delta_finished;
          }


          $this->saveResponseEtagForItem($response, $job_item);
          break;

        case 304:
          // Not modified (i.e. etag matches).
          break;

        case 401:
          $this->showAuthErrorMessage();
          break;

        case 404:
          // No translations are available yet.
          break;

        default:
          drupal_set_message(
            t('There was a problem fetching translations from the service. See messages below.'),
            'warning',
            FALSE);
          $job_item->addMessage(
            'Tried to update translations but got response  code @code when fetching URL @url.',
            array(
              '@code' => $response->code,
              '@url' => $url));
          break;
      }
    }

    if ($count_items_with_changes === 0) {
      $job->addMessage('Checked for translations. No new translations were available.');
    }
    else {
      $job->addMessage(format_plural($count_items_with_changes,
        'Checked for translations. Fetched new translations for 1 node (total translations: @new_translations).',
        'Checked for translations. Fetched new translations for @count nodes (total translations: @new_translations).',
        array(
          '@new_translations' => $count_strings_changed
        )));
    }
  }

  /**
   * Get the number of translations that have been fetched from Zanata.
   */
  private function countFinishedTranslations(TMGMTJobItem $job_item) {
    // fetched translations show up as translated or accepted, depending whether
    // all translations are available for the node, and whether auto-accept is
    // turned on.
    return $job_item->getCountAccepted() + $job_item->getCountTranslated();
  }

  /**
   * Process translations of segmented text.
   *
   * returns translation data with segments joined for any text that has all
   *         segments translated.
   */
  private function processTranslatedSegments($job_item, $flat_targets) {
    // put targets in an associative array by id for segmenter to do lookup
    $targets = $this->associationWithKey($flat_targets, function ($target) {
      return (string) $target['resId'];
    });

    $source_data = array_filter(tmgmt_flatten_data($job_item->getData()),
                                '_tmgmt_filter_data');
    return $this->segmenter->joinTranslatedSegments($source_data, $targets);
  }

  /**
   * Map all array values to calculated keys.
   *
   * Return a new associative array with the values from the given array mapped
   * to keys generated with the given function.
   */
  private function associationWithKey($arr, $key_function) {
    return array_reduce($arr, function ($carry, $item) use ($key_function) {
      $carry[$key_function($item)] = $item;
      return $carry;
    }, array());
  }

  /**
   * Process translations that do not use segmentation.
   * returns $translated (data that is ready to be unflattened).
   */
  private function processUnsegmentedTranslations($targets) {
    $translated = array();

    foreach ($targets as $target) {
      // TODO map Zanata states sensibly to TMGMT states.
      // FIXME do I need translated and reviewed?
      if ($target['state'] == 'Approved') {
        $key = (string) $target['resId'];
        $translated[$key]['#text'] = (string) $target['content'];
      }
    }

    return $translated;
  }

  /**
   * Store the etag from a response.
   *
   * Stores the etag from the response (if present) as remote mapping data in
   * the given translation job item.
   */
  private function saveResponseEtagForItem($response, TMGMTJobItem $job_item) {
    if (isset($response->headers['etag'])) {
      $etag = $response->headers['etag'];
      $mapping = $this->getRemoteMapping($job_item);
      if ($mapping) {
        $mapping->addRemoteData('etag', $etag);
        $mapping->save();
      }
    }
  }

  /**
   * Set a request etag header to the stored etag value.
   *
   * Extracts the stored etag (if present) from the remote mapping data of the
   * given translation job item, and sets it as the appropriate header in the
   * given request options.
   */
  private function setEtagHeaderFromItem(&$request_options, $job_item) {
    $mapping = $this->getRemoteMapping($job_item);
    if ($mapping) {
      $etag = $mapping->getRemoteData('etag');
      if ($etag) {
        $request_options['headers']['If-None-Match'] = $etag;
      }
    }
  }

  /**
   * Get the first remote mapping for a translation job item.
   *
   * This connector is only using a single mapping for each item, so this should
   * always return that mapping, if present.
   */
  private function getRemoteMapping(TMGMTJobItem $job_item) {
    // Only ever expecting a single mapping.
    // reset resets internal array pointer to the first element and returns it.
    $remote_mappings = $job_item->getRemoteMappings();
    return reset($remote_mappings);
  }


  /**
   * Get the remote locale for the translation job.
   */
  private function getLocale() {
    return $this->controller->mapToRemoteLanguage($this->translator,
                                                  $this->job->target_language);
  }

  /**
   * Generate some common options for a HTTP request.
   *
   * @return options for a HTTP request that include authorization information
   *         and content type headers.
   */
  public function getBaseOptions() {
    return array(
      'headers' => array(
        'X-Auth-User' => $this->username,
        'X-Auth-Token' => $this->apiKey,
        'Content-Type' => 'application/json',
        'Accept' => 'application/json',
      ),
    );
  }

  /**
   * Request translation of all unsubmitted items in the job.
   */
  public function requestTranslation($skip_submitted = TRUE) {
    $all_submissions_worked = TRUE;
    $sent_translations_for_how_many = 0;
    $base_options = $this->getBaseOptions();
    $base_options['method'] = 'PUT';

    // TODO record successful translation submission and skip it next time
    //      (may already be handled with the submitted check, but don't want
    //      failed translation send to cause re-send of source).

    foreach ($this->job->getItems() as $job_item) {
      if ($skip_submitted) {
        $mapping = $this->getRemoteMapping($job_item);
        if ($mapping) {
          // There is a mapping, only added when submitting.
          drupal_set_message(
            t('Skipped an item because it has already been submitted'));
          continue;
        }
      }

      $submitted = $this->requestItemTranslation($job_item, $base_options);
      $all_submissions_worked = $submitted && $all_submissions_worked;

      $translation_submitted = $this->sendExistingTranslation($job_item, $base_options);
      if ($translation_submitted) {
        $sent_translations_for_how_many++;
      }
    }

    if ($all_submissions_worked) {
      $this->job->submitted(
        t('All items have been submitted to Zanata for translation.'));
    }
    else {
      // TODO include count of failed items.
      $this->job->addMessage(
        'One or more items were not successfully sent to Zanata. Try again after correcting the problem items.',
        array(), 'error');
    }

    $this->job->addMessage(
      'Sent existing translations for @sent of @total nodes.',
        array(
          '@sent' => $sent_translations_for_how_many,
          '@total' => count($this->job->getItems())
        ));
  }


  /**
   * Check for existing translations to send for a job item, and send them if
   * they are found.
   *
   * @return bool
   *   TRUE if the translation was submitted successfully, FALSE otherwise
   */
  private function sendExistingTranslation(TMGMTJobItem $job_item, $base_options) {
    // FIXME this check may not be necessary, but keep it for now until it is
    //       clear what an item type can be other than node.
    $is_node = strcmp($job_item->item_type, 'node') === 0;
    $id = $job_item->item_id;

    if ($is_node) {
      $trans_data = $this->getExistingTransData($job_item);
      if ($trans_data !== NULL) {
        $url = $this->getItemTranslationUrl($job_item);

        $data = array(
          'textFlowTargets' => $this->prepareTargets($trans_data, $job_item),
        );

        // makes a copy
        $options = $base_options;
        $options['data'] = json_encode($data);
        $response = drupal_http_request($url, $options);

        switch ($response->code) {
          case 200:
          case 201:
            // translations were successfully sent
            return TRUE;

          case 400:
            drupal_set_message(
              t('There was a problem with the request to send existing translations: "@error"',
                array('@error' => $response->error)),
              'error',
              FALSE);
            return FALSE;

          case 404:
            $this->showDocumentNotFoundErrorMessage();
            return FALSE;

          case 401:
            $this->showAuthErrorMessage();
            return FALSE;

          case 500:
          default:
            drupal_set_message(
              t('There was a problem sending translations to Zanata. See messages below.'),
              'warning',
              FALSE);
            $job_item->addMessage(
              'Tried to send existing translations but got response code @code when sending to URL @url.',
              array(
                '@code' => $response->code,
                '@url' => $url));
            return FALSE;
        }
      }
      else {
        // no existing translations found. Probably don't need a log message.
      }

    }
    else {
      $job_item->addMessage(
        'Item "@id" is not type "node" (is type "@type"), unable to look up translations.',
        array(
          '@id' => $id,
          '@type' => $job_item->item_type,
        ));
    }

    return FALSE;
  }

  /**
   * $data should be an already filtered and flattened array of translations
   *       that all have an id matching a source string id.
   */
  private function prepareTargets($data, $job_item) {
    if ($this->useSegmentation) {
      // Not completely reliable if translations do not follow same sentence or
      // paragraph structure (segments may not line up properly between source
      // and translation).
      // TODO collapse trailing segments if there are too many, so that the last
      //      source segment gets all the translations.
      $data = $this->segmenter->segmentText($data);
    }

    $items = array();
    $document = $this->getDocId($job_item);
    foreach ($data as $key => $value) {
      $text = $value['#text'];
      $items[] = $this->createTranslationItem($key, $text, $document);
    }

    return $items;
  }

  /**
   * Create an associative array for a translation item ready for JSON
   * encoding. The format is suitable for the Zanata server.
   *
   * This is for sending existing translations to Zanata.
   */
  private function createTranslationItem($id, $text, $document) {
    return array(
      'resId' => $id,
      'state' => 'NeedReview',
      'content' => $text,
      'extensions' => array(
        array(
          "object-type" => "comment",
          "space" => "preserve",
          "value" => "Translation sent by tmgmt-zanata because it matches id \"$id\" in document \"$document\"."
        ),
      ),
      'revision' => 1,
      'textFlowRevision' => 1,
    );
  }

  private function getExistingTransData(TMGMTJobItem $job_item) {
    $id = $job_item->item_id;
    $node = node_load($id);

    $translations = translation_node_get_translations($node->tnid);

    if ($translations) {
      $job = $job_item->getJob();
      $language = $job->target_language;
      $translated = array_key_exists($language, $translations);
      if ($translated) {
        $specific_trans = $translations[$language];
        $translated_node = node_load($specific_trans->nid);

        $source_data = $this->simplifyStringData($job_item->getData());

        // transform trans data to the same structure as the source data
        $trans_data = $this->simplifyStringData(
                        $this->getTransData($translated_node, $language));

        $trans_data_matching_source = array();

        foreach (array_keys($source_data) as $key) {
          if (array_key_exists($key, $trans_data)) {
            $trans_value = $trans_data[$key];
            $trans_data_matching_source[$key] = $trans_value;
          }
        }

        // TODO use docId when it is being cached
        $job_item->addMessage(
          'Found existing "@lang" translations for @count of @total strings in "node/@id".',
          array(
            '@lang' => $language,
            '@count' => count($trans_data_matching_source),
            '@total' => count($source_data),
            '@id' => $id,
          ));

        return $trans_data_matching_source;
      }
      else {
        $job_item->addMessage(
          'No existing "@lang" translation found for node "@id".',
          array(
            '@lang' => $language,
            '@id' => $id,
          ));
      }

    }
    else {
      $job_item->addMessage(
        'No existing translations found for node "@id".',
        array('@id' => $id));
    }
  }

  /**
   * Convert source or translation strings to a simple array that has only the
   * translatable fields.
   */
  private function simplifyStringData($data) {
    $flat_data = tmgmt_flatten_data($data);
    return array_filter($flat_data, '_tmgmt_filter_data');
  }

  /**
   * Get fields for a translated node, in the same structure as the source data.
   *
   * Does this need to look up the translated node instead?
   *
   * Based on TMGMTNodeSourcePluginController::getData()
   */
  private function getTransData($translated_node, $target_language) {
    $node = $translated_node;
    $type = node_type_get_type($node);
    // $target_language = $job_item->getJob()->target_language;

    // Get all the fields that can be translated and arrange their values into
    // a specific structure.
    $structure = tmgmt_field_get_source_data('node', $node, $target_language);
    $structure['node_title']['#label'] = $type->title_label;
    $structure['node_title']['#text'] = $node->title;
    return $structure;
  }

  /**
   * Request translation for a single item.
   *
   * @return bool
   *   TRUE if the source was submitted successfully, FALSE otherwise
   */
  private function requestItemTranslation(TMGMTJobItem $job_item, $base_options) {
    $url = $this->getItemUrl($job_item);
    // Makes a copy.
    $options = $base_options;

    $common_data = array(
      // Required.
      'contentType' => 'text/plain',
      // Required.
      'lang' => 'en-US',
      // Required. Must be FILE, DOCUMENT or PAGE.
      'type' => 'FILE',
      // TODO look at using extensions for source comments.
      // 'extensions' => array(),
    );
    $data = $common_data;

    $data['textFlows'] = $this->prepareStrings($job_item);

    $options['data'] = json_encode($data);

    /* TODO consider copytrans */

    $response = drupal_http_request($url, $options);

    switch ($response->code) {
      case 200:
      case 201:
        $doc_id = $this->getDocId($job_item);
        $job_item->addMessage(
          'Item "@title" sent to Zanata as document "@doc_id".',
          array(
            '@title' => $job_item->defaultLabel(),
            '@doc_id' => $doc_id,
          ));
        // Add doc_id as remote_identifier_1, full URL as remote_url.
        $job_item->addRemoteMapping(NULL, $doc_id, array(
          'remote_url' => $url,
        ));
        return TRUE;

      case 401:
        $this->showAuthErrorMessage();
        $job_item->addMessage(
          'Item "@title" was not successfully sent to Zanata due to an authorization error.',
          array(
            '@title' => $job_item->defaultLabel(),
            '@code' => $response->code,
          ), 'error');
        return FALSE;

      case 403:
        $this->showPermissionErrorMessage();
        return FALSE;

      case 404:
        $this->showNotFoundErrorMessage();
        return FALSE;

      default:
        $job_item->addMessage(
          'Item "@title" was not successfully sent to Zanata. Status code: @code.',
          array(
            '@title' => $job_item->defaultLabel(),
            '@code' => $response->code,
          ), 'error');
        return FALSE;
    }
  }

  private function showNotFoundErrorMessage() {
    drupal_set_message(t('The configured project or version does not exist on Zanata, please check that the project slug and version slug in the translator configuration match a project-version on Zanata.'),
                         'error',
                         FALSE);
  }

  private function showDocumentNotFoundErrorMessage($document) {
    drupal_set_message(t('The configured project or version does not exist on Zanata, or there is no document matching "@document". Please check that the project slug and version slug in the translator configuration match a project-version on Zanata, and try to re-send the documents.',
                         array('@document' => $document)),
                       'error',
                       FALSE);
  }

  private function showAuthErrorMessage() {
    drupal_set_message(t('There was an authorization problem, please check your username and API key in the translator configuration.'),
                         'error',
                         FALSE);
  }

  private function showPermissionErrorMessage() {
    drupal_set_message(t('There was a permission problem, please check that the correct project-version and user are specified in the translator configuration, that the user is a maintainer of the project, and that the project-version is not read-only.'),
                         'error',
                         FALSE);
  }

  /**
   * Prepare strings for encoding as JSON.
   *
   * @return an array of text flow arrays ready to encode as JSON and send to
   *         Zanata server
   */
  protected function prepareStrings(TMGMTJobItem $job_item) {
    $data = array_filter(tmgmt_flatten_data($job_item->getData()),
                         '_tmgmt_filter_data');

    // The document on Zanata should have the title at the beginning to help
    // translators understand the context.
    if (array_key_exists('node_title', $data)) {
      $data = array('node_title' => $data['node_title']) + $data;
    }

    if ($this->useSegmentation) {
      $data = $this->segmenter->segmentText($data);
    }

    $items = array();
    foreach ($data as $key => $value) {
      $text = $value['#text'];
      $items[] = $this->createItem($key, $text);
    }

    return $items;
  }

  /**
   * Create an associative array for a translation item ready for JSON
   * encoding. The format is suitable for the Zanata server.
   */
  private function createItem($id, $text) {
    return array(
      'id' => $id,
      // Zanata can only handle source as en-US locale.
      'lang' => 'en-US',
      'content' => $text,
      'contents' => NULL,
      'plural' => FALSE,
      'extensions' => array(),
      'revision' => 1,
    );
  }

}
