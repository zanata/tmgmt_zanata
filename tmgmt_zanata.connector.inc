<?php
/**
 * @file
 * Connector to Zanata server.
 */

/**
 * Handles operations relating to translation job requests and responses.
 */
class TmgmtZanataConnector {

  // docId is stored in a varchar(255) in the Zanata database
  const MAX_DOCID_LENGTH = 255;

  private $useSegmentation;

  private $segmenter;
  private $emailSettings;

  private $job;
  private $translator;
  private $controller;
  private $notification;
  private $restclient;

  private $threshold;

  private $count_items_with_changes;
  private $count_strings_changed;

  /**
   * Create a new connector for the given job.
   *
   * A separate connector should be used for each job.
   */
  public function __construct(TMGMTJob $job) {

    $translator = $job->getTranslator();

    $this->emailSettings = $translator->getSetting('email');
    $segmentation = $translator->getSetting('segmentation');
    $this->useSegmentation = $segmentation != 'NONE';
    // TODO only create if $useSegmentation
    $this->segmenter = new TmgmtZanataSegmenter($segmentation);
    $this->notification = new TMGMTZanataTranslatorNotification();
    $this->restclient = new TMGMTZanataTranslatorRESTClient($job);

    $this->job = $job;
    $this->translator = $translator;
    $this->controller = $translator->getController();

    $this->threshold = 100;

    $this->count_items_with_changes = 0;
    $this->count_strings_changed = 0;
  }

  /**
   * the URL for the translation editor in Zanata.
   *
   */
  public function getEditorUrl() {
    return $this->restclient->getEditorUrl();
  }

  /**
   * Saves received translations to TMGMT job_item
   *
   * @param $job_item
   * @param $response
   * @param $translated
   */
  private function saveTranslations($job_item, $response, $translated) {

    $expanded_translated = tmgmt_unflatten_data($translated);

    $count_finished_before = $this->countFinishedTranslations($job_item);
    $job_item->addTranslatedData($expanded_translated);
    $count_finished_after = $this->countFinishedTranslations($job_item);

    $delta_finished = $count_finished_after - $count_finished_before;
    if ($delta_finished !== 0) {
      $job_item->addMessage(format_plural($delta_finished,
          '1 new translation fetched.',
          '@count new translations fetched.'));
      $this->count_items_with_changes++;
      $this->count_strings_changed += $delta_finished;
    }

    $this->saveResponseEtagForItem($response, $job_item);
  }

