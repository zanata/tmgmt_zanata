<?php

/**
 * @file
 * Provides Zanata translation plugin controller.
 */

/**
 * Zanata translation plugin controller.
 */
class TMGMTZanataTranslatorPluginController extends TMGMTDefaultTranslatorPluginController {

  // This will vary depending on the specific Zanata server setup
  // Using active locales from translate.zanata.org as a reasonable default
  // Zanata gets locales from icu4j
  protected $languageMappings = array(
    'af' => 'af',
    'sq' => 'sq',
    'am' => 'am',
    'anp' => 'anp',
    'ar' => 'ar',
    'as' => 'as',
    'ast' => 'ast',
    'az' => 'az',
    'eu' => 'eu',
    'bn-BD' => 'bn-BD',
    'bn-IN' => 'bn-IN',
    'bs' => 'bs',
    'br' => 'br',
    'bg' => 'bg',
    'ca' => 'ca',
    'zh-CN' => 'zh-CN',
    'zh-Hans-CN' => 'zh-Hans-CN',
    'zh-TW' => 'zh-TW',
    'zh-Hant-TW' => 'zh-Hant-TW',
    'hr' => 'hr',
    'cs' => 'cs',
    'da' => 'da',
    'doi' => 'doi',
    'nl' => 'nl',
    'en' => 'en',
    'en-GB' => 'en-GB',
    'en-US' => 'en-US',
    'eo' => 'eo',
    'et' => 'et',
    'fi' => 'fi',
    'fr' => 'fr',
    'gl' => 'gl',
    'ka' => 'ka',
    'de' => 'de',
    'de-DE' => 'de-DE',
    'de-CH' => 'de-CH',
    'el' => 'el',
    'gu' => 'gu',
    'he' => 'he',
    'hi' => 'hi',
    'hu' => 'hu',
    'is' => 'is',
    'id' => 'id',
    'ia' => 'ia',
    'ga' => 'ga',
    'it' => 'it',
    'ja' => 'ja',
    'kn' => 'kn',
    'kk' => 'kk',
    'ky' => 'ky',
    'ko' => 'ko',
    'la' => 'la',
    'lv' => 'lv',
    'lt' => 'lt',
    'nds' => 'nds',
    'mai' => 'mai',
    'ms' => 'ms',
    'ml' => 'ml',
    'mni' => 'mni',
    'mr' => 'mr',
    'mn' => 'mn',
    'nb' => 'nb',
    'nn' => 'nn',
    'or' => 'or',
    'fa' => 'fa',
    'pl' => 'pl',
    'pt' => 'pt',
    'pt-BR' => 'pt-BR',
    'pt-PT' => 'pt-PT',
    'pa' => 'pa',
    'ro' => 'ro',
    'ru' => 'ru',
    'sat' => 'sat',
    'sr' => 'sr',
    'sr-Cyrl' => 'sr-Cyrl',
    'sr-Latn' => 'sr-Latn',
    'sd' => 'sd',
    'si' => 'si',
    'sk' => 'sk',
    'sl' => 'sl',
    'es' => 'es',
    'es-MX' => 'es-MX',
    'es-ES' => 'es-ES',
    'sv' => 'sv',
    'tl' => 'tl',
    'ta' => 'ta',
    'ta-IN' => 'ta-IN',
    'te' => 'te',
    'th' => 'th',
    'tr' => 'tr',
    'uk' => 'uk',
    'ur' => 'ur',
    'ur-PK' => 'ur-PK',
    'vi' => 'vi',
    'cy' => 'cy',
    'xh' => 'xh',
    'brx' => 'brx',
    'hne' => 'hne',
  );

  /**
   * Implements TMGMTTranslatorPluginControllerInterface::getDefaultRemoteLanguagesMappings().
   *
   * @return default mappings for local to remote language codes.
   */
  public function getDefaultRemoteLanguagesMappings() {
    return $this->languageMappings;
  }

