var keys = require('message_keys'); // generated at build time from package.json

// Persist the most recently loaded document here so it's accessible
// throughout this module (file). It's set after a successful GET.
let documentLines = [];
let checklistItems = [];
let listTitle = 'Checklist';

function resetState() {
  documentLines = [];
  checklistItems = [];
  listTitle = 'Checklist';
}

// Default Nextcloud details (will be overridden by user configuration if available)
let webdavUrl = ''; // default URL
let username = ''; // default username
let appPassword = ''; // default app password (will be overridden by config)

let Clay = require('pebble-clay');
const clayConfig = require('./config');
let clay = new Clay(clayConfig, null, { autoHandleEvents: false });

function loadSettings() {
  webdavUrl = localStorage.getItem("CONFIG_WEB_DAV_URL");
  username = localStorage.getItem("CONFIG_USER");
  appPassword = localStorage.getItem("CONFIG_APP_PASSWORD");

  console.log(`Loaded setting: ${username}@${webdavUrl}`);
}

Pebble.addEventListener('showConfiguration', function (e) {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (e && !e.response) {
    return;
  }

  console.log(`RESPONSE: ${e.response}`);

  // Get the keys and values from each config item
  var settings = clay.getSettings(e.response, false);
  console.log(`DICT: ${JSON.stringify(settings)}`);

  let storeSetting = (key) => {
    localStorage.setItem(key, settings[key].value);
  };
  storeSetting("CONFIG_WEB_DAV_URL");
  storeSetting("CONFIG_USER");
  storeSetting("CONFIG_APP_PASSWORD");

  loadDocument();
});

function loadDocument() {
  loadSettings();

  if (!webdavUrl || !username || !appPassword) {
    console.log("No configuration - aborting!");
    setStatus('No config!');
    return;
  }

  // Create a new web request
  let request = new XMLHttpRequest();

  request.onload = function () {
    console.log("Request finished");
    if (this.status >= 200 && this.status < 300) {
      // SUCCESS! The file content is in request.responseText
      // Persist the raw document so other functions in this file can access it.
      documentLines = splitDocumentIntoLines(this.responseText || '');
      checklistItems = ExtractItemsFromLines(documentLines);
      listTitle = webdavUrl.split('/').pop() || 'Checklist';

      sendItemsToWatch();

      if (checklistItems.length === 0) {
        console.log('No unchecked checklist items found.');
        setStatus('All done!');
      } else {
        console.log('Unchecked items:');
        checklistItems.forEach((it, idx) => console.log(`${idx + 1}: ${it}`));
      }
    } else {
      // FAILED
      console.log('Error reading file: ' + this.status + ' ' + this.responseText);
      setStatus('Load error: ' + this.status);
      resetState();
      sendItemsToWatch();
    }
  };

  request.onerror = function () {
    console.log('Request failed!');
    setStatus('Network error!');
    resetState();
    sendItemsToWatch();
  };

  request.open('GET', webdavUrl, true, username, appPassword);
  request.send();
  console.log('Sent request');
  setStatus('', true);
}

function listFolder() {
  loadSettings();

  if (!webdavUrl || !username || !appPassword) {
    console.log("No configuration - aborting!");
    return;
  }

  // Extract just the path portion of the URL so we can strip it from hrefs later.
  // e.g. "https://host/remote.php/dav/files/user/folder/" -> "/remote.php/dav/files/user/folder/"
  let folderPath = webdavUrl.replace(/^https?:\/\/[^\/]+/, '');
  if (!folderPath.endsWith('/')) folderPath += '/';

  let request = new XMLHttpRequest();

  request.onload = function () {
    console.log("PROPFIND status: " + this.status);
    if (this.status === 207) {
      // Extract href values — namespace prefix varies by server so match any prefix
      let hrefRegex = /<[a-zA-Z0-9]*:?href[^>]*>([^<]+)<\/[a-zA-Z0-9]*:?href>/gi;
      let match;
      let files = [];
      while ((match = hrefRegex.exec(this.responseText)) !== null) {
        let href = match[1].trim();
        if (!href.endsWith('.md')) continue;
        // Make path relative to the configured folder
        let rel = href.startsWith(folderPath) ? href.slice(folderPath.length) : href;
        files.push(rel);
      }

      console.log("Found " + files.length + " markdown file(s):");
      files.forEach(function (f) { console.log(" - " + f); });

      listTitle = 'Select files';
      checklistItems = files.map(function (rel) {
        return { name: rel, line: 0, checked: false };
      });
      sendItemsToWatch();
    } else {
      console.log("PROPFIND failed with status: " + this.status + " " + this.responseText);
    }
  };

  request.onerror = function () {
    console.log("PROPFIND request failed (network error)");
  };

  request.open('PROPFIND', webdavUrl, true, username, appPassword);
  request.setRequestHeader('Depth', 'infinity');
  request.setRequestHeader('Content-Type', 'application/xml; charset=utf-8');
  request.send('<?xml version="1.0" encoding="utf-8"?><propfind xmlns="DAV:"><prop><resourcetype/><getcontenttype/></prop></propfind>');
  console.log("Sent PROPFIND to: " + webdavUrl);
}

