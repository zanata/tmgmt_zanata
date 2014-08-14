<?php

/**
 * @file
 * Provides Zanata translation plugin UI
 */

/**
 * Zanata translation plugin UI
 */
class TMGMTZanataTranslatorUIController extends TMGMTDefaultTranslatorUIController {


  /**
   * Overrides TMGMTDefaultTranslatorUIController::pluginSettingsForm().
   *
   * These settings are accessed in the UI via:
   *  Administration -> Configuration -> Regional and language
   *  -> Translation management translators
   *  then edit under Operations
   */
  public function pluginSettingsForm($form, &$form_state, TMGMTTranslator $translator, $busy = FALSE) {
    $form['server'] = array(
      '#type' => 'textfield', // TODO check whether there is a specific type for URLs
      '#title' => t('Zanata server URL'),
      '#description' => t('Base URL for the Zanata server. This should not '
                          . 'include a / at the end, for example, '
                          . 'http://translate.zanata.org'),
      '#size' => 120,
      '#required' => TRUE,
      '#default_value' => $translator->getSetting('server'),
    );
    $form['username'] = array(
      '#type' => 'textfield',
      '#title' => t('username'),
      '#description' => t('Your username on Zanata.'),
      '#size' => 20,
      '#maxlength' => 20,
      '#default_value' => $translator->getSetting('username'),
    );
    $form['api_key'] = array(
      '#type' => 'textfield',
      '#title' => t('API key'),
      '#description' => t('Your API key on Zanata, found in the "Client" '
                          . 'section of your user settings (via the dashboard).'),
      '#size' => 32,
      '#maxlength' => 32,
      '#default_value' => $translator->getSetting('api_key'),
    );
    $form['project'] = array(
      '#type' => 'textfield',
      '#title' => t('Project ID'),
      '#description' => t('Project ID on Zanata. This should match a project '
                          . 'that exists on Zanata.'),
      '#size' => 40,
      '#maxlength' => 40,
      '#default_value' => $translator->getSetting('project'),
    );
    $form['version'] = array(
      '#type' => 'textfield',
      '#title' => t('Version ID'),
      '#description' => t('Version ID on Zanata. This should match a version '
                          . 'that exists in the given project.'),
      '#size' => 40,
      '#maxlength' => 40,
      '#default_value' => $translator->getSetting('version'),
    );

    return parent::pluginSettingsForm($form, $form_state, $translator, $busy);
  }


  /**
   * Implements TMGMTTranslatorUIControllerInterface::checkoutInfo();
   *
   * @return an associative array of UI items to display on the management page
   *         for the given translation job, under the heading "Translation
   *         Information".
   */
  public function checkoutInfo(TMGMTJob $job) {
    // TODO show last known translation statistics overall and per node

    $info['actions'] = array('#type' => 'actions');
    // TODO limit access of this button to only jobs that have been submitted to
    //      Zanata. Use '#access' key on the button, with an appropriate check
    //      on the job such as $job->isSubmitted() if that exists
    $info['actions']['poll'] = array(
      '#type' => 'submit',
      '#value' => t('Update translation info'),
      '#description' => t('Check for updated translations on the Zanata server.'),
      '#submit' => array('tmgmt_zanata_poll_translations'),
    );

    $info['actions']['resubmit'] = array(
      '#type' => 'submit',
      '#value' => t('Resubmit to translator'),
      '#description' => t('Send the source text to the Zanata server again.'),
      '#submit' => array('tmgmt_zanata_resubmit_translations'),
    );

    return $info;
  }
}
