// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_ACTIONS_DIR = Object.freeze({
  isDirectory: true,
  name: 'actions',
  size: 0,
  modificationTime: new Date(2014, 3, 27, 9, 38, 14)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_NO_ACTIONS_DIR = Object.freeze({
  isDirectory: true,
  name: 'no-actions',
  size: 0,
  modificationTime: new Date(2014, 3, 27, 9, 38, 14)
});

/**
 * @type {Array<Object>}
 * @const
 */
var TESTING_ACTIONS_DIR_ACTIONS = Object.freeze([
  {
    id: "SHARE"
  },
  {
    id: "SomeCustomAction",
    title: "Do something custom"
  }
]);

/**
 * Returns a list of actions for the requested entry
 *
 * @param {GetActionsRequestedOptions} options Options.
 * @param {function(Array<Object>)} onSuccess Success callback with a list of
 *     actions.
 * @param {function(string)} onError Error callback with an error code.
 */
function onGetActionsRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath === '/' + TESTING_ACTIONS_DIR.name) {
    onSuccess(TESTING_ACTIONS_DIR_ACTIONS);
    return;
  }

  if (options.entryPath === '/' + TESTING_NO_ACTIONS_DIR.name) {
    onSuccess([]);
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);

  test_util.defaultMetadata['/' + TESTING_ACTIONS_DIR.name] =
      TESTING_ACTIONS_DIR;
  test_util.defaultMetadata['/' + TESTING_NO_ACTIONS_DIR.name] =
      TESTING_NO_ACTIONS_DIR;

  chrome.fileSystemProvider.onGetActionsRequested.addListener(
      onGetActionsRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Get actions for a directory with actions.
    function getActionsSuccess() {
      test_util.fileSystem.root.getDirectory(
          TESTING_ACTIONS_DIR.name,
          {create: false},
          chrome.test.callbackPass(function(dirEntry) {
            chrome.fileManagerPrivate.getEntryActions(
                dirEntry,
                chrome.test.callbackPass(function(actions) {
                  chrome.test.assertEq(2, actions.length);
                  chrome.test.assertEq(TESTING_ACTIONS_DIR_ACTIONS[0].id,
                      actions[0].id);
                  chrome.test.assertFalse(!!actions[0].title);
                  chrome.test.assertEq(TESTING_ACTIONS_DIR_ACTIONS[1].id,
                      actions[1].id);
                  chrome.test.assertEq(
                      TESTING_ACTIONS_DIR_ACTIONS[1].title,
                      actions[1].title);
                }));
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Get actions for a directory with no actions.
    function getNoActionsSuccess() {
      test_util.fileSystem.root.getDirectory(
          TESTING_NO_ACTIONS_DIR.name,
          {create: false},
          chrome.test.callbackPass(function(dirEntry) {
            chrome.fileManagerPrivate.getEntryActions(
                dirEntry,
                chrome.test.callbackPass(function(actions) {
                  chrome.test.assertEq(0, actions.length);
                }));
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
