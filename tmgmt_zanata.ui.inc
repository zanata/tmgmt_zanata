<?php

/**
 * @file
 * Provides Zanata translation plugin UI.
 */

/**
 * Zanata translation plugin UI.
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
      // TODO check whether there is a specific type for URLs.
      '#type' => 'textfield',
      '#title' => t('Zanata Server URL'),
      '#description' => t(
        'Base URL for the Zanata server. This should not include a / at the end, for example, http://translate.zanata.org/zanata'),
      '#size' => 120,
      '#required' => TRUE,
      '#default_value' => $translator->getSetting('server'),
    );
    $form['username'] = array(
      '#type' => 'textfield',
      '#title' => t('Zanata Username'),
      '#description' => t('Your username on Zanata.'),
      '#size' => 20,
      '#maxlength' => 20,
      '#default_value' => $translator->getSetting('username'),
    );
    $form['api_key'] = array(
      '#type' => 'textfield',
      '#title' => t('Zanata API Key'),
      '#description' => t(
        'Your API key on Zanata, found in the "Client" section of your user settings (via the dashboard).'),
      '#size' => 32,
      '#maxlength' => 32,
      '#default_value' => $translator->getSetting('api_key'),
    );
    $form['project'] = array(
      '#type' => 'textfield',
      '#title' => t('Project ID'),
      '#description' => t(
        'Project ID on Zanata. This should match a project that exists on Zanata.'),
      '#size' => 40,
      '#maxlength' => 40,
      '#default_value' => $translator->getSetting('project'),
    );
    $form['version'] = array(
      '#type' => 'textfield',
      '#title' => t('Version ID'),
      '#description' => t(
        'Version ID on Zanata. This should match a version that exists in the given project.'),
      '#size' => 40,
      '#maxlength' => 40,
      '#default_value' => $translator->getSetting('version'),
    );
    $form['transcheck'] = array(
      '#type' => 'checkbox',
      '#title' => t('Translation Periodic Check'),
      '#description' => t(
        'Automatically fetch new translations every time cron runs.'),
      '#default_value' => $translator->getSetting('transcheck'),
    );
    $form['sendtrans'] = array(
      '#type' => 'checkbox',
      '#title' => t('Send translations to Zanata if they are found'),
      '#description' => t(
            'Check for translated documents when a translation job is created, and send the translations to Zanata. Translations will only be sent for strings that have not started translation in Zanata.'),
      '#default_value' => $translator->getSetting('sendtrans'),
    );
    $form['email'] = array(
      '#type' => 'fieldset',
      '#title' => t('Notification'),
      '#collapsible' => FALSE,
      '#collapsed' => FALSE,
    );
    $emailSettings = $translator->getSetting('email');
    $form['email']['notification'] = array(
      '#type' => 'radios',
      '#title' => t('Send an email when translation jobs are submitted.'),
      '#options' => array(
        0 => t('Do not send email notifications.'),
        'express' => t('Send one email when a set of translation jobs is created. The email shows a summary of all the documents and languages that were requested. Users must use express checkout form to allow all requested languages to be listed in a single email.'),
        'default' => t('Send one email per language when a set of translation jobs is created.'),
      ),
      '#default_value' => $emailSettings['notification'],
    );
    $form['email']['address'] = array(
      '#type' => 'textfield',
      '#title' => t('Email address to send to when a job is created. Leave blank to disable email notifications.'),
      '#states' => array(
        "visible" => array(
          "input[name='emailnotification']" => array("checked" => TRUE)),
      ),
      '#description' => t(
            'Notification will be posted to this email address when a job is created.'),
      '#size' => 50,
      '#maxlength' => 50,
      '#default_value' => $emailSettings['address'],
    );
    $form['segmentation'] = array(
      '#type' => 'select',
      '#title' => t('Segmentation'),
      '#description' => t(
        'Should text be split into sentences or paragraphs before it is sent for translation? Note each translation will only download when all of its segments are translated.'),
      '#required' => TRUE,
      '#options' => array(
        'NONE' => t('Do not segment.'),
        'PARAGRAPHS' => t('Segment by paragraphs.'),
        'SENTENCES' => t('Segment by sentences.'),
        'HTML' => t('HTML Content.'),
      ),
      '#default_value' => $translator->getSetting('segmentation'),
    );

    return parent::pluginSettingsForm($form, $form_state, $translator, $busy);
  }


  /**
   * Specifies interface to poll translations and resubmit source text.
   *
   * @return an associative array of UI items to display on the management page
   *         for the given translation job, under the heading "Translation
   *         Information".
   */
  public function checkoutInfo(TMGMTJob $job) {
    /* TODO show last known translation statistics overall and per node */

    $info['actions'] = array('#type' => 'actions');

    if ($job->isActive()) {
      $info['actions']['poll'] = array(
        '#type' => 'submit',
        '#value' => t('Fetch available translations'),
        '#description' => t('Check for updated translations on the Zanata server.'),
        '#submit' => array('tmgmt_zanata_poll_translations'),
      );

      $info['actions']['resubmit'] = array(
        '#type' => 'submit',
        '#value' => t('Resubmit to translator'),
        '#description' => t('Send the source text to the Zanata server again.'),
        '#submit' => array('tmgmt_zanata_resubmit_translations'),
      );
    }

    $connector = $job->getTranslatorController()->getConnector($job);
    $url = $connector->getEditorUrl();
    $link = l(t('Open in Zanata translation editor'),
              $url,
              array(
                'external' => TRUE,
                'attributes' => array('target' => '_blank')));

    $info['actions']['open_zanata'] = array(
      '#markup' => $link,
    );

    return $info;
  }
}
