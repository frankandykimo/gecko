// |reftest| error:SyntaxError
// Copyright 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Caitlin Potter <caitp@igalia.com>
esid: 12.1.1
description: >
  `yield` is a reserved keyword within async generator function bodies and may
  not be used as a label.
negative:
  phase: early
  type: SyntaxError
features: [async-iteration]
---*/

(async function*() {
  yield: 1;
});