  /**
   *Implements TMGMTTranslatorPluginControllerInterface::getSupportedRemoteLanguages().
   *
   * @return all supported languages of the translator.
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

  /**
   *Implements TMGMTTranslatorPluginControllerInterface::getNotAvailableReason().
   */
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
    return array_keys($this->languageMappings);
  }

  public function canTranslate(TMGMTTranslator $translator, TMGMTJob $job) {
    $result = $this->canTranslateWithReason($translator, $job);
    return $result['can translate'];
  }

  public function getNotCanTranslateReason(TMGMTJob $job) {
    $translator = $job->getTranslator();
    $result = $this->canTranslateWithReason($translator, $job);

    if (isset($result['reason'])) {
      return $result['reason'];
    }
    return '';
  }

  protected function canTranslateWithReason(TMGMTTranslator $translator, TMGMTJob $job) {
    $result = array();

    if (!$this->isAvailable($translator)) {
      $result['can translate'] = FALSE;
      $result['reason'] = t('The translator is currently unavailable. Check the configuration');
      return $result;
    }

    $local_source_language = $job->source_language;
    $source_language = $this->mapToRemoteLanguage($translator, $local_source_language);

    // Zanata can only translate from en-US at the moment. Using any en locale as a good approximation.
    $from_english = substr($source_language, 0, 2) === 'en';
    if (!$from_english) {
      $result['can translate'] = FALSE;
      $result['reason'] = t("\"{$local_source_language}\" is not a valid source language. Zanata currently only handles source strings in English locales.");
      return $result;
    }

    $supported_languages = $this->getSupportedRemoteLanguages($translator);
    $target_language = $this->mapToRemoteLanguage($translator, $job->target_language);
    $supported_target = in_array($target_language, $supported_languages);
    if (!$supported_target) {
      $result['can translate'] = FALSE;
      $result['reason'] = t("\"{$job->target_language}\" is not a supported target language for translation. Check language mappings.");
      return $result;
    }

    $result['can translate'] = TRUE;

    return $result;
  }


  /**
   * Implements TMGMTTranslatorPluginControllerInterface::requestTranslation().
   */
  public function requestTranslation(TMGMTJob $job) {

    $translator = $job->getTranslator();
    $base_url = _tmgmt_zanata_base_url($translator);
    $username = $translator->getSetting('username');
    $api_key = $translator->getSetting('api_key');

    $all_responses_ok_so_far = TRUE;

    // FIXME skip any item that has already been requested successfully
    foreach ($job->getItems() as $job_item) {
      $url = _tmgmt_zanata_item_url($base_url, $job_item);

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

      // TODO consider copytrans

      $response = drupal_http_request($url, $options);

      if ($response->code != 200) {
        $all_responses_ok_so_far = FALSE;
        $job_item->addMessage(
          t('Item "@title" was not successfully sent to Zanata. Status code: @code.'),
          array(
            '@title' => $job_item->defaultLabel(),
            '@code' => $response->code,
          ), 'error');
      } else {
        $job_item->addMessage(
          t('Item "@title" sent to Zanata as document "@doc_id".'),
          array(
            '@title' => $job_item->defaultLabel(),
            '@doc_id' => _tmgmt_zanata_item_doc_id($job_item),
          )
        );
      }
    }

    if ($all_responses_ok_so_far) {
      $job->submitted(t('All items have been submitted to Zanata for translation.'));
    } else {
      // TODO include count of failed items
      $job->addMessage(
        t('One or more items were not successfully sent to Zanata. Try again after correcting the problem items.'),
        array(), 'error');
    }
  }

  /**
   * @return an array of text flow arrays ready to encode as JSON and send to Zanata server
   */
  protected function prepareStrings(TMGMTJobItem $job_item) {
    $data = $job_item->getData();
    $data = array_filter(tmgmt_flatten_data($job_item->getData()), '_tmgmt_filter_data');

    // The document on Zanata should have the title at the beginning to help
    // translators understand the context.
    if (array_key_exists('node_title', $data)) {
      $data = array('node_title' => $data['node_title']) + $data;
    }

    $items = array();
    foreach ($data as $key => $value) {
      $items[] = array(
        'id' => $key,
        'lang' => 'en-US', // Zanata can only handle source as en-US locale
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