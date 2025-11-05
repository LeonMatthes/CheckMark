// Clay configuration for the Nextcloud checklist app.
// Fields: CONFIG_WEB_DAV_URL, CONFIG_USER, CONFIG_APP_PASSWORD

module.exports = [
  {
    "type": "input",
    "messageKey": "CONFIG_WEB_DAV_URL",
    "label": "Nextcloud WebDAV URL",
    "description": "Full WebDAV URL to the markdown file (e.g. https://cloud.example/remote.php/dav/files/user/Path/Checklist.md)"
  },
  {
    "type": "input",
    "messageKey": "CONFIG_USER",
    "label": "Nextcloud username"
  },
  {
    "type": "input",
    "messageKey": "CONFIG_APP_PASSWORD",
    "label": "Nextcloud app password",
    "attributes": {
      "type": "password"
    }
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
