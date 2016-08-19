<?php
/**
 * @file
 * Program Utilities.
 */

/**
 * To build TMGMT job item data in more consumable format.
 */
class TmgmtZanataJobItemDataBuilder {

  private $jobItem;

  // Data fields.
  private $nodeTitle;
  private $nodeNotes;
  private $nodeHead;
  private $nodeBody;
  private $customFields;
  // Array.
  private $flattenSource;

  /**
   * Create a new job item data builder.
   *
   * @param TMGMTJobItem $job_item
   *   A single document in this job.
   */
  public function __construct(TMGMTJobItem $job_item) {

    $this->jobItem = $job_item;

    foreach ($job_item->data as $node_field => $field_details) {
      $other_fields = array();
      switch ($node_field) {
        case 'node_title':
                    $this->nodeTitle = $field_details;
          break;

        case 'notes':
                    $this->nodeNotes = $field_details;
          break;

        case 'head':
                    $this->nodeHead = $field_details;
          break;

        case 'body':
                    $this->nodeBody = $field_details;
          break;

        default:
                    $other_fields[$node_field] = $field_details;
          break;
      }
      $this->customFields = $other_fields;
      $this->flattenSource = tmgmt_flatten_data($job_item->getData());
    }
  }

  /**
   * Get data from job item.
   *
   * @return mixed
   *   TMGMT Job Item Data
   */
  public function getJobItemData() {
    return $this->job_item->data;
  }

  /**
   * Get counts of different translations states for the current job item.
   *
   * @return array
   *   Job item translation stats
   */
  public function getJobItemStats() {
    return array(
      'count_pending' => $this->job_item->count_pending,
      'count_translated' => $this->job_item->count_translated,
      'count_accepted' => $this->job_item->count_accepted,
      'count_reviewed' => $this->job_item->count_reviewed,
      'word_count' => $this->job_item->word_count,
    );
  }

  /**
   * Get the node title text.
   *
   * @return mixed
   *   Node Title
   */
  public function getNodeTitle() {
    return $this->nodeTitle['#text'];
  }

  /**
   * Map translatable content keys to their formats.
   *
   * Creates mapping between translatable content keys
   * in the order, formats to their values
   * example: body][0][format => body][0][value
   *
   * @param array $src_msg_keys
   *   Keys from tmgmt_flatten_data.
   *
   * @return array
   *   format to value mappings
   */
  private function formatValueMapping(array $src_msg_keys) {
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
   * @return array
   *   TmgmtZanataNodeFieldContent elements
   */
  public function processNodeContents() {

    $node_contents = array();
    foreach ($this->formatValueMapping(array_keys($this->flattenSource)) as $format => $value) {
      if (is_int($format)) {
        array_push($node_contents, new TmgmtZanataNodeFieldContent($value, $this->flattenSource[$value]));
      }
      else {
        array_push($node_contents, new TmgmtZanataNodeFieldContent(
                    $value, $this->flattenSource[$value], $this->flattenSource[$format])
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
