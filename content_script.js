var isSelectedFrame = function() {
  // Top window.
  if ((""+window.getSelection()).length > 0) {
    return window;
  }
  return null;
}

var getNodeWithText = function(root, string) {
  if(root.nodeType == Node.TEXT_NODE && root.data.indexOf(string) >= 0) {
    return root;
  } else if (root.nodeType != Node.TEXT_NODE) {
    if (root.value && root.value.indexOf(string) >= 0) {
      return root;
    }

    for (var i=0; i < root.childNodes.length;i++) {
      var ret = getNodeWithText(root.childNodes[i], string);
      if (ret) {
        return ret;
      }
    }
  }
  return null;
}

var channel = channel || function(request, sender, sendResponse) {
  console.log(sender.tab ?
      "from a content script:" + sender.tab.url : "from the extension");
  // The background page is asking us for the selected text
  if (request.cmd == "get_selected")
  {
    var txt = '';
    if(!isSelectedFrame()) {
      return;
    }
	if (window.getSelection) {
	  txt = window.getSelection();
	} else if (window.document.getSelection) {
	  txt = window.document.getSelection();
	}
    sendResponse( { msg: ""+txt } );
  } else if (request.cmd == "replace_selected") {
    var replacement = request.text;
    if(!isSelectedFrame()) {
      return;
    }
    var selection = window.getSelection();
    var el = getNodeWithText(selection.anchorNode, selection.toString());
    if (!el) {
      alert(replacement);
    } else if (el.nodeType == Node.TEXT_NODE) {
      var pos = el.data.indexOf(selection.toString());
      var newText = window.document.createTextNode(el.data.substring(0,pos) + replacement + el.data.substring(pos+selection.toString().length));
      el.parentNode.insertBefore(newText, el);
      el.parentNode.removeChild(el);
    } else {
      var pos = el.value.indexOf(selection.toString());
      el.value = el.value.substring(0,pos) + replacement + el.value.substring(pos+selection.toString().length);
    }
  } else {
    sendResponse({}); // snub them.
  }
};

if (!chrome.extension.onRequest.hasListener(channel)) {
  chrome.extension.onRequest.addListener(channel);
}

// Search the text nodes for PGP message.
// Return null if none is found.

/*function decrypt() {
	console.log("called");
	chrome.extension.sendRequest($("#crypto_text").text(), function(response) {
		console.log(response);
		$("#crypto_text").text(response)
	});
	}

var findCrypto = function() {
	var found;
	var re = /-----[\s\S]+?-----[\s\S]+?-----[\s\S]+?-----/gm;
	var matches = [];
	var node = document.body;

	while (e = re.exec(node.innerHTML))
	{
		// store match position and original text
		if (e[0].search("PGP") > 0)
		{
			matches.push( [re.lastIndex - e[0].length,
						   e[0],
						   e[0].replace(/<[^>]+>/g, "")] );
		}
	}

	var template = '<div style="outline-width:5px; outline-style:solid; outline-color:red"><pre style="word-wrap; break-word; white-space; pre-wrap;"><div id="crypto_text">cryptotext<div><a href="#">Decrypt</a></div></div></div></pre>';
	

	for (var i = matches.length - 1; i >= 0; --i)
	{
		node.innerHTML = node.innerHTML.replace(matches[i][1], template.replace("cryptotext", matches[i][2]) );
	}

	$("#crypto_text > div > a").click(decrypt); 
  return null;
}*/

