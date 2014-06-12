<?php
/**
 * @file
 * Connector to Zanata server
 */

/**
 * Handles operations relating to Zanata URLs and requests.
 */
class ZanataConnector {

  private $server_url;
  private $project_id;
  private $version_id;
  private $username;
  private $api_key;

  function __construct(TMGMTTranslator $translator) {
    $this->server_url = $translator->getSetting('server');
    $this->project_id = $translator->getSetting('project');
    $this->version_id = $translator->getSetting('version');
    $this->username = $translator->getSetting('username');
    $this->api_key = $translator->getSetting('api_key');
  }

  /**
   * @return the base REST URL for documents in the configured project/version
   */
  public function getBaseUrl() {
    return "{$this->server_url}/rest/projects/p/{$this->project_id}/iterations/i/{$this->version_id}/r";
  }

}
