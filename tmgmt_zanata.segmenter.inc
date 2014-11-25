<?php
/**
 * @file
 * Segmenter to split up text before translation.
 */

/**
 * Handles splitting content into segments.
 */
class TmgmtZanataSegmenter {
  const SEGMENT_PREFIX = '-segment-';

  private $segmentRegex;

  /**
   * Create a new segmenter for general use.
   */
  public function __construct($segmentationType) {
    switch ($segmentationType) {
      case 'PARAGRAPHS':
        // TODO look at PHP_EOL, it may be relevant.
        $this->segmentRegex = "@(\r\n\r\n|\n\n|(?<=</p>)|<br>|<br/>|<br />)@";
        break;
      case 'SENTENCES':
        $this->segmentRegex =
          // non-ellipsis period before whitespace
          '/((?<!\.\.)(?<=[.])\s+'
          // punctuation at end of sentence before whitespace
          . '|(?<=[!?؟])\s+'
          // ideographic period regardless of whitespace
          . '|(?<=。)\s*)/';
        break;
      case 'NONE':
        // Regex purposely matches nothing so there will be a single segment.
        $this->segmentRegex = '/$^/';
        break;
      default:
        drupal_set_message(
          t('Segmentation type setting "@segtype" is not valid.',
            array ('@segtype' => $segmentationType)),
         'error');
        // TODO fail with error, since segmenter cannot behave sensibly.
    }
  }

  /**
   * Join segmented translations to full translations. Translations with missing
   * segments are not included in returned data.
   *
   * returns $translated (data that is ready to be unflattened).
   */
  public function joinTranslatedSegments($source_data, $translated_segments) {
    $source_segments = $this->segmentText($source_data);

    $translated = array();

    foreach ($source_data as $source_id => $source_value) {
      // first source segment is always present, so count can be found there.
      $first_segment_id = $this->segmentId($source_id, 0);
      $segment_count = $source_segments[$first_segment_id]['segment_count'];
      $index = 0;
      $translation = '';

      while ($index < $segment_count) {
        $next_segment_id = $this->segmentId($source_id, $index);
        if (array_key_exists($next_segment_id, $translated_segments)) {
          $next_segment = $translated_segments[$next_segment_id];
          // TODO check whether I need translated and reviewed?
          if ($next_segment['state'] == 'Approved') {
            $next_source_segment = $source_segments[$next_segment_id];
            $translation .= (string) $next_source_segment['pre_skeleton'];
            $translation .= (string) $next_segment['content'];
            $translation .= (string) $next_source_segment['post_skeleton'];
          }
          else {
            continue 2;
          }
        }
        else {
          continue 2;
        }
        $index++;
      }
      $translated[$source_id]['#text'] = $translation;
    }

    return $translated;
  }

  /**
   * Generate a unique identifier for a segment.
   */
  private function segmentId($parent_key, $index) {
    return $parent_key . $this::SEGMENT_PREFIX . $index;
  }

  // Reference, splits text at a delimiter:
  //   https://api.drupal.org/api/drupal/modules!field!modules!text!text.module/function/text_summary/7
  public function segmentText($data) {
    $segmented = array();
    foreach ($data as $key => $value) {
      $new_segments = $this->calculateSegments($key, $value);
      $segmented = array_merge($segmented, $new_segments);
    }
    return $segmented;
  }

  /**
   * This is a fake segmentation algorithm, useful for testing the control
   * flow regardless of actual segmentation. It just splits strings in half.
   */
  private function bisectSegments($parent_key, $value) {
    $segments = array();

    // $value will act as a template for each segment
    $value['parent_key'] = $parent_key;
    $value['pre_skeleton'] = '';
    $value['post_skeleton'] = '';

    $text = $value['#text'];
    $text_length = strlen($text);
    if ($text_length > 1) {
      $value['segment_count'] = 2;

      $split_point = $text_length / 2;
      $key_zero = $this->segmentId($parent_key, 0);
      $key_one = $this->segmentId($parent_key, 1);
      $text_zero = substr($text, 0, $split_point);
      $text_one = substr($text, $split_point);

      $value['segment_index'] = 0;
      $value['#text'] = $text_zero;
      $segments[$key_zero] = $value;

      $value['segment_index'] = 1;
      $value['#text'] = $text_one;
      $segments[$key_one] = $value;
    }
    else {
      $value['segment_index'] = 0;
      $value['segment_count'] = 1;
      $segments[$key] = $value;
    }
    return $segments;
  }

