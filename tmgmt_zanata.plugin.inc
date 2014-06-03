<?php

/**
 * @file
 * Provides Zanata translation plugin controller.
 */

/**
 * Zanata translation plugin controller.
 */
class TMGMTZanataTranslatorPluginController extends TMGMTDefaultTranslatorPluginController {

  protected $languageMappings = array(
    'de' => 'de',
  );

  /**
   * Implements TMGMTTranslatorPluginControllerInterface::getDefaultRemoteLanguagesMappings().
   */
  public function getDefaultRemoteLanguagesMappings() {
    return $this->languageMappings;
  }

  /**
   *Implements getSupportedRemoteLanguages().
   */
  public function getSupportedRemoteLanguages(TMGMTTranslator $translator) {
    return array_values($this->languageMappings);
  }

  public function isAvailable(TMGMTTranslator $translator) {
    return $translator->getSetting('server')
        && $translator->getSetting('project')
        && $translator->getSetting('version')
        && $translator->getSetting('username')
        && $translator->getSetting('api_key');
  }

  public function getNotAvailableReason(TMGMTTranslator $translator) {
    $items = array();
    if (!$translator->getSetting('server')) {
      $items[] = 'Zanata server';
    }
    if (!$translator->getSetting('project')) {
      $items[] = 'Project ID';
    }
    if (!$translator->getSetting('version')) {
      $items[] = 'Version ID';
    }
    if (!$translator->getSetting('username')) {
      $items[] = 'username';
    }
    if (!$translator->getSetting('api_key')) {
      $items[] = 'API key';
    }
    $items_string = implode (", ", $items);
    return t("Configuration value needed for {$items_string}.");
  }



  public function getSupportedTargetLanguages(TMGMTTranslator $translator, $source_language) {
    // FIXME make this more discerning
    // default implementaion removes the none language and the source language
    return array_keys($this->languageMappings);
  }


  public function canTranslate(TMGMTTranslator $translator, TMGMTJob $job) {
    // FIXME this is too broad
    return TRUE;
  }


  /**
   * Implements TMGMTTranslatorPluginControllerInterface::requestTranslation().
   */
  public function requestTranslation(TMGMTJob $job) {

    $base_url = _tmgmt_zanata_base_url($job->getTranslator());
    $translator = $job->getTranslator();
    $username = $translator->getSetting('username');
    $api_key = $translator->getSetting('api_key');

    foreach ($job->getItems() as $job_item) {
      $url = _tmgmt_zanata_item_url($base_url, $job_item);

      // FIXME make sure plugin only allows en-US to other languages
      // JSON format for source doc is at https://zanata.ci.cloudbees.com/job/zanata-api-site/site/zanata-common-api/rest-api-docs/el_ns0_resource.html
      $data = array(
        'contentType' => 'text/plain', // required
        'lang' => 'en-US', // required! TODO use source language, if Zanata can handle it
        'textFlows' => $this->prepareStrings($job_item),
        'type' => 'FILE', // required! can be FILE, DOCUMENT or PAGE (what does this even mean?)
        // 'revision' => 2, // TODO make sure this doesn't call failure of second upload. Not listed as required...
        // 'extensions' => array(), // TODO look at using this for source comments
      );

      $options = array(
        'method' => 'PUT',
        'headers' => array(
          'X-Auth-User' => $username,
          'X-Auth-Token' => $api_key,
          'Content-Type' => 'application/json',
          'Accept' => 'application/json',
        ),
        'data' => json_encode($data),
      );

      drupal_http_request($url, $options);

      $doc_id = _tmgmt_zanata_item_doc_id($job_item);
      $title = $this->getTitle($job_item);
      $job_item->addMessage("Item \"{$title}\" sent to Zanata as document \"{$doc_id}\"");
    }

    $job->submitted('Job has been submitted to Zanata for translation.');
  }

  protected function getTitle(TMGMTJobItem $job_item) {
    $job_data = $job_item->getData();
    // FIXME defensive access, don't want to fail if node title is not set
    $title_data = $job_data['node_title'];
    return $title_data['#text'];
  }

  /**
   * @return an array of text flow arrays ready to encode as JSON and send to Zanata server
   */
  protected function prepareStrings(TMGMTJobItem $job_item) {
    $data = $job_item->getData();
    $data = array_filter(tmgmt_flatten_data($job_item->getData()), '_tmgmt_filter_data');

    $items = array();
    foreach ($data as $key => $value) {
      $items[] = array(
        'id' => $key,
        'lang' => 'en-US',
        'content' => $value['#text'],
        'contents' => NULL,
        'plural' => FALSE,
        'extensions' => array(),
        'revision' => 1
      );
    }

    return $items;    
  }

}