Pebble.addEventListener('ready',
  function (e) {
    loadSettings();
    if (webdavUrl.endsWith('/')) {
      listFolder();
    } else {
      loadDocument();
    }
  }
);

function splitDocumentIntoLines(doc) {
  return doc.split(/\r?[\n]/);
}

const ITEM_REGEX = /^\s*-\s\[\s\]\s*(.*)$/;

// Parse the file and extract only unchecked items (lines starting with: - [ ] )
// Split into lines and capture the item text after the '- [ ]' prefix
function ExtractItemsFromLines(lines) {
  let uncheckedItems = lines
    .map((line, line_index) => {

      const m = line.match(ITEM_REGEX);
      return m ? { match: m[1].trim(), line: line_index } : null;
    })
    .filter(Boolean) // remove nulls/empty
    .map(item => {
      return {
        name: item.match,
        line: item.line,
        checked: false,
      };
    });

  return uncheckedItems;
}

// Send items to the watch one-by-one using numeric keys.
// We use keys: count, index, and item (string)
// This is robust against AppMessage size limits and lets the C side
// assemble items deterministically.
function sendItemsToWatch() {
  // First send the total count once. On success, stream items one-by-one.
  let countPayload = {};
  countPayload[keys.ITEMS_COUNT] = checklistItems.length;
  countPayload[keys.LIST_TITLE] = listTitle;

  Pebble.sendAppMessage(countPayload,
    function () {
      if (checklistItems.length > 0) {
        sendNextItem(checklistItems, 0);
      }
    },
    function (err) {
      console.log('Send count failed: ' + JSON.stringify(err));
      // retry the whole sequence after a short delay
      setTimeout(function () { sendItemsToWatch(); }, 500);
    }
  );
}

function sendNextItem(items, index) {
  console.log(`Sending item at index ${index}`);

  if (index >= items.length) {
    // We're done here...
    setStatus('');
    return;
  }

  let payload = {};
  payload[keys.ITEMS_INDEX] = index;
  payload[keys.ITEMS_ITEM] = items[index].name;
  console.log(`Sending ${index} - ${items[index].name}`);

  Pebble.sendAppMessage(payload,
    () => {
      console.log('')
      sendNextItem(items, index + 1);
    },
    (err) => {
      console.log('Send failed for index ' + index + ': ' + JSON.stringify(err));
      // retry this item after a short delay
      setTimeout(function () { sendNextItem(items, index); }, 500);
    }
  );
}

function setItemCheckedState(index, checked) {
  if (index < 0 || index >= checklistItems.length) {
    console.log('Invalid item index: ' + index);
    return;
  }

  let lineNr = checklistItems[index].line;
  let line = documentLines[checklistItems[index].line];
  documentLines[lineNr] = checked ? line.replace("- [ ]", "- [x]") : line.replace("- [x]", "- [ ]");

  checklistItems[index].checked = checked;

  console.log(`New document: \n${documentLines.join('\n')}`);
  uploadUpdatedDocument();
}

function uploadUpdatedDocument() {
  let updatedDocument = documentLines.join('\n');

  // Create a new web request
  let request = new XMLHttpRequest();

  request.onload = function () {
    if (this.status >= 200 && this.status < 300) {
      console.log("Successfully updated the file on the server.");
      setStatus('');
    } else {
      // FAILED
      console.log('Error updating file: ' + this.status + ' ' + this.responseText);
      setStatus('Upload err: ' + this.status);
    }
  };

  request.onerror = function () {
    console.log('Request failed!');
    setStatus('Upload error!');
  };
  request.open('PUT', webdavUrl, true, username, appPassword);
  request.setRequestHeader('Content-Type', 'text/markdown');
  request.send(updatedDocument);
  console.log('Sent updated document to server.');
  setStatus('', true);
}

// Listen for messages from the watch (item actions) and log them.
Pebble.addEventListener('appmessage', function (e) {
  try {
    var payload = e.payload || {};
    let retrieve = (key) => {
      let maybe = payload[key];
      if (typeof maybe !== 'undefined') {
        return maybe;
      }
      maybe = payload[keys[key]];
      if (typeof maybe !== 'undefined') {
        return maybe;
      }
      return null;
    };

    let checked = retrieve("ITEM_CHECKED");
    let unchecked = retrieve("ITEM_UNCHECKED");
    if (checked != null) {
      console.log('Received item CHECKED from watch: index=', checked);
      setItemCheckedState(checked, true);
    } else if (unchecked != null) {
      console.log('Received item UNCHECKED from watch: index=', unchecked);
      setItemCheckedState(unchecked, false);
    } else {
      console.log('Received unknown appmessage payload:', payload);
    }
  } catch (ex) {
    console.log('Error handling appmessage: ' + ex);
  }
});


// Send a short status message to the watch. Kept short to fit the UI.
function setStatus(text, progressing = false) {
  try {
    var payload = {};
    payload[keys.SET_STATUS] = text || '';
    payload[keys.SET_PROGRESSING] = progressing ? 1 : 0;

    Pebble.sendAppMessage(payload,
      function () { },
      function (err) { console.log('Status send failed: ' + JSON.stringify(err)); }
    );
  } catch (ex) {
    console.log('Error sending status: ' + ex);
  }
}