  /**
   * Segment the text in a given value, and generate a new
   * value for each segment.
   * Returns an associative array with each segment associated
   * with its unique segment key.
   */
  private function calculateSegments($parent_key, $value) {
    $segments = array();

    // $value will act as a template for each segment
    $value['parent_key'] = $parent_key;
    $text = $value['#text'];

    list($segment_texts, $separators) = $this->toSegmentsAndSeparators($text);
    $segment_count = count($segment_texts);

    // Keep track of which index the next valid segment should use
    // (invalid segments become skeleton data).
    $next_segment_index = 0;

    // skeleton data to attach to a segment
    $skeleton_buffer = '';

    foreach ($segment_texts as $index => $segment_text) {
      $is_last_segment = $index == ($segment_count - 1);
      $post_separator = $separators[$index];

      if ($this->isSegmentValid($segment_text)) {
        // move whitespace from string to skeleton
        // this could be done before iterating the segments
        list($pre, $cleaned, $post) = $this->partitionWhitespace($segment_text);

        $new_segment = $this->createSegment($value,
                                            $next_segment_index,
                                            $skeleton_buffer . $pre,
                                            $cleaned,
                                            $post . $post_separator);
        $segment_id = $this->segmentId($parent_key, $next_segment_index);
        $segments[$segment_id] = $new_segment;
        $next_segment_index++;
        $skeleton_buffer = '';
      }
      else {
        $skeleton_buffer .= $segment_text;
        $skeleton_buffer .= $post_separator;

        if ($is_last_segment) {
          $have_valid_segments = $next_segment_index != 0;
          if ($have_valid_segments) {
            // just add the buffer to the valid segment
            $last_segment_index = $next_segment_index - 1;
            $last_segment_id = $this->segmentId($parent_key,
                                                $last_segment_index);
            $last_segment = $segments[$last_segment_id];
            $final_skeleton = $last_segment['post_skeleton'] . $skeleton_buffer;
            $last_segment['post_skeleton'] = $final_skeleton;
            $segments[$last_segment_id] = $last_segment;
            $skeleton_buffer = '';
          }
          else {
            // make a segment out of the buffer (to avoid an empty document).
            // expect $skeleton_buffer to be identical to $text here
            // This is done in case the empty document is due to a problem with
            // the segmentation, so that there is still a way to translate.
            $new_segment = $this->createSegment($value, $next_segment_index, '',
                                         $skeleton_buffer, '');
            $segment_id = $this->segmentId($parent_key, $next_segment_index);
            $segments[$segment_id] = $new_segment;

            // used below for the valid segment count, will be 1 here.
            $next_segment_index++;
            $skeleton_buffer = '';
          } 
        }
      }
    }
    // set the segment count, now that it is known
    $valid_segment_count = $next_segment_index;
    // Note: $segment is not unset after this, so it will reference the last
    //       segment after this loop. Safest not to use it after this.
    foreach ($segments as $index => &$segment) {
      $segment['segment_count'] = $valid_segment_count;
    }

    return $segments;
  }


  /**
   * Split text into segments and separators.
   *
   * Returns an array with an array of segments at index 0, and an array of
   * separators at index 1. Both arrays are the same length.
   */
  private function toSegmentsAndSeparators($text) {
    $flags = PREG_SPLIT_DELIM_CAPTURE;
    $exploded = preg_split($this->segmentRegex, $text, NULL, $flags);

    $segments = array();
    $separators = array();

    foreach ($exploded as $index => $text) {
      if (($index % 2) === 0) {
        $segments[] = $text;
      }
      else {
        $separators[] = $text;
      }
    }

    // Ensure $separators is the same length as $segments.
    $separators[] = '';

    return array($segments, $separators);
  }

  /**
   * Separate leading and trailing whitespace from the content of the text.
   *
   * Returns an array of length 3 in the form:
   *     (<leading whitespace>, <content>, <trailing whitespace>)
   */
  private function partitionWhitespace($text) {
    $content = trim($text);
    $content_with_post = ltrim($text);
    $pre_length = strlen($text) - strlen($content_with_post);
    $content_pos = $pre_length;
    $post_pos = $content_pos + strlen($content);

    $pre = substr($text, 0, $pre_length);
    $post = substr($text, $post_pos);
    return array($pre, $content, $post);
  }

  /**
   * Create a new segment object based on a template segment object.
   */
  private function createSegment($template, $index, $pre, $text, $post) {
    $template['pre_skeleton'] = $pre;
    $template['post_skeleton'] = $post;
    $template['#text'] = $text;
    $template['segment_index'] = $index;
    return $template;
  }

  /**
   * Check whether a segment is valid for translation.
   *
   * Segments that are entirely whitespace or punctuation are considered
   * invalid for translation. Zero-length segments are also invalid.
   */
  private function isSegmentValid($segment_text) {
    $empty = strlen($segment_text) == 0;
    $all_whitespace = ctype_space($segment_text);

    return !$empty && !$all_whitespace;
  }

}