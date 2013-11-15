// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="assert.js">

/**
 * The global object.
 * @type {!Object}
 * @const
 */
var global = this;

/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * Calls chrome.send with a callback and restores the original afterwards.
 * @param {string} name The name of the message to send.
 * @param {!Array} params The parameters to send.
 * @param {string} callbackName The name of the function that the backend calls.
 * @param {!Function} callback The function to call.
 */
function chromeSend(name, params, callbackName, callback) {
  var old = global[callbackName];
  global[callbackName] = function() {
    // restore
    global[callbackName] = old;

    var args = Array.prototype.slice.call(arguments);
    return callback.apply(global, args);
  };
  chrome.send(name, params);
}

/**
 * Returns the scale factors supported by this platform.
 * @return {array} The supported scale factors.
 */
function getSupportedScaleFactors() {
  var supportedScaleFactors = [];
  if (cr.isMac || cr.isChromeOS) {
    supportedScaleFactors.push(1);
    supportedScaleFactors.push(2);
  } else {
    // Windows must be restarted to display at a different scale factor.
    supportedScaleFactors.push(window.devicePixelRatio);
  }
  return supportedScaleFactors;
}

/**
 * Generates a CSS url string.
 * @param {string} s The URL to generate the CSS url for.
 * @return {string} The CSS url string.
 */
