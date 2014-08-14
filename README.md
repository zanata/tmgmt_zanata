# TMGMT Zanata

TMGMT Zanata is a plugin for Drupal's Translation Management Module
[TMGMT](https://drupal.org/project/tmgmt). The plugin can send content to a
configured Zanata project for translation, and can download translations as
they are ready. Zanata is a web-based system for translators, content creators
and developers to manage localisation projects
(see [zanata.org](http://zanata.org/)).

## Requirements

This module requires TMGMT module to be installed.


## Installation

Place the module directory in your usual Drupal modules directory.

Activate the module through the Drupal administration interface, or whichever
way you prefer to activate modules.


## Setup

After installing and activating the module, the plugin needs some details about
your Zanata user and the project on Zanata. You can set these on the translator
congiguration page.

You will need a free Zanata account, and a project with at least one version.

 - To get a Zanata account, see [Signing Up](http://zanata.org/help/accounts/sign-up/).
 - For instructions on finding your API key, see "User Configuration" on the
   [Configure the Client](http://zanata.org/help/cli/cli-configuration/) help page.
   - User settings are accessed through the Dashboard.
   - Note that you do not need to create any config files for this plugin.
 - For help creating a project, see [Project Creation](http://zanata.org/help/projects/create-project/).
 - For help creating a version, see [Version Creation](http://zanata.org/help/projects/create-version/).

To open the translator configuration page, navigate to the list of translators
via `Administration -> Configuration -> Regional and language -> Translation management translators`,
find the Zanata translator in the table, and click `edit`.


## Usage

Once the plugin has been configured, content can be sent to Zanata for translation.
Note that content must have an English locale to be a source language for Zanata -
make sure it is not "Language Neutral" or it will not be available for translation.

TMGMT uses translation jobs to request and keep track of translations.

To create a translation job:

 1. Select a source by either:

   - Through the admin menu, `Translation -> Sources` and select one or more sources.
   - For any node, select the `translate` tab and check one or more languages to translate to.

 2. Click `Request Translation`.
 3. Make sure "Zanata Translator" is the selected Translator (it can be moved to the top of the translator list in configuration).
 4. Press `Submit to translator`.

The progress of the translation job can be checked on the management page.

 1. Through the admin menu, open `Translation -> Jobs`.
 2. Next to your translation job, click `manage`.
 3. To fetch available translations on the Zanata server, click `Update translation info`.
