<?php
/**
 * @file
 * Program Utilities
 */

/**
 * To build TMGMT job item data in more consumable format.
 */
class TmgmtZanataJobItemDataBuilder {

    private $job_item;

    // data fields
    private $node_title;
    private $node_notes;
    private $node_head;
    private $node_body;
    private $custom_fields; // array
    private $flatten_source;

    /**
     * Create a new job item data builder.
     *
     * @param TMGMTJobItem $job_item
     */
    public function __construct(TMGMTJobItem $job_item) {

        $this->job_item = $job_item;

        foreach ($job_item->data as $node_field => $field_details) {
            $other_fields = array();
            switch ($node_field) {
                case 'node_title':
                    $this->node_title = $field_details; break;
                case 'notes':
                    $this->node_notes = $field_details; break;
                case 'head':
                    $this->node_head = $field_details; break;
                case 'body':
                    $this->node_body = $field_details; break;
                default:
                    $other_fields[$node_field] = $field_details;
                    break;
            }
            $this->custom_fields = $other_fields;
            $this->flatten_source = tmgmt_flatten_data($job_item->getData());
        }
    }

    /**
     * @return mixed
     */
    public function getJobItemData() {
        return $this->job_item->data;
    }

    /**
     * @return array
     */
    public function getJobItemStats() {
        return Array(
            'count_pending' => $this->job_item->count_pending,
            'count_translated' => $this->job_item->count_translated,
            'count_accepted' => $this->job_item->count_accepted,
            'count_reviewed' => $this->job_item->count_reviewed,
            'word_count' => $this->job_item->word_count
        );
    }

    /**
     * @return mixed Node Title
     */
    public function get_node_title() {
        return $this->node_title['#text'];
    }

    /**
     * @param $keys
     * @return array
     */
    private function value_format_mapping($keys) {
        $values = array();
        $formats = array();
        $others = array();
        foreach ($keys as $key) {
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

    public function process_node_contents() {

        $node_contents = array();
        foreach ($this->value_format_mapping(array_keys($this->flatten_source)) as $format => $value) {
            if (is_int($format)) {
                array_push($node_contents, new NodeFieldContent($value, $this->flatten_source[$value]));
            } else {
                array_push($node_contents, new NodeFieldContent(
                    $value, $this->flatten_source[$value], $this->flatten_source[$format])
                );
            }
        }
        return $node_contents;
    }
}

/**
 * Class NodeFieldContent
 */
class NodeFieldContent {

    public $key;
    public $label;
    public $data;
    public $is_html;
    public $is_title;

    /**
     * Create a new Node Field Content
     *
     * @param $source
     * @param $data
     * @param array $format
     */
    public function __construct($source, $data, $format = array('#text' => 'plain_text')) {
        $this->key = $source;
        $this->label = $data['#label'];
        $this->data = $data['#text'];
        $this->is_html = strstr($format['#text'], 'html');
        $this->is_title = $source === 'node_title';
    }

}
