var LibraryWget = {
  /**
   * @license
   * Copyright 2011 The Emscripten Authors
   * SPDX-License-Identifier: MIT
   */

  $wget: {
    wgetRequests: {},
    nextWgetRequestHandle: 0,

    getNextWgetRequestHandle() {
      var handle = wget.nextWgetRequestHandle;
      wget.nextWgetRequestHandle++;
      return handle;
    },
  },

  emscripten_async_wget2__deps: ['$PATH_FS', '$wget', '$stringToNewUTF8'],
  emscripten_async_wget2__proxy: 'sync',
  emscripten_async_wget2: (url, file, request, param, userdata, onload, onerror, onprogress) => {
    {{{ runtimeKeepalivePush() }}}

    var _url = UTF8ToString(url);
    var _file = UTF8ToString(file);
    _file = PATH_FS.resolve(_file);
    var _request = UTF8ToString(request);
    var _param = UTF8ToString(param);
    var index = _file.lastIndexOf('/');

    var http = new XMLHttpRequest();
    http.open(_request, _url, true);
    http.responseType = 'arraybuffer';

    var handle = wget.getNextWgetRequestHandle();

    var destinationDirectory = PATH.dirname(_file);

    // LOAD
    http.onload = (e) => {
      {{{ runtimeKeepalivePop() }}}
      if (http.status >= 200 && http.status < 300) {
        // if a file exists there, we overwrite it
        try {
          FS.unlink(_file);
        } catch (e) {}
        // if the destination directory does not yet exist, create it
        FS.mkdirTree(destinationDirectory);

        FS.createDataFile( _file.substr(0, index), _file.substr(index + 1), new Uint8Array(/** @type{ArrayBuffer}*/(http.response)), true, true, false);
        if (onload) {
          var _file_ptr = stringToNewUTF8(_file);
          {{{ makeDynCall('vipp', 'onload') }}}(handle, userdata, _file_ptr);
          _free(_file_ptr);
        }
      } else {
        if (onerror) {{{ makeDynCall('vipi', 'onerror') }}}(handle, userdata, http.status);
      }

      delete wget.wgetRequests[handle];
    };

    // ERROR
    http.onerror = (e) => {
      {{{ runtimeKeepalivePop() }}}
      if (onerror) {{{ makeDynCall('vipi', 'onerror') }}}(handle, userdata, http.status);
      delete wget.wgetRequests[handle];
    };

    // PROGRESS
    http.onprogress = (e) => {
      var percentComplete;
      if (e.lengthComputable || (e.lengthComputable === undefined && e.total != 0)) {
        percentComplete = (e.loaded / e.total) * 100;
      } else {
        percentComplete = bytesToPercent(e.loaded);
      }
      if (onprogress) {{{ makeDynCall('vipi', 'onprogress') }}}(handle, userdata, percentComplete);
    };

    // ABORT
    http.onabort = (e) => {
      {{{ runtimeKeepalivePop() }}}
      delete wget.wgetRequests[handle];
    };

    if (_request == "POST") {
      // Send the proper header information along with the request
      http.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
      http.send(_param);
    } else {
      http.send(null);
    }

    wget.wgetRequests[handle] = http;

    return handle;
  },

  emscripten_async_wget2_abort__deps: ['$wget'],
  emscripten_async_wget2_abort__proxy: 'sync',
  emscripten_async_wget2_abort: (handle) => {
    var http = wget.wgetRequests[handle];
    if (http) {
      http.abort();
    }
  },
};

addToLibrary(LibraryWget);
