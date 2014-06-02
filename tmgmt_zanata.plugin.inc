<?php

/**
 * @file
 * Provides Zanata translation plugin controller.
 */

/**
 * Zanata translation plugin controller.
 */
class TMGMTZanataTranslatorPluginController extends TMGMTDefaultTranslatorPluginController {

  protected $languageMappings = array(
    'de' => 'de',
  );

  /**
   * Implements TMGMTTranslatorPluginControllerInterface::getDefaultRemoteLanguagesMappings().
   */
  public function getDefaultRemoteLanguagesMappings() {
    return $this->languageMappings;
  }

  /**
   *Implements getSupportedRemoteLanguages().
   */
  public function getSupportedRemoteLanguages(TMGMTTranslator $translator) {
    return array_values($this->languageMappings);
  }

  public function isAvailable(TMGMTTranslator $translator) {
    // TODO check for presence of username and api key
    return TRUE;
  }



  public function getSupportedTargetLanguages(TMGMTTranslator $translator, $source_language) {
    // FIXME make this more discerning
    // default implementaion removes the none language and the source language

    // FIXME was indicating that languageMappings was undefined.
    //       maybe it was calling it from a different context so the $this was different...

    // or (trying now) I accidentally left the $ in front of languageMappings.
    return array_keys($this->languageMappings);
  }


  public function canTranslate(TMGMTTranslator $translator, TMGMTJob $job) {
    // FIXME this is too broad
    return TRUE;
  }


// MUST implement TMGMTTranslatorPluginControllerInterface::requestTranslation
// or it will refuse to activate the plugin.

  public function requestTranslation(TMGMTJob $job) {

    // TODO move these to configuration
    $server_url = 'http://localhost:8080/zanata';
    $project_id = 'drupal-test';
    $version_id = '1';
//    $doc_id = ''; // I should name the documents node/{$nid} so that they are basically the path on the drupal server

    // TODO actually send something to Zanata server

    $base_url = "{$server_url}/rest/projects/p/{$project_id}/iterations/i/{$version_id}/r";

    // add '/{document id}' and do a PUT to create or modify a document

    // JSON format is at https://zanata.ci.cloudbees.com/job/zanata-api-site/site/zanata-common-api/rest-api-docs/el_ns0_resource.html

    foreach ($job->getItems() as $job_item) {

      // FIXME get the node id or whatever, might need to ask the source module for it
      // $doc_id = 'test';
      // $doc_id = $job_item->item_type . '/' . $job_item->item_id; // was having some issues with the '/' in the path, need %2F but encode path may not be doing that
      $doc_id = $job_item->item_type . $job_item->item_id;

      $encoded_id = drupal_encode_path($doc_id);

      $url = "{$base_url}/{$encoded_id}";


      // $data = $this->prepareData($job_item);

      // $data = '{
      //   "name":"' . $doc_id . '",
      //   "contentType":"text/plain",
      //   "lang":"en-US",
      //   "extensions":[],
      //   "textFlows":' . $this->prepareData($job_item) . ',
      //   "type":"FILE",
      //   "revision":1
      // }'; // might want to change the revision to something sensible. This may make it fail the second time.

      $data = array(
        'name' => $doc_id,
        'contentType' => 'text/plain',
        'lang' => 'en-US', // TODO use source language, if Zanata can handle it
        'extensions' => array(), // TODO check if this is optional and leave it off
        'textFlows' => $this->prepareStrings($job_item),
        'type' => 'FILE', // what does this even mean?
        'revision' => 2, // TODO make sure this doesn't call failure of second upload
      );


      $options = array(
        'method' => 'PUT',
        'headers' => array(
          'X-Auth-User' => 'admin', // FIXME get from config
          'X-Auth-Token' => 'b6d7044e9ee3b2447c28fb7c50d86d98', // FIXME get from config
          'Content-Type' => 'application/json',
          'Accept: application/json',
        ),
        'data' => json_encode($data),
      );

      drupal_http_request($url, $options);

      $job_item->addMessage("added to Zanata at {$url}");
    }

    $job->submitted('Job has been pretend-submitted to Zanata for translation');
  }


