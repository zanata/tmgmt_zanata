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
   * Implements
   * TMGMTTranslatorPluginControllerInterface::getDefaultRemoteLanguagesMappings().
   *
   * @return array
   *   default mappings for local to remote language codes.
   */
  public function getDefaultRemoteLanguagesMappings() {
    return $this->languageMappings;
  }

  /**
   * Implements
   * TMGMTTranslatorPluginControllerInterface::getSupportedRemoteLanguages().
   *
   * @return array
   *   all supported languages of the translator.
   */
  public function getSupportedRemoteLanguages(TMGMTTranslator $translator) {
    return array_values($this->languageMappings);
  }

  /**
   * Implements TMGMTTranslatorPluginControllerInterface::isAvailable().
   */
  public function isAvailable(TMGMTTranslator $translator) {
    return $translator->getSetting('server')
        && $translator->getSetting('project')
        && $translator->getSetting('version')
        && $translator->getSetting('username')
        && $translator->getSetting('api_key');
  }

  /**
   * Implements
   * TMGMTTranslatorPluginControllerInterface::getNotAvailableReason().
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
    return t('Configuration value needed for @items.',
             array('@items' => implode(", ", $items)));
  }

  /**
   * Implements
   * TMGMTTranslatorPluginControllerInterface::getSupportedTargetLanguages().
   */
  public function getSupportedTargetLanguages(TMGMTTranslator $translator, $source_language) {
    return array_keys($this->languageMappings);
  }

  /**
   * Implements TMGMTTranslatorPluginControllerInterface::canTranslate().
   */
  public function canTranslate(TMGMTTranslator $translator, TMGMTJob $job) {
    $result = $this->canTranslateWithReason($translator, $job);
    return $result['can translate'];
  }

  /**
   * Implements
   * TMGMTTranslatorPluginControllerInterface::getNotCanTranslateReason().
   */
  public function getNotCanTranslateReason(TMGMTJob $job) {
    $translator = $job->getTranslator();
    $result = $this->canTranslateWithReason($translator, $job);

    if (isset($result['reason'])) {
      return $result['reason'];
    }
    return '';
  }

  /**
   * Check whether translation is possible, and why or why not.
   *
   * @return array
   *   an associative array with 'can translate' indicating whether translation
   *   is possible, and 'reason' containing a localised string with the reason
   *   that translation is not possible.
   */
  private function canTranslateWithReason(TMGMTTranslator $translator, TMGMTJob $job) {
    $result = array();

    if (!$this->isAvailable($translator)) {
      $result['can translate'] = FALSE;
      $result['reason'] = t(
        'The translator is currently unavailable. Check the configuration');
      return $result;
    }

    $local_source_language = $job->source_language;
    $source_language = $this->mapToRemoteLanguage($translator,
                                                  $local_source_language);

    // Zanata can only translate from en-US at the moment.
    // Using any en locale as a good approximation.
    $from_english = substr($source_language, 0, 2) === 'en';
    if (!$from_english) {
      $result['can translate'] = FALSE;
      $result['reason'] = t(
        '"@locale" is not a valid source language. Zanata currently only handles source strings in English locales.',
        array('@locale' => $local_source_language));
      return $result;
    }

    $supported_languages = $this->getSupportedRemoteLanguages($translator);
    $target_language = $this->mapToRemoteLanguage($translator,
                                                  $job->target_language);
    $supported_target = in_array($target_language, $supported_languages);
    if (!$supported_target) {
      $result['can translate'] = FALSE;
      $result['reason'] = t(
        '"@locale" is not a supported target language for translation. Check language mappings.',
        array('@locale' => $job->target_language));
      return $result;
    }

    $result['can translate'] = TRUE;

    return $result;
  }

  /**
   * Similar to requestTranslation(), but will always submit all items.
   *
   * Items are submitted even if they have already been submitted to the Zanata
   * server.
   */
  public function resubmitTranslation(TMGMTJob $job) {
    $this->requestTranslation($job, FALSE);
  }

  /**
   * Implements TMGMTTranslatorPluginControllerInterface::requestTranslation().
   */
  public function requestTranslation(TMGMTJob $job, $skip_submitted = TRUE) {
    $this->getConnector($job)->requestTranslation($skip_submitted);
  }

  /**
   * Create a connector for a job.
   *
   * @return TmgmtZanataConnector
   *   a new connector for the given job.
   */
  public function getConnector(TMGMTJob $job) {
    return new TmgmtZanataConnector($job);
  }

}
