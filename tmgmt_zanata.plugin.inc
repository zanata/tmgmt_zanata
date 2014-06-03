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
    // TODO check for presence of username and api key
    return TRUE;
  }



  public function getSupportedTargetLanguages(TMGMTTranslator $translator, $source_language) {
    // FIXME make this more discerning
    // default implementaion removes the none language and the source language

    // FIXME was indicating that languageMappings was undefined.
    //       maybe it was calling it from a different context so the $this was different...

    // or (trying now) I accidentally left the $ in front of languageMappings.
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

      // $doc_id = $job_item->item_type . '/' . $job_item->item_id; // was having some issues with the '/' in the path, need %2F but encode path may not be doing that
      $doc_id = $job_item->item_type . $job_item->item_id;

      $encoded_id = drupal_encode_path($doc_id);

      $url = "{$base_url}/{$encoded_id}";


      // FIXME make sure plugin only allows en-US to other languages
      // JSON format for source doc is at https://zanata.ci.cloudbees.com/job/zanata-api-site/site/zanata-common-api/rest-api-docs/el_ns0_resource.html
      $data = array(
        'contentType' => 'text/plain', // required!
        'lang' => 'en-US', // required! TODO use source language, if Zanata can handle it
        'textFlows' => $this->prepareStrings($job_item),
        'type' => 'FILE', // required! can be FILE, DOCUMENT or PAGE (what does this even mean?)
        // 'revision' => 2, // TODO make sure this doesn't call failure of second upload. Not listed as required...
        // 'extensions' => array(), // TODO check if this is optional and leave it off
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

      $job_item->addMessage("added to Zanata at {$url}");
    }

    $job->submitted('Job has been pretend-submitted to Zanata for translation');
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