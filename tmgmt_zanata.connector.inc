<?php
/**
 * @file
 * Connector to Zanata server.
 */

/**
 * Handles operations relating to Zanata URLs and translation job requests.
 */
class TmgmtZanataConnector {

  // const SEGMENT_PREFIX = '-segment-';

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
    return $this->getItemUrl($job_item) . '/translations/' . $this->getLocale();
  }

  /**
   * Check for and download available translations from Zanata server.
   */
  public function pollTranslations() {


    $job = $this->job;
    $base_options = $this->getBaseOptions();

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
          $job_item->addTranslatedData($expanded_translated);
          $this->saveResponseEtagForItem($response, $job_item);
          break;

        case 304:
          // Not modified (i.e. etag matches).
          $job_item->addMessage(t('No new translations are available yet.'));
          break;

        case 401:
          drupal_set_message(
            t('There was an authorization problem, please check your username and API key in the translator configuration.'),
            'warning',
            FALSE);
          break;

        case 404:
          // This may not be worth logging, is could just be considered noise.
          $job_item->addMessage(t('No translations are available yet.'));
          break;

        default:
          drupal_set_message(
            t('There was a problem fetching translations from the service. See messages below.'),
            'warning',
            FALSE);
          $job_item->addMessage(
            t('Tried to update translations but got response  code @code when fetching URL @url.'),
            array(
              '@code' => $response->code,
              '@url' => $url));
          break;
      }
    }
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
    $base_options = $this->getBaseOptions();
    $base_options['method'] = 'PUT';

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
    }

    if ($all_submissions_worked) {
      $this->job->submitted(
        t('All items have been submitted to Zanata for translation.'));
    }
    else {
      // TODO include count of failed items.
      $this->job->addMessage(
        t('One or more items were not successfully sent to Zanata. Try again after correcting the problem items.'),
        array(), 'error');
    }
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
          t('Item "@title" sent to Zanata as document "@doc_id".'),
          array(
            '@title' => $job_item->defaultLabel(),
            '@doc_id' => $doc_id,
          )
        );
        // Add doc_id as remote_identifier_1, full URL as remote_url.
        $job_item->addRemoteMapping(NULL, $doc_id, array(
          'remote_url' => $url,
        ));
        return TRUE;

      case 401:
        drupal_set_message(t('There was an authorization problem, please check your username and API key in the translator configuration.'),
                             'error',
                             FALSE);
        $job_item->addMessage(
          t('Item "@title" was not successfully sent to Zanata due to an authorization error.'),
          array(
            '@title' => $job_item->defaultLabel(),
            '@code' => $response->code,
          ), 'error');
        return FALSE;

      default:
        $job_item->addMessage(
          t('Item "@title" was not successfully sent to Zanata. Status code: @code.'),
          array(
            '@title' => $job_item->defaultLabel(),
            '@code' => $response->code,
          ), 'error');
        return FALSE;
    }
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
