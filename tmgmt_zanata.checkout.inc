<?php

/**
 * @file
 * TMGMT Zanata Express Checkout
 */

/**
 * Express checkout form. Used to specify job name and translator plugin on a single
 * form for a set of languages, instead of one form per language.
 */
function tmgmt_zanata_express_checkout_form($form, &$form_state) {

  $job_ids = array();

  if (isset($_SESSION['tmgmt_express_checkout'])) {
    $cache = $_SESSION['tmgmt_express_checkout'];
    if (isset($cache['job_ids'])) {
      $job_ids = $cache['job_ids'];
    }
  }

  $form['job_ids'] = array(
        // value type is not sent to client, but is available in submit function
    '#type' => 'value',
    '#value' => $job_ids,
  );

  $form['description'] = array(
    '#markup' => t('Express checkout can configure and submit all new translation jobs at once (%job_count jobs).',
            array(
              '%job_count' => count($job_ids),
            )),
  );

  // Use the first job to generate a default job label.
  if (count($job_ids) >= 1) {
    $first_job_id = reset($job_ids);
    $first_job = tmgmt_job_load($first_job_id);
    $default_job_name = $first_job->defaultLabel();
    // TODO consider whether to use ->label() instead
  }
  else {
    $default_job_name = 'Translation job';
  }

  $form['job_name_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Job name'),
    '#description' => t('The default name to use for the created translation jobs.'),
    '#default_value' => $default_job_name,
    '#required' => TRUE,
  );

  $translators = tmgmt_translator_labels();
  // reset returns first element, or FALSE
  $default_translator = reset($translators);

  $form['translator_wrapper'] = array(
    '#type' => 'fieldset',
    '#title' => t('Translator selection'),
  );

  $form['translator_wrapper']['translator'] = array(
    '#type' => 'select',
    '#title' => t('Translator'),
    '#description' => t('The configured translator plugin that will process of the translation.'),
    '#options' => $translators,
    '#default_value' => $default_translator,
    '#required' => TRUE,
  );

  $form['translator_wrapper']['settings'] = array(
    '#markup' => t('Express checkout does not allow custom translator settings.'),
  );

  // TODO only allow submit if 1 or more languages are selected
  $form['actions'] = array(
    '#type' => 'fieldset',
  );

  $form['actions']['request_translation_express'] = array(
    '#type' => 'submit',
    '#value' => t('Request translation'),
        // '#submit' => array('tmgmt_ui_cart_request_translation_form_submit'),
    '#submit' => array('tmgmt_zanata_express_checkout_form_submit'),
    '#validate' => array('tmgmt_zanata_express_checkout_form_validate'),
  );

  $form['actions']['cancel'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel (resume normal checkout)'),
        // '#submit' => array('tmgmt_ui_cart_request_translation_form_submit'),
    '#submit' => array('tmgmt_zanata_express_checkout_form_cancel'),
        // '#validate' => array('tmgmt_zanata_express_checkout_form_validate'),
  );

  return $form;
}

function tmgmt_zanata_express_checkout_form_validate($form, &$form_state) {
}

/**
 * Submit handler for express checkout form.
 */
function tmgmt_zanata_express_checkout_form_submit($form, &$form_state) {
  // TODO check if 'values' is right, there may be a different field for the
  //      submitted values when the form is complete.
  $default_label = $form_state['values']['job_name_prefix'];
  $translator = $form_state['values']['translator'];
  $job_ids = $form_state['values']['job_ids'];

  $jobs = array();
  $contents = array();
  foreach ($job_ids as $job_id) {
    $job = tmgmt_job_load($job_id);
    $node = array();

    // TODO append language name to label ($job->target_language)
    // TODO show example language name where the label is entered
    $job->label = $default_label;
    $job->translator = $translator;

    $job->requestTranslation();

    $email_setting = $job->getTranslator()->getSetting('email');
    $express_setting = $email_setting['notification'] === 'express';
    $email_address = $email_setting['address'];

    if ($express_setting and $email_address) {
      $node['locale'] = $job->target_language;
      $node['editor'] = $job->getTranslatorController()->getConnector($job)->getEditorUrl();
      $items = array();
      foreach ($job->getItems() as $job_item) {
        $items[$job_item->item_type.'/'.$job_item->item_id] =
                    $job_item->data['node_title']['#text'];
      }
      $node['docs'] = $items;
      array_push($contents, $node);
      array_push($jobs, $job);
    }

    $job->save();

  }

  if (!empty($contents) and !empty($jobs)) {
    // contents will have nodes only if notification settings are set! lets send email then..
    $jobs[0]->getTranslatorController()->getConnector($job)->express_checkout_notification($contents);
  }

  drupal_set_message(t('Checked out %job_count translation jobs.', array(
        '%job_count' => count($job_ids),
    )));

  // clear the cached job_ids now that they have been processed.
  if (!isset($_SESSION['tmgmt_express_checkout'])) {
    $_SESSION['tmgmt_express_checkout'] = array();
  }
  unset($_SESSION['tmgmt_express_checkout']['job_ids']);

  // clear the tmgmt redirect queue since all jobs in the queue are processed.
  tmgmt_ui_redirect_queue_set(array());

  $form_state['redirect'] = 'admin/tmgmt';
}

/**
 * Cancel handler for express checkout form (resume normal checkout instead).
 */
function tmgmt_zanata_express_checkout_form_cancel($form, &$form_state) {
  // clear the cached job_ids now that they will not be used.
  if (!isset($_SESSION['tmgmt_express_checkout'])) {
    $_SESSION['tmgmt_express_checkout'] = array();
  }
  unset($_SESSION['tmgmt_express_checkout']['job_ids']);

  // redirect to default tmgmt job checkout queue
  $form_state['redirect'] = tmgmt_ui_redirect_queue_dequeue();
}
