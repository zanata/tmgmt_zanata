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


// MUST implement TMGMTTranslatorPluginControllerInterface::requestTranslation
// or it will refuse to activate the plugin.

  public function requestTranslation(TMGMTJob $job) {
    $job->submitted('Job has been pretend-submitted to Zanata for translation');
  }


}


/*
Zanata translator is showing as '(unsupported)' - figure out what that means.
Ah! it says "Zanata translator (added) can not translate from English to German." - must be the specific German it is using...
*/