function url(s) {
  // http://www.w3.org/TR/css3-values/#uris
  // Parentheses, commas, whitespace characters, single quotes (') and double
  // quotes (") appearing in a URI must be escaped with a backslash
  var s2 = s.replace(/(\(|\)|\,|\s|\'|\"|\\)/g, '\\$1');
  // WebKit has a bug when it comes to URLs that end with \
  // https://bugs.webkit.org/show_bug.cgi?id=28885
  if (/\\\\$/.test(s2)) {
    // Add a space to work around the WebKit bug.
    s2 += ' ';
  }
  return 'url("' + s2 + '")';
}

/**
 * Generates a CSS -webkit-image-set for a chrome:// url.
 * An entry in the image set is added for each of getSupportedScaleFactors().
 * The scale-factor-specific url is generated by replacing the first instance of
 * 'scalefactor' in |path| with the numeric scale factor.
 * @param {string} path The URL to generate an image set for.
 *     'scalefactor' should be a substring of |path|.
 * @return {string} The CSS -webkit-image-set.
 */
function imageset(path) {
  var supportedScaleFactors = getSupportedScaleFactors();

  var replaceStartIndex = path.indexOf('scalefactor');
  if (replaceStartIndex < 0)
    return url(path);

  var s = '';
  for (var i = 0; i < supportedScaleFactors.length; ++i) {
    var scaleFactor = supportedScaleFactors[i];
    var pathWithScaleFactor = path.substr(0, replaceStartIndex) + scaleFactor +
        path.substr(replaceStartIndex + 'scalefactor'.length);

    s += url(pathWithScaleFactor) + ' ' + scaleFactor + 'x';

    if (i != supportedScaleFactors.length - 1)
      s += ', ';
  }
  return '-webkit-image-set(' + s + ')';
}

/**
 * Parses query parameters from Location.
 * @param {string} location The URL to generate the CSS url for.
 * @return {object} Dictionary containing name value pairs for URL
 */
function parseQueryParams(location) {
  var params = {};
  var query = unescape(location.search.substring(1));
  var vars = query.split('&');
  for (var i = 0; i < vars.length; i++) {
    var pair = vars[i].split('=');
    params[pair[0]] = pair[1];
  }
  return params;
}

function findAncestorByClass(el, className) {
  return findAncestor(el, function(el) {
    if (el.classList)
      return el.classList.contains(className);
    return null;
  });
}

/**
 * Return the first ancestor for which the {@code predicate} returns true.
 * @param {Node} node The node to check.
 * @param {function(Node) : boolean} predicate The function that tests the
 *     nodes.
 * @return {Node} The found ancestor or null if not found.
 */
function findAncestor(node, predicate) {
  var last = false;
  while (node != null && !(last = predicate(node))) {
    node = node.parentNode;
  }
  return last ? node : null;
}

function swapDomNodes(a, b) {
  var afterA = a.nextSibling;
  if (afterA == b) {
    swapDomNodes(b, a);
    return;
  }
  var aParent = a.parentNode;
  b.parentNode.replaceChild(a, b);
  aParent.insertBefore(b, afterA);
}

/**
 * Disables text selection and dragging, with optional whitelist callbacks.
 * @param {function(Event):boolean=} opt_allowSelectStart Unless this function
 *    is defined and returns true, the onselectionstart event will be
 *    surpressed.
 * @param {function(Event):boolean=} opt_allowDragStart Unless this function
 *    is defined and returns true, the ondragstart event will be surpressed.
 */
function disableTextSelectAndDrag(opt_allowSelectStart, opt_allowDragStart) {
  // Disable text selection.
  document.onselectstart = function(e) {
    if (!(opt_allowSelectStart && opt_allowSelectStart.call(this, e)))
      e.preventDefault();
  };

  // Disable dragging.
  document.ondragstart = function(e) {
    if (!(opt_allowDragStart && opt_allowDragStart.call(this, e)))
      e.preventDefault();
  };
}

/**
 * Call this to stop clicks on <a href="#"> links from scrolling to the top of
 * the page (and possibly showing a # in the link).
 */
function preventDefaultOnPoundLinkClicks() {
  document.addEventListener('click', function(e) {
    var anchor = findAncestor(e.target, function(el) {
      return el.tagName == 'A';
    });
    // Use getAttribute() to prevent URL normalization.
    if (anchor && anchor.getAttribute('href') == '#')
      e.preventDefault();
  });
}

/**
 * Check the directionality of the page.
 * @return {boolean} True if Chrome is running an RTL UI.
 */
function isRTL() {
  return document.documentElement.dir == 'rtl';
}

/**
 * Get an element that's known to exist by its ID. We use this instead of just
 * calling getElementById and not checking the result because this lets us
 * satisfy the JSCompiler type system.
 * @param {string} id The identifier name.
 * @return {!Element} the Element.
 */
function getRequiredElement(id) {
  var element = $(id);
  assert(element, 'Missing required element: ' + id);
  return element;
}

// Handle click on a link. If the link points to a chrome: or file: url, then
// call into the browser to do the navigation.
document.addEventListener('click', function(e) {
  if (e.defaultPrevented)
    return;

  var el = e.target;
  if (el.nodeType == Node.ELEMENT_NODE &&
      el.webkitMatchesSelector('A, A *')) {
    while (el.tagName != 'A') {
      el = el.parentElement;
    }

    if ((el.protocol == 'file:' || el.protocol == 'about:') &&
        (e.button == 0 || e.button == 1)) {
      chrome.send('navigateToUrl', [
        el.href,
        el.target,
        e.button,
        e.altKey,
        e.ctrlKey,
        e.metaKey,
        e.shiftKey
      ]);
      e.preventDefault();
    }
  }
});

/**
 * Creates a new URL which is the old URL with a GET param of key=value.
 * @param {string} url The base URL. There is not sanity checking on the URL so
 *     it must be passed in a proper format.
 * @param {string} key The key of the param.
 * @param {string} value The value of the param.
 * @return {string} The new URL.
 */
function appendParam(url, key, value) {
  var param = encodeURIComponent(key) + '=' + encodeURIComponent(value);

  if (url.indexOf('?') == -1)
    return url + '?' + param;
  return url + '&' + param;
}

/**
 * Creates a CSS -webkit-image-set for a favicon request.
 * @param {string} url The url for the favicon.
 * @param {number=} opt_size Optional preferred size of the favicon.
 * @param {string=} opt_type Optional type of favicon to request. Valid values
 *     are 'favicon' and 'touch-icon'. Default is 'favicon'.
 * @return {string} -webkit-image-set for the favicon.
 */
function getFaviconImageSet(url, opt_size, opt_type) {
  var size = opt_size || 16;
  var type = opt_type || 'favicon';
  return imageset(
      'chrome://' + type + '/size/' + size + '@scalefactorx/' + url);
}

/**
 * Creates a new URL for a favicon request for the current device pixel ratio.
 * The URL must be updated when the user moves the browser to a screen with a
 * different device pixel ratio. Use getFaviconImageSet() for the updating to
 * occur automatically.
 * @param {string} url The url for the favicon.
 * @param {number=} opt_size Optional preferred size of the favicon.
 * @param {string=} opt_type Optional type of favicon to request. Valid values
 *     are 'favicon' and 'touch-icon'. Default is 'favicon'.
 * @return {string} Updated URL for the favicon.
 */
function getFaviconUrlForCurrentDevicePixelRatio(url, opt_size, opt_type) {
  var size = opt_size || 16;
  var type = opt_type || 'favicon';
  return 'chrome://' + type + '/size/' + size + '@' +
      window.devicePixelRatio + 'x/' + url;
}

/**
 * Creates an element of a specified type with a specified class name.
 * @param {string} type The node type.
 * @param {string} className The class name to use.
 * @return {Element} The created element.
 */
function createElementWithClassName(type, className) {
  var elm = document.createElement(type);
  elm.className = className;
  return elm;
}

/**
 * webkitTransitionEnd does not always fire (e.g. when animation is aborted
 * or when no paint happens during the animation). This function sets up
 * a timer and emulate the event if it is not fired when the timer expires.
 * @param {!HTMLElement} el The element to watch for webkitTransitionEnd.
 * @param {number} timeOut The maximum wait time in milliseconds for the
 *     webkitTransitionEnd to happen.
 */
function ensureTransitionEndEvent(el, timeOut) {
  var fired = false;
  el.addEventListener('webkitTransitionEnd', function f(e) {
    el.removeEventListener('webkitTransitionEnd', f);
    fired = true;
  });
  window.setTimeout(function() {
    if (!fired)
      cr.dispatchSimpleEvent(el, 'webkitTransitionEnd');
  }, timeOut);
}

/**
 * Alias for document.scrollTop getter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @return {number} The Y document scroll offset.
 */
function scrollTopForDocument(doc) {
  return doc.documentElement.scrollTop || doc.body.scrollTop;
}

/**
 * Alias for document.scrollTop setter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @param {number} value The target Y scroll offset.
 */
function setScrollTopForDocument(doc, value) {
  doc.documentElement.scrollTop = doc.body.scrollTop = value;
}

/**
 * Alias for document.scrollLeft getter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @return {number} The X document scroll offset.
 */
function scrollLeftForDocument(doc) {
  return doc.documentElement.scrollLeft || doc.body.scrollLeft;
}

/**
 * Alias for document.scrollLeft setter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @param {number} value The target X scroll offset.
 */
function setScrollLeftForDocument(doc, value) {
  doc.documentElement.scrollLeft = doc.body.scrollLeft = value;
}
