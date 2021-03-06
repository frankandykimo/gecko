# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
    '../../cmd/platlibs.gypi'
  ],
  'targets': [
    {
      'target_name': 'certcgi',
      'type': 'executable',
      'sources': [
        'certcgi.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/sqlite/sqlite.gyp:sqlite3'
      ]
    }
  ],
  'target_defaults': {
    'defines': [
      'NSPR20',
      'NSS_USE_STATIC_LIBS'
    ]
  },
  'variables': {
    'module': 'nss',
    'use_static_libs': 1
  }
}