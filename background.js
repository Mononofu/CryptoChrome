function plugin0() {
  return document.getElementById('plugin');
}

plugin = plugin0;

function saveToClipboard(text) {
  // now save it to clipboard
  var textarea = document.getElementById("tmp-clipboard");

  // now we put the message in the textarea
  textarea.value = text;

  // and copy the text from the textarea
  textarea.select();
  document.execCommand("copy", false, null);
}

function readClipboard() {
  var textarea = document.getElementById("tmp-clipboard");
  textarea.select();
  document.execCommand("paste", false, null);
  return textarea.value;
}

function replaceText(tab_id, next_txt) {
  chrome.tabs.sendRequest(tab_id, {cmd: "replace_selected", text: next_txt}, function() {});
}

function version() {
  try {
    return plugin().gpg_version();
  } catch(e) {
    if (e.type == "undefined_method") {
      return "Plugin Unavailable.";
    } else {
      return e;
    }
  }
}

function setPath(path) {
  if (path.length > 0 && localStorage["gpg-path"] != "gpg") {
    localStorage["gpg-path"] = path;
  } else {
    localStorage.removeItem("gpg-path");
  }
  return plugin().set_gpg_path(path);
}


function encryptText(clear_txt) {
  if(clear_txt && clear_txt.length) {
    var recipient = prompt("Please enter the recipients email","");
    return plugin().encrypt(recipient, clear_txt);
  }
  return "";
}

function decryptText(cipher_txt) {
  if (cipher_txt && cipher_txt.length) {
    return plugin().decrypt(cipher_txt);
  }
  return "";
}

function clearsignText(clear_txt) {
  if (clear_txt && clear_txt.length) {
    return plugin().clearsign(clear_txt);
  }
  return "";
}

function encryptSignText (clear_txt) {
  if(clear_txt && clear_txt.length) {
    var recipient = prompt("Please enter the recipients email","");
    return plugin().encrypt_sign(recipient, clear_txt);
  }
  return "";
}

function withTabSelection (tab, processor, replace) {
  responseHandler = replace ? replaceText.bind(null, tab.id) : saveToClipboard;

  chrome.tabs.executeScript(tab.id, {"file": "content_script.js", "allFrames": true}, function() {
    chrome.tabs.sendRequest(tab.id, {cmd: "get_selected"}, function(response) {
      responseHandler(processor(response.msg));
    });
  });
}

function withClipboard(tab, processor) {
  text = readClipboard();

  chrome.tabs.executeScript(tab.id, {"file": "content_script.js"}, function() {
    var processed = processor(text);
    replaceText(tab.id, processed);
  });
}


function load()
{
  // load path from storage
  var path = localStorage["gpg-path"];
  if(path != undefined)
    setPath(path);

  // add entry to context menu
  var root = chrome.contextMenus.create({"title": "CryptoChrome", "contexts": ["selection", "editable"]});

  var selection = root;
  //var selection = chrome.contextMenus.create({"parentId": root, "title": "Selection", "contexts": ["selection"]});
  chrome.contextMenus.create({"parentId": selection, "title": "Selection - Encrypt", "contexts":["selection"], "onclick": function(info, tab) {
    withTabSelection(tab, encryptText, info.editable);
  } });
  chrome.contextMenus.create({"parentId": selection, "title": "Selection - Clearsign", "contexts":["selection"], "onclick": function(info, tab) {
    withTabSelection(tab, clearsignText, info.editable);
  } });
  chrome.contextMenus.create({"parentId": selection, "title": "Selection - Encrypt && Sign", "contexts":["selection"], "onclick": function(info, tab) {
    withTabSelection(tab, encryptSignText, info.editable);
  } });
  chrome.contextMenus.create({"parentId": selection, "title": "Selection - Decrypt", "contexts":["selection"], "onclick": function(info, tab) {
    withTabSelection(tab, decryptText, info.editable);
  } });

  var fromClipboard = root;
  //var fromClipboard = chrome.contextMenus.create({"parentId": root, "title": "From Clipboard", "contexts": ["editable"]});
  chrome.contextMenus.create({"parentId": fromClipboard, "title": "From Clipboard - Encrypt", "contexts":["editable"], "onclick": function(info, tab) {
    withClipboard(tab, encryptText);
  } });
  chrome.contextMenus.create({"parentId": fromClipboard, "title": "From Clipboard - Clearsign", "contexts":["editable"], "onclick": function(info, tab) {
    withClipboard(tab, clearsignText);
  } });
  chrome.contextMenus.create({"parentId": fromClipboard, "title": "From Clipboard - Encrypt && Sign", "contexts":["editable"], "onclick": function(info, tab) {
    withClipboard(tab, encryptSignText);
  } });
  chrome.contextMenus.create({"parentId": fromClipboard, "title": "From Clipboard - Decrypt", "contexts":["editable"], "onclick": function(info, tab) {
    withClipboard(tab, decryptText);
  } });
}

chrome.extension.onRequest.addListener(function(request, sender, sendResponse) {
  if (request.cmd == "version") {
    path = localStorage["gpg-path"] || "";
    sendResponse({version:version(), path:path});
  } else if (request.cmd == "setpath") {
    sendResponse({version:setPath(request.path)});
  }
});


window.onload = load;