  /**
   * Check for and download available translations from Zanata server.
   */
  public function pollTranslations() {
    $job = $this->job;
    $base_options = $this->restclient->getBaseOptions();

    $response = null;
    $entire_translations = null;

    foreach ($job->getItems() as $job_item) {

      $html_node_content = $this->determineNodeContentFormat($job_item);

      if (!$html_node_content) {
        $url = $this->restclient->getItemTranslationUrl($job_item);
        $options = $base_options;

        /* TODO consider skipping any item that is marked as completed, to avoid
         * excess network traffic. This would miss translations that have been
         * updated since the last check.
         */
        $this->setEtagHeaderFromItem($options, $job_item);
        $response = $this->restclient->process_request($url, $options);

        if ($response->code == 200) {

          $data = drupal_json_decode($response->data);
          $targets = $data['textFlowTargets'];

          if ($this->useSegmentation) {
            $translated = $this->processTranslatedSegments($job_item, $targets);
          } else {
            $translated = $this->processUnsegmentedTranslations($targets);
          }
          $this->saveTranslations($job_item, $response, $translated);

        } else {
          $this->switchResponseCode($response, $job_item, $url);
        }

      } else {

        // determine what all documents belong to this job_item
        // and from all docs, filter html ones
        global $docId;
        $docId = $this->restclient->getDocId($job_item);
        $docsList = array();
        $url = $this->restclient->getDocsListUrl();
        $options = $base_options;
        $response = $this->restclient->process_request($url, $options);
        $data = drupal_json_decode($response->data);

        foreach ($data as $doc) {
          array_push($docsList, $doc['name']);
        }

        if(!function_exists("filter_docs")) {
          function filter_docs($value) {
            global $docId;
            return strstr($value, $docId);
          }
        }

        if(!function_exists("filter_html_docs")) {
          function filter_html_docs($value)
          {
            return strstr($value, '.html');
          }
        }

        $related_docs = array_filter($docsList, 'filter_docs');
        $html_docs = array_filter($related_docs, 'filter_html_docs');
        $non_html_docs = array_diff($related_docs, $html_docs);

        foreach ($non_html_docs as $index => $doc) {
          if ($doc == $docId) {
            $url = $this->restclient->getItemTranslationUrl($job_item);
            $options = $base_options;
            $response = $this->restclient->process_request($url, $options);

            if ($response->code == 200) {

              $data = drupal_json_decode($response->data);
              $targets = $data['textFlowTargets'];
              $translated = $this->processUnsegmentedTranslations($targets);
              $this->saveTranslations($job_item, $response, $translated);

            } else {
              $this->switchResponseCode($response, $job_item, $url);
            }
          }
        }

        foreach ($html_docs as $index => $doc) {
          // check stats
          $url = $this->restclient->getDocStatsUrl($doc);
          $options = $this->restclient->getBaseOptions();
          $response = $this->restclient->process_request($url, $options);
          if ($response->code == 200) {
            $translation_threshold = $this->getTranslationThreshold(
                drupal_json_decode($response->data)
            );

            if ($translation_threshold >= $this->threshold) {
              // download file
              $url = $this->restclient->getFileDownloadUrl($doc);
              $options = $this->restclient->getBaseOptions(
                  'application/octet-stream', 'application/octet-stream'
              );
              $response = $this->restclient->process_request($url, $options, true);

              if ($response->code == 200) {
                $data = $this->createItemResponse(
                    rtrim(substr($doc, strlen($docId . '.')), '.html'), $response->data
                );
                // save translations
                $targets = $data['textFlowTargets'];
                $translated = $this->processUnsegmentedTranslations($targets);
                $this->saveTranslations($job_item, $response, $translated);

              } else {
                $this->switchResponseCode($response, $job_item, $url);
              }
            }
          }
        }
      }

      if ($this->count_items_with_changes === 0) {
        $job->addMessage('Checked for translations. No new translations were available.');
      } else {
        $job->addMessage(format_plural($this->count_items_with_changes,
            'Checked for translations. Fetched new translations for 1 node (total translations: @new_translations).',
            'Checked for translations. Fetched new translations for @count nodes (total translations: @new_translations).',
            array(
                '@new_translations' => $this->count_strings_changed
            )));
      }
    }
  }

  /**
   * Handles REST responses which are not succeed
   *
   * @param $response
   * @param $job_item
   * @param $url
   */
  private function switchResponseCode($response, $job_item, $url) {
    switch ($response->code) {
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
        $this->addPollTranslationErrorToItem($job_item, $response, $url);
        break;
    }
  }

