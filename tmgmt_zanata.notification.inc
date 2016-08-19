<?php
/**
 * @file
 * Email Notification Manager.
 */

/**
 * Handles operations related to email notifications.
 */
class TMGMTZanataTranslatorNotification {

  private $pageTitle;
  private $fromEmail;
  private $requesterName;
  private $requesterEmail;

  /**
   * Create a new email for the given notification.
   *
   * A email should be sent for TMGMT jobs.
   */
  public function __construct() {
    $title = drupal_get_title();
    $site_name = variable_get('site_name', "Drupal");
    $this->pageTitle = $title . " | " . $site_name;
    $this->fromEmail = 'no-reply@zanata.org';

    global $user;
    $this->requesterName = property_exists($user, "name") ? $user->name : NULL;
    $this->requesterEmail = property_exists($user, "mail") ? $user->mail : NULL;
  }

  /**
   * Function to format notification email body.
   *
   * @param array $email_contents
   *   Details to include in the email.
   *
   * @return string
   *   The message body for the email.
   */
  public function formatEmailBody(array $email_contents) {

    $msg_body = "";
    if (array_key_exists("locales", $email_contents) and array_key_exists("docs", $email_contents)) {
      $msg = "A translation job has been submitted to Zanata from '" . $this->pageTitle . "'. Details:";
      $msg_body = "Hello,\n\n";
      $msg_body .= $msg . "\n";
      $msg_body .= "Language(s): " . implode(' ', array_keys($email_contents['locales'])) . "\n\n";
      foreach ($email_contents['locales'] as $locale => $editor) {
        $msg_body .= "For language: " . $locale . ",\n";
        $msg_body .= "Zanata URL: " . $editor . "\n\n";
      }
      $msg_body .= "\n";
      $msg_body .= "Documents...\n\n";
      foreach ($email_contents['docs'] as $doc_path => $title_url) {
        $msg_body .= "Path: " . $doc_path . "\n";
        $msg_body .= "Document: " . $title_url['title'] . "\n";
        $msg_body .= "Drupal URL: " . $title_url['drupal'] . "\n\n";
      }
      if ($this->requesterName and $this->requesterEmail) {
        $msg_body .= "\n";
        $msg_body .= "Requester...\n\n";
        $msg_body .= "Name: " . $this->requesterName . "\n";
        $msg_body .= "Email: " . $this->requesterEmail . "\n";
      }
      $msg_body .= "\n\n";
      $msg_body .= "Zanata Team";
    }
    return $msg_body;
  }

  /**
   * Simple wrapper function for drupal_mail().
   *
   * @param string $to
   *   Email to address.
   * @param string $subject
   *   Email subject.
   * @param string $message
   *   Email message body.
   * @param string $from
   *   Email from address.
   *
   * @return bool
   *   TRUE if the mail sent successfully, otherwise FALSE.
   */
  public function sendDrupalMail($to, $subject, $message, $from = 'default_from') {
    $module = 'tmgmt_zanata';
    $mail_token = microtime();
    if ($from == 'default_from') {
      $from = variable_get('system_mail', $this->fromEmail);
    }
    $message = array(
      'id' => $module . '_' . $mail_token,
      'to' => $to,
      'subject' => $subject,
      'body' => array($message),
      'headers' => array(
        'CC' => $this->requesterEmail,
        'From' => $from,
        'Sender' => $from,
        'Return-Path' => $from,
        'MIME-Version' => '1.0',
        'Content-Type' => "text/plain; charset=UTF-8",
        'Content-Transfer-Encoding' => '8Bit',
        'X-Mailer' => 'Drupal',
      ),
    );
    $system = drupal_mail_system($module, $mail_token);
    $message = $system->format($message);
    if ($system->mail($message)) {
      return TRUE;
    }
    else {
      return FALSE;
    }
  }
}
