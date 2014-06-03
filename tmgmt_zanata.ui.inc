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
   * Implements TMGMTTranslatorUIControllerInterface::checkoutInfo();
   *
   * @return an associative array of UI items to display on the management page
   *         for the given translation job, under the heading "Translation
   *         Information".
   */
  public function checkoutInfo(TMGMTJob $job) {
    // TODO use the same URL-generation method as for the plugin controller
    // Documentation says to put a poll button here
    // So I want to do something like this to put a button, and wire it up to do the
    // appropriate thing when it is clicked.

    // Here is an ajax example that may be useful for showing updated results with or without a reload:
    // https://api.drupal.org/api/examples/ajax_example!ajax_example_graceful_degradation.inc/function/ajax_example_add_more/7

    // This mentions making a URL with a placeholder for doing an operation on something,
    // such as doing poll translations given the tjid
    // but it also seems like maybe I could just add TMGMTJob $job onto the arguments and
    // it would automatically pass it in?

    // TODO limit access of this button to only jobs that have been submitted to Zanata
    //      use '#access' key on the button, with an appropriate check on the job such
    //      as $job->isSubmitted() if that exists

    $info['tjid'] = array(
      '#type' => 'hidden',
      '#value' => $job->tjid,
    );

    $info['actions'] = array('#type' => 'actions');
    $info['actions']['poll'] = array(
      '#type' => 'submit', // type 'button' is also available, but I am not sure how to attach that to a function
      '#value' => t('Update translation info'),
      '#description' => t('Check for updated translations on the Zanata server.'),
      '#submit' => array('tmgmt_zanata_poll_translations'),
    );

    return $info;
  }
}

// function to get a translation job:
// tmgmt_job_load($tjid)


// Note that the UI controller has a checkoutInfo($job) method and recommend that a 'Poll' button be placed to sync the job state.
// http://api.worldempire.ch/api/tmgmt/plugin%21tmgmt.ui.interface.translator.inc/function/TMGMTTranslatorUIControllerInterface%3A%3AcheckoutInfo/7-1


