var isSelectedFrame = function() {
  // Top window.
  if ((""+window.getSelection()).length > 0) {
    return window;
  }
  return null;
}

var channel = channel || function(request, sender, sendResponse) {
  // The background page is asking for selected text.
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
  // The background page is asking us to replace selection with processed text.
  } else if (request.cmd == "replace_selected") {
    var replacement = request.text;
    if(!isSelectedFrame()) {
      return;
    }
    var selection = window.getSelection();
    if (selection.anchorNode == selection.focusNode && selection.anchorNode.nodeType == Node.TEXT_NODE) {
      var el = selection.anchorNode;
      var pos = el.data.indexOf(selection.toString());
      var newText = window.document.createTextNode(el.data.substring(0,pos) + replacement + el.data.substring(pos+selection.toString().length));
      el.parentNode.insertBefore(newText, el);
      el.parentNode.removeChild(el);
    } else if (selection.anchorNode == selection.focusNode && selection.anchorNode.value) {
      var el = selection.anchorNode;
      var pos = el.value.indexOf(selection.toString());
      el.value = el.value.substring(0,pos) + replacement + el.value.substring(pos+selection.toString().length);
    } else if (selection.anchorNode != selection.focusNode) {
      if (selection.anchorOffset > 0) {
      }
      selection.deleteFromDocument();
      var el = selection.anchorNode;
      var newText = document.createElement("span");
      newText.innerHTML = replacement.replace(/\n/g,"<br/>");
      if (el.nodeType == Node.TEXT_NODE) {
        el.parentNode.insertBefore(newText, el);
        el.parentNode.removeChild(el);
      } else {
        el.appendChild(newText);
      }
    } else {
      // A single selected element with no value or text.  Hopefully shouldn't happen.
      alert(replacement);
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

