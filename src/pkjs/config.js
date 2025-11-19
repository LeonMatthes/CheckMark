// Clay configuration for the Nextcloud checklist app.
// Fields: CONFIG_WEB_DAV_URL, CONFIG_USER, CONFIG_APP_PASSWORD

module.exports = [
  {
    "type": "section",
    "items": [
      {
        "type": "input",
        "messageKey": "CONFIG_WEB_DAV_URL",
        "label": "WebDAV File URL",
        "description": "Full WebDAV URL to the Markdown file (e.g. https://cloud.example/remote.php/dav/files/user/Path/Checklist.md)"
      },
      {
        "type": "input",
        "messageKey": "CONFIG_USER",
        "label": "Username"
      },
      {
        "type": "input",
        "messageKey": "CONFIG_APP_PASSWORD",
        "label": "Password",
        "attributes": {
          "type": "password"
        }
      },
      {
        "type": "submit",
        "defaultValue": "Save Settings"
      }
    ]
  }
];