  /**
   * Add a message to a job item with detail of a failed translation poll.
   */
  private function addPollTranslationErrorToItem(TMGMTJobItem $job_item, $response, $url) {
    if (isset($response->error)) {
      $job_item->addMessage(
        'Tried to update translations but got response code @code when fetching URL @url. Error message: "@error".',
        array(
          '@code' => $response->code,
          '@url' => $url,
          '@error' => $response->error,
        ));
    }
    else {
      $job_item->addMessage(
        'Tried to update translations but got response code @code when fetching URL @url.',
        array(
          '@code' => $response->code,
          '@url' => $url,
        ));
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
   * Request translation of all unsubmitted items in the job.
   */
  public function requestTranslation($skip_submitted = TRUE) {
    $all_submissions_worked = TRUE;
    $sent_translations_for_how_many = 0;
    $base_options = $this->restclient->getBaseOptions();
    $base_options['method'] = 'PUT';
    $email_contents = Array();

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

      $docNode = $job_item->item_type.'/'.$job_item->item_id;
      $docTitle = $job_item->data['node_title']['#text'];
      global $base_url;
      $docDetails = array(
        'docTitle' => $docTitle, 'docName' => $this->restclient->getDocId($job_item),
        'zanataDocURL' => $this->restclient->getDocEditorUrl($job_item),
        'editorURL' => $this->getEditorUrl(),
        'drupalURL' => $base_url.'/'.$docNode
      );

      array_push($email_contents, $docDetails);

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

      // Send email notification
      $locale = $this->restclient->getLocale();
      $notificationEmail = $this->emailSettings['address'];
      $emailSubject = 'Translation job submitted to Zanata for locale: '.$locale;
      $body = $this->notification->formatEmailBody($email_contents, $locale);

      if ($this->emailSettings['notification'] === 'default' and $notificationEmail){
        $this->notification->send_drupal_mail('default_from', $notificationEmail, $emailSubject, $body);
      }
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

    if (!$is_node) {
      $job_item->addMessage(
        'Item "@id" is not type "node" (is type "@type"), unable to look up translations.',
        array(
          '@id' => $id,
          '@type' => $job_item->item_type,
        ));
      return FALSE;
    }

    $trans_data = $this->getExistingTransData($job_item);

    if ($trans_data === NULL) {
      // no existing translations found. Probably don't need a log message.
      return FALSE;
    }

    $url = $this->restclient->getItemTranslationUrl($job_item);

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
        $this->showDocumentNotFoundErrorMessage($this->restclient->getDocId($job_item));
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
        $this->addSendTranslationsErrorToItem($job_item, $response, $url);
        return FALSE;
    }
  }

  private function addSendTranslationsErrorToItem(TMGMTJobItem $job_item, $response, $url) {
    if (isset($response->error)) {
      $job_item->addMessage(
        'Tried to send existing translations but got response code @code when sending to URL @url. Error: "@error"',
        array(
          '@code' => $response->code,
          '@url' => $url,
          '@error' => $response->error,
        ));
    }
    else {
      $job_item->addMessage(
        'Tried to send existing translations but got response code @code when sending to URL @url.',
        array(
          '@code' => $response->code,
          '@url' => $url,
        ));
    }
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
      $source_data = $this->getFlattenedSourceData($job_item);
      $data = $this->segmenter->segmentTranslations($source_data, $data);
    }

    $items = array();
    $document = $this->restclient->getDocId($job_item);
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

  /**
   * Find any existing translations for a job item, in the target
   * language of the job.
   *
   * @return array
   *   Translations with keys that match source keys, or NULL if there are no
   *   translations of the item in the job's target language.
   */
  private function getExistingTransData(TMGMTJobItem $job_item) {
    $id = $job_item->item_id;
    $node = node_load($id);

    $translations = translation_node_get_translations($node->tnid);

    if (!$translations) {
      $job_item->addMessage(
        'No existing translations found for node "@id".',
        array('@id' => $id));
      return;
    }

    $job = $job_item->getJob();
    $language = $job->target_language;
    $translated = array_key_exists($language, $translations);

    if (!$translated) {
      $job_item->addMessage(
        'No existing "@lang" translation found for node "@id".',
        array(
          '@lang' => $language,
          '@id' => $id,
        ));
      return;
    }

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
      'Existing translations: found "@lang" translations for @count of @total strings in "node/@id".',
      array(
        '@lang' => $language,
        '@count' => count($trans_data_matching_source),
        '@total' => count($source_data),
        '@id' => $id,
      ));

    return $trans_data_matching_source;
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
   * To determine content format is HTML or not.
   *
   * @param $job_item
   * @return bool
   */
  private function determineNodeContentFormat($job_item) {

    // check HTML segmentation setting
    if ($this->translator->getSetting('segmentation') !== 'HTML') {
      return FALSE;
    }

    // check html format for any of the elements
    $job_data = $job_item->data;
    foreach ($job_data as $node_field => $value_array) {
      foreach ($value_array as $field => $contents) {
        if (is_int($field) and array_key_exists('format', $contents) and
            strstr($contents['format']['#text'], 'html')) {
            return TRUE;
        }
      }
    }
    return FALSE;
  }

  /**
   * @return array
   */
  private function getCommonData() {
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
    return $common_data;
  }

  /**
   * @param $haystack
   * @param $needle
   * @return bool
   */
  private function ends_with($haystack, $needle) {
    return $needle === "" || (($temp = strlen($haystack) - strlen($needle))
        >= 0 && strpos($haystack, $needle, $temp) !== false);
  }

  /**
   * Request translation for a single item.
   *
   * @return bool
   *   TRUE if the source was submitted successfully, FALSE otherwise
   */
  private function requestItemTranslation(TMGMTJobItem $job_item, $base_options) {

    $html_flag = $this->determineNodeContentFormat($job_item);
    $html_doc = null;
    $process_request_queue = array();

    if (!$html_flag) {
      $url = $this->restclient->getItemUrl($job_item);
      $options = $base_options;
      $data = $this->getCommonData();
      $data['textFlows'] = $this->prepareStrings($job_item);
      $options['data'] = json_encode($data);
      $process_request_queue[$url] =  $options;

    } else {

      $job_item_data = new TmgmtZanataJobItemDataBuilder($job_item);
      $job_item_contents = $job_item_data->process_node_contents();

      foreach ($job_item_contents as $content) {
        if ($content->is_html) {
          $html_doc = $this->restclient->getDocId($job_item).'.'.$content->key.'.html';
          $url = $this->restclient->getFileUploadUrl($html_doc);
          $options = $base_options;
          $options['data'] = $content->data;
          $options = $this->restclient->handleFileUpload($options);
          $process_request_queue[$url] =  $options;
        } else {
          $url = $this->restclient->getItemUrl($job_item);
          $options = $base_options;
          $data = $this->getCommonData();
          $data['textFlows'] = array($this->createItem($content->key, $content->data));
          $options['data'] = json_encode($data);
          $process_request_queue[$url] = $options;
        }
      }
    }

    /* TODO consider copytrans */

    // Pushing node contents to server
    $rest_responses = array();

    foreach ($process_request_queue as $url => $options) {
      $response = $this->restclient->process_request($url, $options);
      array_push($rest_responses, $response);

      switch ($response->code) {
        case 200:
        case 201:
          $doc_id = $this->ends_with($url, '.html') ? $html_doc : $this->restclient->getDocId($job_item);
          $job_item->addMessage(
              'Sent document: item "@title" sent to Zanata as document "@doc_id".',
              array(
                  '@title' => $job_item->defaultLabel(),
                  '@doc_id' => $doc_id,
              ));
          // Add doc_id as remote_identifier_1, full URL as remote_url.
          $job_item->addRemoteMapping(NULL, $doc_id, array(
              'remote_url' => $url,
          ));
          break;

        case 401:
          $this->showAuthErrorMessage();
          $job_item->addMessage(
              'Item "@title" was not successfully sent to Zanata due to an authorization error.',
              array(
                  '@title' => $job_item->defaultLabel(),
                  '@code' => $response->code,
              ), 'error');
          break;

        case 403:
          $this->showPermissionErrorMessage();
          break;

        case 404:
          $this->showNotFoundErrorMessage();
          break;

        default:
          $doc = $this->ends_with($url, '.html') ? $html_doc : $this->restclient->getDocId($job_item);
          $this->addSendSourceErrorToItem($job_item, $response, $doc);
          break;
      }
    }

    if(!function_exists("not_success_response")) {
      function not_success_response($resp){
        return !in_array($resp->code, array(200, 201));
      }
    }
    $failed_responses = array_filter($rest_responses, 'not_success_response');
    return empty($failed_responses);
  }

  private function addSendSourceErrorToItem(TMGMTJobItem $job_item, $response, $doc) {
    if (isset($response->error)) {
      $job_item->addMessage(
        'Item "@title" was not successfully sent to Zanata. Status code: @code. Error: "@error"',
        array(
          '@title' => $doc,
          '@code' => $response->code,
          '@error' => $response->error,
        ), 'error');
    }
    else {
      $job_item->addMessage(
        'Item "@title" was not successfully sent to Zanata. Status code: @code.',
        array(
          '@title' => $doc,
          '@code' => $response->code,
        ), 'error');
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
    $data = $this->getFlattenedSourceData($job_item);

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
   * Get flattened, filtered source data for a given job item.
   */
  private function getFlattenedSourceData(TMGMTJobItem $job_item) {
    $data = array_filter(tmgmt_flatten_data($job_item->getData()),
                         '_tmgmt_filter_data');

    // The document on Zanata should have the title at the beginning to help
    // translators understand the context.
    if (array_key_exists('node_title', $data)) {
      $data = array('node_title' => $data['node_title']) + $data;
    }

    return $data;
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

  /**
   * @param $resId
   * @param $content
   * @return array
   */
  private function createItemResponse($resId, $content) {
    $item_response = array();
    $item_response['extensions'] = array();
    $item_response['textFlowTargets'] = array();

    $node = array(
        'resId' => $resId,
        'state' => 'Approved',
        'translator' => array(),
        'content' => $content,
        'extensions' => array(),
        'revision' => 1,
        'textFlowRevision' => 1,
    );

    array_push($item_response['textFlowTargets'], $node);
    return $item_response;
  }

  /**
   * @param $translations
   * @return array
   */
  private function combineTextFlowTargets($translations) {
    $targets = array();
    foreach ($translations as $node => $value) {
        foreach ($value['textFlowTargets'] as $index => $text) {
          array_push($targets, $text);
        }
    }
    return $targets;
  }

  /**
   * Calculates translation threshold
   *
   * @param $stats_data
   * @return float
   */
  private function getTranslationThreshold($stats_data) {

    $translated = 0;
    $total = 0;

    if (array_key_exists('stats', $stats_data)) {
      $stats = $stats_data['stats'];
      foreach ($stats as $index => $stat) {
        foreach ($stat as $item => $value) {
          if ($item === 'translated') {
            $translated += $value;
          } elseif ($item === 'total') {
            $total += $value;
          }
        }
      }
    }

    return ($translated * 100) / $total;
  }

  /**
   * Categorise locales and documents in express checkout
   *
   * @param $contents
   * @return array
   */
  private function process_email_contents($contents) {
    $locales_editor = array();
    $docs = array();

    foreach ($contents as $node) {
      $locales_editor[$node['locale']] = $node['editor'];
      foreach ($node['docs'] as $doc_path => $doc_title) {
        if (!array_key_exists($doc_path, $docs)) {
          global $base_url;
          $docs[$doc_path] = array(
              'title' => $doc_title,
              'drupal' => $base_url . '/' . $doc_path
          );
        }
      }
    }
    return array('locales' => $locales_editor, 'docs' => $docs);
  }

  /**
   * Express checkout email notification
   *
   * @param $contents
   */
  public function express_checkout_notification($contents) {
    $email_contents = $this->process_email_contents($contents);
    $locales = array_keys($email_contents['locales']);

    // Send email notification
    $notificationEmail = $this->emailSettings['address'];
    $emailSubject = 'Translation job submitted to Zanata for ' . count($locales) . ' languages.';
    $body = $this->notification->formatEmailBody($email_contents, $locales, "express");

    if ($this->emailSettings['notification'] === 'express' and $notificationEmail){
      $this->notification->send_drupal_mail('default_from', $notificationEmail, $emailSubject, $body);
    }
  }
}
