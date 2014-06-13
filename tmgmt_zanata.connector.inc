<?php
/**
 * @file
 * Connector to Zanata server
 */

/**
 * Handles operations relating to Zanata URLs and requests for a translation job.
 */
class ZanataConnector {

  private $server_url;
  private $project_id;
  private $version_id;
  private $username;
  private $api_key;

  private $job;
  private $translator;
  private $controller;

  /**
   * Create a new connector for the given job. A separate connector should be
   * used for each job.
   */
  function __construct(TMGMTJob $job) {
    $translator = $job->getTranslator();

    $this->server_url = $translator->getSetting('server');
    $this->project_id = $translator->getSetting('project');
    $this->version_id = $translator->getSetting('version');
    $this->username = $translator->getSetting('username');
    $this->api_key = $translator->getSetting('api_key');

    $this->job = $job;
    $this->translator = $translator;
    $this->controller = $translator->getController();
  }

  /**
   * @return the base REST URL for documents in the configured project/version
   */
  public function getBaseUrl() {
    return "{$this->server_url}/rest/projects/p/{$this->project_id}/iterations/i/{$this->version_id}/r";
  }

  /**
   * @return the unencoded document id on Zanata for the given job item
   */
  function getDocId($job_item) {
    return "{$job_item->item_type}/{$job_item->item_id}";
  }

  /**
   * @return the REST URL for the given job item
   */
  function getItemUrl(TMGMTJobItem $job_item) {
    $encoded_id = strtr($this->getDocId($job_item), '/', ',');
    return "{$this->getBaseUrl()}/{$encoded_id}";
  }

  /**
   * @return the REST URL for the translations of the given job item for the given locale
   */
  private function getItemTranslationUrl(TMGMTJobItem $job_item) {
    return $this->getItemUrl($job_item) . '/translations/' . $this->getLocale();
  }

  /**
   * Check for and download available translations from Zanata server.
   */
  public function pollTranslations() {
    $job = $this->job;
    $base_options = $this->controller->getBaseOptions($this->translator);

    // FIXME skip any item that is completed (to avoid excess network traffic)

    foreach ($job->getItems() as $job_item) {
      $url = $this->getItemTranslationUrl($job_item);

      $options = $base_options;

      $response = drupal_http_request($url, $options);

      // TODO replace with switch
      if ($response->code != 200) {
        if ($response->code == 404) {
          // this may not really be worth logging, is could just be considered noise
          $job_item->addMessage(t('No translations are available yet.'));
        }
        else {
          drupal_set_message(t('There was a problem fetching translations from the service. See messages below.'), 'warning', FALSE);
          $job_item->addMessage(t('Tried to update translations but got response code @code when fetching URL @url.'),
                                array(
                                  '@code' => $response->code,
                                  '@url' => $url));
        }
      }
      else {
        $data = drupal_json_decode($response->data);
        $targets = $data['textFlowTargets'];

        $translated = array();
        foreach ($targets as $target) {
          // TODO map Zanata states sensibly to TMGMT states
          if ($target['state'] == 'Approved') {
            $key = (string) $target['resId'];
            $translated[$key]['#text'] = (string) $target['content'];
          }
        }

        $expanded_translated = tmgmt_unflatten_data($translated);
        $job_item->addTranslatedData($expanded_translated);
      }
    }
  }

  /**
   * Get the remote locale for the translation job.
   */
  private function getLocale() {
    return $this->controller->mapToRemoteLanguage($this->translator, $this->job->target_language);
  }

}
