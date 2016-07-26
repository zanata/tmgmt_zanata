<?php
/**
 * @file
 * Email Notification Manager
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
    public function __construct()
    {
        $title = drupal_get_title();
        $site_name = variable_get('site_name', "Drupal");
        $this->pageTitle = $title." | ".$site_name;
        $this->fromEmail = 'no-reply@zanata.org';

        global $user;
        $this->requesterName = property_exists($user, "name") ? $user->name : NULL;
        $this->requesterEmail = property_exists($user, "mail") ? $user->mail : NULL;

    }

    /**
     * Function to format notification email body
     *
     * @param $email_contents   array()
     * @return string           string
     */
    public function formatEmailBody($email_contents){

        $msgBody = "";
        if (array_key_exists("locales", $email_contents) and array_key_exists("docs", $email_contents))
        {
            $msg = "A translation job has been submitted to Zanata from '".$this->pageTitle."'. Details:";
            $msgBody = "Hello,\n\n";
            $msgBody .= $msg."\n";
            $msgBody .= "Language(s): " .join(' ', array_keys($email_contents['locales'])). "\n\n";
            foreach ($email_contents['locales'] as $locale => $editor) {
                $msgBody .= "For language: ".$locale.",\n";
                $msgBody .= "Zanata URL: ".$editor."\n\n";
            }
            $msgBody .= "\n";
            $msgBody .= "Documents...\n\n";
            foreach ($email_contents['docs'] as $doc_path => $title_url) {
                $msgBody .= "Path: ".$doc_path."\n";
                $msgBody .= "Document: ".$title_url['title']."\n";
                $msgBody .= "Drupal URL: ".$title_url['drupal']."\n\n";
            }
            if ($this->requesterName and $this->requesterEmail) {
                $msgBody .= "\n";
                $msgBody .= "Requester...\n\n";
                $msgBody .= "Name: ".$this->requesterName."\n";
                $msgBody .= "Email: ".$this->requesterEmail."\n";
            }
            $msgBody .= "\n\n";
            $msgBody .= "Zanata Team";
        }
        return $msgBody;
    }

    /**
     * Simple wrapper function for drupal_mail()
     *
     * @param string $from
     * @param $to
     * @param $subject
     * @param $message
     * @return bool
     */
    public function send_drupal_mail($from = 'default_from', $to, $subject, $message) {
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
                'X-Mailer' => 'Drupal'
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