  protected function prepareStrings(TMGMTJobItem $job_item) {
    $data = $job_item->getData();
    $data = array_filter(tmgmt_flatten_data($job_item->getData()), '_tmgmt_filter_data');

    $items = array();
    foreach ($data as $key => $value) {
      $items[] = array(
        'id' => $key,
        'lang' => 'en-US',
        'content' => $value['#text'],
        'contents' => NULL,
        'plural' => FALSE,
        'extensions' => array(),
        'revision' => 1
      );
    }

    return $items;    
  }


  /**
   * @return a JSON array with all the text flows
   */
  protected function prepareData(TMGMTJobItem $job_item) {
    $data = $job_item->getData();

    // $source = $data['#text']

    // FIXME just sending raw data for now to see what it looks like
    // actually in the other plugin they manually build up some XML with the key
    // and value from each item in $data.
    // return $data;
    // return "Just giving it some kind of raw string to see what happens with that";


    // This is what OHT does:
    // '_tmgmt_filter_data' is a callback to filter out untranslatable strings
    // the flatten data concats keys together
    $data = array_filter(tmgmt_flatten_data($job_item->getData()), '_tmgmt_filter_data');

    $items = array();
    foreach ($data as $key => $value) {
      $items[] = str_replace(array('@key', '@text'), array($key, $value['#text']), '{
        "id":"@key",
        "lang":"en-US",
        "content":"@text",
        "contents":null,
        "plural":false,
        "extensions":[],
        "revision":1
      }');
    }

    return '[' . implode(', ', $items) . ']';

    // $encoded_data = json_encode($data);

    // drupal_set_message('Encoded data: ' . json_encode($job_item));

    // produces this output:
    // Encoded data: {"body":{"#label":"Body","0":{"#label":"Delta #0","value":{"#label":"Body","#text":"You must rally the horde, and lead your people to their destiny.","#translate":true,"#status":0},"format":{"#label":"","#text":"filtered_html","#translate":false}}},"node_title":{"#label":"Title","#text":"For the Horde!","#status":0}}
    //
    /* formatted:
      {
        "body": {
          "#label":"Body",
          "0": {
            "#label":"Delta #0",
            "value": {
              "#label": "Body",
              "#text": "You must rally the horde, and lead your people to their destiny.",
              "#translate": true,
              "#status": 0
            },
            "format": {
              "#label":"",
              "#text":"filtered_html",
              "#translate":false
            }
          }
        },
        "node_title": {
          "#label":"Title",
          "#text":"For the Horde!",
          "#status":0
        }
      }
    */

    /* output from more complex document... doesn't seem to be segmenting it anyway.
    {
      "body": {
        "#label":"Body",
        "0": {
          "#label":"Delta #0",
          "value": {
            "#label":"Body",
            "#text":"You must rally the horde, and lead your people to their destiny.\r\n\r\nAfter you have done that, you must rally some milk, flour, and eggs, and lead your people to their pancakes.\r\n\r\nhttp:\/\/www.yeah.com\r\n",
            "#translate":true,
            "#status":0
          },
          "format": {
            "#label":"",
            "#text":"filtered_html",
            "#translate":false
          }
        }
      },
      "node_title":{"#label":"Title","#text":"For the Horde!","#status":0}}
    */

    /* whole job item:

    I could do:
      doc_id = {$item_type}/{$item_id}
      res_id = $data->key (probably from flattened data)



    {
      "plugin":"node",
      "tjid":"21",
      "tjiid":"21",
      "item_type":"node",
      "item_id":"3",
      "changed":"1401675580",
      "data":{
        "body":{
          "#label":"Body",
          "0":{
            "#label":"Delta #0",
            "value":{
              "#label":"Body",
              "#text":"You must rally the horde, and lead your people to their destiny.\r\n\r\n
                       After you have done that, you must rally some milk, flour, and eggs, and lead your people to their pancakes.\r\n\r\n
                       http:\/\/www.yeah.com\r\n",
              "#translate":true,
              "#status":0
            },
            "format":{
              "#label":"",
              "#text":"filtered_html",
              "#translate":false
            }
          }
        },
        "node_title":{
          "#label":"Title",
          "#text":"For the Horde!",
          "#status":0
        }
      },
      "count_pending":"2",
      "count_translated":"0",
      "count_accepted":"0",
      "count_reviewed":"0",
      "word_count":"39",
      "state":"1",
      "rdf_mapping":[]
    }
    */
    // return $encoded_data;
  }

