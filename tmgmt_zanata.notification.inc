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
        $this->requesterName = $user->name;
        $this->requesterEmail = $user->mail;

    }

    /**
     * Function to format notification email body
     */
    public function formatEmailBody($email_contents, $locale, $type='PLAIN'){
        $msg = "A translation job has been submitted to Zanata from '".$this->pageTitle."'. Details:";
        if ($type === 'HTML') {
            $msgBody = "<html><body>";
            $msgBody .= "<h3>Hello,</h3>";
            $msgBody .= "<br/><br/>";
            $msgBody .= $msg;
            $msgBody .= "<br/>";
            $msgBody .= "Locale: ".$locale;
            $msgBody .= "<br/>";
            $msgBody .= "Documents...";
            $msgBody .= "<br/>";
            $msgBody .= "<table rules='all' style='border-color: #666;' cellpadding='10'>";
            $msgBody .= "<tr align='left' style='background: #eee;'><td><strong>Title</strong></td>";
            $msgBody .= "<td><strong>Name in Zanata</strong></td>";
            $msgBody .= "<td><strong>Zanata URL</strong></td><td><strong>Drupal URL</strong></td></tr>";

            foreach ($email_contents as $content) {
                $msgBody .= "<tr>";
                $msgBody .= "<td>" . $content['docTitle'] . "</td>";
                $msgBody .= "<td>" . $content['docName'] . "</td>";
                $msgBody .= "<td><a href=\"" . $content['zanataURL'] . "\">translate in Zanata</a></td>";
                $msgBody .= "<td><a href=\"" . $content['drupalURL'] . "\">view source document</a></td>";
                $msgBody .= "</tr>";
            }

            $msgBody .= "</table>";
            $msgBody .= "<br/>";
            $msgBody .= "Requester..."."<br/>";
            $msgBody .= "Name: ".$this->requesterName."<br/>";
            $msgBody .= "Email: ".$this->requesterEmail."<br/>";
            $msgBody .= "<br/><br/>";
            $msgBody .= "Zanata Team";
            $msgBody .= "</body></html>";
        }
        else {
            $msgBody = "Hello,\n\n";
            $msgBody .= $msg."\n";
            $msgBody .= "Locale: ".$locale."\n\n";
            $msgBody .= "Documents...\n\n";
            foreach ($email_contents as $content) {
                $msgBody .= "Doc Title: ".$content['docTitle']."\n";
                $msgBody .= "Name in Zanata: ".$content['docName']."\n";
                $msgBody .= "Zanata URL: ".$content['zanataURL']."\n";
                $msgBody .= "Drupal URL: ".$content['drupalURL']."\n\n";
            }
            $msgBody .= "\n";
            $msgBody .= "Requester...\n\n";
            $msgBody .= "Name: ".$this->requesterName."\n";
            $msgBody .= "Email: ".$this->requesterEmail."\n";
            $msgBody .= "\n\n";
            $msgBody .= "Zanata Team";
        }
        return $msgBody;
    }

    /**
     * Simple wrapper function for drupal_mail()
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
?>