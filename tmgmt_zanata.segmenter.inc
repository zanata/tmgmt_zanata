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

  /**
   * Create a new segmenter for general use.
   */
  public function __construct() {
  }

  /**
   * Process segmented translations.
   * returns $translated (data that is ready to be unflattened).
   */
  // FIXME this seems to know too much about the specific output format
  public function processSegmentedTranslations($job_item, $flat_targets) {

    // put targets in an associative array by id to allow lookup
    $targets = $this->associationWithKey($flat_targets, function ($target) {
      return (string) $target['resId'];
    });

    $source_data = array_filter(tmgmt_flatten_data($job_item->getData()),
                                '_tmgmt_filter_data');
    $source_segments = $this->segmentText($source_data);

    $translated = array();

    foreach ($source_data as $source_id => $source_value) {
      // first segment is always present, so count can be found there.
      $first_segment_id = $source_id . $this::SEGMENT_PREFIX . 0;
      $segment_count = $source_segments[$first_segment_id]['segment_count'];
      $index = 0;
      $translation = '';

      while ($index < $segment_count) {
        $next_segment_id = $source_id . $this::SEGMENT_PREFIX . $index;
        if (array_key_exists($next_segment_id, $targets)) {
          $next_segment = $targets[$next_segment_id];
          // FIXME do I need translated and reviewed?
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
   * Map all array values to calculated keys.
   *
   * Return a new associative array with the values from the given array mapped
   * to keys generated with the given function.
   */
  private function associationWithKey($arr, $key_function) {
    return array_reduce($arr, function ($carry, $item) use ($key_function) {
      $carry[$key_function($item)] = $item;
      return $carry;
    }, array());
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
   * Segment the text in a given value, and generate a new
   * value for each segment.
   * Returns an associative array with each segment associated
   * with its unique segment key.
   */
  private function calculateSegments($parent_key, $value) {
    return $this->calculateParagraphSegments($parent_key, $value);
    // return $this->bisectSegments($parent_key, $value);
  }


  /**
   * This is a fake segmentation algorithm, useful for testing the control
   * flow regardless of actual segmentation. It just splits strings in half.
   *
   * FIXME this does not populate required skeleton fields
   */
  // FIXME make private when calling function is moved local
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
      $key_zero = $parent_key . $this::SEGMENT_PREFIX . 0;
      $key_one = $parent_key . $this::SEGMENT_PREFIX . 1;
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

  private function calculateParagraphSegments($parent_key, $value) {
    $segments = array();

    // NOTE: I do need to keep the structure of the document intact, so must
    //       either include the structural parts within the text flows, or use
    //       a skeleton approach.
    //       Simple skeleton might be to get each source segment content +
    //       separator, replace the content, then append them. Separators
    //       with no content would just not generate anything to send to the
    //       server, and would not count towards not-translated.

    // $value will act as a template for each segment
    $value['parent_key'] = $parent_key;
    $text = $value['#text'];

    $segment_id_prefix = $parent_key . $this::SEGMENT_PREFIX;

    // Should short strings be segmented? Maybe not.
    //$text_length = strlen($text);


    // TODO I can make this better by having a separator expand to include any
    //    whitespace around it:
    //     - 2 or more newlines should act the same, so 5 newlines would be
    //       the same as 2.
    //     - sentence boundaries should exclude whitespace between the ending
    //       punctuation and the start of the next sentence, so the standard
    //       space or two spaces after a period would not be included in either
    //       sentence.
    //    I can also fix this by not just using explode (which seems like an
    //    idiot coded it - it gets 19 segments out of something that has 13
    //    lines so something is awry. Perhaps it is unicode-related. If I find
    //    break points manually and check between them for any content, I can
    //    explode apart the skeleton pieces and translatable pieces and go
    //    from there.

    // FIXME seems to add a newline at the end of joined segments where
    //       original has no newline.

    // With this sequence it is splitting off a paragraph.
    // The break occurs just before the sequence.
    $break_sequence = "\r\n\r\n";

    // FIXME should handle \r, \n and \r\n (as well as others).
    //       there is PHP_EOL, see what that does.
    //$break_sequence = "\r\n";
    $exploded_text = explode($break_sequence, $text);

    $segment_count = count($exploded_text);

    // Keep track of which index the next valid segment should use
    // (invalid segments become skeleton data).
    $next_segment_index = 0;

    // skeleton data to attach to a segment
    $skeleton_buffer = '';

    // TODO I could first iterate the exploded text to find all the invalid
    //      segments and combine any that are adjacent. I could do with a
    //      representation that starts with skeleton (possibly empty) and
    //      alternates between skeleton and segment, ending with skeleton.
    //      Could then just run through all making a segment with each
    //      skeleton + segment pair, and add final skeleton to last one.

    // iterate text, using index
    foreach ($exploded_text as $index => $segment_text) {
      $is_last_segment = $index == ($segment_count - 1);

      // TODO this will need to be more sophisticated when working with
      //      multiple different paragraph (or sentence) separators.
      $post_separator = $break_sequence;
      if ($is_last_segment) {
        $post_separator = '';
      }

      if ($this->isSegmentValid($segment_text)) {
        $new_segment = $this->createSegment($value, $next_segment_index,
                                     $skeleton_buffer, $segment_text,
                                     $post_separator);

        $segment_id = $segment_id_prefix . $next_segment_index;
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
            $last_valid_segment_index = $next_segment_index - 1;
            $last_valid_segment_id = $segment_id_prefix . $last_valid_segment_index;
            $final_segment = $segments[$last_valid_segment_id];
            $final_skeleton = $final_segment['post_skeleton'] . $skeleton_buffer;
            $final_segment['post_skeleton'] = $final_skeleton;
            $segments[$last_valid_segment_id] = $final_segment;

            $skeleton_buffer = '';
          }
          else {
            // make a segment out of the buffer (to avoid having an empty document).
            // expect $skeleton_buffer to be identical to $text here
            $new_segment = $this->createSegment($value, $next_segment_index, '',
                                         $skeleton_buffer, '');
            $segment_id = $segment_id_prefix . $next_segment_index;
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
   * Create a new segment object based on a template segment object.
   */
  private function createSegment($base, $index, $pre, $text, $post) {
    $base['pre_skeleton'] = $pre;
    $base['post_skeleton'] = $post;
    $base['#text'] = $text;
    $base['segment_index'] = $index;
    return $base;
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