    // for JSON decoding: json_decode($json)
    // and its partner: json_encode($something)





/*

This is what we show on the REST API documentation for a resource:

{
  "text-flows" : [
    { },
    { }
  ],
  "revision" : ...,
  "content-type" : "...",
  "type" : "FILE",
  "lang" : "...",
  "name" : {
  },
  "extensions" : [
    { },
    { }
  ]
}
*/


/*

This is what I get with curl:

$ curl -H "Accept: application/json" http://localhost:8080/zanata/rest/projects/p/hatter-user-guide/iterations/i/1.0/r/file-a.txt
{"name":"file-a.txt","contentType":"text/plain","lang":"en-US","extensions":[],"textFlows":[{"id":"1","lang":"en-US","content":"fileA","contents":null,"plural":false,"extensions":[],"revision":1}],"type":"FILE","revision":1}


And if I format it:

{
  "name":"file-a.txt",
  "contentType":"text/plain",
  "lang":"en-US",
  "extensions":[],
  "textFlows":[
    {
      "id":"1",
      "lang":"en-US",
      "content":"fileA",
      "contents":null,
      "plural":false,
      "extensions":[],
      "revision":1
    }
  ],
  "type":"FILE",
  "revision":1
}


Here is an old REST request I did with CURL showing how to include authentication stuff:

curl -F type=txt -F file=@thing.txt -F hash=`md5sum thing.txt | awk '{print $1}'`   -F first=true -F last=true   -H "X-Auth-User:damason" -H "X-Auth-Token:cdb5b26e3f8c3bd0ac07aea6fa58b92c"    "http://localhost:8080/zanata/rest/file/source/file-upload/FOUR-point-OH?docId=thing"

use these:

localhost.username=admin
localhost.key=b6d7044e9ee3b2447c28fb7c50d86d98


curl -i -H "X-Auth-User:admin" -H "X-Auth-Token:b6d7044e9ee3b2447c28fb7c50d86d98" -H 'Content-Type: application/json' -H 'Accept: application/json' -X PUT -d '{"name":"itworks","contentType":"text/plain","lang":"en-US","extensions":[],  "textFlows":[    {      "id":"1",      "lang":"en-US",      "content":"I am the very model of a modern major general",      "contents":null,      "plural":false,      "extensions":[],      "revision":1}  ], "type":"FILE",  "revision":1}' http://localhost:8080/zanata/rest/projects/p/hatter-user-guide/iterations/i/1.0/r/itworks

This works!

Now, to download the translations...

/projects/p/{projectSlug}/iterations/i/{iterationSlug}/r/{id}/translations/{locale}

http://localhost:8080/zanata/rest/projects/p/hatter-user-guide/iterations/i/1.0/r/itworks/translations/de


$ curl -H "Accept: application/json" http://localhost:8080/zanata/rest/projects/p/hatter-user-guide/iterations/i/1.0/r/itworks/translations/de
{"links":null,"extensions":[],"textFlowTargets":[{"resId":"1","state":"Approved","translator":{"email":"admin@example.com","name":"Administrator","links":null},"content":"Ich binst das Modell von einer modernen Generalmajor","contents":null,"sourceHash":null,"extensions":[],"description":null,"revision":3,"textFlowRevision":2}],"revision":null}

When I format the response:

{
  "links":null,
  "extensions":[],
  "textFlowTargets":[
    {
      "resId":"1",
      "state":"Approved",
      "translator": {
        "email":"admin@example.com",
        "name":"Administrator",
        "links":null
      },
      "content":"Ich binst das Modell von einer modernen Generalmajor",
      "contents":null,
      "sourceHash":null,
      "extensions":[],
      "description":null,
      "revision":3,
      "textFlowRevision":2
    }
  ],
  "revision":null
}


So that should basically be plenty to go with, as long as I can write and read JSON it should be easy to put together the URLs.



*/


}


/*
Zanata translator is showing as '(unsupported)' - figure out what that means.
Ah! it says "Zanata translator (added) can not translate from English to German." - must be the specific German it is using...
*/

