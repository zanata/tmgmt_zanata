<?php
/**
 * @file
 * Program Utilities.
 */

/**
 * To build TMGMT job item data in more consumable format.
 */
class TmgmtZanataJobItemDataBuilder {
  private $node_contents;

  /**
   * Create a new job item data builder.
   *
   * @param TMGMTJobItem $job_item
   *   A single document in this job.
   */
  public function __construct(TMGMTJobItem $job_item) {
      $flattened_source = tmgmt_flatten_data($job_item->getData());
      $this->node_contents = $this->processNodeContents($flattened_source);
  }

  public function getNodeContents() {
      return $this->node_contents;
  }

  /**
   * Map translatable content keys to their formats.
   *
   * Creates mapping between translatable content keys
   * in the order, formats to their values
   * example: body][0][format => body][0][value.
   *
   * @param array $src_msg_keys
   *   Keys from tmgmt_flatten_data.
   *
   * @return array
   *   format to value mappings
   */
  private function mapFormatsToValues(array $src_msg_keys) {
    $values = array();
    $formats = array();
    $others = array();
    foreach ($src_msg_keys as $key) {
      if (strstr($key, 'value')) {
        array_push($values, $key);
      }
      elseif (strstr($key, 'format')) {
        array_push($formats, $key);
      }
      else {
        array_push($others, $key);
      }
    }

    $result = array();
    foreach ($values as $value) {
      foreach ($formats as $format) {
        if (strstr($format, rtrim($value, 'value'))) {
          $result[$format] = $value;
        }
      }
    }
    return array_merge($result, $others);
  }

  /**
   * Process TMGMT flatten data and make it more descriptive.
   *
   * @param array $flattened_source
   *   Job item data flattened with tmgmt's helper method.
   *
   * @return array
   *   TmgmtZanataNodeFieldContent elements
   */
  private function processNodeContents(array $flattened_source) {
    $node_contents = array();
    foreach ($this->mapFormatsToValues(array_keys($flattened_source)) as $format => $value) {
      if (is_int($format)) {
        array_push($node_contents, new TmgmtZanataNodeFieldContent($value, $flattened_source[$value]));
      }
      else {
        array_push($node_contents, new TmgmtZanataNodeFieldContent(
                    $value, $flattened_source[$value], $flattened_source[$format])
                );
      }
    }
    return $node_contents;
  }

}

/**
 * Class TmgmtZanataNodeFieldContent.
 */
class TmgmtZanataNodeFieldContent {

  public $key;
  public $label;
  public $data;
  public $isHtml;
  public $isTitle;

  /**
   * Create a new Node Field Content.
   *
   * @param string $source
   *   The key for the field.
   * @param array $data
   *   Data containing label and text.
   * @param array $format
   *   Format that describes whether the field has html content.
   */
  public function __construct($source, array $data, $format = array('#text' => 'plain_text')) {
    $this->key = $source;
    $this->label = $data['#label'];
    $this->data = $data['#text'];
    $this->isHtml = strstr($format['#text'], 'html');
    $this->isTitle = $source === 'node_title';
  }

}
